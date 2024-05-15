// XpressDecoder.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/RotateDefs.h"

#include "HuffmanDecoder.h"
#include "XpressDecoder.h"

#ifdef MY_CPU_LE_UNALIGN
  #define Z7_XPRESS_DEC_USE_UNALIGNED_COPY
#endif

#ifdef Z7_XPRESS_DEC_USE_UNALIGNED_COPY

  #define COPY_CHUNK_SIZE 16

    #define COPY_CHUNK_4_2(dest, src) \
    { \
      ((UInt32 *)(void *)dest)[0] = ((const UInt32 *)(const void *)src)[0]; \
      ((UInt32 *)(void *)dest)[1] = ((const UInt32 *)(const void *)src)[1]; \
      src += 4 * 2; \
      dest += 4 * 2; \
    }

  /* sse2 doesn't help here in GCC and CLANG.
     so we disabled sse2 here */
#if 0
  #if defined(MY_CPU_AMD64)
    #define Z7_XPRESS_DEC_USE_SSE2
  #elif defined(MY_CPU_X86)
    #if defined(_MSC_VER) && _MSC_VER >= 1300 && defined(_M_IX86_FP) && (_M_IX86_FP >= 2) \
      || defined(__SSE2__) \
      // || 1 == 1  // for debug only
      #define Z7_XPRESS_DEC_USE_SSE2
    #endif
  #endif
#endif

  #if defined(MY_CPU_ARM64)
  #include <arm_neon.h>
    #define COPY_OFFSET_MIN  16
    #define COPY_CHUNK1(dest, src) \
    { \
      vst1q_u8((uint8_t *)(void *)dest, \
      vld1q_u8((const uint8_t *)(const void *)src)); \
      src += 16; \
      dest += 16; \
    }
    
    #define COPY_CHUNK(dest, src) \
    { \
      COPY_CHUNK1(dest, src) \
      if (dest >= dest_lim) break; \
      COPY_CHUNK1(dest, src) \
    }

  #elif defined(Z7_XPRESS_DEC_USE_SSE2)
    #include <emmintrin.h> // sse2
    #define COPY_OFFSET_MIN  16

    #define COPY_CHUNK1(dest, src) \
    { \
      _mm_storeu_si128((__m128i *)(void *)dest, \
      _mm_loadu_si128((const __m128i *)(const void *)src)); \
      src += 16; \
      dest += 16; \
    }

    #define COPY_CHUNK(dest, src) \
    { \
      COPY_CHUNK1(dest, src) \
      if (dest >= dest_lim) break; \
      COPY_CHUNK1(dest, src) \
    }

  #elif defined(MY_CPU_64BIT)
    #define COPY_OFFSET_MIN  8

    #define COPY_CHUNK(dest, src) \
    { \
      ((UInt64 *)(void *)dest)[0] = ((const UInt64 *)(const void *)src)[0]; \
      ((UInt64 *)(void *)dest)[1] = ((const UInt64 *)(const void *)src)[1]; \
      src += 8 * 2; \
      dest += 8 * 2; \
    }

  #else
    #define COPY_OFFSET_MIN  4

    #define COPY_CHUNK(dest, src) \
    { \
      COPY_CHUNK_4_2(dest, src); \
      COPY_CHUNK_4_2(dest, src); \
    }

  #endif
#endif


#ifndef COPY_CHUNK_SIZE
    #define COPY_OFFSET_MIN  4
    #define COPY_CHUNK_SIZE  8
    #define COPY_CHUNK_2(dest, src) \
    { \
      const Byte a0 = src[0]; \
      const Byte a1 = src[1]; \
      dest[0] = a0; \
      dest[1] = a1; \
      src += 2; \
      dest += 2; \
    }
    #define COPY_CHUNK(dest, src) \
    { \
      COPY_CHUNK_2(dest, src) \
      COPY_CHUNK_2(dest, src) \
      COPY_CHUNK_2(dest, src) \
      COPY_CHUNK_2(dest, src) \
    }
