// LzxDecoder.cpp

#include "StdAfx.h"

#include <string.h>
// #include <stdio.h>

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#define PRF(x) x
#else
#define PRF(x)
#endif

#include "../../../C/Alloc.h"
#include "../../../C/RotateDefs.h"
#include "../../../C/CpuArch.h"

#include "LzxDecoder.h"


#ifdef MY_CPU_X86_OR_AMD64
#if defined(MY_CPU_AMD64)  \
    || defined(__SSE2__) \
    || defined(_M_IX86_FP) && (_M_IX86_FP >= 2) \
    || 0 && defined(_MSC_VER) && (_MSC_VER >= 1400) // set (1 &&) for debug
  
#if defined(__clang__) && (__clang_major__ >= 2) \
    || defined(__GNUC__) && (__GNUC__ >= 4) \
    || defined(_MSC_VER) && (_MSC_VER >= 1400)
#define Z7_LZX_X86_FILTER_USE_SSE2
#endif
#endif
#endif


#ifdef Z7_LZX_X86_FILTER_USE_SSE2
// #ifdef MY_CPU_X86_OR_AMD64
#include <emmintrin.h> // SSE2
// #endif
  #if defined(__clang__) || defined(__GNUC__)
    typedef int ctz_type;
    #define MY_CTZ(dest, mask) dest = __builtin_ctz((UInt32)(mask))
  #else  // #if defined(_MSC_VER)
    #if (_MSC_VER >= 1600)
      // #include <intrin.h>
    #endif
    typedef unsigned long ctz_type;
    #define MY_CTZ(dest, mask)  _BitScanForward(&dest, (mask));
  #endif // _MSC_VER
#endif

// when window buffer is filled, we must wrap position to zero,
// and we want to wrap at same points where original-lzx must wrap.
// But the wrapping is possible in point where chunk is finished.
// Usually (chunk_size == 32KB), but (chunk_size != 32KB) also is allowed.
// So we don't use additional buffer space over required (winSize).
// And we can't use large overwrite after (len) in CopyLzMatch().
// But we are allowed to write 3 bytes after (len), because
// (delta <= _winSize - 3).

// #define k_Lz_OverwriteSize  0  // for debug : to disable overwrite
#define k_Lz_OverwriteSize  3 // = kNumReps
#if k_Lz_OverwriteSize > 0
// (k_Lz_OutBufSize_Add >= k_Lz_OverwriteSize) is required
// we use value 4 to simplify memset() code.
#define k_Lz_OutBufSize_Add  (k_Lz_OverwriteSize + 1) // == 4
#else
#define k_Lz_OutBufSize_Add  0
#endif

