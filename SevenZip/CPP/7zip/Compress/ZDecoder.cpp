// ZDecoder.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/Alloc.h"

#include "../Common/InBuffer.h"
#include "../Common/OutBuffer.h"

#include "ZDecoder.h"

namespace NCompress {
namespace NZ {

static const UInt32 kBufferSize = (1 << 20);
static const Byte kNumBitsMask = 0x1F;
static const Byte kBlockModeMask = 0x80;
static const unsigned kNumMinBits = 9;
static const unsigned kNumMaxBits = 16;

void CDecoder::Free()
{
  MyFree(_parents); _parents = 0;
  MyFree(_suffixes); _suffixes = 0;
  MyFree(_stack); _stack = 0;
}

CDecoder::~CDecoder() { Free(); }

HRESULT CDecoder::CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
  CInBuffer inBuffer;
  COutBuffer outBuffer;

  PackSize = 0;
  
  if (!inBuffer.Create(kBufferSize))
    return E_OUTOFMEMORY;
  inBuffer.SetStream(inStream);
  inBuffer.Init();

  if (!outBuffer.Create(kBufferSize))
    return E_OUTOFMEMORY;
  outBuffer.SetStream(outStream);
  outBuffer.Init();

  Byte buf[kNumMaxBits + 4];
  {
    if (inBuffer.ReadBytes(buf, 3) < 3)
      return S_FALSE;
    if (buf[0] != 0x1F || buf[1] != 0x9D)
      return S_FALSE;;
  }
  Byte prop = buf[2];

  if ((prop & 0x60) != 0)
    return S_FALSE;
  unsigned maxbits = prop & kNumBitsMask;
  if (maxbits < kNumMinBits || maxbits > kNumMaxBits)
    return S_FALSE;
  UInt32 numItems = 1 << maxbits;
  // Speed optimization: blockSymbol can contain unused velue.

  if (maxbits != _numMaxBits || _parents == 0 || _suffixes == 0 || _stack == 0)
  {
    Free();
    _parents = (UInt16 *)MyAlloc(numItems * sizeof(UInt16)); if (_parents == 0) return E_OUTOFMEMORY;
    _suffixes = (Byte *)MyAlloc(numItems * sizeof(Byte)); if (_suffixes == 0) return E_OUTOFMEMORY;
    _stack = (Byte *)MyAlloc(numItems * sizeof(Byte)); if (_stack == 0) return E_OUTOFMEMORY;
    _numMaxBits = maxbits;
  }

  UInt64 prevPos = 0;
  UInt32 blockSymbol = ((prop & kBlockModeMask) != 0) ? 256 : ((UInt32)1 << kNumMaxBits);
  unsigned numBits = kNumMinBits;
  UInt32 head = (blockSymbol == 256) ? 257 : 256;
  bool needPrev = false;
  unsigned bitPos = 0;
  unsigned numBufBits = 0;

  _parents[256] = 0; // virus protection
  _suffixes[256] = 0;
  HRESULT res = S_OK;

  for (;;)
  {
    if (numBufBits == bitPos)
    {
      numBufBits = (unsigned)inBuffer.ReadBytes(buf, numBits) * 8;
      bitPos = 0;
      UInt64 nowPos = outBuffer.GetProcessedSize();
      if (progress && nowPos - prevPos >= (1 << 13))
      {
        prevPos = nowPos;
        UInt64 packSize = inBuffer.GetProcessedSize();
        RINOK(progress->SetRatioInfo(&packSize, &nowPos));
      }
    }
    unsigned bytePos = bitPos >> 3;
    UInt32 symbol = buf[bytePos] | ((UInt32)buf[(size_t)bytePos + 1] << 8) | ((UInt32)buf[(size_t)bytePos + 2] << 16);
    symbol >>= (bitPos & 7);
    symbol &= (1 << numBits) - 1;
    bitPos += numBits;
    if (bitPos > numBufBits)
      break;
    if (symbol >= head)
    {
      res = S_FALSE;
      break;
    }
    if (symbol == blockSymbol)
    {
      numBufBits = bitPos = 0;
      numBits = kNumMinBits;
      head = 257;
      needPrev = false;
      continue;
    }
    UInt32 cur = symbol;
    unsigned i = 0;
    while (cur >= 256)
    {
      _stack[i++] = _suffixes[cur];
      cur = _parents[cur];
    }
    _stack[i++] = (Byte)cur;
    if (needPrev)
    {
      _suffixes[(size_t)head - 1] = (Byte)cur;
      if (symbol == head - 1)
        _stack[0] = (Byte)cur;
    }
    do
      outBuffer.WriteByte((_stack[--i]));
    while (i > 0);
    if (head < numItems)
    {
      needPrev = true;
      _parents[head++] = (UInt16)symbol;
      if (head > ((UInt32)1 << numBits))
      {
        if (numBits < maxbits)
        {
          numBufBits = bitPos = 0;
          numBits++;
        }
      }
    }
    else
      needPrev = false;
  }
  PackSize = inBuffer.GetProcessedSize();
  HRESULT res2 = outBuffer.Flush();
  return (res == S_OK) ? res2 : res;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  try { return CodeReal(inStream, outStream, inSize, outSize, progress); }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const COutBufferException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}

bool CheckStream(const Byte *data, size_t size)
{
  if (size < 3)
    return false;
  if (data[0] != 0x1F || data[1] != 0x9D)
    return false;
  Byte prop = data[2];
  if ((prop & 0x60) != 0)
    return false;
  unsigned maxbits = prop & kNumBitsMask;
  if (maxbits < kNumMinBits || maxbits > kNumMaxBits)
    return false;
  UInt32 numItems = 1 << maxbits;
  UInt32 blockSymbol = ((prop & kBlockModeMask) != 0) ? 256 : ((UInt32)1 << kNumMaxBits);
  unsigned numBits = kNumMinBits;
  UInt32 head = (blockSymbol == 256) ? 257 : 256;
  unsigned bitPos = 0;
  unsigned numBufBits = 0;
  Byte buf[kNumMaxBits + 4];
  data += 3;
  size -= 3;
  // printf("\n\n");
  for (;;)
  {
    if (numBufBits == bitPos)
    {
      unsigned num = (numBits < size) ? numBits : (unsigned)size;
      memcpy(buf, data, num);
      data += num;
      size -= num;
      numBufBits = num * 8;
      bitPos = 0;
    }
    unsigned bytePos = bitPos >> 3;
    UInt32 symbol = buf[bytePos] | ((UInt32)buf[bytePos + 1] << 8) | ((UInt32)buf[bytePos + 2] << 16);
    symbol >>= (bitPos & 7);
    symbol &= (1 << numBits) - 1;
    bitPos += numBits;
    if (bitPos > numBufBits)
    {
      // printf("  OK", symbol);
      return true;
    }
    // printf("%3X ", symbol);
    if (symbol >= head)
      return false;
    if (symbol == blockSymbol)
    {
      numBufBits = bitPos = 0;
      numBits = kNumMinBits;
      head = 257;
      continue;
    }
    if (head < numItems)
    {
      head++;
      if (head > ((UInt32)1 << numBits))
      {
        if (numBits < maxbits)
        {
          numBufBits = bitPos = 0;
          numBits++;
        }
      }
    }
  }
}

}}
