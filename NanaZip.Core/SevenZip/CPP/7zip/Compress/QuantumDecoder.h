// QuantumDecoder.h

#ifndef ZIP7_INC_COMPRESS_QUANTUM_DECODER_H
#define ZIP7_INC_COMPRESS_QUANTUM_DECODER_H

#include "../../Common/MyCom.h"

#include "LzOutWindow.h"

namespace NCompress {
namespace NQuantum {

class CBitDecoder
{
  UInt32 Value;
  bool _extra;
  const Byte *_buf;
  const Byte *_bufLim;
public:
  void SetStreamAndInit(const Byte *inData, size_t inSize)
  {
    _buf = inData;
    _bufLim = inData + inSize;
    Value = 0x10000;
    _extra = false;
  }

  bool WasExtraRead() const { return _extra; }

  bool WasFinishedOK() const
  {
    return !_extra && _buf == _bufLim;
  }
  
  UInt32 ReadBit()
  {
    if (Value >= 0x10000)
    {
      Byte b;
      if (_buf >= _bufLim)
      {
        b = 0xFF;
        _extra = true;
      }
      else
        b = *_buf++;
      Value = 0x100 | b;
    }
    UInt32 res = (Value >> 7) & 1;
    Value <<= 1;
    return res;
  }

  UInt32 ReadStart16Bits()
  {
    // we use check for extra read in another code.
    UInt32 val = ((UInt32)*_buf << 8) | _buf[1];
    _buf += 2;
    return val;
  }

  UInt32 ReadBits(unsigned numBits) // numBits > 0
  {
    UInt32 res = 0;
    do
      res = (res << 1) | ReadBit();
    while (--numBits);
    return res;
  }
};


class CRangeDecoder
{
  UInt32 Low;
  UInt32 Range;
  UInt32 Code;
public:
  CBitDecoder Stream;

  void Init()
  {
    Low = 0;
    Range = 0x10000;
    Code = Stream.ReadStart16Bits();
  }

  bool Finish()
  {
    // do all streams use these two bits at end?
    if (Stream.ReadBit() != 0) return false;
    if (Stream.ReadBit() != 0) return false;
    return Stream.WasFinishedOK();
  }

  UInt32 GetThreshold(UInt32 total) const
  {
    return ((Code + 1) * total - 1) / Range; // & 0xFFFF is not required;
  }

  void Decode(UInt32 start, UInt32 end, UInt32 total)
  {
    UInt32 high = Low + end * Range / total - 1;
    UInt32 offset = start * Range / total;
    Code -= offset;
    Low += offset;
    for (;;)
    {
      if ((Low & 0x8000) != (high & 0x8000))
      {
        if ((Low & 0x4000) == 0 || (high & 0x4000) != 0)
          break;
        Low &= 0x3FFF;
        high |= 0x4000;
      }
      Low = (Low << 1) & 0xFFFF;
      high = ((high << 1) | 1) & 0xFFFF;
      Code = ((Code << 1) | Stream.ReadBit());
    }
    Range = high - Low + 1;
  }
};


const unsigned kNumLitSelectorBits = 2;
const unsigned kNumLitSelectors = (1 << kNumLitSelectorBits);
const unsigned kNumLitSymbols = 1 << (8 - kNumLitSelectorBits);
const unsigned kNumMatchSelectors = 3;
const unsigned kNumSelectors = kNumLitSelectors + kNumMatchSelectors;
const unsigned kNumSymbolsMax = kNumLitSymbols; // 64


class CModelDecoder
{
  unsigned NumItems;
  unsigned ReorderCount;
  UInt16 Freqs[kNumSymbolsMax + 1];
  Byte Vals[kNumSymbolsMax];
public:
  void Init(unsigned numItems);
  unsigned Decode(CRangeDecoder *rc);
};


Z7_CLASS_IMP_COM_0(
  CDecoder
)
  CLzOutWindow _outWindow;
  unsigned _numDictBits;

  CModelDecoder m_Selector;
  CModelDecoder m_Literals[kNumLitSelectors];
  CModelDecoder m_PosSlot[kNumMatchSelectors];
  CModelDecoder m_LenSlot;

  void Init();
  HRESULT CodeSpec(const Byte *inData, size_t inSize, UInt32 outSize);
public:
  HRESULT Code(const Byte *inData, size_t inSize,
      ISequentialOutStream *outStream, UInt32 outSize,
      bool keepHistory);
  HRESULT SetParams(unsigned numDictBits);

  CDecoder(): _numDictBits(0) {}
};

}}

#endif
