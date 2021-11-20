/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_COUNT_H_
#define ZSTD_COUNT_H_

#include "mem.h"

#if defined (__cplusplus)
extern "C" {
#endif

/*-*************************************
*  Match length counter
***************************************/
static unsigned ZSTD_NbCommonBytes(register size_t val)
{
    if (MEM_isLittleEndian()) {
        if (MEM_64bits()) {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanForward64(&r, (U64)val);
            return (unsigned)(r >> 3);
#       elif defined(__GNUC__) && (__GNUC__ >= 4)
            return (__builtin_ctzll((U64)val) >> 3);
#       else
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2,
                0, 3, 1, 3, 1, 4, 2, 7,
                0, 2, 3, 6, 1, 5, 3, 5,
                1, 3, 4, 4, 2, 5, 6, 7,
                7, 0, 1, 2, 3, 3, 4, 6,
                2, 6, 5, 5, 3, 4, 5, 6,
                7, 1, 2, 4, 6, 4, 4, 5,
                7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#       endif
        }
        else { /* 32 bits */
#       if defined(_MSC_VER)
            unsigned long r = 0;
            _BitScanForward(&r, (U32)val);
            return (unsigned)(r >> 3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctz((U32)val) >> 3);
#       else
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0,
                3, 2, 2, 1, 3, 2, 0, 1,
                3, 3, 1, 2, 2, 2, 2, 0,
                3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#       endif
        }
    }
    else {  /* Big Endian CPU */
        if (MEM_64bits()) {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanReverse64(&r, val);
            return (unsigned)(r >> 3);
#       elif defined(__GNUC__) && (__GNUC__ >= 4)
            return (__builtin_clzll(val) >> 3);
#       else
            unsigned r;
            const unsigned n32 = sizeof(size_t) * 4;   /* calculate this way due to compiler complaining in 32-bits mode */
            if (!(val >> n32)) { r = 4; }
            else { r = 0; val >>= n32; }
            if (!(val >> 16)) { r += 2; val >>= 8; }
            else { val >>= 24; }
            r += (!val);
            return r;
#       endif
        }
        else { /* 32 bits */
#       if defined(_MSC_VER)
            unsigned long r = 0;
            _BitScanReverse(&r, (unsigned long)val);
            return (unsigned)(r >> 3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_clz((U32)val) >> 3);
#       else
            unsigned r;
            if (!(val >> 16)) { r = 2; val >>= 8; }
            else { r = 0; val >>= 24; }
            r += (!val);
            return r;
#       endif
        }
    }
}


static size_t ZSTD_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* const pInLimit)
{
    const BYTE* const pStart = pIn;
    const BYTE* const pInLoopLimit = pInLimit - (sizeof(size_t) - 1);

    if (pIn < pInLoopLimit) {
        { size_t const diff = MEM_readST(pMatch) ^ MEM_readST(pIn);
        if (diff) return ZSTD_NbCommonBytes(diff); }
        pIn += sizeof(size_t); pMatch += sizeof(size_t);
        while (pIn < pInLoopLimit) {
            size_t const diff = MEM_readST(pMatch) ^ MEM_readST(pIn);
            if (!diff) { pIn += sizeof(size_t); pMatch += sizeof(size_t); continue; }
            pIn += ZSTD_NbCommonBytes(diff);
            return (size_t)(pIn - pStart);
        }
    }
    if (MEM_64bits() && (pIn<(pInLimit - 3)) && (MEM_read32(pMatch) == MEM_read32(pIn))) { pIn += 4; pMatch += 4; }
    if ((pIn<(pInLimit - 1)) && (MEM_read16(pMatch) == MEM_read16(pIn))) { pIn += 2; pMatch += 2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (size_t)(pIn - pStart);
}

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_COUNT_H_ */