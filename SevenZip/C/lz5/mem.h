/* ******************************************************************
   mem.h
   low-level memory access routines
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef MEM_H_MODULE
#define MEM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/******************************************
*  Includes
******************************************/
#include <stddef.h>    /* size_t, ptrdiff_t */
#include <string.h>    /* memcpy */



/******************************************
*  Compiler-specific
******************************************/
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define MEM_STATIC static inline
#elif defined(_MSC_VER)
#  define MEM_STATIC static __inline
#elif defined(__GNUC__)
#  define MEM_STATIC static __attribute__((unused))
#else
#  define MEM_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif



/****************************************************************
*  Memory I/O
*****************************************************************/
/* MEM_FORCE_MEMORY_ACCESS
 * By default, access to unaligned memory is controlled by `memcpy()`, which is safe and portable.
 * Unfortunately, on some target/compiler combinations, the generated assembly is sub-optimal.
 * The below switch allow to select different access method for improved performance.
 * Method 0 (default) : use `memcpy()`. Safe and portable.
 * Method 1 : `__packed` statement. It depends on compiler extension (ie, not portable).
 *            This method is safe if your compiler supports it, and *generally* as fast or faster than `memcpy`.
 * Method 2 : direct access. This method is portable but violate C standard.
 *            It can generate buggy code on targets generating assembly depending on alignment.
 *            But in some circumstances, it's the only known way to get the most performance (ie GCC + ARMv6)
 * See http://fastcompression.blogspot.fr/2015/08/accessing-unaligned-memory.html for details.
 * Prefer these methods in priority order (0 > 1 > 2)
 */
#ifndef MEM_FORCE_MEMORY_ACCESS   /* can be defined externally, on command line for example */
#  if defined(__GNUC__) && ( defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) )
#    define MEM_FORCE_MEMORY_ACCESS 2
#  elif defined(__INTEL_COMPILER) || \
  (defined(__GNUC__) && ( defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) ))
#    define MEM_FORCE_MEMORY_ACCESS 1
#  endif
#endif

MEM_STATIC unsigned MEM_32bits(void) { return sizeof(void*)==4; }
MEM_STATIC unsigned MEM_64bits(void) { return sizeof(void*)==8; }

MEM_STATIC unsigned MEM_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental  */
    return one.c[0];
}

#if defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==2)

/* violates C standard on structure alignment.
Only use if no other choice to achieve best performance on target platform */
MEM_STATIC U16 MEM_read16(const void* memPtr) { return *(const U16*) memPtr; }
MEM_STATIC U32 MEM_read32(const void* memPtr) { return *(const U32*) memPtr; }
MEM_STATIC U64 MEM_read64(const void* memPtr) { return *(const U64*) memPtr; }

MEM_STATIC void MEM_write16(void* memPtr, U16 value) { *(U16*)memPtr = value; }
MEM_STATIC void MEM_write32(void* memPtr, U32 value) { *(U32*)memPtr = value; }
MEM_STATIC void MEM_write64(void* memPtr, U64 value) { *(U64*)memPtr = value; }

#elif defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==1)

/* __pack instructions are safer, but compiler specific, hence potentially problematic for some compilers */
/* currently only defined for gcc and icc */
typedef union { U16 u16; U32 u32; U64 u64; } __attribute__((packed)) unalign;

MEM_STATIC U16 MEM_read16(const void* ptr) { return ((const unalign*)ptr)->u16; }
MEM_STATIC U32 MEM_read32(const void* ptr) { return ((const unalign*)ptr)->u32; }
MEM_STATIC U64 MEM_read64(const void* ptr) { return ((const unalign*)ptr)->u64; }

MEM_STATIC void MEM_write16(void* memPtr, U16 value) { ((unalign*)memPtr)->u16 = value; }
MEM_STATIC void MEM_write32(void* memPtr, U32 value) { ((unalign*)memPtr)->u32 = value; }
MEM_STATIC void MEM_write64(void* memPtr, U64 value) { ((unalign*)memPtr)->u64 = value; }

#else

/* default method, safe and standard.
   can sometimes prove slower */

