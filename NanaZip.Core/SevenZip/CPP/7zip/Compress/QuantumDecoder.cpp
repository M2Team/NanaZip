// QuantumDecoder.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/Defs.h"

#include "QuantumDecoder.h"

namespace NCompress {
namespace NQuantum {

static const unsigned kNumLenSymbols = 27;
static const unsigned kMatchMinLen = 3;
static const unsigned kNumSimpleLenSlots = 6;

static const unsigned kUpdateStep = 8;
static const unsigned kFreqSumMax = 3800;
static const unsigned kReorderCount_Start = 4;
static const unsigned kReorderCount = 50;


class CRangeDecoder
{
  UInt32 Low;
  UInt32 Range;
  UInt32 Code;

  unsigned _bitOffset;
  const Byte *_buf;
  const Byte *_bufLim;

public:

  Z7_FORCE_INLINE
  void Init(const Byte *inData, size_t inSize)
  {
    Code = ((UInt32)*inData << 8) | inData[1];
    _buf = inData + 2;
    _bufLim = inData + inSize;
    _bitOffset = 0;
    Low = 0;
    Range = 0x10000;
  }

  Z7_FORCE_INLINE
  bool WasExtraRead() const
  {
    return _buf > _bufLim;
  }

  Z7_FORCE_INLINE
  UInt32 ReadBits(unsigned numBits) // numBits > 0
  {
    unsigned bitOffset = _bitOffset;
    const Byte *buf = _buf;
    const UInt32 res = GetBe32(buf) << bitOffset;
    bitOffset += numBits;
    _buf = buf + (bitOffset >> 3);
    _bitOffset = bitOffset & 7;
    return res >> (32 - numBits);
  }

  // ---------- Range Decoder functions ----------

  Z7_FORCE_INLINE
  bool Finish()
  {
    const unsigned numBits = 2 + ((16 - 2 - _bitOffset) & 7);
    if (ReadBits(numBits) != 0)
      return false;
    return _buf == _bufLim;
  }

  Z7_FORCE_INLINE
  UInt32 GetThreshold(UInt32 total) const
  {
    return ((Code + 1) * total - 1) / Range; // & 0xFFFF is not required;
  }
  