// (len != 0)
// (0 < delta <= _winSize - 3)
Z7_FORCE_INLINE
void CopyLzMatch(Byte *dest, const Byte *src, UInt32 len, UInt32 delta)
{
  if (delta >= 4)
  {
#if k_Lz_OverwriteSize >= 3
    // optimized code with overwrite to reduce the number of branches
  #ifdef MY_CPU_LE_UNALIGN
    *(UInt32 *)(void *)(dest) = *(const UInt32 *)(const void *)(src);
  #else
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
  #endif
    len--;
    src++;
    dest++;
    {
#else
    // no overwrite in out buffer
    dest[0] = src[0];
    {
      const unsigned m = (unsigned)len & 1;
      src += m;
      dest += m;
    }
    if (len &= ~(unsigned)1)
    {
      dest[0] = src[0];
      dest[1] = src[1];
#endif
      // len == 0 is allowed here
      {
        const unsigned m = (unsigned)len & 3;
        src += m;
        dest += m;
      }
      if (len &= ~(unsigned)3)
      {
#ifdef MY_CPU_LE_UNALIGN
      #if 1
        *(UInt32 *)(void *)(dest) = *(const UInt32 *)(const void *)(src);
        {
          const unsigned m = (unsigned)len & 7;
          dest += m;
          src += m;
        }
        if (len &= ~(unsigned)7)
          do
          {
            *(UInt32 *)(void *)(dest    ) = *(const UInt32 *)(const void *)(src);
            *(UInt32 *)(void *)(dest + 4) = *(const UInt32 *)(const void *)(src + 4);
            src += 8;
            dest += 8;
          }
          while (len -= 8);
      #else
        // gcc-11 -O3 for x64 generates incorrect code here
        do
        {
          *(UInt32 *)(void *)(dest) = *(const UInt32 *)(const void *)(src);
          src += 4;
          dest += 4;
        }
        while (len -= 4);
      #endif
#else
        do
        {
          const Byte b0 = src[0];
          const Byte b1 = src[1];
          dest[0] = b0;
          dest[1] = b1;
          const Byte b2 = src[2];
          const Byte b3 = src[3];
          dest[2] = b2;
          dest[3] = b3;
          src += 4;
          dest += 4;
        }
        while (len -= 4);
#endif
      }
    }
  }
  else // (delta < 4)
  {
    const unsigned b0 = *src;
    *dest = (Byte)b0;
    if (len >= 2)
    {
      if (delta < 2)
      {
        dest += (unsigned)len & 1;
        dest[0] = (Byte)b0;
        dest[1] = (Byte)b0;
        dest += (unsigned)len & 2;
        if (len &= ~(unsigned)3)
        {
#ifdef MY_CPU_LE_UNALIGN
          #ifdef MY_CPU_64BIT
          const UInt64 a = (UInt64)b0 * 0x101010101010101;
          *(UInt32 *)(void *)dest = (UInt32)a;
          dest += (unsigned)len & 7;
          if (len &= ~(unsigned)7)
          {
            // *(UInt64 *)(void *)dest = a;
            // dest += 8;
            // len -= 8;
            // if (len)
            {
              // const ptrdiff_t delta = (ptrdiff_t)dest & 7;
              // dest -= delta;
              do
              {
                *(UInt64 *)(void *)dest = a;
                dest += 8;
              }
              while (len -= 8);
              // dest += delta - 8;
              // *(UInt64 *)(void *)dest = a;
            }
          }
          #else
          const UInt32 a = (UInt32)b0 * 0x1010101;
          do
          {
            *(UInt32 *)(void *)dest = a;
            dest += 4;
          }
          while (len -= 4);
          #endif
#else
          do
          {
            dest[0] = (Byte)b0;
            dest[1] = (Byte)b0;
            dest[2] = (Byte)b0;
            dest[3] = (Byte)b0;
            dest += 4;
          }
          while (len -= 4);
#endif
        }
      }
      else if (delta == 2)
      {
        const unsigned m = (unsigned)len & 1;
        len &= ~(unsigned)1;
        src += m;
        dest += m;
        {
          const Byte a0 = src[0];
          const Byte a1 = src[1];
          do
          {
            dest[0] = a0;
            dest[1] = a1;
            dest += 2;
          }
          while (len -= 2);
        }
      }
      else /* if (delta == 3) */
      {
        const unsigned b1 = src[1];
        dest[1] = (Byte)b1;
        if (len -= 2)
        {
          const unsigned b2 = src[2];
          dest += 2;
          do
          {
            dest[0] = (Byte)b2;  if (--len == 0) break;
            dest[1] = (Byte)b0;  if (--len == 0) break;
            dest[2] = (Byte)b1;
            dest += 3;
          }
          while (--len);
        }
      }
    }
  }
}

// #define Z7_LZX_SHOW_STAT
#ifdef Z7_LZX_SHOW_STAT
#include <stdio.h>
#endif

namespace NCompress {
namespace NLzx {

// #define Z7_LZX_SHOW_STAT
#ifdef Z7_LZX_SHOW_STAT
static UInt32 g_stats_Num_x86[3];
static UInt32 g_stats_NumTables;
static UInt32 g_stats_NumLits;
static UInt32 g_stats_NumAlign;
static UInt32 g_stats_main[kMainTableSize];
static UInt32 g_stats_len[kNumLenSymbols];
static UInt32 g_stats_main_levels[kNumHuffmanBits + 1];
static UInt32 g_stats_len_levels[kNumHuffmanBits + 1];
#define UPDATE_STAT(a) a
static void PrintVal(UInt32 v)
{
  printf("\n    : %9u", v);
}
static void PrintStat(const char *name, const UInt32 *a, size_t num)
{
  printf("\n\n==== %s:", name);
  UInt32 sum = 0;
  size_t i;
  for (i = 0; i < num; i++)
    sum += a[i];
  PrintVal(sum);
  if (sum != 0)
  {
    for (i = 0; i < num; i++)
    {
      if (i % 8 == 0)
        printf("\n");
      printf("\n%3x : %9u : %5.2f", (unsigned)i, (unsigned)a[i], (double)a[i] * 100 / sum);
    }
  }
  printf("\n");
}

static struct CStat
{
  ~CStat()
  {
    PrintStat("x86_filter", g_stats_Num_x86, Z7_ARRAY_SIZE(g_stats_Num_x86));
    printf("\nTables:"); PrintVal(g_stats_NumTables);
    printf("\nLits:");   PrintVal(g_stats_NumLits);
    printf("\nAlign:");  PrintVal(g_stats_NumAlign);
    PrintStat("Main", g_stats_main, Z7_ARRAY_SIZE(g_stats_main));
    PrintStat("Len", g_stats_len, Z7_ARRAY_SIZE(g_stats_len));
    PrintStat("Main Levels", g_stats_main_levels, Z7_ARRAY_SIZE(g_stats_main_levels));
    PrintStat("Len Levels", g_stats_len_levels, Z7_ARRAY_SIZE(g_stats_len_levels));
  }
} g_stat;
#else
#define UPDATE_STAT(a)
#endif



/*
3 p015  : ivb-   : or r32,r32 / add r32,r32
4 p0156 : hsw+
5 p0156b: adl+
2 p0_5  : ivb-   : shl r32,i8
2 p0__6 : hsw+
1 p5    : ivb-   : jb
2 p0__6 : hsw+
2 p0_5  : wsm-    : SSE2  : pcmpeqb  : _mm_cmpeq_epi8
2 p_15  : snb-bdw
2 p01   : skl+
1 p0              : SSE2  : pmovmskb : _mm_movemask_epi8
*/
/*
  v24.00: the code was fixed for more compatibility with original-ms-cab-decoder.
  for ((Int32)translationSize >= 0) : LZX specification shows the code with signed Int32.
  for ((Int32)translationSize <  0) : no specification for that case, but we support that case.
  We suppose our code now is compatible with original-ms-cab-decoder.

  Starting byte of data stream (real_pos == 0) is special corner case,
  where we don't need any conversion (as in original-ms-cab-decoder).
  Our optimization: we use unsigned (UInt32 pos) (pos = -1 - real_pos).
  So (pos) is always negative: ((Int32)pos < 0).
  It allows us to use simple comparison (v > pos) instead of more complex comparisons.
*/
// (p) will point 5 bytes after 0xe8 byte:
// pos == -1 - (p - 5 - data_start) == 4 + data_start - p
// (FILTER_PROCESSED_SIZE_DELTA == 4) is optimized value for better speed in some compilers:
#define FILTER_PROCESSED_SIZE_DELTA  4

#if defined(MY_CPU_X86_OR_AMD64) || defined(MY_CPU_ARM_OR_ARM64)
  // optimized branch:
  // size_t must be at least 32-bit for this branch.
  #if 1 // use 1 for simpler code
    // use integer (low 32 bits of pointer) instead of pointer
    #define X86_FILTER_PREPARE  processedSize4 = (UInt32)(size_t)(ptrdiff_t)data + \
        (UInt32)(4 - FILTER_PROCESSED_SIZE_DELTA) - processedSize4;
    #define X86_FILTER_CALC_pos(p)  const UInt32 pos = processedSize4 - (UInt32)(size_t)(ptrdiff_t)p;
  #else
    // note: (dataStart) pointer can point out of array ranges:
    #define X86_FILTER_PREPARE  const Byte *dataStart = data + \
                (4 - FILTER_PROCESSED_SIZE_DELTA) - processedSize4;
    #define X86_FILTER_CALC_pos(p)  const UInt32 pos = (UInt32)(size_t)(dataStart - p);
  #endif
#else
  // non-optimized branch for unusual platforms (16-bit size_t or unusual size_t):
    #define X86_FILTER_PREPARE  processedSize4 = \
        (UInt32)(4 - FILTER_PROCESSED_SIZE_DELTA) - processedSize4;
    #define X86_FILTER_CALC_pos(p)  const UInt32 pos = processedSize4 - (UInt32)(size_t)(p - data);
#endif

#define X86_TRANSLATE_PRE(p) \
    UInt32 v = GetUi32((p) - 4);

#define X86_TRANSLATE_POST(p) \
  { \
    X86_FILTER_CALC_pos(p) \
    if (v < translationSize) { \
      UPDATE_STAT(g_stats_Num_x86[0]++;) \
      v += pos + 1; \
      SetUi32((p) - 4, v) \
    } \
    else if (v > pos) { \
      UPDATE_STAT(g_stats_Num_x86[1]++;) \
      v += translationSize; \
      SetUi32((p) - 4, v) \
    } else { UPDATE_STAT(g_stats_Num_x86[2]++;) } \
  }


/*
  if (   defined(Z7_LZX_X86_FILTER_USE_SSE2)
      && defined(Z7_LZX_X86_FILTER_USE_SSE2_ALIGNED))
    the function can read up to aligned_for_32_up_from(size) bytes in (data).
*/
// processedSize < (1 << 30)
Z7_NO_INLINE
static void x86_Filter4(Byte *data, size_t size, UInt32 processedSize4, UInt32 translationSize)
{
  const size_t kResidue = 10;
  if (size <= kResidue)
    return;
  Byte * const lim = data + size - kResidue + 4;
  const Byte save = lim[0];
  lim[0] = 0xe8;
  X86_FILTER_PREPARE
  Byte *p = data;

#define FILTER_RETURN_IF_LIM(_p_)  if (_p_ > lim) { lim[0] = save; return; }

#ifdef Z7_LZX_X86_FILTER_USE_SSE2

// sse2-aligned/sse2-unaligned provide same speed on real data.
// but the code is smaller for sse2-unaligned version.
// for debug : define it to get alternative version with aligned 128-bit reads:
// #define Z7_LZX_X86_FILTER_USE_SSE2_ALIGNED

#define FILTER_MASK_INT  UInt32
#define FILTER_NUM_VECTORS_IN_CHUNK   2
#define FILTER_CHUNK_BYTES_OFFSET     (16 * FILTER_NUM_VECTORS_IN_CHUNK - 5)

#ifdef Z7_LZX_X86_FILTER_USE_SSE2_ALIGNED
  // aligned version doesn't uses additional space if buf size is aligned for 32
  #define k_Filter_OutBufSize_Add  0
  #define k_Filter_OutBufSize_AlignMask  (16 * FILTER_NUM_VECTORS_IN_CHUNK - 1)
  #define FILTER_LOAD_128(p)  _mm_load_si128 ((const __m128i *)(const void *)(p))
#else
  #define k_Filter_OutBufSize_Add  (16 * FILTER_NUM_VECTORS_IN_CHUNK)
  #define k_Filter_OutBufSize_AlignMask 0
  #define FILTER_LOAD_128(p)  _mm_loadu_si128((const __m128i *)(const void *)(p))
#endif

#define GET_E8_MASK(dest, dest1, p) \
{ \
  __m128i v0 = FILTER_LOAD_128(p); \
  __m128i v1 = FILTER_LOAD_128(p + 16); \
  p += 16 * FILTER_NUM_VECTORS_IN_CHUNK; \
  v0 = _mm_cmpeq_epi8(v0, k_e8_Vector); \
  v1 = _mm_cmpeq_epi8(v1, k_e8_Vector); \
  dest  = (unsigned)_mm_movemask_epi8(v0); \
  dest1 = (unsigned)_mm_movemask_epi8(v1); \
}

  const __m128i k_e8_Vector = _mm_set1_epi32((Int32)(UInt32)0xe8e8e8e8);
  for (;;)
  {
      // for debug: define it for smaller code:
      // #define Z7_LZX_X86_FILTER_CALC_IN_LOOP
      // without Z7_LZX_X86_FILTER_CALC_IN_LOOP, we can get faster and simpler loop
    FILTER_MASK_INT mask;
    {
      FILTER_MASK_INT mask1;
      do
      {
        GET_E8_MASK(mask, mask1, p)
        #ifndef Z7_LZX_X86_FILTER_CALC_IN_LOOP
          mask += mask1;
        #else
          mask |= mask1 << 16;
        #endif
      }
      while (!mask);
     
      #ifndef Z7_LZX_X86_FILTER_CALC_IN_LOOP
        mask -= mask1;
        mask |= mask1 << 16;
      #endif
    }
      
#ifdef Z7_LZX_X86_FILTER_USE_SSE2_ALIGNED
    for (;;)
    {
      ctz_type index;
      typedef
      #ifdef MY_CPU_64BIT
        UInt64
      #else
        UInt32
      #endif
        SUPER_MASK_INT;
      SUPER_MASK_INT superMask;
      {
        MY_CTZ(index, mask);
        Byte *p2 = p - FILTER_CHUNK_BYTES_OFFSET + (unsigned)index;
        X86_TRANSLATE_PRE(p2)
        superMask = ~(SUPER_MASK_INT)0x1f << index;
        FILTER_RETURN_IF_LIM(p2)
        X86_TRANSLATE_POST(p2)
        mask &= (UInt32)superMask;
      }
      if (mask)
        continue;
      if (index <= FILTER_CHUNK_BYTES_OFFSET)
        break;
      {
        FILTER_MASK_INT mask1;
        GET_E8_MASK(mask, mask1, p)
        mask &=
            #ifdef MY_CPU_64BIT
              (UInt32)(superMask >> 32);
            #else
              ((FILTER_MASK_INT)0 - 1) << ((int)index - FILTER_CHUNK_BYTES_OFFSET);
            #endif
        mask |= mask1 << 16;
      }
      if (!mask)
        break;
    }
#else // ! Z7_LZX_X86_FILTER_USE_SSE2_ALIGNED
    {
      // we use simplest version without loop:
      // for (;;)
      {
        ctz_type index;
        MY_CTZ(index, mask);
        /*
        printf("\np=%p, mask=%8x, index = %2d, p + index = %x\n",
            (p - 16 * FILTER_NUM_VECTORS_IN_CHUNK), (unsigned)mask,
            (unsigned)index, (unsigned)((unsigned)(ptrdiff_t)(p - 16 * FILTER_NUM_VECTORS_IN_CHUNK) + index));
        */
        p += (size_t)(unsigned)index - FILTER_CHUNK_BYTES_OFFSET;
        FILTER_RETURN_IF_LIM(p)
        // mask &= ~(FILTER_MASK_INT)0x1f << index;  mask >>= index;
        X86_TRANSLATE_PRE(p)
        X86_TRANSLATE_POST(p)
        // if (!mask) break; // p += 16 * FILTER_NUM_VECTORS_IN_CHUNK;
      }
    }
#endif // ! Z7_LZX_X86_FILTER_USE_SSE2_ALIGNED
  }

#else // ! Z7_LZX_X86_FILTER_USE_SSE2

#define k_Filter_OutBufSize_Add  0
#define k_Filter_OutBufSize_AlignMask 0


  for (;;)
  {
    for (;;)
    {
      if (p[0] == 0xe8) { p += 5; break; }
      if (p[1] == 0xe8) { p += 6; break; }
      if (p[2] == 0xe8) { p += 7; break; }
      p += 4;
      if (p[-1] == 0xe8) { p += 4; break; }
    }
    FILTER_RETURN_IF_LIM(p)
    X86_TRANSLATE_PRE(p)
    X86_TRANSLATE_POST(p)
  }

#endif // ! Z7_LZX_X86_FILTER_USE_SSE2
}


CDecoder::CDecoder() throw():
    _win(NULL),
    _isUncompressedBlock(false),
    _skipByte(false),
    _keepHistory(false),
    _keepHistoryForNext(true),
    _needAlloc(true),
    _wimMode(false),
    _numDictBits(15),
    _unpackBlockSize(0),
    _x86_translationSize(0),
    _x86_buf(NULL),
    _unpackedData(NULL)
{
  {
    // it's better to get empty virtual entries, if mispredicted value can be used:
    memset(_reps, 0, kPosSlotOffset * sizeof(_reps[0]));
    memset(_extra, 0, kPosSlotOffset);
#define SET_NUM_BITS(i) i // #define NUM_BITS_DELTA 31
    _extra[kPosSlotOffset + 0] = SET_NUM_BITS(0);
    _extra[kPosSlotOffset + 1] = SET_NUM_BITS(0);
    // reps[0] = 0 - (kNumReps - 1);
    // reps[1] = 1 - (kNumReps - 1);
    UInt32 a = 2 - (kNumReps - 1);
    UInt32 delta = 1;
    unsigned i;
    for (i = 0; i < kNumLinearPosSlotBits; i++)
    {
      _extra[(size_t)i * 2 + 2 + kPosSlotOffset] = (Byte)(SET_NUM_BITS(i));
      _extra[(size_t)i * 2 + 3 + kPosSlotOffset] = (Byte)(SET_NUM_BITS(i));
      _reps [(size_t)i * 2 + 2 + kPosSlotOffset] = a;  a += delta;
      _reps [(size_t)i * 2 + 3 + kPosSlotOffset] = a;  a += delta;
      delta += delta;
    }
    for (i = kNumLinearPosSlotBits * 2 + 2; i < kNumPosSlots; i++)
    {
      _extra[(size_t)i + kPosSlotOffset] = SET_NUM_BITS(kNumLinearPosSlotBits);
      _reps [(size_t)i + kPosSlotOffset] = a;
      a += (UInt32)1 << kNumLinearPosSlotBits;
    }
  }
}

CDecoder::~CDecoder() throw()
{
  if (_needAlloc)
    // BigFree
    z7_AlignedFree
      (_win);
  z7_AlignedFree(_x86_buf);
}

HRESULT CDecoder::Flush() throw()
{
  // UInt32 t = _x86_processedSize; for (int y = 0; y < 50; y++) { _x86_processedSize = t; // benchmark: (branch predicted)
  if (_x86_translationSize != 0)
  {
    Byte *destData = _win + _writePos;
    const UInt32 curSize = _pos - _writePos;
    if (_keepHistoryForNext)
    {
      const size_t kChunkSize = (size_t)1 << 15;
      if (curSize > kChunkSize)
        return E_NOTIMPL;
      if (!_x86_buf)
      {
        // (kChunkSize % 32 == 0) is required in some cases, because
        // the filter can read data by 32-bytes chunks in some cases.
        // if (chunk_size > (1 << 15)) is possible, then we must the code:
        const size_t kAllocSize = kChunkSize + k_Filter_OutBufSize_Add;
        _x86_buf = (Byte *)z7_AlignedAlloc(kAllocSize);
        if (!_x86_buf)
          return E_OUTOFMEMORY;
        #if 0 != k_Filter_OutBufSize_Add || \
            0 != k_Filter_OutBufSize_AlignMask
          // x86_Filter4() can read after curSize.
          // So we set all data to zero to prevent reading of uninitialized data:
          memset(_x86_buf, 0, kAllocSize); // optional
        #endif
      }
      // for (int yy = 0; yy < 1; yy++) // for debug
      memcpy(_x86_buf, destData, curSize);
      _unpackedData = _x86_buf;
      destData = _x86_buf;
    }
    else
    {
      // x86_Filter4() can overread after (curSize),
      // so we can do memset() after (curSize):
      // k_Filter_OutBufSize_AlignMask also can be used
      // if (!_overDict) memset(destData + curSize, 0, k_Filter_OutBufSize_Add);
    }
    x86_Filter4(destData, curSize, _x86_processedSize - FILTER_PROCESSED_SIZE_DELTA, _x86_translationSize);
    _x86_processedSize += (UInt32)curSize;
    if (_x86_processedSize >= ((UInt32)1 << 30))
      _x86_translationSize = 0;
  }
  // }
  return S_OK;
}



// (NUM_DELTA_BYTES == 2) reduces the code in main loop.
#if 1
  #define NUM_DELTA_BYTES  2
#else
  #define NUM_DELTA_BYTES  0
#endif

#define NUM_DELTA_BIT_OFFSET_BITS  (NUM_DELTA_BYTES * 8)

#if NUM_DELTA_BIT_OFFSET_BITS > 0
  #define DECODE_ERROR_CODE  0
  #define IS_OVERFLOW_bitOffset(bo)  ((bo) >= 0)
  // ( >= 0) comparison after bitOffset change gives simpler commands than ( > 0) comparison
#else
  #define DECODE_ERROR_CODE  1
  #define IS_OVERFLOW_bitOffset(bo)  ((bo) >  0)
#endif

// (numBits != 0)
#define GET_VAL_BASE(numBits)  (_value >> (32 - (numBits)))

#define Z7_LZX_HUFF_DECODE( sym, huff, kNumTableBits, move_pos_op, check_op, error_op) \
    Z7_HUFF_DECODE_VAL_IN_HIGH32(sym, huff, kNumHuffmanBits, kNumTableBits,  \
        _value, check_op, error_op, move_pos_op, NORMALIZE, bs)

#define Z7_LZX_HUFF_DECODE_CHECK_YES(sym, huff, kNumTableBits, move_pos_op) \
        Z7_LZX_HUFF_DECODE(          sym, huff, kNumTableBits, move_pos_op, \
            Z7_HUFF_DECODE_ERROR_SYM_CHECK_YES, { return DECODE_ERROR_CODE; })

#define Z7_LZX_HUFF_DECODE_CHECK_NO( sym, huff, kNumTableBits, move_pos_op) \
        Z7_LZX_HUFF_DECODE(          sym, huff, kNumTableBits, move_pos_op, \
            Z7_HUFF_DECODE_ERROR_SYM_CHECK_NO, {})

#define NORMALIZE \
{ \
  const Byte *ptr = _buf + (_bitOffset >> 4) * 2; \
  /* _value = (((UInt32)GetUi16(ptr) << 16) | GetUi16(ptr + 2)) << (_bitOffset & 15); */ \
  const UInt32 v = GetUi32(ptr); \
  _value = rotlFixed (v, ((int)_bitOffset & 15) + 16); \
}

#define MOVE_POS(bs, numBits) \
{ \
  _bitOffset += numBits; \
}

#define MOVE_POS_STAT(bs, numBits) \
{ \
  UPDATE_STAT(g_stats_len_levels[numBits]++;) \
  MOVE_POS(bs, numBits); \
}

#define MOVE_POS_CHECK(bs, numBits) \
{ \
  if (IS_OVERFLOW_bitOffset(_bitOffset += numBits)) return DECODE_ERROR_CODE; \
}

#define MOVE_POS_CHECK_STAT(bs, numBits) \
{ \
  UPDATE_STAT(g_stats_main_levels[numBits]++;) \
  MOVE_POS_CHECK(bs, numBits) \
}


// (numBits == 0) is supported

#ifdef Z7_HUFF_USE_64BIT_LIMIT

#define MACRO_ReadBitsBig_pre(numBits) \
{ \
  _bitOffset += (numBits); \
  _value >>= 32 - (numBits); \
}

#else

#define MACRO_ReadBitsBig_pre(numBits) \
{ \
  _bitOffset += (numBits); \
  _value = (UInt32)((UInt32)_value >> 1 >> (31 ^ (numBits))); \
}

#endif


#define MACRO_ReadBitsBig_add(dest) \
  { dest += (UInt32)_value; }

#define MACRO_ReadBitsBig_add3(dest) \
  { dest += (UInt32)(_value) << 3; }


// (numBits != 0)
#define MACRO_ReadBits_NonZero(val, numBits) \
{ \
  val = (UInt32)(_value >> (32 - (numBits))); \
  MOVE_POS(bs, numBits); \
  NORMALIZE \
}


struct CBitDecoder
{
  ptrdiff_t _bitOffset;
  const Byte *_buf;

  Z7_FORCE_INLINE
  UInt32 GetVal() const
  {
    const Byte *ptr = _buf + (_bitOffset >> 4) * 2;
    const UInt32 v = GetUi32(ptr);
    return rotlFixed (v, ((int)_bitOffset & 15) + 16);
  }

  Z7_FORCE_INLINE
  bool IsOverRead() const
  {
    return _bitOffset > (int)(0 - NUM_DELTA_BIT_OFFSET_BITS);
  }


  Z7_FORCE_INLINE
  bool WasBitStreamFinishedOK() const
  {
    // we check that all 0-15 unused bits are zeros:
    if (_bitOffset == 0 - NUM_DELTA_BIT_OFFSET_BITS)
      return true;
    if ((_bitOffset + NUM_DELTA_BIT_OFFSET_BITS + 15) & ~(ptrdiff_t)15)
      return false;
    const Byte *ptr = _buf - NUM_DELTA_BYTES - 2;
    if ((UInt16)(GetUi16(ptr) << (_bitOffset & 15)))
      return false;
    return true;
  }

  // (numBits != 0)
  Z7_FORCE_INLINE
  UInt32 ReadBits_NonZero(unsigned numBits) throw()
  {
    const UInt32 val = GetVal() >> (32 - numBits);
    _bitOffset += numBits;
    return val;
  }
};


class CBitByteDecoder: public CBitDecoder
{
  size_t _size;
public:

  Z7_FORCE_INLINE
  void Init_ByteMode(const Byte *data, size_t size)
  {
    _buf = data;
    _size = size;
  }

  Z7_FORCE_INLINE
  void Init_BitMode(const Byte *data, size_t size)
  {
    _size = size & 1;
    size &= ~(size_t)1;
    _buf = data + size + NUM_DELTA_BYTES;
    _bitOffset = 0 - (ptrdiff_t)(size * 8) - NUM_DELTA_BIT_OFFSET_BITS;
  }

  Z7_FORCE_INLINE
  void Switch_To_BitMode()
  {
    Init_BitMode(_buf, _size);
  }

  Z7_FORCE_INLINE
  bool Switch_To_ByteMode()
  {
    /* here we check that unused bits in high 16-bits word are zeros.
       If high word is full (all 16-bits are unused),
       we check that all 16-bits are zeros.
       So we check and skip (1-16 bits) unused bits */
    if ((GetVal() >> (16 + (_bitOffset & 15))) != 0)
      return false;
    _bitOffset += 16;
    _bitOffset &= ~(ptrdiff_t)15;
    if (_bitOffset > 0 - NUM_DELTA_BIT_OFFSET_BITS)
      return false;
    const ptrdiff_t delta = _bitOffset >> 3;
    _size = (size_t)((ptrdiff_t)(_size) - delta - NUM_DELTA_BYTES);
    _buf += delta;
    // _bitOffset = 0; // optional
    return true;
  }

  Z7_FORCE_INLINE
  size_t GetRem() const { return _size; }

  Z7_FORCE_INLINE
  UInt32 ReadUInt32()
  {
    const Byte *ptr = _buf;
    const UInt32 v = GetUi32(ptr);
    _buf += 4;
    _size -= 4;
    return v;
  }

  Z7_FORCE_INLINE
  void CopyTo(Byte *dest, size_t size)
  {
    memcpy(dest, _buf, size);
    _buf += size;
    _size -= size;
  }

  Z7_FORCE_INLINE
  bool IsOneDirectByteLeft() const
  {
    return GetRem() == 1;
  }

  Z7_FORCE_INLINE
  Byte DirectReadByte()
  {
    _size--;
    return *_buf++;
  }
};


// numBits != 0
// Z7_FORCE_INLINE
Z7_NO_INLINE
static
UInt32 ReadBits(CBitDecoder &_bitStream, unsigned numBits)
{
  return _bitStream.ReadBits_NonZero(numBits);
}

#define RIF(x) { if (!(x)) return false; }


/*
MSVC compiler adds extra move operation,
  if we access array with 32-bit index
    array[calc_index_32_bit(32-bit_var)]
    where calc_index_32_bit operations are: ((unsigned)a>>cnt), &, ^, |
  clang is also affected for ((unsigned)a>>cnt) in byte array.
*/

// it can overread input buffer for 7-17 bytes.
// (levels != levelsEnd)
Z7_NO_INLINE
static ptrdiff_t ReadTable(ptrdiff_t _bitOffset, const Byte *_buf, Byte *levels, const Byte *levelsEnd)
{
  const unsigned kNumTableBits_Level = 7;
  NHuffman::CDecoder256<kNumHuffmanBits, kLevelTableSize, kNumTableBits_Level> _levelDecoder;
  NHuffman::CValueInt _value;
  // optional check to reduce size of overread zone:
  if (_bitOffset > (int)0 - (int)NUM_DELTA_BIT_OFFSET_BITS - (int)(kLevelTableSize * kNumLevelBits))
    return DECODE_ERROR_CODE;
  NORMALIZE
  {
    Byte levels2[kLevelTableSize / 4 * 4];
    for (size_t i = 0; i < kLevelTableSize / 4 * 4; i += 4)
    {
      UInt32 val;
      MACRO_ReadBits_NonZero(val, kNumLevelBits * 4)
      levels2[i + 0] = (Byte)((val >> (3 * kNumLevelBits)));
      levels2[i + 1] = (Byte)((val >> (2 * kNumLevelBits)) & ((1u << kNumLevelBits) - 1));
      levels2[i + 2] = (Byte)((Byte)val >> (1 * kNumLevelBits));
      levels2[i + 3] = (Byte)((val) & ((1u << kNumLevelBits) - 1));
    }
    RIF(_levelDecoder.Build(levels2, NHuffman::k_BuildMode_Full))
  }
  
  do
  {
    unsigned sym;
    Z7_LZX_HUFF_DECODE_CHECK_NO(sym, &_levelDecoder, kNumTableBits_Level, MOVE_POS_CHECK)
    // Z7_HUFF_DECODE_CHECK(sym, &_levelDecoder, kNumHuffmanBits, kNumTableBits_Level, &bitStream, return false)
    // sym = _levelDecoder.Decode(&bitStream);
    // if (!_levelDecoder.Decode_SymCheck_MovePosCheck(&bitStream, sym)) return false;

    if (sym <= kNumHuffmanBits)
    {
      int delta = (int)*levels - (int)sym;
      delta += delta < 0 ? kNumHuffmanBits + 1 : 0;
      *levels++ = (Byte)delta;
      continue;
    }
    
    unsigned num;
    int symbol;

    if (sym < kLevelSym_Same)
    {
      // sym -= kLevelSym_Zero1;
      MACRO_ReadBits_NonZero(num, kLevelSym_Zero1_NumBits + (sym - kLevelSym_Zero1))
      num += (sym << kLevelSym_Zero1_NumBits) - (kLevelSym_Zero1 << kLevelSym_Zero1_NumBits) + kLevelSym_Zero1_Start;
      symbol = 0;
    }
    // else if (sym != kLevelSym_Same) return DECODE_ERROR_CODE;
    else // (sym == kLevelSym_Same)
    {
      MACRO_ReadBits_NonZero(num, kLevelSym_Same_NumBits)
      num += kLevelSym_Same_Start;
      // + (unsigned)bitStream.ReadBitsSmall(kLevelSym_Same_NumBits);
      // Z7_HUFF_DECODE_CHECK(sym, &_levelDecoder, kNumHuffmanBits, kNumTableBits_Level, &bitStream, return DECODE_ERROR_CODE)
      // if (!_levelDecoder.Decode2(&bitStream, sym)) return DECODE_ERROR_CODE;
      // sym = _levelDecoder.Decode(&bitStream);

      Z7_LZX_HUFF_DECODE_CHECK_NO(sym, &_levelDecoder, kNumTableBits_Level, MOVE_POS)

      if (sym > kNumHuffmanBits) return DECODE_ERROR_CODE;
      symbol = *levels - (int)sym;
      symbol += symbol < 0 ? kNumHuffmanBits + 1 : 0;
    }

    if (num > (size_t)(levelsEnd - levels))
      return false;
    const Byte *limit = levels + num;
    do
      *levels++ = (Byte)symbol;
    while (levels != limit);
  }
  while (levels != levelsEnd);

  return _bitOffset;
}


static const unsigned kPosSlotDelta = 256 / kNumLenSlots - kPosSlotOffset;


#define READ_TABLE(_bitStream, levels, levelsEnd) \
{ \
  _bitStream._bitOffset = ReadTable(_bitStream._bitOffset, _bitStream._buf, levels, levelsEnd); \
  if (_bitStream.IsOverRead()) return false; \
}

// can over-read input buffer for less than 32 bytes
bool CDecoder::ReadTables(CBitByteDecoder &_bitStream) throw()
{
  UPDATE_STAT(g_stats_NumTables++;)
  {
    const unsigned blockType = (unsigned)ReadBits(_bitStream, kBlockType_NumBits);
    // if (blockType > kBlockType_Uncompressed || blockType == 0)
    if ((unsigned)(blockType - 1) > kBlockType_Uncompressed - 1)
      return false;
    _unpackBlockSize = 1u << 15;
    if (!_wimMode || ReadBits(_bitStream, 1) == 0)
    {
      _unpackBlockSize = ReadBits(_bitStream, 16);
      // wimlib supports chunks larger than 32KB (unsupported my MS wim).
      if (!_wimMode || _numDictBits >= 16)
      {
        _unpackBlockSize <<= 8;
        _unpackBlockSize |= ReadBits(_bitStream, 8);
      }
    }

    PRF(printf("\nBlockSize = %6d   %s  ", _unpackBlockSize, (_pos & 1) ? "@@@" : "   "));

    _isUncompressedBlock = (blockType == kBlockType_Uncompressed);
    _skipByte = false;

    if (_isUncompressedBlock)
    {
      _skipByte = ((_unpackBlockSize & 1) != 0);
      // printf("\n UncompressedBlock %d", _unpackBlockSize);
      PRF(printf(" UncompressedBlock ");)
      // if (_unpackBlockSize & 1) { PRF(printf(" ######### ")); }
      if (!_bitStream.Switch_To_ByteMode())
        return false;
      if (_bitStream.GetRem() < kNumReps * 4)
        return false;
      for (unsigned i = 0; i < kNumReps; i++)
      {
        const UInt32 rep = _bitStream.ReadUInt32();
        // here we allow only such values for (rep) that can be set also by LZ code:
        if (rep == 0 || rep > _winSize - kNumReps)
          return false;
        _reps[(size_t)i + kPosSlotOffset] = rep;
      }
      // printf("\n");
      return true;
    }
    
    // _numAlignBits = 64;
    // const UInt32 k_numAlignBits_PosSlots_MAX = 64 + kPosSlotDelta;
    // _numAlignBits_PosSlots = k_numAlignBits_PosSlots_MAX;
    const UInt32 k_numAlignBits_Dist_MAX = (UInt32)(Int32)-1;
    _numAlignBits_Dist = k_numAlignBits_Dist_MAX;
    if (blockType == kBlockType_Aligned)
    {
      Byte levels[kAlignTableSize];
      // unsigned not0 = 0;
      unsigned not3 = 0;
      for (unsigned i = 0; i < kAlignTableSize; i++)
      {
        const unsigned val = ReadBits(_bitStream, kNumAlignLevelBits);
        levels[i] = (Byte)val;
        // not0 |= val;
        not3 |= (val ^ 3);
      }
      // static unsigned number = 0, all = 0; all++;
      // if (!not0) return false; // Build(true) will test this case
      if (not3)
      {
        // _numAlignBits_PosSlots = (kNumAlignBits + 1) * 2 + kPosSlotDelta;
        // _numAlignBits = kNumAlignBits;
        _numAlignBits_Dist = (1u << (kNumAlignBits + 1)) - (kNumReps - 1);
        RIF(_alignDecoder.Build(levels, true)) // full
      }
      // else { number++; if (number % 4 == 0) printf("\nnumber= %u : %u%%", number, number * 100 / all); }
    }
    // if (_numAlignBits_PosSlots == k_numAlignBits_PosSlots_MAX)
    if (_numAlignBits_Dist == k_numAlignBits_Dist_MAX)
    {
      size_t i;
      for (i = 3; i < kNumLinearPosSlotBits; i++)
      {
        _extra[i * 2 + 2 + kPosSlotOffset] = (Byte)(SET_NUM_BITS(i));
        _extra[i * 2 + 3 + kPosSlotOffset] = (Byte)(SET_NUM_BITS(i));
      }
      for (i = kNumLinearPosSlotBits * 2 + 2; i < kNumPosSlots; i++)
        _extra[i + kPosSlotOffset] = (Byte)SET_NUM_BITS(kNumLinearPosSlotBits);
    }
    else
    {
      size_t i;
      for (i = 3; i < kNumLinearPosSlotBits; i++)
      {
        _extra[i * 2 + 2 + kPosSlotOffset] = (Byte)(SET_NUM_BITS(i) - 3);
        _extra[i * 2 + 3 + kPosSlotOffset] = (Byte)(SET_NUM_BITS(i) - 3);
      }
      for (i = kNumLinearPosSlotBits * 2 + 2; i < kNumPosSlots; i++)
        _extra[i + kPosSlotOffset] = (Byte)(SET_NUM_BITS(kNumLinearPosSlotBits) - 3);
    }
  }

  READ_TABLE(_bitStream, _mainLevels, _mainLevels + 256)
  READ_TABLE(_bitStream, _mainLevels + 256, _mainLevels + 256 + _numPosLenSlots)
  const unsigned end = 256 + _numPosLenSlots;
  memset(_mainLevels + end, 0, kMainTableSize - end);
  // #define NUM_CYC 1
  // unsigned j; for (j = 0; j < NUM_CYC; j++)
  RIF(_mainDecoder.Build(_mainLevels, NHuffman::k_BuildMode_Full))
  // if (kNumLenSymols_Big_Start)
  memset(_lenLevels, 0, kNumLenSymols_Big_Start);
  READ_TABLE(_bitStream,
      _lenLevels + kNumLenSymols_Big_Start,
      _lenLevels + kNumLenSymols_Big_Start + kNumLenSymbols)
  // for (j = 0; j < NUM_CYC; j++)
  RIF(_lenDecoder.Build(_lenLevels, NHuffman::k_BuildMode_Full_or_Empty))
  return true;
}



static ptrdiff_t CodeLz(CDecoder *dec, size_t next, ptrdiff_t _bitOffset, const Byte *_buf) throw()
{
  {
    Byte *const win = dec->_win;
    const UInt32 winSize = dec->_winSize;
    Byte *pos = win + dec->_pos;
    const Byte * const posEnd = pos + next;
    NHuffman::CValueInt _value;

    NORMALIZE

#if 1
  #define HUFF_DEC_PREFIX  dec->
#else
    const NHuffman::CDecoder<kNumHuffmanBits, kMainTableSize, kNumTableBits_Main> _mainDecoder = dec->_mainDecoder;
    const NHuffman::CDecoder256<kNumHuffmanBits, kNumLenSymbols, kNumTableBits_Len> _lenDecoder = dec->_lenDecoder;
    const NHuffman::CDecoder7b<kAlignTableSize> _alignDecoder = dec->_alignDecoder;
  #define HUFF_DEC_PREFIX
#endif

    do
    {
      unsigned sym;
      // printf("\npos = %6u", pos - win);
      {
        const NHuffman::CDecoder<kNumHuffmanBits, kMainTableSize, kNumTableBits_Main>
            *mainDecoder = & HUFF_DEC_PREFIX _mainDecoder;
        Z7_LZX_HUFF_DECODE_CHECK_NO(sym, mainDecoder, kNumTableBits_Main, MOVE_POS_CHECK_STAT)
      }
      // if (!_mainDecoder.Decode_SymCheck_MovePosCheck(&bitStream, sym)) return DECODE_ERROR_CODE;
      // sym = _mainDecoder.Decode(&bitStream);
      // if (bitStream.WasExtraReadError_Fast()) return DECODE_ERROR_CODE;

      // printf(" sym = %3x", sym);
      UPDATE_STAT(g_stats_main[sym]++;)
      
      if (sym < 256)
      {
        UPDATE_STAT(g_stats_NumLits++;)
        *pos++ = (Byte)sym;
      }
      else
      {
        // sym -= 256;
        // if (sym >= _numPosLenSlots) return DECODE_ERROR_CODE;
        const unsigned posSlot = sym / kNumLenSlots;
        unsigned len = sym % kNumLenSlots + kMatchMinLen;
        if (len == kNumLenSlots - 1 + kMatchMinLen)
        {
          const NHuffman::CDecoder256<kNumHuffmanBits, kNumLenSymbols, kNumTableBits_Len>
              *lenDecoder = & HUFF_DEC_PREFIX _lenDecoder;
          Z7_LZX_HUFF_DECODE_CHECK_YES(len, lenDecoder, kNumTableBits_Len, MOVE_POS_STAT)
          // if (!_lenDecoder.Decode2(&bitStream, len)) return DECODE_ERROR_CODE;
          // len = _lenDecoder.Decode(&bitStream);
          // if (len >= kNumLenSymbols) return DECODE_ERROR_CODE;
          UPDATE_STAT(g_stats_len[len - kNumLenSymols_Big_Start]++;)
          len += kNumLenSlots - 1 + kMatchMinLen - kNumLenSymols_Big_Start;
        }
        /*
        if ((next -= len) < 0)
          return DECODE_ERROR_CODE;
        */
        UInt32 dist;
        
        dist = dec->_reps[(size_t)posSlot - kPosSlotDelta];
        if (posSlot < kNumReps + 256 / kNumLenSlots)
        {
          // if (posSlot != kNumReps + kPosSlotDelta)
          // if (posSlot - (kNumReps + kPosSlotDelta + 1) < 2)
          dec->_reps[(size_t)posSlot - kPosSlotDelta] = dec->_reps[kPosSlotOffset];
          /*
          if (posSlot != kPosSlotDelta)
          {
            UInt32 temp = dist;
            if (posSlot == kPosSlotDelta + 1)
            {
              dist = reps[1];
              reps[1] = temp;
            }
            else
            {
              dist = reps[2];
              reps[2] = temp;
            }
            // dist = reps[(size_t)(posSlot) - kPosSlotDelta];
            // reps[(size_t)(posSlot) - kPosSlotDelta] = reps[0];
            // reps[(size_t)(posSlot) - kPosSlotDelta] = temp;
          }
          */
        }
        else // if (posSlot != kNumReps + kPosSlotDelta)
        {
          unsigned numDirectBits;
#if 0
          if (posSlot < kNumPowerPosSlots + kPosSlotDelta)
          {
            numDirectBits = (posSlot - 2 - kPosSlotDelta) >> 1;
            dist = (UInt32)(2 | (posSlot & 1)) << numDirectBits;
          }
          else
          {
            numDirectBits = kNumLinearPosSlotBits;
            dist = (UInt32)(posSlot - 0x22 - kPosSlotDelta) << kNumLinearPosSlotBits;
          }
          dist -= kNumReps - 1;
#else
          numDirectBits = dec->_extra[(size_t)posSlot - kPosSlotDelta];
          // dist = reps[(size_t)(posSlot) - kPosSlotDelta];
#endif
          dec->_reps[kPosSlotOffset + 2] =
          dec->_reps[kPosSlotOffset + 1];
          dec->_reps[kPosSlotOffset + 1] =
          dec->_reps[kPosSlotOffset + 0];

          // dist += val; dist += bitStream.ReadBitsBig(numDirectBits);
          // if (posSlot >= _numAlignBits_PosSlots)
          // if (numDirectBits >= _numAlignBits)
          // if (val >= _numAlignBits_Dist)
          // UInt32 val; MACRO_ReadBitsBig(val , numDirectBits)
          // dist += val;
          // dist += (UInt32)((UInt32)_value >> 1 >> (/* 31 ^ */ (numDirectBits)));
          // MOVE_POS((numDirectBits ^ 31))
          MACRO_ReadBitsBig_pre(numDirectBits)
          // dist += (UInt32)_value;
          if (dist >= dec->_numAlignBits_Dist)
          {
            // if (numDirectBits != _numAlignBits)
            {
              // UInt32 val;
              // dist -= (UInt32)_value;
              MACRO_ReadBitsBig_add3(dist)
              NORMALIZE
              // dist += (val << kNumAlignBits);
              // dist += bitStream.ReadBitsSmall(numDirectBits - kNumAlignBits) << kNumAlignBits;
            }
            {
              // const unsigned alignTemp = _alignDecoder.Decode(&bitStream);
              const NHuffman::CDecoder7b<kAlignTableSize> *alignDecoder = & HUFF_DEC_PREFIX _alignDecoder;
              unsigned alignTemp;
              UPDATE_STAT(g_stats_NumAlign++;)
              Z7_HUFF_DECODER_7B_DECODE(alignTemp, alignDecoder, GET_VAL_BASE, MOVE_POS, bs)
              // NORMALIZE
              // if (alignTemp >= kAlignTableSize) return DECODE_ERROR_CODE;
              dist += alignTemp;
            }
          }
          else
          {
            {
              MACRO_ReadBitsBig_add(dist)
              // dist += bitStream.ReadBitsSmall(numDirectBits - kNumAlignBits) << kNumAlignBits;
            }
          }
          NORMALIZE
          /*
          else
          {
            UInt32 val;
            MACRO_ReadBitsBig(val, numDirectBits)
            dist += val;
            // dist += bitStream.ReadBitsBig(numDirectBits);
          }
          */
        }
        dec->_reps[kPosSlotOffset + 0] = dist;

        Byte *dest = pos;
        if (len > (size_t)(posEnd - pos))
          return DECODE_ERROR_CODE;
        Int32 srcPos = (Int32)(pos - win);
        pos += len;
        srcPos -= (Int32)dist;
        if (srcPos < 0) // fast version
        {
          if (!dec->_overDict)
            return DECODE_ERROR_CODE;
          srcPos &= winSize - 1;
          UInt32 rem = winSize - (UInt32)srcPos;
          if (len > rem)
          {
            len -= rem;
            const Byte *src = win + (UInt32)srcPos;
            do
              *dest++ = *src++;
            while (--rem);
            srcPos = 0;
          }
        }
        CopyLzMatch(dest, win + (UInt32)srcPos, len, dist);
      }
    }
    while (pos != posEnd);
    
    return _bitOffset;
  }
}




// inSize != 0
// outSize != 0 ???
HRESULT CDecoder::CodeSpec(const Byte *inData, size_t inSize, UInt32 outSize) throw()
{
  // ((inSize & 1) != 0) case is possible, if current call will be finished with Uncompressed Block.
  CBitByteDecoder _bitStream;
  if (_keepHistory && _isUncompressedBlock)
    _bitStream.Init_ByteMode(inData, inSize);
  else
    _bitStream.Init_BitMode(inData, inSize);
 
  if (!_keepHistory)
  {
    _isUncompressedBlock = false;
    _skipByte = false;
    _unpackBlockSize = 0;
    memset(_mainLevels, 0, sizeof(_mainLevels));
    memset(_lenLevels, 0, sizeof(_lenLevels));
    {
      _x86_translationSize = 12000000;
      if (!_wimMode)
      {
        _x86_translationSize = 0;
        if (ReadBits(_bitStream, 1) != 0)
        {
          UInt32 v = ReadBits(_bitStream, 16) << 16;
          v       |= ReadBits(_bitStream, 16);
          _x86_translationSize = v;
        }
      }
      _x86_processedSize = 0;
    }
    _reps[0 + kPosSlotOffset] = 1;
    _reps[1 + kPosSlotOffset] = 1;
    _reps[2 + kPosSlotOffset] = 1;
  }

  while (outSize)
  {
    /*
    // check it for bit mode only:
    if (_bitStream.WasExtraReadError_Fast())
      return S_FALSE;
    */
    if (_unpackBlockSize == 0)
    {
      if (_skipByte)
      {
        if (_bitStream.GetRem() < 1)
          return S_FALSE;
        if (_bitStream.DirectReadByte() != 0)
          return S_FALSE;
      }
      if (_isUncompressedBlock)
        _bitStream.Switch_To_BitMode();
      if (!ReadTables(_bitStream))
        return S_FALSE;
      continue;
    }

    // _unpackBlockSize != 0
    UInt32 next = _unpackBlockSize;
    if (next > outSize)
        next = outSize;
    // next != 0

    // PRF(printf("\nnext = %d", (unsigned)next);)
    
    if (_isUncompressedBlock)
    {
      if (_bitStream.GetRem() < next)
        return S_FALSE;
      _bitStream.CopyTo(_win + _pos, next);
      _pos += next;
      _unpackBlockSize -= next;
    }
    else
    {
      _unpackBlockSize -= next;
      _bitStream._bitOffset = CodeLz(this, next, _bitStream._bitOffset, _bitStream._buf);
      if (_bitStream.IsOverRead())
        return S_FALSE;
      _pos += next;
    }
    outSize -= next;
  }

  // outSize == 0

  if (_isUncompressedBlock)
  {
    /* we don't know where skipByte can be placed, if it's end of chunk:
        1) in current chunk - there are such cab archives, if chunk is last
        2) in next chunk - are there such archives ? */
    if (_unpackBlockSize == 0
        && _skipByte
        // && outSize == 0
        && _bitStream.IsOneDirectByteLeft())
    {
      _skipByte = false;
      if (_bitStream.DirectReadByte() != 0)
        return S_FALSE;
    }
  }

  if (_bitStream.GetRem() != 0)
    return S_FALSE;
  if (!_isUncompressedBlock)
    if (!_bitStream.WasBitStreamFinishedOK())
      return S_FALSE;
  return S_OK;
}


#if k_Filter_OutBufSize_Add > k_Lz_OutBufSize_Add
  #define k_OutBufSize_Add  k_Filter_OutBufSize_Add
#else
  #define k_OutBufSize_Add  k_Lz_OutBufSize_Add
#endif

HRESULT CDecoder::Code_WithExceedReadWrite(const Byte *inData, size_t inSize, UInt32 outSize) throw()
{
  if (!_keepHistory)
  {
    _pos = 0;
    _overDict = false;
  }
  else if (_pos == _winSize)
  {
    _pos = 0;
    _overDict = true;
#if k_OutBufSize_Add > 0
    // data after (_winSize) can be used, because we can use overwrite.
    // memset(_win + _winSize, 0, k_OutBufSize_Add);
#endif
  }
  _writePos = _pos;
  _unpackedData = _win + _pos;
 
  if (outSize > _winSize - _pos)
    return S_FALSE;
  
  PRF(printf("\ninSize = %d", (unsigned)inSize);)
  PRF(if ((inSize & 1) != 0) printf("---------");)

  if (inSize == 0)
    return S_FALSE;
  const HRESULT res = CodeSpec(inData, inSize, outSize);
  const HRESULT res2 = Flush();
  return (res == S_OK ? res2 : res);
}


HRESULT CDecoder::SetParams2(unsigned numDictBits) throw()
{
  if (numDictBits < kNumDictBits_Min ||
      numDictBits > kNumDictBits_Max)
    return E_INVALIDARG;
  _numDictBits = (Byte)numDictBits;
  const unsigned numPosSlots2 = (numDictBits < 20) ?
      numDictBits : 17 + (1u << (numDictBits - 18));
  _numPosLenSlots = numPosSlots2 * (kNumLenSlots * 2);
  return S_OK;
}
  

HRESULT CDecoder::Set_DictBits_and_Alloc(unsigned numDictBits) throw()
{
  RINOK(SetParams2(numDictBits))
  const UInt32 newWinSize = (UInt32)1 << numDictBits;
  if (_needAlloc)
  {
    if (!_win || newWinSize != _winSize)
    {
      // BigFree
      z7_AlignedFree
        (_win);
      _winSize = 0;
      const size_t alloc_size = newWinSize + k_OutBufSize_Add;
      _win = (Byte *)
          // BigAlloc
          z7_AlignedAlloc
          (alloc_size);
      if (!_win)
        return E_OUTOFMEMORY;
      // optional:
      memset(_win, 0, alloc_size);
    }
  }
  _winSize = newWinSize;
  return S_OK;
}

}}
