// LzhDecoder.cpp

#include "StdAfx.h"

#include "LzhDecoder.h"

namespace NCompress{
namespace NLzh {
namespace NDecoder {

static const UInt32 kWindowSizeMin = 1 << 16;

static bool CheckCodeLens(const Byte *lens, unsigned num)
{
  UInt32 sum = 0;
  for (unsigned i = 0; i < num; i++)
  {
    const unsigned len = lens[i];
    if (len != 0)
      sum += ((UInt32)1 << (NUM_CODE_BITS - len));
  }
  return sum == ((UInt32)1 << NUM_CODE_BITS);
}

bool CCoder::ReadTP(unsigned num, unsigned numBits, int spec)
{
  _symbolT = -1;

  const UInt32 n = _inBitStream.ReadBits(numBits);
  if (n == 0)
  {
    const unsigned s = _inBitStream.ReadBits(numBits);
    _symbolT = (int)s;
    return (s < num);
  }

  if (n > num)
    return false;

  {
    Byte lens[NPT];
    unsigned i;
    for (i = 0; i < NPT; i++)
      lens[i] = 0;

    i = 0;
    
    do
    {
      const UInt32 val = _inBitStream.GetValue(16);
      unsigned c = val >> 13;
      
      if (c == 7)
      {
        UInt32 mask = 1 << 12;
        while (mask & val)
        {
          mask >>= 1;
          c++;
        }
        if (c > 16)
          return false;
      }
      
      _inBitStream.MovePos(c < 7 ? 3 : c - 3);
      lens[i++] = (Byte)c;
      
      if (i == (unsigned)spec)
        i += _inBitStream.ReadBits(2);
    }
    while (i < n);
    
    if (!CheckCodeLens(lens, NPT))
      return false;
    return _decoderT.Build(lens);
  }
}

static const unsigned NUM_C_BITS = 9;

bool CCoder::ReadC()
{
  _symbolC = -1;

  unsigned n = _inBitStream.ReadBits(NUM_C_BITS);
  
  if (n == 0)
  {
    const unsigned s = _inBitStream.ReadBits(NUM_C_BITS);
    _symbolC = (int)s;
    return (s < NC);
  }

  if (n > NC)
    return false;

  {
    Byte lens[NC];

    unsigned i = 0;
  
    do
    {
      UInt32 c = (unsigned)_symbolT;
      if (_symbolT < 0)
        c = _decoderT.Decode(&_inBitStream);
      
      if (c <= 2)
      {
        if (c == 0)
          c = 1;
        else if (c == 1)
          c = _inBitStream.ReadBits(4) + 3;
        else
          c = _inBitStream.ReadBits(NUM_C_BITS) + 20;
    
        if (i + c > n)
          return false;
        
        do
          lens[i++] = 0;
        while (--c);
      }
      else
        lens[i++] = (Byte)(c - 2);
    }
    while (i < n);
    
    while (i < NC)
      lens[i++] = 0;
    
    if (!CheckCodeLens(lens, NC))
      return false;
    return _decoderC.Build(lens);
  }
}

HRESULT CCoder::CodeReal(UInt64 rem, ICompressProgressInfo *progress)
{
  const unsigned pbit = (DictSize <= (1 << 14) ? 4 : 5);

  UInt32 blockSize = 0;

  while (rem != 0)
  {
    if (blockSize == 0)
    {
      if (_inBitStream.ExtraBitsWereRead())
        return S_FALSE;

      if (progress)
      {
        const UInt64 packSize = _inBitStream.GetProcessedSize();
        const UInt64 pos = _outWindow.GetProcessedSize();
        RINOK(progress->SetRatioInfo(&packSize, &pos))
      }
      
      blockSize = _inBitStream.ReadBits(16);
      if (blockSize == 0)
        return S_FALSE;
      
      if (!ReadTP(NT, 5, 3))
        return S_FALSE;
      if (!ReadC())
        return S_FALSE;
      if (!ReadTP(NP, pbit, -1))
        return S_FALSE;
    }
  
    blockSize--;

    UInt32 number = (unsigned)_symbolC;
    if (_symbolC < 0)
      number = _decoderC.Decode(&_inBitStream);

    if (number < 256)
    {
      _outWindow.PutByte((Byte)number);
      rem--;
    }
    else
    {
      UInt32 len = number - 256 + kMatchMinLen;

      UInt32 dist = (unsigned)_symbolT;
      if (_symbolT < 0)
        dist = _decoderT.Decode(&_inBitStream);
      
      if (dist > 1)
      {
        dist--;
        dist = ((UInt32)1 << dist) + _inBitStream.ReadBits((unsigned)dist);
      }
      
      if (dist >= DictSize)
        return S_FALSE;

      if (len > rem)
        len = (UInt32)rem;

      if (!_outWindow.CopyBlock(dist, len))
        return S_FALSE;
      rem -= len;
    }
  }

  if (FinishMode)
  {
    if (blockSize != 0)
      return S_FALSE;
    if (_inBitStream.ReadAlignBits() != 0)
      return S_FALSE;
  }

  if (_inBitStream.ExtraBitsWereRead())
    return S_FALSE;

  return S_OK;
}


Z7_COM7F_IMF(CCoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  try
  {
    if (!outSize)
      return E_INVALIDARG;
    
    if (!_outWindow.Create(DictSize > kWindowSizeMin ? DictSize : kWindowSizeMin))
      return E_OUTOFMEMORY;
    if (!_inBitStream.Create(1 << 17))
      return E_OUTOFMEMORY;
    
    _outWindow.SetStream(outStream);
    _outWindow.Init(false);
    _inBitStream.SetStream(inStream);
    _inBitStream.Init();
    
    CCoderReleaser coderReleaser(this);
    
    RINOK(CodeReal(*outSize, progress))

    coderReleaser.Disable();
    return _outWindow.Flush();
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}

}}}
