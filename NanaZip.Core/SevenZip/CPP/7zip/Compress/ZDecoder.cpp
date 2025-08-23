﻿// ZDecoder.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/Alloc.h"

#include "../Common/InBuffer.h"
#include "../Common/OutBuffer.h"

#include "ZDecoder.h"

namespace NCompress {
namespace NZ {

static const size_t kBufferSize = 1 << 20;
static const Byte kNumBitsMask = 0x1F;
static const Byte kBlockModeMask = 0x80;
static const unsigned kNumMinBits = 9;
static const unsigned kNumMaxBits = 16;

void CDecoder::Free()
{
  MyFree(_parents); _parents = NULL;
  MyFree(_suffixes); _suffixes = NULL;
  MyFree(_stack); _stack = NULL;
}

CDecoder::~CDecoder() { Free(); }

HRESULT CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    ICompressProgressInfo *progress)
{
  try {
  // PackSize = 0;

  CInBuffer inBuffer;
  COutBuffer outBuffer;

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
      return S_FALSE;
  }
  const Byte prop = buf[2];

  if ((prop & 0x60) != 0)
    return S_FALSE;
  const unsigned maxbits = prop & kNumBitsMask;
  if (maxbits < kNumMinBits || maxbits > kNumMaxBits)
    return S_FALSE;
  const UInt32 numItems = (UInt32)1 << maxbits;
  // Speed optimization: blockSymbol can contain unused velue.

  if (maxbits != _numMaxBits || !_parents || !_suffixes || !_stack)
  {
    Free();
    _parents = (UInt16 *)MyAlloc(numItems * sizeof(UInt16)); if (!_parents) return E_OUTOFMEMORY;
    _suffixes = (Byte *)MyAlloc(numItems * sizeof(Byte)); if (!_suffixes) return E_OUTOFMEMORY;
    _stack = (Byte *)MyAlloc(numItems * sizeof(Byte)); if (!_stack) return E_OUTOFMEMORY;
    _numMaxBits = maxbits;
  }

  UInt64 prevPos = 0;
  const UInt32 blockSymbol = ((prop & kBlockModeMask) != 0) ? 256 : ((UInt32)1 << kNumMaxBits);
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
      const UInt64 nowPos = outBuffer.GetProcessedSize();
      if (progress && nowPos - prevPos >= (1 << 13))
      {
        prevPos = nowPos;
        const UInt64 packSize = inBuffer.GetProcessedSize();
        RINOK(progress->SetRatioInfo(&packSize, &nowPos))
      }
    }
    const unsigned bytePos = bitPos >> 3;
    UInt32 symbol = buf[bytePos] | ((UInt32)buf[(size_t)bytePos + 1] << 8) | ((UInt32)buf[(size_t)bytePos + 2] << 16);
    symbol >>= (bitPos & 7);
    symbol &= ((UInt32)1 << numBits) - 1;
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
  // PackSize = inBuffer.GetProcessedSize();
  const HRESULT res2 = outBuffer.Flush();
  return (res == S_OK) ? res2 : res;
 
  }
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
  const Byte prop = data[2];
  if ((prop & 0x60) != 0)
    return false;
  const unsigned maxbits = prop & kNumBitsMask;
  if (maxbits < kNumMinBits || maxbits > kNumMaxBits)
    return false;
  const UInt32 numItems = (UInt32)1 << maxbits;
  const UInt32 blockSymbol = ((prop & kBlockModeMask) != 0) ? 256 : ((UInt32)1 << kNumMaxBits);
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
      const unsigned num = (numBits < size) ? numBits : (unsigned)size;
      memcpy(buf, data, num);
      data += num;
      size -= num;
      numBufBits = num * 8;
      bitPos = 0;
    }
    const unsigned bytePos = bitPos >> 3;
    UInt32 symbol = buf[bytePos] | ((UInt32)buf[bytePos + 1] << 8) | ((UInt32)buf[bytePos + 2] << 16);
    symbol >>= (bitPos & 7);
    symbol &= ((UInt32)1 << numBits) - 1;
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
