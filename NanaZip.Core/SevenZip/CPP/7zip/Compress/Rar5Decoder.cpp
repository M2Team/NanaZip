// Rar5Decoder.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives

#include "StdAfx.h"

#define DICT_SIZE_MAX ((UInt64)1 << DICT_SIZE_BITS_MAX)

// #include <emmintrin.h> // SSE2
// #endif

#include "../../../C/CpuArch.h"
#if 0
#include "../../../C/Bra.h"
#endif

#if defined(MY_CPU_ARM64)
#include <arm_neon.h>
#endif

// #define Z7_RAR5_SHOW_STAT
// #include <stdio.h>
#ifdef Z7_RAR5_SHOW_STAT
#include <stdio.h>
#endif

#include "../Common/StreamUtils.h"

#include "Rar5Decoder.h"

/*
Note: original-unrar claims that encoder has limitation for Distance:
  (Distance <= MaxWinSize - MAX_INC_LZ_MATCH)
  MAX_INC_LZ_MATCH = 0x1001 + 3;
*/

#define LZ_ERROR_TYPE_NO      0
#define LZ_ERROR_TYPE_HEADER  1
// #define LZ_ERROR_TYPE_SYM     1
#define LZ_ERROR_TYPE_DIST    2

static
void My_ZeroMemory(void *p, size_t size)
{
  #if defined(MY_CPU_AMD64) && !defined(_M_ARM64EC) \
    && defined(Z7_MSC_VER_ORIGINAL) && (Z7_MSC_VER_ORIGINAL <= 1400)
      // __stosq((UInt64 *)(void *)win, 0, size / 8);
      /*
      printf("\n__stosb \n");
      #define STEP_BIG (1 << 28)
      for (size_t i = 0; i < ((UInt64)1 << 50); i += STEP_BIG)
      {
        printf("\n__stosb end %p\n", (void *)i);
        __stosb((Byte *)p + i, 0, STEP_BIG);
      }
      */
      // __stosb((Byte *)p, 0, 0);
      __stosb((Byte *)p, 0, size);
  #else
    // SecureZeroMemory (win, STEP);
    // ZeroMemory(win, STEP);
    // memset(win, 0, STEP);
    memset(p, 0, size);
  #endif
}



#ifdef MY_CPU_LE_UNALIGN
  #define Z7_RAR5_DEC_USE_UNALIGNED_COPY
#endif

#ifdef Z7_RAR5_DEC_USE_UNALIGNED_COPY

  #define COPY_CHUNK_SIZE 16

    #define COPY_CHUNK_4_2(dest, src) \
    { \
      ((UInt32 *)(void *)dest)[0] = ((const UInt32 *)(const void *)src)[0]; \
      ((UInt32 *)(void *)dest)[1] = ((const UInt32 *)(const void *)src)[1]; \
      src  += 4 * 2; \
      dest += 4 * 2; \
    }

  /* sse2 doesn't help here in GCC and CLANG.
     so we disabled sse2 here */
#if 0
  #if defined(MY_CPU_AMD64)
    #define Z7_RAR5_DEC_USE_SSE2
  #elif defined(MY_CPU_X86)
    #if defined(_MSC_VER) && _MSC_VER >= 1300 && defined(_M_IX86_FP) && (_M_IX86_FP >= 2) \
      || defined(__SSE2__) \
      // || 1 == 1  // for debug only
      #define Z7_RAR5_DEC_USE_SSE2
    #endif
  #endif
#endif

  #if defined(MY_CPU_ARM64)

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
      if (dest >= lim) break; \
      COPY_CHUNK1(dest, src) \
    }

  #elif defined(Z7_RAR5_DEC_USE_SSE2)
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
      if (dest >= lim) break; \
      COPY_CHUNK1(dest, src) \
    }

  #elif defined(MY_CPU_64BIT)
    #define COPY_OFFSET_MIN  8

    #define COPY_CHUNK(dest, src) \
    { \
      ((UInt64 *)(void *)dest)[0] = ((const UInt64 *)(const void *)src)[0]; \
      src  += 8 * 1; dest += 8 * 1; \
      ((UInt64 *)(void *)dest)[0] = ((const UInt64 *)(const void *)src)[0]; \
      src  += 8 * 1; dest += 8 * 1; \
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
  while (dest < lim); \
}

namespace NCompress {
namespace NRar5 {

typedef
#if 1
  unsigned
#else
  size_t
#endif
  CLenType;

// (len != 0)
static
Z7_FORCE_INLINE
// Z7_ATTRIB_NO_VECTOR
void CopyMatch(size_t offset, Byte *dest, const Byte *src, const Byte *lim)
{
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
        #if COPY_CHUNK_SIZE < 16
          if (dest >= lim) break;
        #endif
        COPY_CHUNK_4_2(dest, src)
      }
      while (dest < lim);
      // return;
    }
    else
  #endif
    {
      // (offset < 4)
      const unsigned b0 = src[0];
      if (offset < 2)
      {
      #if defined(Z7_RAR5_DEC_USE_UNALIGNED_COPY) && (COPY_CHUNK_SIZE == 16)
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
          while (dest < lim);
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
          while (dest < lim);
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
        while (dest < lim);
      #endif
      }
      else if (offset == 2)
      {
        const Byte b1 = src[1];
        {
          do
          {
            dest[0] = (Byte)b0;
            dest[1] = b1;
            dest += 2;
          }
          while (dest < lim);
        }
      }
      else // (offset == 3)
      {
        const Byte b1 = src[1];
        const Byte b2 = src[2];
        do
        {
          dest[0] = (Byte)b0;
          dest[1] = b1;
          dest[2] = b2;
          dest += 3;
        }
        while (dest < lim);
      }
    }
  }
}

static const size_t kInputBufSize = 1 << 20;
static const UInt32   k_Filter_BlockSize_MAX = 1 << 22;
static const unsigned k_Filter_AfterPad_Size = 64;

#ifdef Z7_RAR5_SHOW_STAT
static const unsigned kNumStats1 = 10;
static const unsigned kNumStats2 = (1 << 12) + 16;
static UInt32 g_stats1[kNumStats1];
static UInt32 g_stats2[kNumStats1][kNumStats2];
#endif

#if 1
MY_ALIGN(32)
// DICT_SIZE_BITS_MAX-1 are required
static const Byte k_LenPlusTable[DICT_SIZE_BITS_MAX] =
  { 0,0,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3 };
#endif



class CBitDecoder
{
public:
  const Byte *_buf;
  const Byte *_bufCheck_Block;  // min(ptr for _blockEnd, _bufCheck)
  unsigned _bitPos;             // = [0 ... 7]
  bool _wasFinished;
  bool _minorError;
  unsigned _blockEndBits7;      // = [0 ... 7] : the number of additional bits in (_blockEnd) poisition.
  HRESULT _hres;
  const Byte *_bufCheck;        // relaxed limit (16 bytes before real end of input data in buffer)
  Byte *_bufLim;                // end if input data
  Byte *_bufBase;
  ISequentialInStream *_stream;

  UInt64 _processedSize;
  UInt64 _blockEnd;     // absolute end of current block
      // but it doesn't include additional _blockEndBits7 [0 ... 7] bits

  Z7_FORCE_INLINE
  void CopyFrom(const CBitDecoder &a)
  {
    _buf = a._buf;
    _bufCheck_Block = a._bufCheck_Block;
    _bitPos = a._bitPos;
    _wasFinished = a._wasFinished;
    _blockEndBits7 = a._blockEndBits7;
    _bufCheck = a._bufCheck;
    _bufLim = a._bufLim;
    _bufBase = a._bufBase;
    
    _processedSize = a._processedSize;
    _blockEnd = a._blockEnd;
  }

