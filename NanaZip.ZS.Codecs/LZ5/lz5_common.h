#ifndef LZ5COMMON_H
#define LZ5COMMON_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "mem.h"  // MEM_read32, U32


/**************************************
*  Tuning parameters
**************************************/
/*
 * HEAPMODE :
 * Select how default compression functions will allocate memory for their hash table,
 * in memory stack (0:default, fastest), or in memory heap (1:requires malloc()).
 */
#ifdef _MSC_VER
	#define HEAPMODE 1   /* Default stack size for VC++ is 1 MB and size of LZ5_stream_t exceeds that limit */ 
#else
	#define HEAPMODE 0
#endif


/*
 * ACCELERATION_DEFAULT :
 * Select "acceleration" for LZ5_compress_fast() when parameter value <= 0
 */
#define ACCELERATION_DEFAULT 1




/**************************************
*  Compiler Options
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  include <intrin.h>
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4293)        /* disable: C4293: too large shift (32-bits) */
#else
#  if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)   /* C99 */
#    if defined(__GNUC__) || defined(__clang__)
#      define FORCE_INLINE static inline __attribute__((always_inline))
#    else
#      define FORCE_INLINE static inline
#    endif
#  else
#    define FORCE_INLINE static
#  endif   /* __STDC_VERSION__ */
#endif  /* _MSC_VER */

#define LZ5_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)

#if (LZ5_GCC_VERSION >= 302) || (__INTEL_COMPILER >= 800) || defined(__clang__)
#  define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
#  define expect(expr,value)    (expr)
#endif

#define likely(expr)     expect((expr) != 0, 1)
#define unlikely(expr)   expect((expr) != 0, 0)



/**************************************
*  Memory routines
**************************************/
#include <stdlib.h>   /* malloc, calloc, free */
#define ALLOCATOR(n,s) calloc(n,s)
#define FREEMEM        free
#include <string.h>   /* memset, memcpy */
#define MEM_INIT       memset


/**************************************
*  Common Constants
**************************************/
#define MINMATCH 3 // should be 3 or 4

#define WILDCOPYLENGTH 8
#define LASTLITERALS 5
#define MFLIMIT (WILDCOPYLENGTH+MINMATCH)
static const int LZ5_minLength = (MFLIMIT+1);

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define MAXD_LOG 22
#define MAX_DISTANCE ((1 << MAXD_LOG) - 1)
#define LZ5_DICT_SIZE (1 << MAXD_LOG)

#define ML_BITS  3
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS 3
#define RUN_MASK ((1U<<RUN_BITS)-1)
#define RUN_BITS2 2
#define RUN_MASK2 ((1U<<RUN_BITS2)-1)
#define ML_RUN_BITS (ML_BITS + RUN_BITS)
#define ML_RUN_BITS2 (ML_BITS + RUN_BITS2)

#define LZ5_SHORT_OFFSET_BITS 10
#define LZ5_SHORT_OFFSET_DISTANCE (1<<LZ5_SHORT_OFFSET_BITS)
#define LZ5_MID_OFFSET_BITS 16
#define LZ5_MID_OFFSET_DISTANCE (1<<LZ5_MID_OFFSET_BITS)

    
static const U32 prime4bytes = 2654435761U;
static const U64 prime5bytes = 889523592379ULL;



/* **************************************
*  Function body to include for inlining
****************************************/

MEM_STATIC unsigned MEM_NbCommonBytes (register size_t val)
{
    if (MEM_isLittleEndian())
    {
        if (MEM_64bits())
        {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanForward64( &r, (U64)val );
            return (int)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctzll((U64)val) >> 3);
#       else
            static const int DeBruijnBytePos[64] = { 0, 0, 0, 0, 0, 1, 1, 2, 0, 3, 1, 3, 1, 4, 2, 7, 0, 2, 3, 6, 1, 5, 3, 5, 1, 3, 4, 4, 2, 5, 6, 7, 7, 0, 1, 2, 3, 3, 4, 6, 2, 6, 5, 5, 3, 4, 5, 6, 7, 1, 2, 4, 6, 4, 4, 5, 7, 2, 6, 5, 7, 6, 7, 7 };
            return DeBruijnBytePos[((U64)((val & -(long long)val) * 0x0218A392CDABBD3FULL)) >> 58];
#       endif
        }
        else /* 32 bits */
        {
#       if defined(_MSC_VER)
            unsigned long r=0;
            _BitScanForward( &r, (U32)val );
            return (int)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_ctz((U32)val) >> 3);
#       else
            static const int DeBruijnBytePos[32] = { 0, 0, 3, 0, 3, 1, 3, 0, 3, 2, 2, 1, 3, 2, 0, 1, 3, 3, 1, 2, 2, 2, 2, 0, 3, 1, 2, 0, 1, 0, 1, 1 };
            return DeBruijnBytePos[((U32)((val & -(S32)val) * 0x077CB531U)) >> 27];
#       endif
        }
    }
    else   /* Big Endian CPU */
    {
        if (MEM_32bits())
        {
#       if defined(_MSC_VER) && defined(_WIN64)
            unsigned long r = 0;
            _BitScanReverse64( &r, val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_clzll(val) >> 3);
#       else
            unsigned r;
            const unsigned n32 = sizeof(size_t)*4;   /* calculate this way due to compiler complaining in 32-bits mode */
            if (!(val>>n32)) { r=4; } else { r=0; val>>=n32; }
            if (!(val>>16)) { r+=2; val>>=8; } else { val>>=24; }
            r += (!val);
            return r;
#       endif
        }
        else /* 32 bits */
        {
#       if defined(_MSC_VER)
            unsigned long r = 0;
            _BitScanReverse( &r, (unsigned long)val );
            return (unsigned)(r>>3);
#       elif defined(__GNUC__) && (__GNUC__ >= 3)
            return (__builtin_clz((U32)val) >> 3);
#       else
            unsigned r;
            if (!(val>>16)) { r=2; val>>=8; } else { r=0; val>>=24; }
            r += (!val);
            return r;
#       endif
        }
    }
}


MEM_STATIC size_t MEM_count(const BYTE* pIn, const BYTE* pMatch, const BYTE* pInLimit)
{
    const BYTE* const pStart = pIn;

    while ((pIn<pInLimit-(sizeof(size_t)-1)))
    {
        size_t diff = MEM_readST(pMatch) ^ MEM_readST(pIn);
        if (!diff) { pIn+=sizeof(size_t); pMatch+=sizeof(size_t); continue; }
        pIn += MEM_NbCommonBytes(diff);
        return (size_t)(pIn - pStart);
    }

    if (MEM_64bits()) if ((pIn<(pInLimit-3)) && (MEM_read32(pMatch) == MEM_read32(pIn))) { pIn+=4; pMatch+=4; }
    if ((pIn<(pInLimit-1)) && (MEM_read16(pMatch) == MEM_read16(pIn))) { pIn+=2; pMatch+=2; }
    if ((pIn<pInLimit) && (*pMatch == *pIn)) pIn++;
    return (size_t)(pIn - pStart);
}


static void MEM_copy8(void* dst, const void* src) { memcpy(dst, src, 8); }

#define COPY8(d,s) { MEM_copy8(d,s); d+=8; s+=8; }


/* customized variant of memcpy, which can overwrite up to 7 bytes beyond dstEnd */
static void MEM_wildCopy(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;

    do { MEM_copy8(d,s); d+=8; s+=8; } while (d<e);
}


#if defined (__cplusplus)
}
#endif

#endif /* LZ5COMMON_H */
