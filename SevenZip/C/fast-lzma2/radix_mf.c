/*
* Copyright (c) 2018, Conor McCarthy
* All rights reserved.
*
* This source code is licensed under both the BSD-style license (found in the
* LICENSE file in the root directory of this source tree) and the GPLv2 (found
* in the COPYING file in the root directory of this source tree).
* You may select, at your option, one of the above-listed licenses.
*/

#include <stddef.h>     /* size_t, ptrdiff_t */
#include <stdlib.h>     /* malloc, free */
#include "fast-lzma2.h"
#include "fl2_errors.h"
#include "mem.h"          /* U32, U64, MEM_64bits */
#include "fl2_internal.h"
#include "radix_internal.h"

#if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 407)
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized" /* warning: 'rpt_head_next' may be used uninitialized in this function */
#elif defined(_MSC_VER)
#  pragma warning(disable : 4701) /* warning: 'rpt_head_next' may be used uninitialized in this function */
#endif

#define MATCH_BUFFER_SHIFT 8;
#define MATCH_BUFFER_ELBOW_BITS 17
#define MATCH_BUFFER_ELBOW (1UL << MATCH_BUFFER_ELBOW_BITS)
#define MIN_MATCH_BUFFER_SIZE 256U /* min buffer size at least FL2_SEARCH_DEPTH_MAX + 2 for bounded build */
#define MAX_MATCH_BUFFER_SIZE (1UL << 24) /* max buffer size constrained by 24-bit link values */

static void RMF_initTailTable(RMF_builder* const tbl)
{
    for (size_t i = 0; i < RADIX8_TABLE_SIZE; i += 2) {
        tbl->tails_8[i].prev_index = RADIX_NULL_LINK;
        tbl->tails_8[i + 1].prev_index = RADIX_NULL_LINK;
    }
    for (size_t i = 0; i < RADIX16_TABLE_SIZE; i += 2) {
        tbl->tails_16[i].prev_index = RADIX_NULL_LINK;
        tbl->tails_16[i + 1].prev_index = RADIX_NULL_LINK;
    }
}

static RMF_builder* RMF_createBuilder(size_t match_buffer_size)
{
    match_buffer_size = MIN(match_buffer_size, MAX_MATCH_BUFFER_SIZE);
    match_buffer_size = MAX(match_buffer_size, MIN_MATCH_BUFFER_SIZE);

    RMF_builder* const builder = malloc(
        sizeof(RMF_builder) + (match_buffer_size - 1) * sizeof(RMF_buildMatch));

    if (builder == NULL)
        return NULL;

    builder->match_buffer_size = match_buffer_size;
    builder->match_buffer_limit = match_buffer_size;

    RMF_initTailTable(builder);

    return builder;
}

static void RMF_freeBuilderTable(RMF_builder** const builders, unsigned const size)
{
    if (builders == NULL)
        return;

    for (unsigned i = 0; i < size; ++i)
        free(builders[i]);

    free(builders);
}

/* RMF_createBuilderTable() : 
 * Create one match table builder object per thread.
 * max_len : maximum match length supported by the table structure 
 * size : number of threads 
 */
static RMF_builder** RMF_createBuilderTable(U32* const match_table, size_t const match_buffer_size, unsigned const max_len, unsigned const size)
{
    DEBUGLOG(3, "RMF_createBuilderTable : match_buffer_size %u, builders %u", (U32)match_buffer_size, size);

    RMF_builder** const builders = malloc(size * sizeof(RMF_builder*));

    if (builders == NULL)
        return NULL;

    for (unsigned i = 0; i < size; ++i)
        builders[i] = NULL;

    for (unsigned i = 0; i < size; ++i) {
        builders[i] = RMF_createBuilder(match_buffer_size);
        if (builders[i] == NULL) {
            RMF_freeBuilderTable(builders, i);
            return NULL;
        }
        builders[i]->table = match_table;
        builders[i]->max_len = max_len;
    }
    return builders;
}

static int RMF_isStruct(size_t const dictionary_size)
{
    return dictionary_size > ((size_t)1 << RADIX_LINK_BITS);
}