  Z7_FORCE_INLINE
  void RestoreFrom2(const CBitDecoder &a)
  {
    _buf = a._buf;
    _bitPos = a._bitPos;
  }

  Z7_FORCE_INLINE
  void SetCheck_forBlock()
  {
    _bufCheck_Block = _bufCheck;
    if (_bufCheck > _buf)
    {
      const UInt64 processed = GetProcessedSize_Round();
      if (_blockEnd < processed)
        _bufCheck_Block = _buf;
      else
      {
        const UInt64 delta = _blockEnd - processed;
        if ((size_t)(_bufCheck - _buf) > delta)
          _bufCheck_Block = _buf + (size_t)delta;
      }
    }
  }

  Z7_FORCE_INLINE
  bool IsBlockOverRead() const
  {
    const UInt64 v = GetProcessedSize_Round();
    if (v < _blockEnd) return false;
    if (v > _blockEnd) return true;
    return _bitPos > _blockEndBits7;
  }

  /*
  CBitDecoder() throw():
      _buf(0),
      _bufLim(0),
      _bufBase(0),
      _stream(0),
      _processedSize(0),
      _wasFinished(false)
      {}
  */

  Z7_FORCE_INLINE
  void Init() throw()
  {
    _blockEnd = 0;
    _blockEndBits7 = 0;

    _bitPos = 0;
    _processedSize = 0;
    _buf = _bufBase;
    _bufLim = _bufBase;
    _bufCheck = _buf;
    _bufCheck_Block = _buf;
    _wasFinished = false;
    _minorError = false;
  }

  void Prepare2() throw();

  Z7_FORCE_INLINE
  void Prepare() throw()
  {
    if (_buf >= _bufCheck)
      Prepare2();
  }

  Z7_FORCE_INLINE
  bool ExtraBitsWereRead() const
  {
    return _buf >= _bufLim && (_buf > _bufLim || _bitPos != 0);
  }

  Z7_FORCE_INLINE bool InputEofError() const { return ExtraBitsWereRead(); }

  Z7_FORCE_INLINE unsigned GetProcessedBits7() const { return _bitPos; }
  Z7_FORCE_INLINE UInt64 GetProcessedSize_Round() const { return _processedSize + (size_t)(_buf - _bufBase); }
  Z7_FORCE_INLINE UInt64 GetProcessedSize() const { return _processedSize + (size_t)(_buf - _bufBase) + ((_bitPos + 7) >> 3); }

  Z7_FORCE_INLINE
  void AlignToByte()
  {
    if (_bitPos != 0)
    {
#if 1
      // optional check of unused bits for strict checking:
      // original-unrar doesn't check it:
      const unsigned b = (unsigned)*_buf << _bitPos;
      if (b & 0xff)
        _minorError = true;
#endif
      _buf++;
      _bitPos = 0;
    }
    // _buf += (_bitPos + 7) >> 3;
    // _bitPos = 0;
  }

  Z7_FORCE_INLINE
  Byte ReadByte_InAligned()
  {
    return *_buf++;
  }

  Z7_FORCE_INLINE
  UInt32 GetValue(unsigned numBits) const
  {
    // 0 < numBits <= 17 : supported values
#if defined(Z7_CPU_FAST_BSWAP_SUPPORTED) && defined(MY_CPU_LE_UNALIGN)
    UInt32 v = GetBe32(_buf);
#if 1
    return (v >> (32 - numBits - _bitPos)) & ((1u << numBits) - 1);
#else
    return (v << _bitPos) >> (32 - numBits);
#endif
#else
    UInt32 v = ((UInt32)_buf[0] << 16) | ((UInt32)_buf[1] << 8) | (UInt32)_buf[2];
    v >>= 24 - numBits - _bitPos;
    return v & ((1 << numBits) - 1);
#endif
  }

  Z7_FORCE_INLINE
  UInt32 GetValue_InHigh32bits() const
  {
    // 0 < numBits <= 17 : supported vales
#if defined(Z7_CPU_FAST_BSWAP_SUPPORTED) && defined(MY_CPU_LE_UNALIGN)
    return GetBe32(_buf) << _bitPos;
#else
    const UInt32 v = ((UInt32)_buf[0] << 16) | ((UInt32)_buf[1] << 8) | (UInt32)_buf[2];
    return v << (_bitPos + 8);
#endif
  }
  

  Z7_FORCE_INLINE
  void MovePos(unsigned numBits)
  {
    numBits += _bitPos;
    _buf += numBits >> 3;
    _bitPos = numBits & 7;
  }
    

  Z7_FORCE_INLINE
  UInt32 ReadBits9(unsigned numBits)
  {
    const Byte *buf = _buf;
    UInt32 v = ((UInt32)buf[0] << 8) | (UInt32)buf[1];
    v &= (UInt32)0xFFFF >> _bitPos;
    numBits += _bitPos;
    v >>= 16 - numBits;
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return v;
  }

  Z7_FORCE_INLINE
  UInt32 ReadBits_9fix(unsigned numBits)
  {
    const Byte *buf = _buf;
    UInt32 v = ((UInt32)buf[0] << 8) | (UInt32)buf[1];
    const UInt32 mask = (1u << numBits) - 1;
    numBits += _bitPos;
    v >>= 16 - numBits;
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return v & mask;
  }

#if 1 && defined(MY_CPU_SIZEOF_POINTER) && (MY_CPU_SIZEOF_POINTER == 8)
#define Z7_RAR5_USE_64BIT
#endif

#ifdef Z7_RAR5_USE_64BIT
#define MAX_DICT_LOG (sizeof(size_t) / 8 * 5 + 31)
#else
#define MAX_DICT_LOG 31
#endif

#ifdef Z7_RAR5_USE_64BIT

  Z7_FORCE_INLINE
  size_t ReadBits_Big(unsigned numBits, UInt64 v)
  {
    const UInt64 mask = ((UInt64)1 << numBits) - 1;
    numBits += _bitPos;
    const Byte *buf = _buf;
    // UInt64 v = GetBe64(buf);
    v >>= 64 - numBits;
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return (size_t)(v & mask);
  }
  #define ReadBits_Big25 ReadBits_Big

#else

  // (numBits <= 25) for 32-bit mode
  Z7_FORCE_INLINE
  size_t ReadBits_Big25(unsigned numBits, UInt32 v)
  {
    const UInt32 mask = ((UInt32)1 << numBits) - 1;
    numBits += _bitPos;
    v >>= 32 - numBits;
    _buf += numBits >> 3;
    _bitPos = numBits & 7;
    return v & mask;
  }

  // numBits != 0
  Z7_FORCE_INLINE
  size_t ReadBits_Big(unsigned numBits, UInt32 v)
  {
    const Byte *buf = _buf;
    // UInt32 v = GetBe32(buf);
#if 0
    const UInt32 mask = ((UInt32)1 << numBits) - 1;
    numBits += _bitPos;
    if (numBits > 32)
    {
      v <<= numBits - 32;
      v |= (UInt32)buf[4] >> (40 - numBits);
    }
    else
      v >>= 32 - numBits;
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return v & mask;
#else
    v <<= _bitPos;
    v |= (UInt32)buf[4] >> (8 - _bitPos);
    v >>= 32 - numBits;
    numBits += _bitPos;
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return v;
#endif
  }
#endif
};


static const unsigned kLookaheadSize = 16;
static const unsigned kInputBufferPadZone = kLookaheadSize;

