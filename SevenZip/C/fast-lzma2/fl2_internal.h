/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 * Modified for FL2 by Conor McCarthy
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef FL2_INTERNAL_H_
#define FL2_INTERNAL_H_


/*-*************************************
*  Dependencies
***************************************/
#include "mem.h"
#include "compiler.h"


#if defined (__cplusplus)
extern "C" {
#endif


/*-****************************************
*  Error codes handling
******************************************/
#define PREFIX(name) FL2_error_##name
#define FL2_ERROR(name) ((size_t)-PREFIX(name))


/*-*************************************
*  Stream properties
***************************************/
#define FL2_PROP_HASH_BIT 7
#define FL2_LZMA_PROP_MASK 0x3FU
#ifndef NO_XXHASH
#  define XXHASH_SIZEOF sizeof(XXH32_canonical_t)
#endif


/*-*************************************
*  Debug
***************************************/
#if defined(FL2_DEBUG) && (FL2_DEBUG>=1)
#  include <assert.h>
#else
#  ifndef assert
#    define assert(condition) ((void)0)
#  endif
#endif

#define FL2_STATIC_ASSERT(c) { enum { FL2_static_assert = 1/(int)(!!(c)) }; }

#if defined(FL2_DEBUG) && (FL2_DEBUG>=2)
#  include <stdio.h>
extern int g_debuglog_enable;
/* recommended values for FL2_DEBUG display levels :
 * 1 : no display, enables assert() only
 * 2 : reserved for currently active debugging path
 * 3 : events once per object lifetime (CCtx, CDict)
 * 4 : events once per frame
 * 5 : events once per block
 * 6 : events once per sequence (*very* verbose) */
#  define RAWLOG(l, ...) {                                 \
                if ((g_debuglog_enable) & (l<=FL2_DEBUG)) { \
                    fprintf(stderr, __VA_ARGS__);            \
            }   }
#  define DEBUGLOG(l, ...) {                                 \
                if ((g_debuglog_enable) & (l<=FL2_DEBUG)) { \
                    fprintf(stderr, __FILE__ ": ");          \
                    fprintf(stderr, __VA_ARGS__);            \
                    fprintf(stderr, " \n");                  \
            }   }
#else
#  define RAWLOG(l, ...)      {}    /* disabled */
#  define DEBUGLOG(l, ...)    {}    /* disabled */
#endif


/*-*************************************
*  shared macros
***************************************/
#undef MIN
#undef MAX
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define CHECK_F(f) do { size_t const errcod = f; if (FL2_isError(errcod)) return errcod; } while(0)  /* check and Forward error code */
#define CHECK_E(f, e) do { size_t const errcod = f; if (FL2_isError(errcod)) return FL2_ERROR(e); } while(0)  /* check and send Error code */

MEM_STATIC U32 ZSTD_highbit32(U32 val)
{
    assert(val != 0);
    {
#   if defined(_MSC_VER)   /* Visual */
        unsigned long r=0;
        _BitScanReverse(&r, val);
        return (unsigned)r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* GCC Intrinsic */
        return 31 - __builtin_clz(val);
#   else   /* Software version */
        static const int DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
        U32 v = val;
        int r;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        r = DeBruijnClz[(U32)(v * 0x07C4ACDDU) >> 27];
        return r;
#   endif
    }
}


#if defined (__cplusplus)
}
#endif

#endif   /* FL2_INTERNAL_H_ */