/* RMF_clampParams() :
*  Make param values within valid range.
*  Return : valid RMF_parameters */
static RMF_parameters RMF_clampParams(RMF_parameters params)
{
#   define CLAMP(val,min,max) {      \
        if (val<(min)) val=(min);        \
        else if (val>(max)) val=(max);   \
    }
#   define MAXCLAMP(val,max) {      \
        if (val>(max)) val=(max);   \
    }
    CLAMP(params.dictionary_size, DICTIONARY_SIZE_MIN, MEM_64bits() ? DICTIONARY_SIZE_MAX_64 : DICTIONARY_SIZE_MAX_32);
    MAXCLAMP(params.match_buffer_resize, FL2_BUFFER_RESIZE_MAX);
    MAXCLAMP(params.overlap_fraction, FL2_BLOCK_OVERLAP_MAX);
    CLAMP(params.depth, FL2_SEARCH_DEPTH_MIN, FL2_SEARCH_DEPTH_MAX);
    return params;
#   undef MAXCLAMP
#   undef CLAMP
}

static size_t RMF_calBufSize(size_t dictionary_size, unsigned buffer_resize)
{
    size_t buffer_size = dictionary_size >> MATCH_BUFFER_SHIFT;
    if (buffer_size > MATCH_BUFFER_ELBOW) {
        size_t extra = 0;
        unsigned n = MATCH_BUFFER_ELBOW_BITS - 1;
        for (; (4UL << n) <= buffer_size; ++n)
            extra += MATCH_BUFFER_ELBOW >> 4;
        if((3UL << n) <= buffer_size)
            extra += MATCH_BUFFER_ELBOW >> 5;
        buffer_size = MATCH_BUFFER_ELBOW + extra;
    }
    if (buffer_resize > 2)
        buffer_size += buffer_size >> (4 - buffer_resize);
    else if (buffer_resize < 2)
        buffer_size -= buffer_size >> (buffer_resize + 1);
    return buffer_size;
}

/* RMF_applyParameters_internal() :
 * Set parameters to those specified.
 * Create a builder table if none exists. Free an existing one if incompatible.
 * Set match_buffer_limit and max supported match length.
 * Returns an error if dictionary won't fit.
 */
static size_t RMF_applyParameters_internal(FL2_matchTable* const tbl, const RMF_parameters* const params)
{
    int const is_struct = RMF_isStruct(params->dictionary_size);
    size_t const dictionary_size = tbl->params.dictionary_size;
    /* dictionary is allocated with the struct and is immutable */
    if (params->dictionary_size > tbl->params.dictionary_size
        || (params->dictionary_size == tbl->params.dictionary_size && is_struct > tbl->alloc_struct))
        return FL2_ERROR(parameter_unsupported);

    size_t const match_buffer_size = RMF_calBufSize(tbl->unreduced_dict_size, params->match_buffer_resize);
    tbl->params = *params;
    tbl->params.dictionary_size = dictionary_size;
    tbl->is_struct = is_struct;
    if (tbl->builders == NULL
        || match_buffer_size > tbl->builders[0]->match_buffer_size)
    {
        RMF_freeBuilderTable(tbl->builders, tbl->thread_count);
        tbl->builders = RMF_createBuilderTable(tbl->table, match_buffer_size, tbl->is_struct ? STRUCTURED_MAX_LENGTH : BITPACK_MAX_LENGTH, tbl->thread_count);
        if (tbl->builders == NULL) {
            return FL2_ERROR(memory_allocation);
        }
    }
    else {
        for (unsigned i = 0; i < tbl->thread_count; ++i) {
            tbl->builders[i]->match_buffer_limit = match_buffer_size;
            tbl->builders[i]->max_len = tbl->is_struct ? STRUCTURED_MAX_LENGTH : BITPACK_MAX_LENGTH;
        }
    }
    return 0;
}

/* RMF_reduceDict() : 
 * Reduce dictionary and match buffer size if the total input size is known and < dictionary_size.
 */
static void RMF_reduceDict(RMF_parameters* const params, size_t const dict_reduce)
{
    if (dict_reduce)
        params->dictionary_size = MIN(params->dictionary_size, MAX(dict_reduce, DICTIONARY_SIZE_MIN));
}