Z7_NO_INLINE
void CBitDecoder::Prepare2() throw()
{
  if (_buf > _bufLim)
    return;

  size_t rem = (size_t)(_bufLim - _buf);
  if (rem != 0)
    memmove(_bufBase, _buf, rem);

  _bufLim = _bufBase + rem;
  _processedSize += (size_t)(_buf - _bufBase);
  _buf = _bufBase;

  // we do not look ahead more than 16 bytes before limit checks.

  if (!_wasFinished)
  {
    while (rem <= kLookaheadSize)
    {
      UInt32 processed = (UInt32)(kInputBufSize - rem);
      // processed = 33; // for debug
      _hres = _stream->Read(_bufLim, processed, &processed);
      _bufLim += processed;
      rem += processed;
      if (processed == 0 || _hres != S_OK)
      {
        _wasFinished = true;
        // if (_hres != S_OK) throw CInBufferException(result);
        break;
      }
    }
  }

  // we always fill pad zone here.
  // so we don't need to call Prepare2() if (_wasFinished == true)
  memset(_bufLim, 0xFF, kLookaheadSize);

  if (rem < kLookaheadSize)
  {
    _bufCheck = _buf;
    // memset(_bufLim, 0xFF, kLookaheadSize - rem);
  }
  else
    _bufCheck = _bufLim - kLookaheadSize;

  SetCheck_forBlock();
}


enum FilterType
{
  FILTER_DELTA = 0,
  FILTER_E8,
  FILTER_E8E9,
  FILTER_ARM
};

static const size_t kWriteStep = (size_t)1 << 18;
      // (size_t)1 << 22; // original-unrar

// Original unRAR claims that maximum possible filter block size is (1 << 16) now,
// and (1 << 17) is minimum win size required to support filter.
// Original unRAR uses (1u << 18) for "extra safety and possible filter area size expansion"
// We can use any win size, but we use same (1u << 18) for compatibility
// with WinRar

// static const unsigned kWinSize_Log_Min = 17;
static const size_t kWinSize_Min = 1u << 18;

CDecoder::CDecoder():
    _isSolid(false),
    _is_v7(false),
    _wasInit(false),
    // _dictSizeLog(0),
    _dictSize(kWinSize_Min),
    _window(NULL),
    _winPos(0),
    _winSize(0),
    _dictSize_forCheck(0),
    _lzSize(0),
    _lzEnd(0),
    _writtenFileSize(0),
    _filters(NULL),
    _winSize_Allocated(0),
    _inputBuf(NULL)
{
#if 1
  memcpy(m_LenPlusTable, k_LenPlusTable, sizeof(k_LenPlusTable));
#endif
  // printf("\nsizeof(CDecoder) == %d\n", sizeof(CDecoder));
}

CDecoder::~CDecoder()
{
#ifdef Z7_RAR5_SHOW_STAT
  printf("\n%4d :", 0);
  for (unsigned k = 0; k < kNumStats1; k++)
    printf(" %8u", (unsigned)g_stats1[k]);
  printf("\n");
  for (unsigned i = 0; i < kNumStats2; i++)
  {
    printf("\n%4d :", i);
    for (unsigned k = 0; k < kNumStats1; k++)
      printf(" %8u", (unsigned)g_stats2[k][i]);
  }
  printf("\n");
#endif

#define Z7_RAR_FREE_WINDOW ::BigFree(_window);
  
  Z7_RAR_FREE_WINDOW
  z7_AlignedFree(_inputBuf);
  z7_AlignedFree(_filters);
}

Z7_NO_INLINE
void CDecoder::DeleteUnusedFilters()
{
  if (_numUnusedFilters != 0)
  {
    // printf("\nDeleteUnusedFilters _numFilters = %6u\n", _numFilters);
    const unsigned n = _numFilters - _numUnusedFilters;
    _numFilters = n;
    memmove(_filters, _filters + _numUnusedFilters, n * sizeof(CFilter));
    _numUnusedFilters = 0;
  }
}


Z7_NO_INLINE
HRESULT CDecoder::WriteData(const Byte *data, size_t size)
{
  HRESULT res = S_OK;
  if (!_unpackSize_Defined || _writtenFileSize < _unpackSize)
  {
    size_t cur = size;
    if (_unpackSize_Defined)
    {
      const UInt64 rem = _unpackSize - _writtenFileSize;
      if (cur > rem)
        cur = (size_t)rem;
    }
    res = WriteStream(_outStream, data, cur);
    if (res != S_OK)
      _writeError = true;
  }
  _writtenFileSize += size;
  return res;
}


#if defined(MY_CPU_SIZEOF_POINTER) \
    && ( MY_CPU_SIZEOF_POINTER == 4 \
      || MY_CPU_SIZEOF_POINTER == 8)
  #define BR_CONV_USE_OPT_PC_PTR
#endif

#ifdef BR_CONV_USE_OPT_PC_PTR
#define BR_PC_INIT(lim_back)  pc -= (UInt32)(SizeT)data;
#define BR_PC_GET        (pc + (UInt32)(SizeT)data)
#else
#define BR_PC_INIT(lim_back)  pc += (UInt32)dataSize - (lim_back);
#define BR_PC_GET        (pc - (UInt32)(SizeT)(data_lim - data))
#endif

#ifdef MY_CPU_LE_UNALIGN
#define Z7_RAR5_FILTER_USE_LE_UNALIGN
#endif

#ifdef Z7_RAR5_FILTER_USE_LE_UNALIGN
#define RAR_E8_FILT(mask) \
{ \
  for (;;) \
  { UInt32 v; \
    do { \
      v = GetUi32(data) ^ (UInt32)0xe8e8e8e8; \
      data += 4; \
      if ((v & ((UInt32)(mask) << (8 * 0))) == 0) { data -= 3; break; } \
      if ((v & ((UInt32)(mask) << (8 * 1))) == 0) { data -= 2; break; } \
      if ((v & ((UInt32)(mask) << (8 * 2))) == 0) { data -= 1; break; } } \
    while((v & ((UInt32)(mask) << (8 * 3)))); \
    if (data > data_lim) break; \
    const UInt32 offset = BR_PC_GET & (kFileSize - 1); \
    const UInt32 addr = GetUi32(data); \
    data += 4; \
    if (addr < kFileSize) \
      SetUi32(data - 4, addr - offset) \
    else if (addr > ~offset) /* if (addr > ((UInt32)0xFFFFFFFF - offset)) */ \
      SetUi32(data - 4, addr + kFileSize) \
  } \
}
#else
#define RAR_E8_FILT(get_byte) \
{ \
  for (;;) \
  { \
    if ((get_byte) != 0xe8) \
    if ((get_byte) != 0xe8) \
    if ((get_byte) != 0xe8) \
    if ((get_byte) != 0xe8) \
      continue; \
    { if (data > data_lim) break; \
    const UInt32 offset = BR_PC_GET & (kFileSize - 1); \
    const UInt32 addr = GetUi32(data); \
    data += 4; \
    if (addr < kFileSize) \
      SetUi32(data - 4, addr - offset) \
      else if (addr > ~offset) /* if (addr > ((UInt32)0xFFFFFFFF - offset)) */ \
      SetUi32(data - 4, addr + kFileSize) \
    } \
  } \
}
#endif

