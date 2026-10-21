// ZDecoder.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../Common/StreamUtils.h"

#include "ZDecoder.h"

#ifdef MY_CPU_LE_UNALIGN
#define GetUi24or32(p)      (*(const UInt32 *)(const void *)(p))
#define GetUi24or32_TOP(p)  (*(const UInt32 *)(const void *)(p))
#else
#define GetUi24or32(p) ( \
             ((const Byte *)(p))[0]        | \
    ((UInt32)((const Byte *)(p))[1] <<  8) | \
    ((UInt32)((const Byte *)(p))[2] << 16))
#define GetUi24or32_TOP(p) ( \
    ((UInt32)((const Byte *)(p))[1] <<  8) | \
    ((UInt32)((const Byte *)(p))[2] << 16) | \
    ((UInt32)((const Byte *)(p))[3] << 24))
#endif

namespace NCompress {
namespace NZ {

class CAlignedBuffer_NoSize_NoExceptions
{
  Byte *_data;
  Z7_CLASS_NO_COPY(CAlignedBuffer_NoSize_NoExceptions)
public:
  CAlignedBuffer_NoSize_NoExceptions(): _data(NULL) {}
  ~CAlignedBuffer_NoSize_NoExceptions() { z7_AlignedFree(_data); }
  bool IsAllocated() const { return _data != NULL; }
  operator       Byte *()       { return _data; }
  operator const Byte *() const { return _data; }
  void Alloc_after_Constructor(size_t size)
  {
   _data = (Byte *)z7_AlignedAlloc(size);
  }
};

#define PROGRESS_COUNTER_MAX  64

#define kNumMaxBits 16
#define k_OutBuf_Size (1 << 16)
#define k_InBuf_Size  (1 << 16)
#if k_InBuf_Size < kNumMaxBits || k_InBuf_Size >= (1 << (31 - 3))
  #error Stop_Compiling_ZDecoder_InBuf_Size
#endif

static const unsigned kNumBitsMask = 0x7f;
static const unsigned kNumBitsStart = 9;

#define kNumMinBits 9  // to support old rare case Z archives with (maxBits == 9).
// #define kNumMinBits 10  // it ignores (maxBits == 9) cases. The code is smaller.

#if kNumMinBits <= 9
  #define Z7_ZDECODER_BIT9_COMPAT // define to be compatible with original Z decoder
#endif

/*
if (Z7_ZDECODER_BIT9_COMPAT) is defined, it can unpack some old bit9 archives.
if (Z7_ZDECODER_BIT9_COMPAT) is not defined
  { it works as old 7-Zip v26.04-.
    It doesn't use special additional code to process (maxBits == 9) codes.
    So it can't unpack some old bit9 archives correctly.
    But it still can unpack some data sequences from incorrect archives created by "ncompress -b 9".
    "ncompress" encoder probably uses 9-bit codes, instead of switching to 10-bit.
    But "ncompress" encoder writes wrong symbols to stream. }

(maxBits == 9) is a special unusual case in the original Z Compress algorithm:
{
  original Z decoder and 7zip's (Z7_ZDECODER_BIT9_COMPAT) works so:
    after reaching 512 elements of parents[0 ... 511],
    the decoder switches to 10-bit mode. However, all code symbols
      still use only 9 bits from 10 bits, and (sym < 512) for symbols in stream.
  Note: also there are some bugs in "ncompress" encoder program:
    "ncompress" encoder can create incorrect streams for (maxBits == 9) "ncompress -b 9" case,
    and no program can unpack such archives correctly.
}
*/
#ifdef Z7_ZDECODER_BIT9_COMPAT
  /* (numItems == SPEC_NUM_ITEMS_513) allows us to switch to 10-bit mode.
     we reset correct value (numItems = (1 << 9) when we switch to 10-bit mode. */
  #define SPEC_NUM_ITEMS_513  ((1 << 9) + 1)
  
  #define SPEC_SET_513_numItems \
    { if (numItems < SPEC_NUM_ITEMS_513) \
          numItems = SPEC_NUM_ITEMS_513; }
  