static void RMF_initListHeads(FL2_matchTable* const tbl)
{
    for (size_t i = 0; i < RADIX16_TABLE_SIZE; i += 2) {
        tbl->list_heads[i].head = RADIX_NULL_LINK;
        tbl->list_heads[i].count = 0;
        tbl->list_heads[i + 1].head = RADIX_NULL_LINK;
        tbl->list_heads[i + 1].count = 0;
    }
}

/* RMF_createMatchTable() :
 * Create a match table. Reduce the dict size to input size if possible.
 * A thread_count of 0 will be raised to 1.
 */
FL2_matchTable* RMF_createMatchTable(const RMF_parameters* const p, size_t const dict_reduce, unsigned const thread_count)
{
    RMF_parameters params = RMF_clampParams(*p);
    size_t unreduced_dict_size = params.dictionary_size;
    RMF_reduceDict(&params, dict_reduce);

    int const is_struct = RMF_isStruct(params.dictionary_size);
    size_t dictionary_size = params.dictionary_size;

    DEBUGLOG(3, "RMF_createMatchTable : is_struct %d, dict %u", is_struct, (U32)dictionary_size);

    size_t const table_bytes = is_struct ? ((dictionary_size + 3U) / 4U) * sizeof(RMF_unit)
        : dictionary_size * sizeof(U32);
    FL2_matchTable* const tbl = malloc(sizeof(FL2_matchTable) + table_bytes - sizeof(U32));
    if (tbl == NULL)
        return NULL;

    tbl->is_struct = is_struct;
    tbl->alloc_struct = is_struct;
    tbl->thread_count = thread_count + !thread_count;
    tbl->params = params;
    tbl->unreduced_dict_size = unreduced_dict_size;
    tbl->builders = NULL;

    RMF_applyParameters_internal(tbl, &params);

    RMF_initListHeads(tbl);

    RMF_initProgress(tbl);
    
    return tbl;
}

void RMF_freeMatchTable(FL2_matchTable* const tbl)
{
    if (tbl == NULL)
        return;

    DEBUGLOG(3, "RMF_freeMatchTable");

    RMF_freeBuilderTable(tbl->builders, tbl->thread_count);
    free(tbl);
}

BYTE RMF_compatibleParameters(const FL2_matchTable* const tbl, const RMF_parameters * const p, size_t const dict_reduce)
{
    RMF_parameters params = RMF_clampParams(*p);
    RMF_reduceDict(&params, dict_reduce);
    return tbl->params.dictionary_size > params.dictionary_size
        || (tbl->params.dictionary_size == params.dictionary_size && tbl->alloc_struct >= RMF_isStruct(params.dictionary_size));
}

size_t RMF_applyParameters(FL2_matchTable* const tbl, const RMF_parameters* const p, size_t const dict_reduce)
{
    RMF_parameters params = RMF_clampParams(*p);
    RMF_reduceDict(&params, dict_reduce);
    return RMF_applyParameters_internal(tbl, &params);
}

size_t RMF_threadCount(const FL2_matchTable* const tbl)
{
    return tbl->thread_count;
}

void RMF_initProgress(FL2_matchTable * const tbl)
{
    if (tbl != NULL)
        tbl->progress = 0;
}

void RMF_initTable(FL2_matchTable* const tbl, const void* const data, size_t const end)
{
    DEBUGLOG(5, "RMF_initTable : size %u", (U32)end);

    tbl->st_index = ATOMIC_INITIAL_VALUE;

    if (tbl->is_struct)
        RMF_structuredInit(tbl, data, end);
    else
        RMF_bitpackInit(tbl, data, end);
}

static void RMF_handleRepeat(RMF_buildMatch* const match_buffer,
    const BYTE* const data_block,
    size_t const next,
    U32 count,
    U32 const rpt_len,
    U32 const depth,
    U32 const max_len)
{
    size_t pos = next;
    U32 length = depth + rpt_len;

    const BYTE* const data = data_block + match_buffer[pos].from;
    const BYTE* const data_2 = data - rpt_len;

    while (data[length] == data_2[length] && length < max_len)
        ++length;

    for (; length <= max_len && count; --count) {
        size_t next_i = match_buffer[pos].next & 0xFFFFFF;
        match_buffer[pos].next = (U32)next_i | (length << 24);
        length += rpt_len;
        pos = next_i;
    }
    for (; count; --count) {
        size_t next_i = match_buffer[pos].next & 0xFFFFFF;
        match_buffer[pos].next = (U32)next_i | (max_len << 24);
        pos = next_i;
    }
}

