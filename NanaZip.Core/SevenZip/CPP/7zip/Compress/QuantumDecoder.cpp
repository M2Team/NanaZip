// QuantumDecoder.cpp

#include "StdAfx.h"

#include "../../Common/Defs.h"

#include "QuantumDecoder.h"

namespace NCompress {
namespace NQuantum {

static const unsigned kNumLenSymbols = 27;
static const unsigned kMatchMinLen = 3;
static const unsigned kNumSimplePosSlots = 4;
static const unsigned kNumSimpleLenSlots = 6;

static const UInt16 kUpdateStep = 8;
static const UInt16 kFreqSumMax = 3800;
static const unsigned kReorderCountStart = 4;
static const unsigned kReorderCount = 50;

void CModelDecoder::Init(unsigned numItems)
{
  NumItems = numItems;
  ReorderCount = kReorderCountStart;
  for (unsigned i = 0; i < numItems; i++)
  {
    Freqs[i] = (UInt16)(numItems - i);
    Vals[i] = (Byte)i;
  }
  Freqs[numItems] = 0;
}

unsigned CModelDecoder::Decode(CRangeDecoder *rc)
{
  const UInt32 threshold = rc->GetThreshold(Freqs[0]);
  unsigned i;
  for (i = 1; Freqs[i] > threshold; i++);
  
  rc->Decode(Freqs[i], Freqs[(size_t)i - 1], Freqs[0]);
  const unsigned res = Vals[--i];
  
  do
    Freqs[i] = (UInt16)(Freqs[i] + kUpdateStep);
  while (i--);
  
  if (Freqs[0] > kFreqSumMax)
  {
    if (--ReorderCount == 0)
    {
      ReorderCount = kReorderCount;
      for (i = 0; i < NumItems; i++)
        Freqs[i] = (UInt16)(((Freqs[i] - Freqs[(size_t)i + 1]) + 1) >> 1);
      for (i = 0; i < NumItems - 1; i++)
        for (unsigned j = i + 1; j < NumItems; j++)
          if (Freqs[i] < Freqs[j])
          {
            const UInt16 tmpFreq = Freqs[i];
            const Byte tmpVal = Vals[i];
            Freqs[i] = Freqs[j];
            Vals[i] = Vals[j];
            Freqs[j] = tmpFreq;
            Vals[j] = tmpVal;
          }
      
      do
        Freqs[i] = (UInt16)(Freqs[i] + Freqs[(size_t)i + 1]);
      while (i--);
    }
    else
    {
      i = NumItems - 1;
      do
      {
        Freqs[i] = (UInt16)(Freqs[i] >> 1);
        if (Freqs[i] <= Freqs[(size_t)i + 1])
          Freqs[i] = (UInt16)(Freqs[(size_t)i + 1] + 1);
      }
      while (i--);
    }
  }
  
  return res;
}


void CDecoder::Init()
{
  m_Selector.Init(kNumSelectors);
  unsigned i;
  for (i = 0; i < kNumLitSelectors; i++)
    m_Literals[i].Init(kNumLitSymbols);
  const unsigned numItems = (_numDictBits == 0 ? 1 : (_numDictBits << 1));
  const unsigned kNumPosSymbolsMax[kNumMatchSelectors] = { 24, 36, 42 };
  for (i = 0; i < kNumMatchSelectors; i++)
    m_PosSlot[i].Init(MyMin(numItems, kNumPosSymbolsMax[i]));
  m_LenSlot.Init(kNumLenSymbols);
}


HRESULT CDecoder::CodeSpec(const Byte *inData, size_t inSize, UInt32 outSize)
{
  if (inSize < 2)
    return S_FALSE;

  CRangeDecoder rc;
  rc.Stream.SetStreamAndInit(inData, inSize);
  rc.Init();

  while (outSize != 0)
  {
    if (rc.Stream.WasExtraRead())
      return S_FALSE;

    unsigned selector = m_Selector.Decode(&rc);
    
    if (selector < kNumLitSelectors)
    {
      const Byte b = (Byte)((selector << (8 - kNumLitSelectorBits)) + m_Literals[selector].Decode(&rc));
      _outWindow.PutByte(b);
      outSize--;
    }
    else
    {
      selector -= kNumLitSelectors;
      unsigned len = selector + kMatchMinLen;
    
      if (selector == 2)
      {
        unsigned lenSlot = m_LenSlot.Decode(&rc);
        if (lenSlot >= kNumSimpleLenSlots)
        {
          lenSlot -= 2;
          const unsigned numDirectBits = (unsigned)(lenSlot >> 2);
          len += ((4 | (lenSlot & 3)) << numDirectBits) - 2;
          if (numDirectBits < 6)
            len += rc.Stream.ReadBits(numDirectBits);
        }
        else
          len += lenSlot;
      }
      
      UInt32 dist = m_PosSlot[selector].Decode(&rc);
      
      if (dist >= kNumSimplePosSlots)
      {
        unsigned numDirectBits = (unsigned)((dist >> 1) - 1);
        dist = ((2 | (dist & 1)) << numDirectBits) + rc.Stream.ReadBits(numDirectBits);
      }
      
      unsigned locLen = len;
      if (len > outSize)
        locLen = (unsigned)outSize;
      if (!_outWindow.CopyBlock(dist, locLen))
        return S_FALSE;
      outSize -= locLen;
      len -= locLen;
      if (len != 0)
        return S_FALSE;
    }
  }

  return rc.Finish() ? S_OK : S_FALSE;
}

HRESULT CDecoder::Code(const Byte *inData, size_t inSize,
      ISequentialOutStream *outStream, UInt32 outSize,
      bool keepHistory)
{
  try
  {
    _outWindow.SetStream(outStream);
    _outWindow.Init(keepHistory);
    if (!keepHistory)
      Init();
    
    const HRESULT res = CodeSpec(inData, inSize, outSize);
    const HRESULT res2 = _outWindow.Flush();
    return res != S_OK ? res : res2;
  }
  catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}

HRESULT CDecoder::SetParams(unsigned numDictBits)
{
  if (numDictBits > 21)
    return E_INVALIDARG;
  _numDictBits = numDictBits;
  if (!_outWindow.Create((UInt32)1 << _numDictBits))
    return E_OUTOFMEMORY;
  return S_OK;
}

}}