MEM_STATIC U16 MEM_read16(const void* memPtr)
{
    U16 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U32 MEM_read32(const void* memPtr)
{
    U32 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U64 MEM_read64(const void* memPtr)
{
    U64 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC void MEM_write16(void* memPtr, U16 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

MEM_STATIC void MEM_write32(void* memPtr, U32 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

MEM_STATIC void MEM_write64(void* memPtr, U64 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

#endif // MEM_FORCE_MEMORY_ACCESS


MEM_STATIC U16 MEM_readLE16(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read16(memPtr);
    else
    {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)(p[0] + (p[1]<<8));
    }
}

MEM_STATIC void MEM_writeLE16(void* memPtr, U16 val)
{
    if (MEM_isLittleEndian())
    {
        MEM_write16(memPtr, val);
    }
    else
    {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE)val;
        p[1] = (BYTE)(val>>8);
    }
}

MEM_STATIC U32 MEM_readLE24(const void* memPtr)
{
    if (MEM_isLittleEndian())
    {
        U32 val32 = 0;
        memcpy(&val32, memPtr, 3);
        return val32;
    }
    else
    {
        const BYTE* p = (const BYTE*)memPtr;
        return (U32)(p[0] + (p[1]<<8) + (p[2]<<16));
    }
} 

MEM_STATIC void MEM_writeLE24(void* memPtr, U32 value)
{
    if (MEM_isLittleEndian())
    {
        memcpy(memPtr, &value, 3);
    }
    else
    {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE) value;
        p[1] = (BYTE)(value>>8);
        p[2] = (BYTE)(value>>16);
    }
}

 
MEM_STATIC U32 MEM_readLE32(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read32(memPtr);
    else
    {
        const BYTE* p = (const BYTE*)memPtr;
        return (U32)((U32)p[0] + ((U32)p[1]<<8) + ((U32)p[2]<<16) + ((U32)p[3]<<24));
    }
}

MEM_STATIC void MEM_writeLE32(void* memPtr, U32 val32)
{
    if (MEM_isLittleEndian())
    {
        MEM_write32(memPtr, val32);
    }
    else
    {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE)val32;
        p[1] = (BYTE)(val32>>8);
        p[2] = (BYTE)(val32>>16);
        p[3] = (BYTE)(val32>>24);
    }
}

MEM_STATIC U64 MEM_readLE64(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read64(memPtr);
    else
    {
        const BYTE* p = (const BYTE*)memPtr;
        return (U64)((U64)p[0] + ((U64)p[1]<<8) + ((U64)p[2]<<16) + ((U64)p[3]<<24)
                     + ((U64)p[4]<<32) + ((U64)p[5]<<40) + ((U64)p[6]<<48) + ((U64)p[7]<<56));
    }
}

MEM_STATIC void MEM_writeLE64(void* memPtr, U64 val64)
{
    if (MEM_isLittleEndian())
    {
        MEM_write64(memPtr, val64);
    }
    else
    {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE)val64;
        p[1] = (BYTE)(val64>>8);
        p[2] = (BYTE)(val64>>16);
        p[3] = (BYTE)(val64>>24);
        p[4] = (BYTE)(val64>>32);
        p[5] = (BYTE)(val64>>40);
        p[6] = (BYTE)(val64>>48);
        p[7] = (BYTE)(val64>>56);
    }
}

MEM_STATIC size_t MEM_readLEST(const void* memPtr)
{
    if (MEM_32bits())
        return (size_t)MEM_readLE32(memPtr);
    else
        return (size_t)MEM_readLE64(memPtr);
}

MEM_STATIC void MEM_writeLEST(void* memPtr, size_t val)
{
    if (MEM_32bits())
        MEM_writeLE32(memPtr, (U32)val);
    else
        MEM_writeLE64(memPtr, (U64)val);
}


#if MINMATCH == 3
    #define MEM_read24(ptr) (U32)(MEM_read32(ptr)<<8) 
#else
    #define MEM_read24(ptr) (U32)(MEM_read32(ptr)) 
#endif


/* **************************************
*  Function body to include for inlining
****************************************/
static size_t MEM_read_ARCH(const void* p) { size_t r; memcpy(&r, p, sizeof(r)); return r; }

#define MIN(a,b) ((a)<(b) ? (a) : (b))


/*static unsigned MEM_highbit(U32 val)
{
#   if defined(_MSC_VER)   // Visual 
    unsigned long r=0;
    _BitScanReverse(&r, val);
    return (unsigned)r;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   // GCC Intrinsic
    return 31 - __builtin_clz(val);
#   else   // Software version 
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
}*/


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
        size_t diff = MEM_read_ARCH(pMatch) ^ MEM_read_ARCH(pIn);
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


/*! MEM_wildcopy : custom version of memcpy(), can copy up to 7-8 bytes too many */
/*static void MEM_wildcopy(void* dst, const void* src, size_t length)
{
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + length;
    do
        COPY8(op, ip)
    while (op < oend);
} */ 

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

#endif /* MEM_H_MODULE */

