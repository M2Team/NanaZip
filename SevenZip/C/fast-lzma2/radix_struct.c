/*
* Copyright (c) 2018, Conor McCarthy
* All rights reserved.
*
* This source code is licensed under both the BSD-style license (found in the
* LICENSE file in the root directory of this source tree) and the GPLv2 (found
* in the COPYING file in the root directory of this source tree).
* You may select, at your option, one of the above-listed licenses.
*/

#include "mem.h"          /* U32, U64 */
#include "fl2_threading.h"
#include "fl2_internal.h"
#include "radix_internal.h"

#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define RMF_STRUCTURED

#define RADIX_MAX_LENGTH STRUCTURED_MAX_LENGTH

#define InitMatchLink(pos, link) ((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].links[(pos) & UNIT_MASK] = (U32)(link)

#define GetMatchLink(pos) ((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].links[(pos) & UNIT_MASK]

#define GetInitialMatchLink(pos) ((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].links[(pos) & UNIT_MASK]

#define GetMatchLength(pos) ((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].lengths[(pos) & UNIT_MASK]

#define SetMatchLink(pos, link, length) ((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].links[(pos) & UNIT_MASK] = (U32)(link)

#define SetMatchLength(pos, link, length) ((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].lengths[(pos) & UNIT_MASK] = (BYTE)(length)

#define SetMatchLinkAndLength(pos, link, length) do { size_t i_ = (pos) >> UNIT_BITS, u_ = (pos) & UNIT_MASK; ((RMF_unit*)tbl->table)[i_].links[u_] = (U32)(link); ((RMF_unit*)tbl->table)[i_].lengths[u_] = (BYTE)(length); } while(0)

#define SetNull(pos) ((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].links[(pos) & UNIT_MASK] = RADIX_NULL_LINK

#define IsNull(pos) (((RMF_unit*)tbl->table)[(pos) >> UNIT_BITS].links[(pos) & UNIT_MASK] == RADIX_NULL_LINK)

BYTE* RMF_structuredAsOutputBuffer(FL2_matchTable* const tbl, size_t const pos)
{
    return (BYTE*)((RMF_unit*)tbl->table + (pos >> UNIT_BITS) + ((pos & UNIT_MASK) != 0));
}

/* Restrict the match lengths so that they don't reach beyond pos */
void RMF_structuredLimitLengths(FL2_matchTable* const tbl, size_t const pos)
{
    DEBUGLOG(5, "RMF_limitLengths : end %u, max length %u", (U32)pos, RADIX_MAX_LENGTH);
    SetNull(pos - 1);
    for (size_t length = 2; length < RADIX_MAX_LENGTH && length <= pos; ++length) {
        size_t const i = (pos - length) >> UNIT_BITS;
        size_t const u = (pos - length) & UNIT_MASK;
        if (((RMF_unit*)tbl->table)[i].links[u] != RADIX_NULL_LINK) {
            ((RMF_unit*)tbl->table)[i].lengths[u] = MIN((BYTE)length, ((RMF_unit*)tbl->table)[i].lengths[u]);
        }
    }
}

#include "radix_engine.h"