  // reset correct (numItems = head = (1 << 9)) values:
  #define SPEC_RESET_512_numItems_head \
    { if (numItems == SPEC_NUM_ITEMS_513) \
        head = numItems = 1 << 9; }
#else
  #define SPEC_SET_513_numItems
  #define SPEC_RESET_512_numItems_head
#endif


HRESULT Decode(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    ICompressProgressInfo *progress) throw()
{
  CAlignedBuffer_NoSize_NoExceptions inBuf, outBuf;
  inBuf.Alloc_after_Constructor(k_InBuf_Size + 16); // + 16 for overread
  if (!inBuf.IsAllocated())
    return E_OUTOFMEMORY;
  Byte *src_end;
  {
    size_t size = k_InBuf_Size;
    RINOK(ReadStream(inStream, inBuf, &size))
    if (size < 3 || GetUi16a(inBuf) != 0x9d1f) // if (inBuf[0] != 0x1f || inBuf[1] != 0x9d)
      return S_FALSE;
    src_end = inBuf + size;
    memset(src_end, 0, 4); // we initialize data for overread in GetUi24or32(src)
  }

  unsigned prop = inBuf[2];
  const unsigned maxBits = prop & kNumBitsMask;
  if (maxBits < kNumMinBits || maxBits > kNumMaxBits)
    return S_FALSE;
  size_t numItems = (size_t)1 << maxBits;
  
  typedef UInt16 CParent;
  CParent *parents = NULL;
  // Byte *suffixes = NULL;
  // CParent *LENS = NULL;
  
  if (outStream)
  {
    /* the code is smaller and simpler with fixed size arrays.
       so we use max size arrays of 64K items instead of (numItems) arrays */
#define NUM_ITEMS_ALLOC   (1 << kNumMaxBits)
    // we use additional space in output buffer for overwrite
#define PRE_SIZE  (k_OutBuf_Size + NUM_ITEMS_ALLOC)
    /* (kParentsExtra > 0) is required because we write to parents[head].
       (kParentsExtra == 64 + 32) is optional optimization to avoid
       conflicts in the low-address bits between the parents[] and LENS[]. */
#define kParentsExtra   (64 + 32)
    // (kLensExtra > 0) is required because we write to LENS[head = numItems];
#define kLensExtra      (64 + 32)
    // (kSuffixesExtra > 0) is required because we write to SUFFIXES[prev = head = numItems];
    const size_t kSuffixesExtra = 64;
    const size_t size = kParentsExtra + kLensExtra + kSuffixesExtra
        + (size_t)NUM_ITEMS_ALLOC * (2 * sizeof(CParent) + sizeof(Byte));
    
    outBuf.Alloc_after_Constructor(PRE_SIZE + size);
    if (!outBuf.IsAllocated())
      return E_OUTOFMEMORY;
    parents = (CParent *)(void *)(outBuf + PRE_SIZE);
    /* we clear parents[0 ... 255, 256] for mispredicted accesses.
       LENS[] also is accessed in mispredicted branches.
       clearing suffixes[] is optional. */
    memset(parents, 0, size); // clear at least (256 + 32) * sizeof(CParent) bytes to reduce

#define LENS (parents + NUM_ITEMS_ALLOC + kParentsExtra / sizeof(CParent))
#define SUFFIXES ((Byte *)(void *)(LENS + NUM_ITEMS_ALLOC + kLensExtra / sizeof(CParent)))
    // suffixes = SUFFIXES;
  }

  SPEC_SET_513_numItems

  UInt64 inProcessed = 0, outProcessed = 0;
  unsigned progressCounter = 0;
  HRESULT res = S_OK;
  prop >>= 7;
  const size_t blockSym = ((UInt32)prop - 1) ^ 256;
  size_t head = prop + 256;
  size_t prev = head; // or = numItems;
  size_t outPos = 0;
  unsigned numBits = kNumBitsStart;
  Byte *src = inBuf + 3;
  unsigned remBits = 0;
  size_t mask = 0; // ((size_t)1 << numBits) - 1;
  // static unsigned cnt_numLits = 0, cnt_numSymbols = 0;
  
  for (;;)
  {
    if Z7_LIKELY (remBits >= numBits)
    {
      const unsigned bitPos = (0u - remBits) & 7;
      size_t sym = (GetUi24or32(src) >> bitPos) & mask;
      remBits -= numBits;
      src += (bitPos + numBits) >> 3;
      if Z7_UNLIKELY (sym >= head)
      {
        res = S_FALSE;
        break;
      }
      // cnt_numSymbols++;
      if Z7_LIKELY (sym != blockSym)
      {
        if Z7_LIKELY (parents != NULL)
        {
          parents[head] = (CParent)sym;
          
          if (sym < 256)
          {
            // cnt_numLits++;
            LENS[head] = 0;
            SUFFIXES[prev] = (Byte)sym;
          }
          else
          {
            size_t len = LENS[sym];
            LENS[head] = (CParent)(len + 1);
            size_t cur = parents[sym];
            outPos += len;
            Byte *s = outBuf + outPos++;
            // #define suffixes SUFFIXES
            Byte * const suffixes = SUFFIXES;
            if (len) do
            {
              *s-- = suffixes[cur];
              cur = parents[cur];
            }
            while (--len); // fast detection of loop exit.
            // while (cur >= 256); // slow : branch misprediction depending on pointer chasing
            suffixes[prev] = (Byte)cur;
            *s = (Byte)cur;
            sym = suffixes[sym];
          }
          
          outBuf[outPos++] = (Byte)sym;
          // static unsigned cnt2 = 0;
          // printf("\n%4u : head = %5x outPos = %6x", cnt2++, (unsigned)head, (unsigned)outPos);
          if Z7_UNLIKELY (outPos >= k_OutBuf_Size)
          {
            const size_t write =
              #if k_OutBuf_Size >= NUM_ITEMS_ALLOC
                  k_OutBuf_Size;
              #else
                  outPos & ~(size_t)(k_OutBuf_Size - 1);
              #endif
            outProcessed += write;
            progressCounter++;
            if (progressCounter >= PROGRESS_COUNTER_MAX && progress)
            {
              progressCounter = 0;
              const UInt64 inVal = inProcessed + (size_t)(src - inBuf);
              RINOK(progress->SetRatioInfo(&inVal, &outProcessed))
            }
            RINOK(WriteStream(outStream, outBuf, write))
            outPos -= write;
            memcpy(outBuf, outBuf + write, outPos);
          }
        }
        
        prev = head;
        if (head >= numItems)
          continue;
        if Z7_LIKELY (head++ <= mask)
          continue;
        
        SPEC_RESET_512_numItems_head
        /* it increases (src) for +0, if (blockSym == 256 || numBits != 9)
           it increases (src) for +8, if (blockSym != 256 && numBits == 9) */
        src += ((remBits + 7) >> 3) % numBits;
        numBits++;
        // mask = ((size_t)1 << numBits) - 1; // mask = mask * 2 + 1;
      }
      else // NEW BLOCK
      {
#if 0
        static unsigned cnt_newBlock = 0, cnt_numSymbols_prev = 0;
        printf("\n NEW BLOCK %7u ; after %7u symbols", cnt_newBlock++, cnt_numSymbols - cnt_numSymbols_prev);
        cnt_numSymbols_prev = cnt_numSymbols;
#endif
        SPEC_SET_513_numItems
        head = 257;
        // __assume(numBits != 0);
        src += ((remBits + 7) >> 3) % numBits;
        numBits = kNumBitsStart;
        // mask = ((size_t)1 << numBits) - 1;
      }
      remBits = 0;
    }

    {
      // ---------- PROVIDE INPUT DATA ----------
      if Z7_LIKELY (remBits == 0)
      {
        size_t rem = (size_t)(src_end - src);
        if (rem < numBits && inBuf + k_InBuf_Size == src_end)
        {
          inProcessed += (size_t)(src - inBuf);
          progressCounter++;
          memmove(inBuf, src, rem);
          size_t num = k_InBuf_Size - rem;
          res = ReadStream(inStream, inBuf + rem, &num);
          if (res != S_OK)
            break;
          if (progressCounter >= PROGRESS_COUNTER_MAX && progress)
          {
            progressCounter = 0;
            const UInt64 outVal = outProcessed + outPos;
            RINOK(progress->SetRatioInfo(&inProcessed, &outVal))
          }
          rem += num;
          src = inBuf;
          src_end = src + rem;
        }
        if (rem >= numBits)
          rem -= (UInt32)rem % numBits; // rem = (UInt32)rem / numBits * numBits;
        remBits = (unsigned)rem * 8;
      }
      
      mask = ((size_t)1 << numBits) - 1;
      
      if (remBits < numBits)
      {
        if (remBits >= 8  // there is extra byte at the end of stream
            || ((unsigned)*src << remBits) >= (1 << 8))  // non-ZERO bits in bit padding in last byte
          res = S_FALSE;
        break;
      }
    }
  }
  
  if (outBuf)
  {
    const HRESULT res2 = WriteStream(outStream, outBuf, outPos);
    if (res == S_OK)
      res = res2;
  }
  // printf("\n Symbols = %8u, Lits = %8u\n", cnt_numSymbols, cnt_numLits);
  return res;
}



bool CheckStream(const Byte *src, const size_t size) throw()
{
  if (size < 3 || GetUi16(src) != 0x9d1f) // if (src[0] != 0x1f || src[1] != 0x9d)
    return false;
  unsigned prop = src[2];
  const unsigned maxBits = prop & kNumBitsMask;
  if (maxBits < kNumMinBits || maxBits > kNumMaxBits)
    return false;
  UInt32 numItems = (UInt32)1 << maxBits;
  prop >>= 7;
  const UInt32 blockSym = ((UInt32)prop - 1) ^ 256;
  UInt32 head = prop + 256;
  const Byte * const src_end = src + size - 3;
newBlock:
  SPEC_SET_513_numItems
  unsigned numBits = kNumBitsStart;
  for (;;)
  {
    int remBits;
    {
      size_t rem = (size_t)(src_end - src);
      if (rem >= numBits)
          rem  = numBits;
      remBits = (int)((unsigned)rem * 8);
    }
    do
    {
      if Z7_UNLIKELY ((remBits -= (int)numBits) < 0)
        return true; // we don't check the tail because stream is not full
      const unsigned bitPos = remBits & 7;
      src += (bitPos + numBits) >> 3;
      const UInt32 sym = GetUi24or32_TOP(src - (4 - 3)) << bitPos >> (32 - numBits);
      if Z7_UNLIKELY (sym >= head)
        return false;
      if Z7_LIKELY (sym != blockSym)
      {
        // __assume(numBits != 0 && numBits <= kNumMaxBits);
        if (head >= numItems || (head++ >> numBits) == 0)
          continue;
        src += (unsigned)remBits >> 3;
        SPEC_RESET_512_numItems_head
        numBits++;
        break;
      }
      {
        // NEW BLOCK
        src += (unsigned)remBits >> 3;
        head = 257;
        goto newBlock;
      }
    }
    while (remBits);
  }
}

}}