HRESULT CDecoder::ExecuteFilter(const CFilter &f)
{
  Byte *data = _filterSrc;
  UInt32 dataSize = f.Size;
  // printf("\nType = %d offset = %9d  size = %5d", f.Type, (unsigned)(f.Start - _lzFileStart), dataSize);

  if (f.Type == FILTER_DELTA)
  {
    // static unsigned g1 = 0, g2 = 0; g1 += dataSize;
    // if (g2++ % 100 == 0) printf("DELTA  num %8u, size %8u MiB, channels = %2u curSize=%8u\n", g2, (g1 >> 20), f.Channels, dataSize);
    _filterDst.AllocAtLeast_max((size_t)dataSize, k_Filter_BlockSize_MAX);
    if (!_filterDst.IsAllocated())
      return E_OUTOFMEMORY;
    
    Byte *dest = _filterDst;
    const unsigned numChannels = f.Channels;
    unsigned curChannel = 0;
    do
    {
      Byte prevByte = 0;
      Byte *dest2 = dest + curChannel;
      const Byte *dest_lim = dest + dataSize;
      for (; dest2 < dest_lim; dest2 += numChannels)
        *dest2 = (prevByte = (Byte)(prevByte - *data++));
    }
    while (++curChannel != numChannels);
    // return WriteData(dest, dataSize);
    data = dest;
  }
  else if (f.Type < FILTER_ARM)
  {
    // FILTER_E8 or FILTER_E8E9
    if (dataSize > 4)
    {
      UInt32 pc = (UInt32)(f.Start - _lzFileStart);
      const UInt32 kFileSize = (UInt32)1 << 24;
      const Byte *data_lim = data + dataSize - 4;
      BR_PC_INIT(4) // because (data_lim) was moved back for 4 bytes
      data[dataSize] = 0xe8;
      if (f.Type == FILTER_E8)
      {
        // static unsigned g1 = 0; g1 += dataSize; printf("\n  FILTER_E8   %u", (g1 >> 20));
#ifdef Z7_RAR5_FILTER_USE_LE_UNALIGN
        RAR_E8_FILT (0xff)
#else
        RAR_E8_FILT (*data++)
#endif
      }
      else
      {
        // static unsigned g1 = 0; g1 += dataSize; printf("\n  FILTER_E8_E9 %u", (g1 >> 20));
#ifdef Z7_RAR5_FILTER_USE_LE_UNALIGN
        RAR_E8_FILT (0xfe)
#else
        RAR_E8_FILT (*data++ & 0xfe)
#endif
      }
    }
    data = _filterSrc;
  }
  else if (f.Type == FILTER_ARM)
  {
    UInt32 pc = (UInt32)(f.Start - _lzFileStart);
#if 0
    // z7_BranchConv_ARM_Dec expects that (fileOffset & 3) == 0;
    // but even if (fileOffset & 3) then current code
    // in z7_BranchConv_ARM_Dec works same way as unrar's code still.
    z7_BranchConv_ARM_Dec(data, dataSize, pc - 8);
#else
    dataSize &= ~(UInt32)3;
    if (dataSize)
    {
      Byte *data_lim = data + dataSize;
      data_lim[3] = 0xeb;
      BR_PC_INIT(0)
      pc -= 4;  // because (data) will point to next instruction
      for (;;) // do
      {
        data += 4;
        if (data[-1] != 0xeb)
          continue;
        if (data > data_lim)
          break;
        {
          UInt32 v = GetUi32a(data - 4) - (BR_PC_GET >> 2);
          v &= 0x00ffffff;
          v |= 0xeb000000;
          SetUi32a(data - 4, v)
        }
      }
    }
#endif
    data = _filterSrc;
  }
  else
  {
    _unsupportedFilter = true;
    My_ZeroMemory(data, dataSize);
    // return S_OK;  // unrar
  }
  // return WriteData(_filterSrc, (size_t)f.Size);
  return WriteData(data, (size_t)f.Size);
}


HRESULT CDecoder::WriteBuf()
{
  DeleteUnusedFilters();

  const UInt64 lzSize = _lzSize + _winPos;

  for (unsigned i = 0; i < _numFilters;)
  {
    const CFilter &f = _filters[i];
    const UInt64 blockStart = f.Start;
    const size_t lzAvail = (size_t)(lzSize - _lzWritten);
    if (lzAvail == 0)
      break;
    
    if (blockStart > _lzWritten)
    {
      const UInt64 rem = blockStart - _lzWritten;
      size_t size = lzAvail;
      if (size > rem)
        size = (size_t)rem;
      if (size != 0) // is it true always ?
      {
        RINOK(WriteData(_window + _winPos - lzAvail, size))
        _lzWritten += size;
      }
      continue;
    }
    
    const UInt32 blockSize = f.Size;
    size_t offset = (size_t)(_lzWritten - blockStart);
    if (offset == 0)
    {
      _filterSrc.AllocAtLeast_max(
          (size_t)blockSize      + k_Filter_AfterPad_Size,
          k_Filter_BlockSize_MAX + k_Filter_AfterPad_Size);
      if (!_filterSrc.IsAllocated())
        return E_OUTOFMEMORY;
    }
    
    const size_t blockRem = (size_t)blockSize - offset;
    size_t size = lzAvail;
    if (size > blockRem)
        size = blockRem;
    memcpy(_filterSrc + offset, _window + _winPos - lzAvail, size);
    _lzWritten += size;
    offset += size;
    if (offset != blockSize)
      return S_OK;

    _numUnusedFilters = ++i;
    RINOK(ExecuteFilter(f))
  }
      
  DeleteUnusedFilters();

  if (_numFilters)
    return S_OK;
  
  const size_t lzAvail = (size_t)(lzSize - _lzWritten);
  RINOK(WriteData(_window + _winPos - lzAvail, lzAvail))
  _lzWritten += lzAvail;
  return S_OK;
}


Z7_NO_INLINE
static UInt32 ReadUInt32(CBitDecoder &bi)
{
  const unsigned numBits = (unsigned)bi.ReadBits_9fix(2) * 8 + 8;
  UInt32 v = 0;
  unsigned i = 0;
  do
  {
    v += (UInt32)bi.ReadBits_9fix(8) << i;
    i += 8;
  }
  while (i != numBits);
  return v;
}


static const unsigned MAX_UNPACK_FILTERS = 8192;

HRESULT CDecoder::AddFilter(CBitDecoder &_bitStream)
{
  DeleteUnusedFilters();

  if (_numFilters >= MAX_UNPACK_FILTERS)
  {
    RINOK(WriteBuf())
    DeleteUnusedFilters();
    if (_numFilters >= MAX_UNPACK_FILTERS)
    {
      _unsupportedFilter = true;
      InitFilters();
    }
  }

  _bitStream.Prepare();

  CFilter f;
  const UInt32 blockStart = ReadUInt32(_bitStream);
  f.Size = ReadUInt32(_bitStream);

  if (f.Size > k_Filter_BlockSize_MAX)
  {
    _unsupportedFilter = true;
    f.Size = 0;  // unrar 5.5.5
  }

  f.Type = (Byte)_bitStream.ReadBits_9fix(3);
  f.Channels = 0;
  if (f.Type == FILTER_DELTA)
    f.Channels = (Byte)(_bitStream.ReadBits_9fix(5) + 1);
  f.Start = _lzSize + _winPos + blockStart;

#if 0
  static unsigned z_cnt = 0; if (z_cnt++ % 100 == 0)
    printf ("\nFilter %7u : %4u : %8p, st=%8x, size=%8x, type=%u ch=%2u",
      z_cnt, (unsigned)_filters.Size(), (void *)(size_t)(_lzSize + _winPos),
      (unsigned)blockStart, (unsigned)f.Size, (unsigned)f.Type, (unsigned)f.Channels);
#endif

  if (f.Start < _filterEnd)
    _unsupportedFilter = true;
  else
  {
    _filterEnd = f.Start + f.Size;
    if (f.Size != 0)
    {
      if (!_filters)
      {
        _filters = (CFilter *)z7_AlignedAlloc(MAX_UNPACK_FILTERS * sizeof(CFilter));
        if (!_filters)
          return E_OUTOFMEMORY;
      }
      // printf("\n_numFilters = %6u\n", _numFilters);
      const unsigned i = _numFilters++;
      _filters[i] = f;
    }
  }

  return S_OK;
}


#define RIF(x) { if (!(x)) return S_FALSE; }