typedef struct
{
    size_t pos;
    const BYTE* data_src;
    union src_data_u src;
} BruteForceMatch;

static void RMF_bruteForceBuffered(RMF_builder* const tbl,
    const BYTE* const data_block,
    size_t const block_start,
    size_t pos,
    size_t const list_count,
    size_t const slot,
    size_t const depth,
    size_t const max_depth)
{
    BruteForceMatch buffer[MAX_BRUTE_FORCE_LIST_SIZE + 1];
    const BYTE* const data_src = data_block + depth;
    size_t const limit = max_depth - depth;
    const BYTE* const start = data_src + block_start;
    size_t i = 0;
    for (;;) {
        /* Load all locations from the match buffer */
        buffer[i].pos = pos;
        buffer[i].data_src = data_src + tbl->match_buffer[pos].from;
        buffer[i].src.u32 = tbl->match_buffer[pos].src.u32;

        if (++i >= list_count)
            break;

        pos = tbl->match_buffer[pos].next & 0xFFFFFF;
    }
    i = 0;
    do {
        size_t longest = 0;
        size_t j = i + 1;
        size_t longest_index = j;
        const BYTE* const data = buffer[i].data_src;
        do {
            /* Begin with the remaining chars pulled from the match buffer */
            size_t len_test = slot;
            while (len_test < 4 && buffer[i].src.chars[len_test] == buffer[j].src.chars[len_test] && len_test - slot < limit)
                ++len_test;

            len_test -= slot;
            if (len_test) {
                /* Complete the match length count in the raw input buffer */
                const BYTE* data_2 = buffer[j].data_src;
                while (data[len_test] == data_2[len_test] && len_test < limit)
                    ++len_test;
            }
            if (len_test > longest) {
                longest_index = j;
                longest = len_test;
                if (len_test >= limit)
                    break;
            }
        } while (++j < list_count);
        if (longest > 0) {
            /* If the existing match was extended, store the new link and length info in the match buffer */
            pos = buffer[i].pos;
            tbl->match_buffer[pos].next = (U32)(buffer[longest_index].pos | ((depth + longest) << 24));
        }
        ++i;
    } while (i < list_count - 1 && buffer[i].data_src >= start);
}

/* Lengthen and divide buffered chains into smaller chains, save them on a stack and process in turn. 
 * The match finder spends most of its time here.
 */
