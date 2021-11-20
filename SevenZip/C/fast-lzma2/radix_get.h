/*
* Copyright (c) 2018, Conor McCarthy
* All rights reserved.
*
* This source code is licensed under both the BSD-style license (found in the
* LICENSE file in the root directory of this source tree) and the GPLv2 (found
* in the COPYING file in the root directory of this source tree).
* You may select, at your option, one of the above-listed licenses.
*/

#ifndef FL2_RADIX_GET_H_
#define FL2_RADIX_GET_H_

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct
{
    U32 length;
    U32 dist;
} RMF_match;

static size_t RMF_bitpackExtendMatch(const BYTE* const data,
    const U32* const table,
    ptrdiff_t const start_index,
    ptrdiff_t limit,
    U32 const link,
    size_t const length)
{
    ptrdiff_t end_index = start_index + length;
    ptrdiff_t const dist = start_index - link;

    if (limit > start_index + (ptrdiff_t)kMatchLenMax)
        limit = start_index + kMatchLenMax;

    while (end_index < limit && end_index - (ptrdiff_t)(table[end_index] & RADIX_LINK_MASK) == dist)
        end_index += table[end_index] >> RADIX_LINK_BITS;

    if (end_index >= limit) {
        DEBUGLOG(7, "RMF_bitpackExtendMatch : pos %u, link %u, init length %u, full length %u", (U32)start_index, link, (U32)length, (U32)(limit - start_index));
        return limit - start_index;
    }

    while (end_index < limit && data[end_index - dist] == data[end_index])
        ++end_index;

    DEBUGLOG(7, "RMF_bitpackExtendMatch : pos %u, link %u, init length %u, full length %u", (U32)start_index, link, (U32)length, (U32)(end_index - start_index));
    return end_index - start_index;
}

#define GetMatchLink(table, pos) ((const RMF_unit*)(table))[(pos) >> UNIT_BITS].links[(pos) & UNIT_MASK]

#define GetMatchLength(table, pos) ((const RMF_unit*)(table))[(pos) >> UNIT_BITS].lengths[(pos) & UNIT_MASK]

static size_t RMF_structuredExtendMatch(const BYTE* const data,
    const U32* const table,
    ptrdiff_t const start_index,
    ptrdiff_t limit,
    U32 const link,
    size_t const length)
{
    ptrdiff_t end_index = start_index + length;
    ptrdiff_t const dist = start_index - link;

    if (limit > start_index + (ptrdiff_t)kMatchLenMax)
        limit = start_index + kMatchLenMax;

    while (end_index < limit && end_index - (ptrdiff_t)GetMatchLink(table, end_index) == dist)
        end_index += GetMatchLength(table, end_index);

    if (end_index >= limit) {
        DEBUGLOG(7, "RMF_structuredExtendMatch : pos %u, link %u, init length %u, full length %u", (U32)start_index, link, (U32)length, (U32)(limit - start_index));
        return limit - start_index;
    }

    while (end_index < limit && data[end_index - dist] == data[end_index])
        ++end_index;

    DEBUGLOG(7, "RMF_structuredExtendMatch : pos %u, link %u, init length %u, full length %u", (U32)start_index, link, (U32)length, (U32)(end_index - start_index));
    return end_index - start_index;
}

FORCE_INLINE_TEMPLATE
RMF_match RMF_getMatch(FL2_dataBlock block,
    FL2_matchTable* tbl,
    unsigned max_depth,
    int structTbl,
    size_t pos)
{
    if (structTbl)
    {
        U32 const link = GetMatchLink(tbl->table, pos);

        RMF_match match;
        match.length = 0;

        if (link == RADIX_NULL_LINK)
            return match;

        size_t const length = GetMatchLength(tbl->table, pos);
        size_t const dist = pos - link - 1;

        if (length == max_depth || length == STRUCTURED_MAX_LENGTH /* from HandleRepeat */)
            match.length = (U32)RMF_structuredExtendMatch(block.data, tbl->table, pos, block.end, link, length);
        else
            match.length = (U32)length;

        match.dist = (U32)dist;

        return match;
    }
    else {
        U32 link = tbl->table[pos];

        RMF_match match;
        match.length = 0;

        if (link == RADIX_NULL_LINK)
            return match;

        size_t const length = link >> RADIX_LINK_BITS;
        link &= RADIX_LINK_MASK;
        size_t const dist = pos - link - 1;

        if (length == max_depth || length == BITPACK_MAX_LENGTH /* from HandleRepeat */)
            match.length = (U32)RMF_bitpackExtendMatch(block.data, tbl->table, pos, block.end, link, length);
        else
            match.length = (U32)length;

        match.dist = (U32)dist;

        return match;
    }
}

FORCE_INLINE_TEMPLATE
RMF_match RMF_getNextMatch(FL2_dataBlock block,
    FL2_matchTable* tbl,
    unsigned max_depth,
    int structTbl,
    size_t pos)
{
    if (structTbl)
    {
        U32 const link = GetMatchLink(tbl->table, pos);

        RMF_match match;
        match.length = 0;

        if (link == RADIX_NULL_LINK)
            return match;

        size_t const length = GetMatchLength(tbl->table, pos);
        size_t const dist = pos - link - 1;

        /* same distance, one byte shorter */
        if (link - 1 == GetMatchLink(tbl->table, pos - 1))
            return match;

        if (length == max_depth || length == STRUCTURED_MAX_LENGTH /* from HandleRepeat */)
            match.length = (U32)RMF_structuredExtendMatch(block.data, tbl->table, pos, block.end, link, length);
        else
            match.length = (U32)length;

        match.dist = (U32)dist;

        return match;
    }
    else {
        U32 link = tbl->table[pos];

        RMF_match match;
        match.length = 0;

        if (link == RADIX_NULL_LINK)
            return match;

        size_t const length = link >> RADIX_LINK_BITS;
        link &= RADIX_LINK_MASK;
        size_t const dist = pos - link - 1;

        /* same distance, one byte shorter */
        if (link - 1 == (tbl->table[pos - 1] & RADIX_LINK_MASK))
            return match;

        if (length == max_depth || length == BITPACK_MAX_LENGTH /* from HandleRepeat */)
            match.length = (U32)RMF_bitpackExtendMatch(block.data, tbl->table, pos, block.end, link, length);
        else
            match.length = (U32)length;

        match.dist = (U32)dist;

        return match;
    }
}

#if defined (__cplusplus)
}
#endif

#endif /* FL2_RADIX_GET_H_ */