#if 1
#define PRINT_CNT(name, skip)
#else
#define PRINT_CNT(name, skip) \
  { static unsigned g_cnt = 0; if (g_cnt++ % skip == 0) printf("\n%16s:  %8u", name, g_cnt); }
#endif

HRESULT CDecoder::ReadTables(CBitDecoder &_bitStream)
{
  if (_progress)
  {
    const UInt64 packSize = _bitStream.GetProcessedSize();
    if (packSize - _progress_Pack >= (1u << 24)
        || _writtenFileSize - _progress_Unpack >= (1u << 26))
    {
      _progress_Pack = packSize;
      _progress_Unpack = _writtenFileSize;
      RINOK(_progress->SetRatioInfo(&_progress_Pack, &_writtenFileSize))
    }
    // printf("\ntable read pos=%p packSize=%p _writtenFileSize = %p\n", (size_t)_winPos, (size_t)packSize, (size_t)_writtenFileSize);
  }

  // _bitStream is aligned already
  _bitStream.Prepare();
  {
    const unsigned flags = _bitStream.ReadByte_InAligned();
    /* ((flags & 20) == 0) in all rar archives now,
       but (flags & 20) flag can be used as some decoding hint in future versions of original rar.
       So we ignore that bit here. */
    unsigned checkSum = _bitStream.ReadByte_InAligned();
    checkSum ^= flags;
    const unsigned num = (flags >> 3) & 3;
    if (num >= 3)
      return S_FALSE;
    UInt32 blockSize = _bitStream.ReadByte_InAligned();
    checkSum ^= blockSize;
    if (num != 0)
    {
      {
        const unsigned b = _bitStream.ReadByte_InAligned();
        checkSum ^= b;
        blockSize += (UInt32)b << 8;
      }
      if (num > 1)
      {
        const unsigned b = _bitStream.ReadByte_InAligned();
        checkSum ^= b;
        blockSize += (UInt32)b << 16;
      }
    }
    if (checkSum != 0x5A)
      return S_FALSE;
    unsigned blockSizeBits7 = (flags & 7) + 1;
    blockSize += (UInt32)(blockSizeBits7 >> 3);
    if (blockSize == 0)
    {
      // it's error in data stream
      // but original-unrar ignores that error
      _bitStream._minorError = true;
#if 1
      // we ignore that error as original-unrar:
      blockSizeBits7 = 0;
      blockSize = 1;
#else
      // we can stop decoding:
      return S_FALSE;
#endif
    }
    blockSize--;
    blockSizeBits7 &= 7;
    PRINT_CNT("Blocks", 100)
    /*
    {
      static unsigned g_prev = 0;
      static unsigned g_cnt = 0;
      unsigned proc = unsigned(_winPos);
      if (g_cnt++ % 100 == 0) printf("  c_size = %8u  ", blockSize);
      if (g_cnt++ % 100 == 1) printf("  unp_size = %8u", proc - g_prev);
      g_prev = proc;
    }
    */
    _bitStream._blockEndBits7 = blockSizeBits7;
    _bitStream._blockEnd = _bitStream.GetProcessedSize_Round() + blockSize;
    _bitStream.SetCheck_forBlock();
    _isLastBlock = ((flags & 0x40) != 0);
    if ((flags & 0x80) == 0)
    {
      if (!_tableWasFilled)
        // if (blockSize != 0 || blockSizeBits7 != 0)
        if (blockSize + blockSizeBits7 != 0)
          return S_FALSE;
      return S_OK;
    }
    _tableWasFilled = false;
  }

  PRINT_CNT("Tables", 100);

  const unsigned kLevelTableSize = 20;
  const unsigned k_NumHufTableBits_Level = 6;
  NHuffman::CDecoder256<kNumHufBits, kLevelTableSize, k_NumHufTableBits_Level> m_LevelDecoder;
  const unsigned kTablesSizesSum_MAX = kMainTableSize + kDistTableSize_MAX + kAlignTableSize + kLenTableSize;
  Byte lens[kTablesSizesSum_MAX];
  {
    // (kLevelTableSize + 16 < kTablesSizesSum). So we use lens[] array for (Level) table
    // Byte lens2[kLevelTableSize + 16];
    unsigned i = 0;
    do
    {
      if (_bitStream._buf >= _bitStream._bufCheck_Block)
      {
        _bitStream.Prepare();
        if (_bitStream.IsBlockOverRead())
          return S_FALSE;
      }
      const unsigned len = (unsigned)_bitStream.ReadBits_9fix(4);
      if (len == 15)
      {
        unsigned num = (unsigned)_bitStream.ReadBits_9fix(4);
        if (num != 0)
        {
          num += 2;
          num += i;
          // we are allowed to overwrite to lens[] for extra 16 bytes after kLevelTableSize
#if 0
          if (num > kLevelTableSize)
          {
            // we ignore this error as original-unrar
            num = kLevelTableSize;
            // return S_FALSE;
          }
#endif
          do
            lens[i++] = 0;
          while (i < num);
          continue;
        }
      }
      lens[i++] = (Byte)len;
    }
    while (i < kLevelTableSize);
    if (_bitStream.IsBlockOverRead())
      return S_FALSE;
    RIF(m_LevelDecoder.Build(lens, NHuffman::k_BuildMode_Full))
  }

  unsigned i = 0;
  const unsigned tableSize = _is_v7 ?
      kTablesSizesSum_MAX :
      kTablesSizesSum_MAX - kExtraDistSymbols_v7;
  do
  {
    if (_bitStream._buf >= _bitStream._bufCheck_Block)
    {
      // if (_bitStream._buf >= _bitStream._bufCheck)
      _bitStream.Prepare();
      if (_bitStream.IsBlockOverRead())
        return S_FALSE;
    }
    const unsigned sym = m_LevelDecoder.DecodeFull(&_bitStream);
    if (sym < 16)
      lens[i++] = (Byte)sym;
#if 0
    else if (sym > kLevelTableSize)
      return S_FALSE;
#endif
    else
    {
      unsigned num = ((sym /* - 16 */) & 1) * 4;
      num += num + 3 + (unsigned)_bitStream.ReadBits9(num + 3);
      num += i;
      if (num > tableSize)
      {
        // we ignore this error as original-unrar
        num = tableSize;
        // return S_FALSE;
      }
      unsigned v = 0;
      if (sym < 16 + 2)
      {
        if (i == 0)
          return S_FALSE;
        v = lens[(size_t)i - 1];
      }
      do
        lens[i++] = (Byte)v;
      while (i < num);
    }
  }
  while (i < tableSize);

  if (_bitStream.IsBlockOverRead())
    return S_FALSE;
  if (_bitStream.InputEofError())
    return S_FALSE;

  /* We suppose that original-rar encoder can create only two cases for Huffman:
      1) Empty Huffman tree (if num_used_symbols == 0)
      2) Full  Huffman tree (if num_used_symbols != 0)
     Usually the block contains at least one symbol for m_MainDecoder.
     So original-rar-encoder creates full Huffman tree for m_MainDecoder.
     But we suppose that (num_used_symbols == 0) is possible for m_MainDecoder,
     because file must be finished with (_isLastBlock) flag,
     even if there are no symbols in m_MainDecoder.
     So we use k_BuildMode_Full_or_Empty for m_MainDecoder.
  */
  const NHuffman::enum_BuildMode buildMode = NHuffman::
      k_BuildMode_Full_or_Empty; // strict check
      // k_BuildMode_Partial;    // non-strict check (ignore errors)

  RIF(m_MainDecoder.Build(&lens[0], buildMode))
  if (!_is_v7)
  {
#if 1
    /* we use this manual loop to avoid compiler BUG.
       GCC 4.9.2 compiler has BUG with overlapping memmove() to right in local array. */
    Byte *dest = lens + kMainTableSize + kDistTableSize_v6 +
                   kAlignTableSize + kLenTableSize - 1;
    unsigned num = kAlignTableSize + kLenTableSize;
    do
    {
      dest[kExtraDistSymbols_v7] = dest[0];
      dest--;
    }
    while (--num);
#else
    memmove(lens + kMainTableSize + kDistTableSize_v6 + kExtraDistSymbols_v7,
            lens + kMainTableSize + kDistTableSize_v6,
            kAlignTableSize + kLenTableSize);
#endif
    memset(lens + kMainTableSize + kDistTableSize_v6, 0, kExtraDistSymbols_v7);
  }

  RIF(m_DistDecoder.Build(&lens[kMainTableSize], buildMode))
  RIF( m_LenDecoder.Build(&lens[kMainTableSize
        + kDistTableSize_MAX + kAlignTableSize], buildMode))

  _useAlignBits = false;
  for (i = 0; i < kAlignTableSize; i++)
    if (lens[kMainTableSize + kDistTableSize_MAX + (size_t)i] != kNumAlignBits)
    {
      RIF(m_AlignDecoder.Build(&lens[kMainTableSize + kDistTableSize_MAX], buildMode))
      _useAlignBits = true;
      break;
    }

  _tableWasFilled = true;
  return S_OK;
}