FORCE_INLINE_TEMPLATE
void RMF_recurseListChunk_generic(RMF_builder* const tbl,
    const BYTE* const data_block,
    size_t const block_start,
    U32 depth,
    U32 const max_depth,
    U32 list_count,
    size_t const stack_base)
{
    U32 const base_depth = depth;
    size_t st_index = stack_base;
    size_t pos = 0;
    ++depth;
    /* The last element is done separately and won't be copied back at the end */
    --list_count;
    do {
        size_t const radix_8 = tbl->match_buffer[pos].src.chars[0];
        /* Seen this char before? */
        U32 const prev = tbl->tails_8[radix_8].prev_index;
        tbl->tails_8[radix_8].prev_index = (U32)pos;
        if (prev != RADIX_NULL_LINK) {
            ++tbl->tails_8[radix_8].list_count;
            /* Link the previous occurrence to this one and record the new length */
            tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
        }
        else {
            tbl->tails_8[radix_8].list_count = 1;
            /* Add the new sub list to the stack */
            tbl->stack[st_index].head = (U32)pos;
            /* This will be converted to a count at the end */
            tbl->stack[st_index].count = (U32)radix_8;
            ++st_index;
        }
        ++pos;
    } while (pos < list_count);

    {   /* Do the last element */
        size_t const radix_8 = tbl->match_buffer[pos].src.chars[0];
        /* Nothing to do if there was no previous */
        U32 const prev = tbl->tails_8[radix_8].prev_index;
        if (prev != RADIX_NULL_LINK) {
            ++tbl->tails_8[radix_8].list_count;
            tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
        }
    }
    /* Convert radix values on the stack to counts and reset any used tail slots */
    for (size_t j = stack_base; j < st_index; ++j) {
        tbl->tails_8[tbl->stack[j].count].prev_index = RADIX_NULL_LINK;
        tbl->stack[j].count = (U32)tbl->tails_8[tbl->stack[j].count].list_count;
    }
    while (st_index > stack_base) {
        /* Pop an item off the stack */
        --st_index;
        list_count = tbl->stack[st_index].count;
        if (list_count < 2) {
            /* Nothing to match with */
            continue;
        }
        pos = tbl->stack[st_index].head;
        size_t link = tbl->match_buffer[pos].from;
        if (link < block_start) {
            /* Chain starts in the overlap region which is already encoded */
            continue;
        }
        /* Check stack space. The first comparison is unnecessary but it's a constant so should be faster */
        if (st_index > STACK_SIZE - RADIX8_TABLE_SIZE
            && st_index > STACK_SIZE - list_count)
        {
            /* Stack may not be able to fit all possible new items. This is very rare. */
            continue;
        }
        depth = tbl->match_buffer[pos].next >> 24;
        /* Index into the 4-byte pre-loaded input char cache */
        size_t slot = (depth - base_depth) & 3;
        if (list_count <= MAX_BRUTE_FORCE_LIST_SIZE) {
            /* Quicker to use brute force, each string compared with all previous strings */
            RMF_bruteForceBuffered(tbl,
                data_block,
                block_start,
                pos,
                list_count,
                slot,
                depth,
                max_depth);
            continue;
        }
        /* check for repeats at depth 4,8,16,32 etc unless depth is near max_depth */
        U32 const test = max_depth != 6 && ((depth & 3) == 0)
            && (depth & (depth - 1)) == 0
            && (max_depth >= depth + (depth >> 1));
        ++depth;
        /* Create an offset data buffer pointer for reading the next bytes */
        const BYTE* const data_src = data_block + depth;
        /* Last pass is done separately */
        if (!test && depth < max_depth) {
            size_t const prev_st_index = st_index;
            /* Last element done separately */
            --list_count;
            /* If slot is 3 then chars need to be loaded. */
            if (slot == 3 && max_depth != 6) do {
                size_t const radix_8 = tbl->match_buffer[pos].src.chars[3];
                size_t const next_index = tbl->match_buffer[pos].next & BUFFER_LINK_MASK;
                /* Pre-load the next link and data bytes. On some hardware execution can continue
                 * ahead while the data is retrieved if no operations except move are done on the data. */
                tbl->match_buffer[pos].src.u32 = MEM_read32(data_src + link);
                size_t const next_link = tbl->match_buffer[next_index].from;
                U32 const prev = tbl->tails_8[radix_8].prev_index;
                tbl->tails_8[radix_8].prev_index = (U32)pos;
                if (prev != RADIX_NULL_LINK) {
                    /* This char has occurred before in the chain. Link the previous (> pos) occurance with this */
                    ++tbl->tails_8[radix_8].list_count;
                    tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
                }
                else {
                    /* First occurrence in the chain */
                    tbl->tails_8[radix_8].list_count = 1;
                    tbl->stack[st_index].head = (U32)pos;
                    /* Save the char as a reference to load the count at the end */
                    tbl->stack[st_index].count = (U32)radix_8;
                    ++st_index;
                }
                pos = next_index;
                link = next_link;
            } while (--list_count != 0);
            else do {
                size_t const radix_8 = tbl->match_buffer[pos].src.chars[slot];
                size_t const next_index = tbl->match_buffer[pos].next & BUFFER_LINK_MASK;
                /* Pre-load the next link to avoid waiting for RAM access */
                size_t const next_link = tbl->match_buffer[next_index].from;
                U32 const prev = tbl->tails_8[radix_8].prev_index;
                tbl->tails_8[radix_8].prev_index = (U32)pos;
                if (prev != RADIX_NULL_LINK) {
                    ++tbl->tails_8[radix_8].list_count;
                    tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
                }
                else {
                    tbl->tails_8[radix_8].list_count = 1;
                    tbl->stack[st_index].head = (U32)pos;
                    tbl->stack[st_index].count = (U32)radix_8;
                    ++st_index;
                }
                pos = next_index;
                link = next_link;
            } while (--list_count != 0);

            size_t const radix_8 = tbl->match_buffer[pos].src.chars[slot];
            U32 const prev = tbl->tails_8[radix_8].prev_index;
            if (prev != RADIX_NULL_LINK) {
                if (slot == 3)
                    tbl->match_buffer[pos].src.u32 = MEM_read32(data_src + link);

                ++tbl->tails_8[radix_8].list_count;
                tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
            }
            for (size_t j = prev_st_index; j < st_index; ++j) {
                tbl->tails_8[tbl->stack[j].count].prev_index = RADIX_NULL_LINK;
                tbl->stack[j].count = (U32)tbl->tails_8[tbl->stack[j].count].list_count;
            }
        }
        else if (test) {
            S32 rpt = -1;
            size_t rpt_head_next;
            U32 rpt_dist = 0;
            size_t const prev_st_index = st_index;
            U32 const rpt_depth = depth - 1;
            /* Last element done separately */
            --list_count;
            do {
                size_t const radix_8 = tbl->match_buffer[pos].src.chars[slot];
                size_t const next_index = tbl->match_buffer[pos].next & BUFFER_LINK_MASK;
                size_t const next_link = tbl->match_buffer[next_index].from;
                if ((link - next_link) > rpt_depth) {
                    if (rpt > 0)
                        RMF_handleRepeat(tbl->match_buffer, data_block, rpt_head_next, rpt, rpt_dist, rpt_depth, tbl->max_len);

                    rpt = -1;
                    U32 const prev = tbl->tails_8[radix_8].prev_index;
                    tbl->tails_8[radix_8].prev_index = (U32)pos;
                    if (prev != RADIX_NULL_LINK) {
                        ++tbl->tails_8[radix_8].list_count;
                        tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
                    }
                    else {
                        tbl->tails_8[radix_8].list_count = 1;
                        tbl->stack[st_index].head = (U32)pos;
                        tbl->stack[st_index].count = (U32)radix_8;
                        ++st_index;
                    }
                    pos = next_index;
                    link = next_link;
                }
                else {
                    U32 const dist = (U32)(link - next_link);
                    if (rpt < 0 || dist != rpt_dist) {
                        if (rpt > 0)
                            RMF_handleRepeat(tbl->match_buffer, data_block, rpt_head_next, rpt, rpt_dist, rpt_depth, tbl->max_len);

                        rpt = 0;
                        rpt_head_next = next_index;
                        rpt_dist = dist;
                        U32 const prev = tbl->tails_8[radix_8].prev_index;
                        tbl->tails_8[radix_8].prev_index = (U32)pos;
                        if (prev != RADIX_NULL_LINK) {
                            ++tbl->tails_8[radix_8].list_count;
                            tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
                        }
                        else {
                            tbl->tails_8[radix_8].list_count = 1;
                            tbl->stack[st_index].head = (U32)pos;
                            tbl->stack[st_index].count = (U32)radix_8;
                            ++st_index;
                        }
                    }
                    else {
                        ++rpt;
                    }
                    pos = next_index;
                    link = next_link;
                }
            } while (--list_count != 0);

            if (rpt > 0)
                RMF_handleRepeat(tbl->match_buffer, data_block, rpt_head_next, rpt, rpt_dist, rpt_depth, tbl->max_len);

            size_t const radix_8 = tbl->match_buffer[pos].src.chars[slot];
            U32 const prev = tbl->tails_8[radix_8].prev_index;
            if (prev != RADIX_NULL_LINK) {
                if (slot == 3) {
                    tbl->match_buffer[pos].src.u32 = MEM_read32(data_src + link);
                }
                ++tbl->tails_8[radix_8].list_count;
                tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
            }
            for (size_t j = prev_st_index; j < st_index; ++j) {
                tbl->tails_8[tbl->stack[j].count].prev_index = RADIX_NULL_LINK;
                tbl->stack[j].count = (U32)tbl->tails_8[tbl->stack[j].count].list_count;
            }
        }
        else {
            size_t const prev_st_index = st_index;
            /* The last pass at max_depth */
            do {
                size_t const radix_8 = tbl->match_buffer[pos].src.chars[slot];
                size_t const next_index = tbl->match_buffer[pos].next & BUFFER_LINK_MASK;
                /* Pre-load the next link. */
                /* The last element in tbl->match_buffer is circular so this is never an access violation. */
                size_t const next_link = tbl->match_buffer[next_index].from;
                U32 const prev = tbl->tails_8[radix_8].prev_index;
                tbl->tails_8[radix_8].prev_index = (U32)pos;
                if (prev != RADIX_NULL_LINK) {
                    tbl->match_buffer[prev].next = (U32)pos | (depth << 24);
                }
                else {
                    tbl->stack[st_index].count = (U32)radix_8;
                    ++st_index;
                }
                pos = next_index;
                link = next_link;
            } while (--list_count != 0);
            for (size_t j = prev_st_index; j < st_index; ++j) {
                tbl->tails_8[tbl->stack[j].count].prev_index = RADIX_NULL_LINK;
            }
            st_index = prev_st_index;
        }
    }
}

