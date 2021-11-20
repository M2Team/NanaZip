/*
* Copyright (c) 2019, Conor McCarthy
* All rights reserved.
*
* This source code is licensed under both the BSD-style license (found in the
* LICENSE file in the root directory of this source tree) and the GPLv2 (found
* in the COPYING file in the root directory of this source tree).
* You may select, at your option, one of the above-listed licenses.
*/

#include <stdlib.h>
#include "dict_buffer.h"
#include "fl2_internal.h"

#define ALIGNMENT_SIZE 16U
#define ALIGNMENT_MASK (~(size_t)(ALIGNMENT_SIZE-1))

/* DICT_buffer functions */

int DICT_construct(DICT_buffer * const buf, int const async)
{
    buf->data[0] = NULL;
    buf->data[1] = NULL;
    buf->size = 0;

    buf->async = (async != 0);

#ifndef NO_XXHASH
    buf->xxh = NULL;
#endif

    return 0;
}

int DICT_init(DICT_buffer * const buf, size_t const dict_size, size_t const overlap, unsigned const reset_multiplier, int const do_hash)
{
    /* Allocate if not yet allocated or existing dict too small */
    if (buf->data[0] == NULL || dict_size > buf->size) {
        /* Free any existing buffers */
        DICT_destruct(buf);

        buf->data[0] = malloc(dict_size);

        buf->data[1] = NULL;
        if (buf->async)
            buf->data[1] = malloc(dict_size);

        if (buf->data[0] == NULL || (buf->async && buf->data[1] == NULL)) {
            DICT_destruct(buf);
            return 1;
        }
    }
    buf->index = 0;
    buf->overlap = overlap;
    buf->start = 0;
    buf->end = 0;
    buf->size = dict_size;
    buf->total = 0;
    buf->reset_interval = (reset_multiplier != 0) ? dict_size * reset_multiplier : ((size_t)1 << 31);

#ifndef NO_XXHASH
    if (do_hash) {
        if (buf->xxh == NULL) {
            buf->xxh = XXH32_createState();
            if (buf->xxh == NULL) {
                DICT_destruct(buf);
                return 1;
            }
        }
        XXH32_reset(buf->xxh, 0);
    }
    else {
        XXH32_freeState(buf->xxh);
        buf->xxh = NULL;
    }
#else
    (void)do_hash;
#endif

    return 0;
}

void DICT_destruct(DICT_buffer * const buf)
{
    free(buf->data[0]);
    free(buf->data[1]);
    buf->data[0] = NULL;
    buf->data[1] = NULL;
    buf->size = 0;
#ifndef NO_XXHASH
    XXH32_freeState(buf->xxh);
    buf->xxh = NULL;
#endif
}

size_t DICT_size(const DICT_buffer * const buf)
{
    return buf->size;
}

/* Get the dictionary buffer for adding input */
size_t DICT_get(DICT_buffer * const buf, void **const dict)
{
    DICT_shift(buf);

    DEBUGLOG(5, "Getting dict buffer %u, pos %u, avail %u", (unsigned)buf->index, (unsigned)buf->end, (unsigned)(buf->size - buf->end));
    *dict = buf->data[buf->index] + buf->end;
    return buf->size - buf->end;
}

/* Update with the amount added */
int DICT_update(DICT_buffer * const buf, size_t const added_size)
{
    DEBUGLOG(5, "Added %u bytes to dict buffer %u", (unsigned)added_size, (unsigned)buf->index);
    buf->end += added_size;
    assert(buf->end <= buf->size);
    return !DICT_availSpace(buf);
}

/* Read from input and write to the dict */
void DICT_put(DICT_buffer * const buf, FL2_inBuffer * const input)
{
    size_t const to_read = MIN(buf->size - buf->end, input->size - input->pos);

    DEBUGLOG(5, "CStream : reading %u bytes", (U32)to_read);

    memcpy(buf->data[buf->index] + buf->end, (BYTE*)input->src + input->pos, to_read);

    input->pos += to_read;
    buf->end += to_read;
}

size_t DICT_availSpace(const DICT_buffer * const buf)
{
    return buf->size - buf->end;
}

/* Get the size of uncompressed data. start is set to end after compression */
int DICT_hasUnprocessed(const DICT_buffer * const buf)
{
    return buf->start < buf->end;
}

/* Get the buffer, overlap and end for compression */
void DICT_getBlock(DICT_buffer * const buf, FL2_dataBlock * const block)
{
    block->data = buf->data[buf->index];
    block->start = buf->start;
    block->end = buf->end;

#ifndef NO_XXHASH
    if (buf->xxh != NULL)
        XXH32_update(buf->xxh, buf->data[buf->index] + buf->start, buf->end - buf->start);
#endif

    buf->total += buf->end - buf->start;
    buf->start = buf->end;
}

/* Shift occurs when all is processed and end is beyond the overlap size */
int DICT_needShift(DICT_buffer * const buf)
{
    if (buf->start < buf->end)
        return 0;
    /* Reset the dict if the next compression cycle would exceed the reset interval */
    size_t overlap = (buf->total + buf->size - buf->overlap > buf->reset_interval) ? 0 : buf->overlap;
    return buf->start == buf->end && (overlap == 0 || buf->end >= overlap + ALIGNMENT_SIZE);
}

int DICT_async(const DICT_buffer * const buf)
{
    return (int)buf->async;
}

/* Shift the overlap amount to the start of either the only dict buffer or the alternate one
 * if it exists */
void DICT_shift(DICT_buffer * const buf)
{
    if (buf->start < buf->end)
        return;

    size_t overlap = buf->overlap;
    /* Reset the dict if the next compression cycle would exceed the reset interval */
    if (buf->total + buf->size - buf->overlap > buf->reset_interval) {
        DEBUGLOG(4, "Resetting dictionary after %u bytes", (unsigned)buf->total);
        overlap = 0;
    }

    if (overlap == 0) {
        /* No overlap means a simple buffer switch */
        buf->start = 0;
        buf->end = 0;
        buf->index ^= buf->async;
        buf->total = 0;
    }
    else if (buf->end >= overlap + ALIGNMENT_SIZE) {
        size_t const from = (buf->end - overlap) & ALIGNMENT_MASK;
        const BYTE *const src = buf->data[buf->index];
        /* Copy to the alternate if one exists */
        BYTE *const dst = buf->data[buf->index ^ buf->async];

        overlap = buf->end - from;

        if (overlap <= from || dst != src) {
            DEBUGLOG(5, "Copy overlap data : %u bytes from %u", (unsigned)overlap, (unsigned)from);
            memcpy(dst, src + from, overlap);
        }
        else if (from != 0) {
            DEBUGLOG(5, "Move overlap data : %u bytes from %u", (unsigned)overlap, (unsigned)from);
            memmove(dst, src + from, overlap);
        }
        /* New data will be written after the overlap */
        buf->start = overlap;
        buf->end = overlap;
        /* Switch buffers */
        buf->index ^= buf->async;
    }
}

#ifndef NO_XXHASH
XXH32_hash_t DICT_getDigest(const DICT_buffer * const buf)
{
    return XXH32_digest(buf->xxh);
}
#endif

size_t DICT_memUsage(const DICT_buffer * const buf)
{
    return (1 + buf->async) * buf->size;
}