static inline CLenType SlotToLen(CBitDecoder &_bitStream, CLenType slot)
{
  const unsigned numBits = ((unsigned)slot >> 2) - 1;
  return ((4 | (slot & 3)) << numBits) + (CLenType)_bitStream.ReadBits9(numBits);
}


static const unsigned kSymbolRep = 258;
static const unsigned kMaxMatchLen = 0x1001 + 3;

enum enum_exit_type
{
  Z7_RAR_EXIT_TYPE_NONE,
  Z7_RAR_EXIT_TYPE_ADD_FILTER
};


#define LZ_RESTORE \
{ \
  _reps[0] = rep0; \
  _winPos = (size_t)(winPos - _window); \
  _buf_Res = _bitStream._buf; \
  _bitPos_Res = _bitStream._bitPos; \
}

#define LZ_LOOP_BREAK_OK { break; }
// #define LZ_LOOP_BREAK_ERROR { _lzError = LZ_ERROR_TYPE_SYM; break; }
// #define LZ_LOOP_BREAK_ERROR { LZ_RESTORE; return S_FALSE; }
#define LZ_LOOP_BREAK_ERROR { goto decode_error; }
// goto decode_error; }
// #define LZ_LOOP_BREAK_ERROR { break; }

#define Z7_RAR_HUFF_DECODE_CHECK_break(sym, huf, kNumTableBits, bitStream) \
  Z7_HUFF_DECODE_CHECK(sym, huf, kNumHufBits, kNumTableBits, bitStream, { LZ_LOOP_BREAK_ERROR })


HRESULT CDecoder::DecodeLZ2(const CBitDecoder &bitStream) throw()
{
#if 0
  Byte k_LenPlusTable_LOC[DICT_SIZE_BITS_MAX];
  memcpy(k_LenPlusTable_LOC, k_LenPlusTable, sizeof(k_LenPlusTable));
#endif

  PRINT_CNT("DecodeLZ2", 2000);

  CBitDecoder _bitStream;
  _bitStream.CopyFrom(bitStream);
  // _bitStream._stream = _inStream;
  // _bitStream._bufBase = _inputBuf;
  // _bitStream.Init();

  // _reps[*] can be larger than _winSize, if _winSize was reduced in solid stream.
  size_t rep0 = _reps[0];
  // size_t rep1 = _reps[1];
  // Byte *win = _window;
  Byte *winPos = _window + _winPos;
  const Byte *limit = _window + _limit;
  _exitType = Z7_RAR_EXIT_TYPE_NONE;

  for (;;)
  {
    if (winPos >= limit)
      LZ_LOOP_BREAK_OK
    // (winPos < limit)
    if (_bitStream._buf >= _bitStream._bufCheck_Block)
    {
      if (_bitStream.InputEofError())
        LZ_LOOP_BREAK_OK
      if (_bitStream._buf >= _bitStream._bufCheck)
      {
        if (!_bitStream._wasFinished)
          LZ_LOOP_BREAK_OK
        // _bitStream._wasFinished == true
        // we don't need Prepare() here, because all data was read
        // and PadZone (16 bytes) after data was filled.
      }
      const UInt64 processed = _bitStream.GetProcessedSize_Round();
      // some cases are error, but the caller will process such error cases.
      if (processed >= _bitStream._blockEnd &&
          (processed > _bitStream._blockEnd
            || _bitStream.GetProcessedBits7() >= _bitStream._blockEndBits7))
          LZ_LOOP_BREAK_OK
      // that check is not required, but it can help, if there is BUG in another code
      if (!_tableWasFilled)
        LZ_LOOP_BREAK_ERROR
    }
    
#if 0
    const unsigned sym = m_MainDecoder.Decode(&_bitStream);
#else
    unsigned sym;
    Z7_RAR_HUFF_DECODE_CHECK_break(sym, &m_MainDecoder, k_NumHufTableBits_Main, &_bitStream)
#endif
    
    if (sym < 256)
    {
      *winPos++ = (Byte)sym;
      // _lzSize++;
      continue;
    }
   
    CLenType len;

    if (sym < kSymbolRep + kNumReps)
    {
      if (sym >= kSymbolRep)
      {
        if (sym != kSymbolRep)
        {
          size_t dist = _reps[1];
          _reps[1] = rep0;
          rep0 = dist;
          if (sym >= kSymbolRep + 2)
          {
            #if 1
              rep0 = _reps[(size_t)sym - kSymbolRep];
              _reps[(size_t)sym - kSymbolRep] = _reps[2];
              _reps[2] = dist;
            #else
              if (sym != kSymbolRep + 2)
              {
                rep0 = _reps[3];
                _reps[3] = _reps[2];
                _reps[2] = dist;
              }
              else
              {
                rep0 = _reps[2];
                _reps[2] = dist;
              }
            #endif
          }
        }
#if 0
        len = m_LenDecoder.Decode(&_bitStream);
        if (len >= kLenTableSize)
          LZ_LOOP_BREAK_ERROR
#else
        Z7_RAR_HUFF_DECODE_CHECK_break(len, &m_LenDecoder, k_NumHufTableBits_Len, &_bitStream)
#endif
        if (len >= 8)
          len = SlotToLen(_bitStream, len);
        len += 2;
        // _lastLen = (UInt32)len;
      }
      else if (sym != 256)
      {
        len = (CLenType)_lastLen;
        if (len == 0)
        {
          // we ignore (_lastLen == 0) case, like original-unrar.
          // that case can mean error in stream.
          // lzError = true;
          // return S_FALSE;
          continue;
        }
      }
      else
      {
        _exitType = Z7_RAR_EXIT_TYPE_ADD_FILTER;
        LZ_LOOP_BREAK_OK
      }
    }
#if 0
    else if (sym >= kMainTableSize)
      LZ_LOOP_BREAK_ERROR
#endif
    else
    {
      _reps[3] = _reps[2];
      _reps[2] = _reps[1];
      _reps[1] = rep0;
      len = sym - (kSymbolRep + kNumReps);
      if (len >= 8)
        len = SlotToLen(_bitStream, len);
      len += 2;
      // _lastLen = (UInt32)len;
      
#if 0
      rep0 = (UInt32)m_DistDecoder.Decode(&_bitStream);
#else
      Z7_RAR_HUFF_DECODE_CHECK_break(rep0, &m_DistDecoder, k_NumHufTableBits_Dist, &_bitStream)
#endif

      if (rep0 >= 4)
      {
#if 0
        if (rep0 >= kDistTableSize_MAX)
          LZ_LOOP_BREAK_ERROR
#endif
        const unsigned numBits = ((unsigned)rep0 - 2) >> 1;
        rep0 = (2 | (rep0 & 1)) << numBits;

        const Byte *buf = _bitStream._buf;
#ifdef Z7_RAR5_USE_64BIT
        const UInt64 v = GetBe64(buf);
#else
        const UInt32 v = GetBe32(buf);
#endif

        // _lastLen = (UInt32)len;
        if (numBits < kNumAlignBits)
        {
          rep0 += // _bitStream.ReadBits9(numBits);
            _bitStream.ReadBits_Big25(numBits, v);
        }
        else
        {
          #if !defined(MY_CPU_AMD64)
            len += k_LenPlusTable[numBits];
          #elif 0
            len += k_LenPlusTable_LOC[numBits];
          #elif 1
            len += m_LenPlusTable[numBits];
          #elif 1 && defined(MY_CPU_64BIT) && defined(MY_CPU_AMD64)
            // len += (unsigned)((UInt64)0xfffffffeaa554000 >> (numBits * 2)) & 3;
            len += (unsigned)((UInt64)0xfffffffffeaa5540 >> (numBits * 2 - 8)) & 3;
          #elif 1
            len += 3;
            len -= (unsigned)(numBits -  7) >> (sizeof(unsigned) * 8 - 1);
            len -= (unsigned)(numBits - 12) >> (sizeof(unsigned) * 8 - 1);
            len -= (unsigned)(numBits - 17) >> (sizeof(unsigned) * 8 - 1);
          #elif 1
            len += 3;
            len -= (0x155aabf >> (numBits - 4) >> (numBits - 4)) & 3;
          #elif 1
            len += (numBits >= 7);
            len += (numBits >= 12);
            len += (numBits >= 17);
          #endif
          // _lastLen = (UInt32)len;
          if (_useAlignBits)
          {
            // if (numBits > kNumAlignBits)
            rep0 += (_bitStream.ReadBits_Big25(numBits - kNumAlignBits, v) << kNumAlignBits);
#if 0
            const unsigned a = m_AlignDecoder.Decode(&_bitStream);
            if (a >= kAlignTableSize)
              LZ_LOOP_BREAK_ERROR
#else
            unsigned a;
            Z7_RAR_HUFF_DECODE_CHECK_break(a, &m_AlignDecoder, k_NumHufTableBits_Align, &_bitStream)
#endif
            rep0 += a;
          }
          else
            rep0 += _bitStream.ReadBits_Big(numBits, v);
#ifndef Z7_RAR5_USE_64BIT
          if (numBits >= 30) // we don't want 32-bit overflow case
            rep0 = (size_t)0 - 1 - 1;
#endif
        }
      }
      rep0++;
    }

    {
      _lastLen = (UInt32)len;
      // len != 0

#ifdef Z7_RAR5_SHOW_STAT
      {
        size_t index = rep0;
        if (index >= kNumStats1)
          index = kNumStats1 - 1;
        g_stats1[index]++;
        g_stats2[index][len]++;
      }
#endif

      Byte *dest = winPos;
      winPos += len;
      if (rep0 <= _dictSize_forCheck)
      {
        const Byte *src;
        const size_t winPos_temp = (size_t)(dest - _window);
        if (rep0 > winPos_temp)
        {
          if (_lzSize == 0)
            goto error_dist;
          size_t back = rep0 - winPos_temp;
          // STAT_INC(g_NumOver)
          src = dest + (_winSize - rep0);
          if (back < len)
          {
            // len -= (CLenType)back;
            Z7_PRAGMA_OPT_DISABLE_LOOP_UNROLL_VECTORIZE
            do
              *dest++ = *src++;
            while (--back);
            src = dest - rep0;
          }
        }
        else
          src = dest - rep0;
        CopyMatch(rep0, dest, src, winPos);
        continue;
      }

error_dist:
      // LZ_LOOP_BREAK_ERROR;
      _lzError = LZ_ERROR_TYPE_DIST;
      do
        *dest++ = 0;
      while (dest < winPos);
      continue;
    }
  }

  LZ_RESTORE
  return S_OK;

#if 1
decode_error:
  /*
  if (_bitStream._hres != S_OK)
    return _bitStream._hres;
  */
  LZ_RESTORE
  return S_FALSE;
#endif
}



