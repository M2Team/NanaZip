// LzhDecoder.cpp

#include "StdAfx.h"

#include "LzhDecoder.h"

namespace NCompress{
namespace NLzh {
namespace NDecoder {

static const UInt32 kWindowSizeMin = 1 << 16;

bool CCoder::ReadTP(unsigned num, unsigned numBits, int spec)
{
  _symbolT = -1;

  const unsigned n = (unsigned)_inBitStream.ReadBits(numBits);
  if (n == 0)
  {
    const unsigned s = (unsigned)_inBitStream.ReadBits(numBits);
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
      unsigned val = (unsigned)_inBitStream.GetValue(16);
      unsigned c = val >> 13;
      unsigned mov = 3;
      if (c == 7)
      {
        while (val & (1 << 12))
        {
          val += val;
          c++;
        }
        if (c > 16)
          return false;
        mov = c - 3;
      }
      lens[i++] = (Byte)c;
      _inBitStream.MovePos(mov);
      if ((int)i == spec)
        i += _inBitStream.ReadBits(2);
    }
    while (i < n);
    
    return _decoderT.Build(lens, NHuffman::k_BuildMode_Full);
  }
}

static const unsigned NUM_C_BITS = 9;

bool CCoder::ReadC()
{
  _symbolC = -1;

  const unsigned n = (unsigned)_inBitStream.ReadBits(NUM_C_BITS);
  if (n == 0)
  {
    const unsigned s = (unsigned)_inBitStream.ReadBits(NUM_C_BITS);
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
      unsigned c = (unsigned)_symbolT;
      if (_symbolT < 0)
        c = _decoderT.DecodeFull(&_inBitStream);
      
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
    
    while (i < NC) lens[i++] = 0;
    return _decoderC.Build(lens, /* n, */ NHuffman::k_BuildMode_Full);
  }
}

HRESULT CCoder::CodeReal(UInt32 rem, ICompressProgressInfo *progress)
{
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
      const unsigned pbit = (DictSize <= (1 << 14) ? 4 : 5);
      if (!ReadTP(NP, pbit, -1))
        return S_FALSE;
    }
  
    blockSize--;

    unsigned number = (unsigned)_symbolC;
    if (_symbolC < 0)
      number = _decoderC.DecodeFull(&_inBitStream);

    if (number < 256)
    {
      _outWindow.PutByte((Byte)number);
      rem--;
    }
    else
    {
      const unsigned len = number - 256 + kMatchMinLen;

      UInt32 dist = (UInt32)(unsigned)_symbolT;
      if (_symbolT < 0)
        dist = (UInt32)_decoderT.DecodeFull(&_inBitStream);
      
      if (dist > 1)
      {
        dist--;
        dist = ((UInt32)1 << dist) + _inBitStream.ReadBits((unsigned)dist);
      }
      
      if (dist >= DictSize)
        return S_FALSE;

      if (len > rem)
      {
        // if (FinishMode)
        return S_FALSE;
        // len = (unsigned)rem;
      }

      if (!_outWindow.CopyBlock(dist, len))
        return S_FALSE;
      rem -= len;
    }
  }

  // if (FinishMode)
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


HRESULT CCoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt32 outSize, ICompressProgressInfo *progress)
{
  try
  {
    if (!_outWindow.Create(DictSize > kWindowSizeMin ? DictSize : kWindowSizeMin))
      return E_OUTOFMEMORY;
    if (!_inBitStream.Create(1 << 17))
      return E_OUTOFMEMORY;
    _outWindow.SetStream(outStream);
    _outWindow.Init(false);
    _inBitStream.SetStream(inStream);
    _inBitStream.Init();
    {
      CCoderReleaser coderReleaser(this);
      RINOK(CodeReal(outSize, progress))
      coderReleaser.Disable();
    }
    return _outWindow.Flush();
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}

}}}
