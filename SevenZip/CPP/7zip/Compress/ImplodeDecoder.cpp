// ImplodeDecoder.cpp

#include "StdAfx.h"

#include "../../Common/Defs.h"

#include "ImplodeDecoder.h"

namespace NCompress {
namespace NImplode {
namespace NDecoder {

bool CHuffmanDecoder::Build(const Byte *lens, unsigned numSymbols) throw()
{
  unsigned counts[kNumHuffmanBits + 1];
  
  unsigned i;
  for (i = 0; i <= kNumHuffmanBits; i++)
    counts[i] = 0;
  
  unsigned sym;
  for (sym = 0; sym < numSymbols; sym++)
    counts[lens[sym]]++;

  const UInt32 kMaxValue = (UInt32)1 << kNumHuffmanBits;
  
  // _limits[0] = kMaxValue;

  UInt32 startPos = kMaxValue;
  UInt32 sum = 0;

  for (i = 1; i <= kNumHuffmanBits; i++)
  {
    const UInt32 cnt = counts[i];
    const UInt32 range = cnt << (kNumHuffmanBits - i);
    if (startPos < range)
      return false;
    startPos -= range;
    _limits[i] = startPos;
    _poses[i] = sum;
    sum += cnt;
    counts[i] = sum;
  }

  // counts[0] += sum;

  if (startPos != 0)
    return false;

  for (sym = 0; sym < numSymbols; sym++)
  {
    unsigned len = lens[sym];
    if (len != 0)
      _symbols[--counts[len]] = (Byte)sym;
  }

  return true;
}


UInt32 CHuffmanDecoder::Decode(CInBit *inStream) const throw()
{
  UInt32 val = inStream->GetValue(kNumHuffmanBits);
  unsigned numBits;
  for (numBits = 1; val < _limits[numBits]; numBits++);
  UInt32 sym = _symbols[_poses[numBits] + ((val - _limits[numBits]) >> (kNumHuffmanBits - numBits))];
  inStream->MovePos(numBits);
  return sym;
}



static const unsigned kNumLenDirectBits = 8;

static const unsigned kNumDistDirectBitsSmall = 6;
static const unsigned kNumDistDirectBitsBig = 7;

static const unsigned kLitTableSize = (1 << 8);
static const unsigned kDistTableSize = 64;
static const unsigned kLenTableSize = 64;

static const UInt32 kHistorySize = (1 << kNumDistDirectBitsBig) * kDistTableSize; // 8 KB


CCoder::CCoder():
  _flags(0),
  _fullStreamMode(false)
{}


bool CCoder::BuildHuff(CHuffmanDecoder &decoder, unsigned numSymbols)
{
  Byte levels[kMaxHuffTableSize];
  unsigned numRecords = (unsigned)_inBitStream.ReadAlignedByte() + 1;
  unsigned index = 0;
  do
  {
    unsigned b = (unsigned)_inBitStream.ReadAlignedByte();
    Byte level = (Byte)((b & 0xF) + 1);
    unsigned rep = ((unsigned)b >> 4) + 1;
    if (index + rep > numSymbols)
      return false;
    for (unsigned j = 0; j < rep; j++)
      levels[index++] = level;
  }
  while (--numRecords);

  if (index != numSymbols)
    return false;
  return decoder.Build(levels, numSymbols);
}


HRESULT CCoder::CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  if (!_inBitStream.Create(1 << 18))
    return E_OUTOFMEMORY;
  if (!_outWindowStream.Create(kHistorySize << 1)) // 16 KB
    return E_OUTOFMEMORY;
  if (!outSize)
    return E_INVALIDARG;

  _outWindowStream.SetStream(outStream);
  _outWindowStream.Init(false);
  _inBitStream.SetStream(inStream);
  _inBitStream.Init();

  const unsigned numDistDirectBits = (_flags & 2) ?
      kNumDistDirectBitsBig:
      kNumDistDirectBitsSmall;
  const bool literalsOn = ((_flags & 4) != 0);
  const UInt32 minMatchLen = (literalsOn ? 3 : 2);

  if (literalsOn)
    if (!BuildHuff(_litDecoder, kLitTableSize))
      return S_FALSE;
  if (!BuildHuff(_lenDecoder, kLenTableSize))
    return S_FALSE;
  if (!BuildHuff(_distDecoder, kDistTableSize))
    return S_FALSE;
 
  UInt64 prevProgress = 0;
  bool moreOut = false;
  UInt64 pos = 0, unPackSize = *outSize;

  while (pos < unPackSize)
  {
    if (progress && (pos - prevProgress) >= (1 << 18))
    {
      const UInt64 packSize = _inBitStream.GetProcessedSize();
      RINOK(progress->SetRatioInfo(&packSize, &pos));
      prevProgress = pos;
    }

    if (_inBitStream.ReadBits(1) != 0)
    {
      Byte b;
      if (literalsOn)
      {
        UInt32 sym = _litDecoder.Decode(&_inBitStream);
        // if (sym >= kLitTableSize) break;
        b = (Byte)sym;
      }
      else
        b = (Byte)_inBitStream.ReadBits(8);
      _outWindowStream.PutByte(b);
      pos++;
    }
    else
    {
      UInt32 lowDistBits = _inBitStream.ReadBits(numDistDirectBits);
      UInt32 dist = _distDecoder.Decode(&_inBitStream);
      // if (dist >= kDistTableSize) break;
      dist = (dist << numDistDirectBits) + lowDistBits;
      UInt32 len = _lenDecoder.Decode(&_inBitStream);
      // if (len >= kLenTableSize) break;
      if (len == kLenTableSize - 1)
        len += _inBitStream.ReadBits(kNumLenDirectBits);
      len += minMatchLen;

      {
        const UInt64 limit = unPackSize - pos;
        if (len > limit)
        {
          moreOut = true;
          len = (UInt32)limit;
        }
      }

      while (dist >= pos && len != 0)
      {
        _outWindowStream.PutByte(0);
        pos++;
        len--;
      }
      
      if (len != 0)
      {
        _outWindowStream.CopyBlock(dist, len);
        pos += len;
      }
    }
  }

  HRESULT res = _outWindowStream.Flush();

  if (res == S_OK)
  {
    if (_fullStreamMode)
    {
      if (moreOut)
        res = S_FALSE;
      if (inSize && *inSize != _inBitStream.GetProcessedSize())
        res = S_FALSE;
    }
    if (pos != unPackSize)
      res = S_FALSE;
  }

  return res;
}


STDMETHODIMP CCoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  try { return CodeReal(inStream, outStream, inSize, outSize, progress);  }
  // catch(const CInBufferException &e)  { return e.ErrorCode; }
  // catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(const CSystemException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}


STDMETHODIMP CCoder::SetDecoderProperties2(const Byte *data, UInt32 size)
{
  if (size == 0)
    return E_NOTIMPL;
  _flags = data[0];
  return S_OK;
}


STDMETHODIMP CCoder::SetFinishMode(UInt32 finishMode)
{
  _fullStreamMode = (finishMode != 0);
  return S_OK;
}


STDMETHODIMP CCoder::GetInStreamProcessedSize(UInt64 *value)
{
  *value = _inBitStream.GetProcessedSize();
  return S_OK;
}

}}}