HRESULT CDecoder::DecodeLZ()
{
  CBitDecoder _bitStream;
  _bitStream._stream = _inStream;
  _bitStream._bufBase = _inputBuf;
  _bitStream.Init();

  // _reps[*] can be larger than _winSize, if _winSize was reduced in solid stream.
  size_t winPos = _winPos;
  Byte *win = _window;
  size_t limit;
  {
    size_t rem = _winSize - winPos;
    if (rem > kWriteStep)
        rem = kWriteStep;
    limit = winPos + rem;
  }
  
  for (;;)
  {
    if (winPos >= limit)
    {
      _winPos = winPos < _winSize ? winPos : _winSize;
      RINOK(WriteBuf())
      if (_unpackSize_Defined && _writtenFileSize > _unpackSize)
        break; // return S_FALSE;
      const size_t wp = _winPos;
      size_t rem = _winSize - wp;
      if (rem == 0)
      {
        _lzSize += wp;
        winPos -= wp;
        // (winPos < kMaxMatchLen < _winSize)
        // so memmove is not required here
        if (winPos)
          memcpy(win, win + _winSize, winPos);
        limit = _winSize;
        if (limit >= kWriteStep)
        {
          limit = kWriteStep;
          continue;
        }
        rem = _winSize - winPos;
      }
      if (rem > kWriteStep)
          rem = kWriteStep;
      limit = winPos + rem;
      continue;
    }

    // (winPos < limit)

    if (_bitStream._buf >= _bitStream._bufCheck_Block)
    {
      _winPos = winPos;
      if (_bitStream.InputEofError())
        break; // return S_FALSE;
      _bitStream.Prepare();

      const UInt64 processed = _bitStream.GetProcessedSize_Round();
      if (processed >= _bitStream._blockEnd)
      {
        if (processed > _bitStream._blockEnd)
          break; // return S_FALSE;
        {
          const unsigned bits7 = _bitStream.GetProcessedBits7();
          if (bits7 >= _bitStream._blockEndBits7)
          {
            if (bits7 > _bitStream._blockEndBits7)
            {
#if 1
              // we ignore thar error as original unrar
              _bitStream._minorError = true;
#else
              break; // return S_FALSE;
#endif
            }
            _bitStream.AlignToByte();
            // if (!_bitStream.AlignToByte()) break;
            if (_isLastBlock)
            {
              if (_bitStream.InputEofError())
                break;
              /*
              // packSize can be 15 bytes larger for encrypted archive
              if (_packSize_Defined && _packSize < _bitStream.GetProcessedSize())
                break;
              */
              if (_bitStream._minorError)
                return S_FALSE;
              return _bitStream._hres;
              // break;
            }
            RINOK(ReadTables(_bitStream))
            continue;
          }
        }
      }

      // end of block was not reached.
      // so we must decode more symbols
      // that check is not required, but it can help, if there is BUG in another code
      if (!_tableWasFilled)
        break; // return S_FALSE;
    }

    _limit = limit;
    _winPos = winPos;
    RINOK(DecodeLZ2(_bitStream))
    _bitStream._buf = _buf_Res;
    _bitStream._bitPos = _bitPos_Res;

    winPos = _winPos;
    if (_exitType == Z7_RAR_EXIT_TYPE_ADD_FILTER)
    {
      RINOK(AddFilter(_bitStream))
      continue;
    }
  }

  _winPos = winPos;
  
  if (_bitStream._hres != S_OK)
    return _bitStream._hres;

  return S_FALSE;
}