void RMF_recurseListChunk(RMF_builder* const tbl,
    const BYTE* const data_block,
    size_t const block_start,
    U32 const depth,
    U32 const max_depth,
    U32 const list_count,
    size_t const stack_base)
{
    if (list_count < 2)
        return;
    /* Template-like inline functions */
    if (list_count <= MAX_BRUTE_FORCE_LIST_SIZE)
        RMF_bruteForceBuffered(tbl, data_block, block_start, 0, list_count, 0, depth, max_depth);
    else if (max_depth > 6)
        RMF_recurseListChunk_generic(tbl, data_block, block_start, depth, max_depth, list_count, stack_base);
    else
        RMF_recurseListChunk_generic(tbl, data_block, block_start, depth, 6, list_count, stack_base);
}

/* Iterate the head table concurrently with other threads, and recurse each list until max_depth is reached */
int RMF_buildTable(FL2_matchTable* const tbl,
    size_t const job,
    unsigned const multi_thread,
    FL2_dataBlock const block)
{
    DEBUGLOG(5, "RMF_buildTable : thread %u", (U32)job);

    if (tbl->is_struct)
        RMF_structuredBuildTable(tbl, job, multi_thread, block);
    else
        RMF_bitpackBuildTable(tbl, job, multi_thread, block);

    if (job == 0 && tbl->st_index >= RADIX_CANCEL_INDEX) {
        RMF_initListHeads(tbl);
        return 1;
    }
    return 0;
}