#endif


#define COPY_CHUNKS \
{ \
  Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE \
  do { COPY_CHUNK(dest, src) } \
  while (dest < dest_lim); \
}


static
Z7_FORCE_INLINE
// Z7_ATTRIB_NO_VECTOR
void CopyMatch_1(Byte *dest, const Byte *dest_lim)
{
      const unsigned b0 = dest[-1];
      {
#if defined(Z7_XPRESS_DEC_USE_UNALIGNED_COPY) && (COPY_CHUNK_SIZE == 16)
        #if defined(MY_CPU_64BIT)
        {
          const UInt64 v64 = (UInt64)b0 * 0x0101010101010101;
          Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
          do
          {
            ((UInt64 *)(void *)dest)[0] = v64;
            ((UInt64 *)(void *)dest)[1] = v64;
            dest += 16;
          }
          while (dest < dest_lim);
        }
        #else
        {
          UInt32 v = b0;
          v |= v << 8;
          v |= v << 16;
          do
          {
            ((UInt32 *)(void *)dest)[0] = v;
            ((UInt32 *)(void *)dest)[1] = v;
            dest += 8;
            ((UInt32 *)(void *)dest)[0] = v;
            ((UInt32 *)(void *)dest)[1] = v;
            dest += 8;
          }
          while (dest < dest_lim);
        }
        #endif
#else
        do
        {
          dest[0] = (Byte)b0;
          dest[1] = (Byte)b0;
          dest += 2;
          dest[0] = (Byte)b0;
          dest[1] = (Byte)b0;
          dest += 2;
        }
        while (dest < dest_lim);
#endif
      }
}


// (offset != 1)
static
Z7_FORCE_INLINE
// Z7_ATTRIB_NO_VECTOR
void CopyMatch_Non1(Byte *dest, size_t offset, const Byte *dest_lim)
{
  const Byte *src = dest - offset;
  {
    // (COPY_OFFSET_MIN >= 4)
    if (offset >= COPY_OFFSET_MIN)
    {
      COPY_CHUNKS
      // return;
    }
    else
#if (COPY_OFFSET_MIN > 4)
    #if COPY_CHUNK_SIZE < 8
      #error Stop_Compiling_Bad_COPY_CHUNK_SIZE
    #endif
    if (offset >= 4)
    {
      Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
      do
      {
        COPY_CHUNK_4_2(dest, src)
        #if COPY_CHUNK_SIZE != 16
          if (dest >= dest_lim) break;
        #endif
        COPY_CHUNK_4_2(dest, src)
      }
      while (dest < dest_lim);
      // return;
    }
    else
#endif
    {
      // (offset < 4)
      if (offset == 2)
      {
#if defined(Z7_XPRESS_DEC_USE_UNALIGNED_COPY)
        UInt32 w0 = GetUi16(src);
        w0 += w0 << 16;
        do
        {
          SetUi32(dest, w0)
          dest += 4;
        }
        while (dest < dest_lim);
#else
        const unsigned b0 = src[0];
        const Byte b1 = src[1];
        do
        {
          dest[0] = (Byte)b0;
          dest[1] = b1;
          dest += 2;
        }
        while (dest < dest_lim);
#endif
      }
      else // (offset == 3)
      {
        const unsigned b0 = src[0];
#if defined(Z7_XPRESS_DEC_USE_UNALIGNED_COPY)
        const unsigned w1 = GetUi16(src + 1);
        do
        {
          dest[0] = (Byte)b0;
          SetUi16(dest + 1, (UInt16)w1)
          dest += 3;
        }
        while (dest < dest_lim);
#else
        const Byte b1 = src[1];
        const Byte b2 = src[2];
        do
        {
          dest[0] = (Byte)b0;
          dest[1] = b1;
          dest[2] = b2;
          dest += 3;
        }
        while (dest < dest_lim);
#endif
      }
    }
  }
}


