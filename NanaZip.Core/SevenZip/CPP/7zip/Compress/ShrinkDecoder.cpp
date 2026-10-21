// ShrinkDecoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../Common/StreamUtils.h"

#include "ShrinkDecoder.h"

namespace NCompress {
namespace NShrink {

#ifdef MY_CPU_LE_UNALIGN
#define GetUi24or32(p)      (*(const UInt32 *)(const void *)(p))
#else
#define GetUi24or32(p) ( \
             ((const Byte *)(p))[0]        | \
    ((UInt32)((const Byte *)(p))[1] <<  8) | \
    ((UInt32)((const Byte *)(p))[2] << 16))
#endif

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


#define k_OutBuf_Size (1 << 16)
#define k_InBuf_Size  (1 << 15) // >= 8

static const unsigned kEmpty = 256; // don't change it

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * const inSize, const UInt64 * const outSize, ICompressProgressInfo *progress))
{
  const size_t OUT_TOTAL_SIZE = k_OutBuf_Size + kNumItems;
  CAlignedBuffer_NoSize_NoExceptions outBuf;
  outBuf.Alloc_after_Constructor(OUT_TOTAL_SIZE + k_InBuf_Size + 16); // + 16 for overread
  if (!outBuf.IsAllocated())
    return E_OUTOFMEMORY;
  {
    for (size_t i = 0; i < kNumItems; i++)
      _parents[i] = kEmpty;
  }
  Z7_memset_0_ARRAY(_suffixes); // we must initialize _suffixes[] with any data
  _inProcessed = 0;
  HRESULT res = S_FALSE; // default code for ERROR
  Byte * const inBuf = outBuf + OUT_TOTAL_SIZE;
  Byte *src, *src_end;
  src = src_end = inBuf;
  unsigned bitPos = 0;
  unsigned progressCounter = 0;
  UInt64 outProcessed = 0;
  size_t outPos = 0;
  unsigned numBits = 9; // kNumMinBits;
  // size_t head_max = 256;
  size_t head = 256;
  unsigned lastSym = 256; // = 256, so we keep (_parents[256] == 256) after first (sym).
  Byte lastChar = 0;
  bool moreOut = false;

  for (;;)
  {
    bool eofCheck = false;
    if (outSize && outProcessed >= *outSize)
    {
      if (!_fullStreamMode || moreOut)
      {
        res = S_OK;
        break;
      }
      eofCheck = true;
      // Is specSym(=256) allowed after end of stream ?
      // Do we need to read it here ?
    }
    if (progress && progressCounter >= (1 << 12))
    {
      progressCounter = 0;
      RINOK(progress->SetRatioInfo(&_inProcessed, &outProcessed))
    }
    {
      size_t rem = (size_t)(src_end - src);
      if (rem < 4 + 2)
      if (inBuf == src_end || inBuf + k_InBuf_Size == src_end)
      {
        _inProcessed += (size_t)(src - inBuf);
        memmove(inBuf, src, rem);
        size_t num = k_InBuf_Size - rem;
        progressCounter += 16;
        const HRESULT res2 = ReadStream(inStream, inBuf + rem, &num);
        rem += num;
        src = inBuf;
        src_end = src + rem;
        if (res2 != S_OK)
        {
          res = res2;
          break;
        }
        memset(src_end, 0, 4); // we initialize data for overread in GetUi24or32(src)
      }
    }

    UInt32 sym;
    {
      if (src + ((bitPos + numBits + 7) >> 3) > src_end)
      {
        res = S_OK; // OK: end of stream : (num_avail_bits < numBits)
        break;
      }
      sym = (GetUi24or32(src) >> bitPos) & ((1u << numBits) - 1);
      src += (bitPos += numBits) >> 3;
      bitPos &= 7;
    }
    
    if (sym == 256) // ESCAPE_SYMBOL
    {
      if (head == 256) // if it's first Symbol of stream
        break; // ERROR: we expect that first Symbol is not ESCAPE_SYMBOL
      
      if (src + ((bitPos + numBits + 7) >> 3) > src_end)
        break; // ERROR: end of stream : (num_avail_bits < numBits)
      sym = (GetUi24or32(src) >> bitPos) & ((1u << numBits) - 1);
      src += (bitPos += numBits) >> 3;
      bitPos &= 7;

      if (sym == 1)
      {
        if (numBits >= Z7_SHRINK_NUM_MAX_BITS)
          break;
        numBits++;
        continue;
      }
      if (sym != 2)
        break; // ERROR: unsupported control code

      // ---------- Free leaf nodes ----------
      /* Notes : this code can mark _parents[lastSym] as free (Orphan),
                 but such Orphan node will be parent for next inserted node. */
      // "unshrink" processes _parents[] only up to (head) here.
      // we can use calculated (head_max) variable instead of (kNumItems)
      const size_t head_max = kNumItems;
      size_t i;
      for (i = 256; i < head_max; i++)  _stack[i] = 0;
      for (i = 257; i < head_max; i++)  _stack[_parents[i]] = 1;
      for (i = 257; i < head_max; i++)  if (_stack[i] == 0) _parents[i] = kEmpty;
      progressCounter++;
      head = 257;
      continue;
    }

    if (eofCheck)
    {
      // we will check (*inSize != _inProcessed) later
      res = S_OK;
      break;
    }

    // bool needPrev = false;
    // if (lastSym == 256) it's first symbol, and we keep (_parents[256] == 256)
    {
      do
      {
        if (head >= kNumItems)
          goto loop_exit; // v26.05 : similar to "unshrink" code : we require creation of new NODE.
        head++;
      }
      while (_parents[head - 1] != kEmpty);
      // if (head_max < head) head_max = head;
      /* "Orphan" case is possible here, when
            (lastSym >= 256 && _parents[lastSym] == kEmpty)
         That case is possible in standard Z-archives if the node
         NODE[lastSym] has been removed from the tree by the leaf-removal code.
         NODE[lastSym] will be "Orphan" node in that case.
         If node NODE[lastSym] later will be inserted to tree, two cases are possible:
         : cyclic _parents[] sequence.
         : normal _parents[] sequence up to root literal items.
         We must check _stack[i++] overflow in code that walks _parents[].
         Note: nodes in cyclic sequence cannot be removed from the tree.
         Therefore, such cyclic nodes can occupy all elements.
         Example: if (head == lastSym), it updates Orphan to self-linked Orphan.
      */
      // needPrev = true;
      _parents[head - 1] = (UInt16)lastSym;
      _suffixes[head - 1] = (Byte)lastChar; // temporary suffix for "KwKwK" case processing.
    }

    lastSym = sym;
    size_t cur = sym, i = 0;
    if (cur >= 256)
    do
    {
      _stack[i++] = _suffixes[cur];
      cur = _parents[cur];
      // don't change that code:
      if (i >= kNumItems /* - 256 */  // check for cyclic links (to prevent _stack[] overflow)
          || cur == kEmpty)  // Orphan check
        goto loop_exit;
    }
    while (cur > 256);
    
    _stack[i++] = (Byte)cur;
    lastChar = (Byte)cur; // it's first byte of current word

    // if (needPrev)
    _suffixes[(size_t)head - 1] = (Byte)cur; // we replace the temporary suffix with the actual one.

    size_t num = i;
    if (outSize)
    {
      const UInt64 limit = *outSize - outProcessed;
      // (limit > 0) because of (eofCheck) code
      if (i > limit)
      {
        moreOut = true;
        num = (size_t)limit;
        if (num == 0)
          break; // return E_FAIL; // some internal code failure
      }
    }
    Byte *dest = outBuf + outPos;
    outProcessed += num;
    outPos += num;
    do
      *dest++ = _stack[--i];
    while (--num);

    if (outPos >= k_OutBuf_Size)
    {
      const size_t write =
#if k_OutBuf_Size >= Z7_SHRINK_NUM_ITEMS
        k_OutBuf_Size;
#else
        outPos & ~(size_t)(k_OutBuf_Size - 1);
#endif
      progressCounter += 16;
      RINOK(WriteStream(outStream, outBuf, write))
      outPos -= write;
      memcpy(outBuf, outBuf + write, outPos);
    }
  }

loop_exit:
  {
    const HRESULT res2 = WriteStream(outStream, outBuf, outPos);
    if (res == S_OK)
      res = res2;
  }
  src += (bitPos + 7) >> 3;
  _inProcessed += (size_t)(src - inBuf);
  if (res == S_OK && _fullStreamMode)
  {
    if (bitPos && (src[-1] >> bitPos))
      res = S_FALSE;
    if (moreOut)
      res = S_FALSE;
    if (outSize && *outSize != outProcessed)
      res = S_FALSE;
    if (inSize && *inSize != _inProcessed)
      res = S_FALSE;
  }
  return res;
}


Z7_COM7F_IMF(CDecoder::SetFinishMode(UInt32 finishMode))
{
  _fullStreamMode = finishMode;
  return S_OK;
}

Z7_COM7F_IMF(CDecoder::GetInStreamProcessedSize(UInt64 *value))
{
  *value = _inProcessed;
  return S_OK;
}

}}
