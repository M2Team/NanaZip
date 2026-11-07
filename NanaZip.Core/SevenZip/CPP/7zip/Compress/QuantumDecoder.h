// QuantumDecoder.h

#ifndef ZIP7_INC_COMPRESS_QUANTUM_DECODER_H
#define ZIP7_INC_COMPRESS_QUANTUM_DECODER_H

#include "../../../C/Alloc.h"

#include "../../Common/MyTypes.h"

namespace NCompress {
namespace NQuantum {

const unsigned kNumLitSelectorBits = 2;
const unsigned kNumLitSelectors = 1 << kNumLitSelectorBits;
const unsigned kNumLitSymbols = 1 << (8 - kNumLitSelectorBits);
const unsigned kNumMatchSelectors = 3;
const unsigned kNumSelectors = kNumLitSelectors + kNumMatchSelectors;
const unsigned kNumSymbolsMax = kNumLitSymbols; // 64

class CRangeDecoder;

class CModelDecoder
{
  unsigned NumItems;
  unsigned ReorderCount;
  Byte Vals[kNumSymbolsMax];
  UInt16 Freqs[kNumSymbolsMax + 1];
public:
  Byte _pad[64 - 10]; // for structure size alignment

  void Init(unsigned numItems, unsigned startVal) throw();
  unsigned Decode(CRangeDecoder *rc) throw();
};


class CDecoder
{
  UInt32 _winSize;
  UInt32 _winPos;
  UInt32 _winSize_allocated;
  bool _overWin;
  Byte *_win;

  CModelDecoder m_Selector;
  CModelDecoder m_Literals[kNumLitSelectors];
  CModelDecoder m_PosSlot[kNumMatchSelectors];
  CModelDecoder m_LenSlot;

public:
  // outSize <= (1 << 15)
  // if (outSize < (1 << 15)) then it's last chunk of solid sequence.
  HRESULT CodeChunk_32KB_ExceedRead_Solid(const Byte *inData, size_t inSize, UInt32 outSize) throw();
  HRESULT SetParams_and_Init(unsigned numDictBits) throw();

  CDecoder(): _win(NULL) {}
  ~CDecoder() { MidFree(_win); }

  const Byte * GetDataPtr() const { return _win + _winPos; }
};

}}

#endif