namespace NCompress {
namespace NXpress {

#define BIT_STREAM_NORMALIZE \
    if (BitPos > 16) { \
      if (in >= lim) return S_FALSE; \
      BitPos -= 16; \
      Value |= (UInt32)GetUi16(in) << BitPos; \
      in += 2; }

#define MOVE_POS(bs, numBits) \
    BitPos += (unsigned)numBits; \
    Value <<= numBits; \


static const unsigned kNumHuffBits = 15;
static const unsigned kNumTableBits = 10;
static const unsigned kNumLenBits = 4;
static const unsigned kLenMask = (1 << kNumLenBits) - 1;
static const unsigned kNumPosSlots = 16;
static const unsigned kNumSyms = 256 + (kNumPosSlots << kNumLenBits);

HRESULT Decode_WithExceedWrite(const Byte *in, size_t inSize, Byte *out, size_t outSize)
{
  NCompress::NHuffman::CDecoder<kNumHuffBits, kNumSyms, kNumTableBits> huff;
  
  if (inSize < kNumSyms / 2 + 4)
    return S_FALSE;
  {
    Byte levels[kNumSyms];
    for (unsigned i = 0; i < kNumSyms / 2; i++)
    {
      const unsigned b = in[i];
      levels[(size_t)i * 2    ] = (Byte)(b & 0xf);
      levels[(size_t)i * 2 + 1] = (Byte)(b >> 4);
    }
    if (!huff.Build(levels, NHuffman::k_BuildMode_Full))
      return S_FALSE;
  }

  UInt32 Value;
  unsigned BitPos;  // how many bits in (Value) were processed

  const Byte *lim = in + inSize - 1;  // points to last byte
  in += kNumSyms / 2;
#ifdef MY_CPU_LE_UNALIGN
  Value = GetUi32(in);
  Value = rotlFixed(Value, 16);
#else
  Value = ((UInt32)GetUi16(in) << 16) | GetUi16(in + 2);
#endif
    
  in += 4;
  BitPos = 0;
  Byte *dest = out;
  const Byte *outLim = out + outSize;

  for (;;)
  {
    unsigned sym;
    Z7_HUFF_DECODE_VAL_IN_HIGH32(sym, &huff, kNumHuffBits, kNumTableBits,
        Value, Z7_HUFF_DECODE_ERROR_SYM_CHECK_NO, {}, MOVE_POS, {}, bs)
    // 0 < BitPos <= 31
    BIT_STREAM_NORMALIZE
    // 0 < BitPos <= 16

    if (dest >= outLim)
      return (sym == 256 && Value == 0 && in == lim + 1) ? S_OK : S_FALSE;

    if (sym < 256)
      *dest++ = (Byte)sym;
    else
    {
      const unsigned distBits = (unsigned)(Byte)sym >> kNumLenBits; // (sym - 256) >> kNumLenBits;
      UInt32 len = (UInt32)(sym & kLenMask);
      
      if (len == kLenMask)
      {
        if (in > lim)
          return S_FALSE;
        // here we read input bytes in out-of-order related to main input stream (bits in Value):
        len = *in++;
        if (len == 0xff)
        {
          if (in >= lim)
            return S_FALSE;
          len = GetUi16(in);
          in += 2;
        }
        else
          len += kLenMask;
      }

      len += 3;
      if (len > (size_t)(outLim - dest))
        return S_FALSE;

      if (distBits == 0)
      {
        // d == 1
        if (dest == out)
          return S_FALSE;
        Byte *destTemp = dest;
        dest += len;
        CopyMatch_1(destTemp, dest);
      }
      else
      {
        unsigned d = (unsigned)(Value >> (32 - distBits));
        MOVE_POS(bs, distBits)
        d += 1u << distBits;
        // 0 < BitPos <= 31
        BIT_STREAM_NORMALIZE
        // 0 < BitPos <= 16
        if (d > (size_t)(dest - out))
          return S_FALSE;
        Byte *destTemp = dest;
        dest += len;
        CopyMatch_Non1(destTemp, d, dest);
      }
    }
  }
}

}}