  Z7_FORCE_INLINE
  void Decode(UInt32 start, UInt32 end, UInt32 total)
  {
    // UInt32 hi = ~(Low + end * Range / total - 1);
    UInt32 hi = 0 - (Low + end * Range / total);
    const UInt32 offset = start * Range / total;
    UInt32 lo = Low + offset;
    Code -= offset;
    UInt32 numBits = 0;
    lo ^= hi;
    while (lo & (1u << 15))
    {
      lo <<= 1;
      hi <<= 1;
      numBits++;
    }
    lo ^= hi;
    UInt32 an = lo & hi;
    while (an & (1u << 14))
    {
      an <<= 1;
      lo <<= 1;
      hi <<= 1;
      numBits++;
    }
    Low = lo;
    Range = ((~hi - lo) & 0xffff) + 1;
    if (numBits)
      Code = (Code << numBits) + ReadBits(numBits);
  }
};


// Z7_FORCE_INLINE
Z7_NO_INLINE
unsigned CModelDecoder::Decode(CRangeDecoder *rc)
// Z7_NO_INLINE void CModelDecoder::Normalize()
{
  if (Freqs[0] > kFreqSumMax)
  {
    if (--ReorderCount == 0)
    {
      ReorderCount = kReorderCount;
      {
        unsigned i = NumItems;
        unsigned next = 0;
        UInt16 *freqs = &Freqs[i];
        do
        {
          const unsigned freq = *--freqs;
          *freqs = (UInt16)((freq - next + 1) >> 1);
          next = freq;
        }
        while (--i);
      }
      {
        for (unsigned i = 0; i < NumItems - 1; i++)
        {
          UInt16 freq = Freqs[i];
          for (unsigned k = i + 1; k < NumItems; k++)
            if (freq < Freqs[k])
            {
              const UInt16 freq2 = Freqs[k];
              Freqs[k] = freq;
              Freqs[i] = freq2;
              freq = freq2;
              const Byte val = Vals[i];
              Vals[i] = Vals[k];
              Vals[k] = val;
            }
        }
      }
      unsigned i = NumItems;
      unsigned freq = 0;
      UInt16 *freqs = &Freqs[i];
      do
      {
        freq += *--freqs;
        *freqs = (UInt16)freq;
      }
      while (--i);
    }
    else
    {
      unsigned i = NumItems;
      unsigned next = 1;
      UInt16 *freqs = &Freqs[i];
      do
      {
        unsigned freq = *--freqs >> 1;
        if (freq < next)
          freq = next;
        *freqs = (UInt16)freq;
        next = freq + 1;
      }
      while (--i);
    }
  }
  unsigned res;
  {
    const unsigned freq0 = Freqs[0];
    Freqs[0] = (UInt16)(freq0 + kUpdateStep);
    const unsigned threshold = rc->GetThreshold(freq0);
    UInt16 *freqs = &Freqs[1];
    unsigned freq = *freqs;
    while (freq > threshold)
    {
      *freqs++ = (UInt16)(freq + kUpdateStep);
      freq = *freqs;
    }
    res = Vals[freqs - Freqs - 1];
    rc->Decode(freq, freqs[-1] - kUpdateStep, freq0);
  }
  return res;
}


Z7_NO_INLINE
void CModelDecoder::Init(unsigned numItems, unsigned startVal)
{
  NumItems = numItems;
  ReorderCount = kReorderCount_Start;
  UInt16 *freqs = Freqs;
  freqs[numItems] = 0;
  Byte *vals = Vals;
  do
  {
    *freqs++ = (UInt16)numItems;
    *vals++ = (Byte)startVal;
    startVal++;
  }
  while (--numItems);
}


HRESULT CDecoder::Code(const Byte *inData, size_t inSize, UInt32 outSize, bool keepHistory)
{
  if (inSize < 2)
    return S_FALSE;
  if (!keepHistory)
  {
    _winPos = 0;
    m_Selector.Init(kNumSelectors, 0);
    unsigned i;
    for (i = 0; i < kNumLitSelectors; i++)
      m_Literals[i].Init(kNumLitSymbols, i * kNumLitSymbols);
    const unsigned numItems = (_numDictBits == 0 ? 1 : (_numDictBits << 1));
    // const unsigned kNumPosSymbolsMax[kNumMatchSelectors] = { 24, 36, 42 };
    for (i = 0; i < kNumMatchSelectors; i++)
    {
      const unsigned num = 24 + i * 6 + ((i + 1) & 2) * 3;
      m_PosSlot[i].Init(MyMin(numItems, num), 0);
    }
    m_LenSlot.Init(kNumLenSymbols, kMatchMinLen + kNumMatchSelectors - 1);
  }

  CRangeDecoder rc;
  rc.Init(inData, inSize);
  const UInt32 winSize = _winSize;
  Byte *pos;
  {
    UInt32 winPos = _winPos;
    if (winPos == winSize)
    {
      winPos = 0;
      _winPos = winPos;
      _overWin = true;
    }
    if (outSize > winSize - winPos)
      return S_FALSE;
    pos = _win + winPos;
  }

  while (outSize != 0)
  {
    if (rc.WasExtraRead())
      return S_FALSE;

    const unsigned selector = m_Selector.Decode(&rc);
    
    if (selector < kNumLitSelectors)
    {
      const unsigned b = m_Literals[selector].Decode(&rc);
      *pos++ = (Byte)b;
      --outSize;
      // if (--outSize == 0) break;
    }
    else
    {
      unsigned len = selector - kNumLitSelectors + kMatchMinLen;
    
      if (selector == kNumLitSelectors + kNumMatchSelectors - 1)
      {
        len = m_LenSlot.Decode(&rc);
        if (len >= kNumSimpleLenSlots + kMatchMinLen + kNumMatchSelectors - 1)
        {
          len -= kNumSimpleLenSlots - 4 + kMatchMinLen + kNumMatchSelectors - 1;
          const unsigned numDirectBits = (unsigned)(len >> 2);
          len = ((4 | (len & 3)) << numDirectBits) - (4 << 1)
              + kNumSimpleLenSlots
              + kMatchMinLen + kNumMatchSelectors - 1;
          if (numDirectBits < 6)
            len += rc.ReadBits(numDirectBits);
        }
      }
      
      UInt32 dist = m_PosSlot[(size_t)selector - kNumLitSelectors].Decode(&rc);
      
      if (dist >= 4)
      {
        const unsigned numDirectBits = (unsigned)((dist >> 1) - 1);
        dist = ((2 | (dist & 1)) << numDirectBits) + rc.ReadBits(numDirectBits);
      }
      
      if ((Int32)(outSize -= len) < 0)
        return S_FALSE;

      ptrdiff_t srcPos = (ptrdiff_t)(Int32)((pos - _win) - (ptrdiff_t)dist - 1);
      if (srcPos < 0)
      {
        if (!_overWin)
          return S_FALSE;
        UInt32 rem = (UInt32)-srcPos;
        srcPos += winSize;
        if (rem < len)
        {
          const Byte *src = _win + srcPos;
          len -= rem;
          do
            *pos++ = *src++;
          while (--rem);
          srcPos = 0;
        }
      }
      const Byte *src = _win + srcPos;
      do
        *pos++ = *src++;
      while (--len);
      // if (outSize == 0) break;
    }
  }

  _winPos = (UInt32)(size_t)(pos - _win);
  return rc.Finish() ? S_OK : S_FALSE;
}


HRESULT CDecoder::SetParams(unsigned numDictBits)
{
  if (numDictBits > 21)
    return E_INVALIDARG;
  _numDictBits = numDictBits;
  _winPos = 0;
  _overWin = false;

  if (numDictBits < 15)
      numDictBits = 15;
  _winSize = (UInt32)1 << numDictBits;
  if (!_win || _winSize > _winSize_allocated)
  {
    MidFree(_win);
    _win = NULL;
    _win = (Byte *)MidAlloc(_winSize);
    if (!_win)
      return E_OUTOFMEMORY;
    _winSize_allocated = _winSize;
  }
  return S_OK;
}

}}