void RMF_cancelBuild(FL2_matchTable * const tbl)
{
    if(tbl != NULL)
        FL2_atomic_add(tbl->st_index, RADIX_CANCEL_INDEX - ATOMIC_INITIAL_VALUE);
}

void RMF_resetIncompleteBuild(FL2_matchTable * const tbl)
{
    RMF_initListHeads(tbl);
}

int RMF_integrityCheck(const FL2_matchTable* const tbl, const BYTE* const data, size_t const pos, size_t const end, unsigned const max_depth)
{
    if (tbl->is_struct)
        return RMF_structuredIntegrityCheck(tbl, data, pos, end, max_depth);
    else
        return RMF_bitpackIntegrityCheck(tbl, data, pos, end, max_depth);
}

void RMF_limitLengths(FL2_matchTable* const tbl, size_t const pos)
{
    if (tbl->is_struct)
        RMF_structuredLimitLengths(tbl, pos);
    else
        RMF_bitpackLimitLengths(tbl, pos);
}

BYTE* RMF_getTableAsOutputBuffer(FL2_matchTable* const tbl, size_t const pos)
{
    if (tbl->is_struct)
        return RMF_structuredAsOutputBuffer(tbl, pos);
    else
        return RMF_bitpackAsOutputBuffer(tbl, pos);
}

size_t RMF_memoryUsage(size_t const dict_size, unsigned const buffer_resize, unsigned const thread_count)
{
    size_t size = (size_t)(4U + RMF_isStruct(dict_size)) * dict_size;
    size_t const buf_size = RMF_calBufSize(dict_size, buffer_resize);
    size += ((buf_size - 1) * sizeof(RMF_buildMatch) + sizeof(RMF_builder)) * thread_count;
    return size;
}