HRESULT CDecoder::CodeReal()
{
  _unsupportedFilter = false;
  _writeError = false;
  /*
  if (!_isSolid || !_wasInit)
  {
    _wasInit = true;
    // _lzSize = 0;
    _lzWritten = 0;
    _winPos = 0;
    for (unsigned i = 0; i < kNumReps; i++)
      _reps[i] = (size_t)0 - 1;
    _lastLen = 0;
    _tableWasFilled = false;
  }
  */
  _isLastBlock = false;

  InitFilters();

  _filterEnd = 0;
  _writtenFileSize = 0;
  const UInt64 lzSize = _lzSize + _winPos;
  _lzFileStart = lzSize;
  _lzWritten = lzSize;
  
  HRESULT res = DecodeLZ();

  HRESULT res2 = S_OK;
  if (!_writeError && res != E_OUTOFMEMORY)
    res2 = WriteBuf();
  /*
  if (res == S_OK)
    if (InputEofError())
      res = S_FALSE;
  */
  if (res == S_OK)
  {
    // _solidAllowed = true;
    res = res2;
  }
  if (res == S_OK && _unpackSize_Defined && _writtenFileSize != _unpackSize)
    return S_FALSE;
  return res;
}



Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  _lzError = LZ_ERROR_TYPE_NO;
/*
  if file is soild, but decoding of previous file was not finished,
  we still try to decode new file.
  We need correct huffman table at starting block.
  And rar encoder probably writes huffman table at start block, if file is big.
  So we have good chance to get correct huffman table in some file after corruption.
  Also we try to recover window by filling zeros, if previous file
  was decoded to smaller size than required.
  But if filling size is big, we do full reset of window instead.
*/
  #define Z7_RAR_RECOVER_SOLID_LIMIT (1 << 20)
  // #define Z7_RAR_RECOVER_SOLID_LIMIT 0 // do not fill zeros
  {
    // if (_winPos > 100) _winPos -= 100; // for debug: corruption
    const UInt64 lzSize = _lzSize + _winPos;
    if (!_isSolid || !_wasInit
        || (lzSize < _lzEnd
#if Z7_RAR_RECOVER_SOLID_LIMIT != 0
         && lzSize + Z7_RAR_RECOVER_SOLID_LIMIT < _lzEnd
#endif
        ))
    {
      if (_isSolid)
        _lzError = LZ_ERROR_TYPE_HEADER;
      _lzEnd = 0;
      _lzSize = 0;
      _lzWritten = 0;
      _winPos = 0;
      for (unsigned i = 0; i < kNumReps; i++)
        _reps[i] = (size_t)0 - 1;
      _lastLen = 0;
      _tableWasFilled = false;
      _wasInit = true;
    }
#if Z7_RAR_RECOVER_SOLID_LIMIT != 0
    else if (lzSize < _lzEnd)
    {
#if 0
      return S_FALSE;
#else
      // we can report that recovering was made:
      // _lzError = LZ_ERROR_TYPE_HEADER;
      // We write zeros to area after corruption:
      if (_window)
      {
        UInt64 rem = _lzEnd - lzSize;
        const size_t ws = _winSize;
        if (rem >= ws)
        {
          My_ZeroMemory(_window, ws);
          _lzSize = ws;
          _winPos = 0;
        }
        else
        {
          const size_t cur = ws - _winPos;
          if (cur <= rem)
          {
            rem -= cur;
            My_ZeroMemory(_window + _winPos, cur);
            _lzSize += _winPos;
            _winPos = 0;
          }
          My_ZeroMemory(_window + _winPos, (size_t)rem);
          _winPos += (size_t)rem;
        }
      }
      // else return S_FALSE;
#endif
    }
#endif
  }

  // we don't want _lzSize overflow
  if (_lzSize >= DICT_SIZE_MAX)
      _lzSize  = DICT_SIZE_MAX;
  _lzEnd = _lzSize + _winPos;
  // _lzSize <= DICT_SIZE_MAX
  // _lzEnd  <= DICT_SIZE_MAX * 2

  size_t newSize = _dictSize;
  if (newSize < kWinSize_Min)
      newSize = kWinSize_Min;
  
  _unpackSize = 0;
  _unpackSize_Defined = (outSize != NULL);
  if (_unpackSize_Defined)
    _unpackSize = *outSize;
  
  if ((Int64)_unpackSize >= 0)
    _lzEnd += _unpackSize; // known end after current file
  else
    _lzEnd = 0; // unknown end
  
  if (_isSolid && _window)
  {
    // If dictionary was decreased in solid, we use old dictionary.
    if (newSize > _dictSize_forCheck)
    {
      // If dictionary was increased in solid, we don't want grow.
      return S_FALSE; // E_OUTOFMEMORY
    }
    // (newSize <= _winSize)
  }
  else
  {
    _dictSize_forCheck = newSize;
    {
      size_t newSize_small = newSize;
      const size_t k_Win_AlignSize = 1u << 18;
      /* here we add (1 << 7) instead of (COPY_CHUNK_SIZE - 1), because
      we want to get same (_winSize) for different COPY_CHUNK_SIZE values. */
      // newSize += (COPY_CHUNK_SIZE - 1) + (k_Win_AlignSize - 1); // for debug : we can get smallest (_winSize)
      newSize += (1 << 7) + k_Win_AlignSize;
      newSize &= ~(size_t)(k_Win_AlignSize - 1);
      if (newSize < newSize_small)
        return E_OUTOFMEMORY;
    }
    // (!_isSolid || !_window)
    const size_t allocSize = newSize + kMaxMatchLen + 64;
    if (allocSize < newSize)
      return E_OUTOFMEMORY;
    if (!_window || allocSize > _winSize_Allocated)
    {
      Z7_RAR_FREE_WINDOW
        _window = NULL;
      _winSize_Allocated = 0;
      Byte *win = (Byte *)::BigAlloc(allocSize);
      if (!win)
        return E_OUTOFMEMORY;
      _window = win;
      _winSize_Allocated = allocSize;
    }
    _winSize = newSize;
  }
  
  if (!_inputBuf)
  {
    _inputBuf = (Byte *)z7_AlignedAlloc(kInputBufSize + kInputBufferPadZone);
    if (!_inputBuf)
      return E_OUTOFMEMORY;
  }
  
  _inStream = inStream;
  _outStream = outStream;
  _progress = progress;
  _progress_Pack = 0;
  _progress_Unpack = 0;
  
  const HRESULT res = CodeReal();
  
  if (res != S_OK)
    return res;
  // _lzError = LZ_ERROR_TYPE_HEADER; // for debug
  if (_lzError)
    return S_FALSE;
  if (_unsupportedFilter)
    return E_NOTIMPL;
  return S_OK;
}


Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size))
{
  if (size != 2)
    return E_INVALIDARG;
  const unsigned pow = data[0];
  const unsigned b1 = data[1];
  const unsigned frac = b1 >> 3;
  // unsigned pow = 15 + 8;
  // unsigned frac = 1;
  if (pow + ((frac + 31) >> 5) > MAX_DICT_LOG - 17)
  // if (frac + (pow << 8) >= ((8 * 2 + 7) << 5) + 8 / 8)
    return E_NOTIMPL;
  _dictSize = (size_t)(frac + 32) << (pow + 12);
  _isSolid = (b1 & 1) != 0;
  _is_v7   = (b1 & 2) != 0;
  // printf("\ndict size = %p\n", (void *)(size_t)_dictSize);
  return S_OK;
}

}}
