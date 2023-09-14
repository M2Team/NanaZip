// LzmsDecoder.h
// The code is based on LZMS description from wimlib code

#ifndef ZIP7_INC_LZMS_DECODER_H
#define ZIP7_INC_LZMS_DECODER_H

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#define PRF(x) x
#else
// #define PRF(x)
#endif

#include "../../../C/CpuArch.h"
#include "../../../C/HuffEnc.h"

#include "../../Common/MyBuffer.h"
#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "HuffmanDecoder.h"

namespace NCompress {
namespace NLzms {

class CBitDecoder
{
public:
  const Byte *_buf;
  unsigned _bitPos;

  void Init(const Byte *buf, size_t size) throw()
  {
    _buf = buf + size;
    _bitPos = 0;
  }

  UInt32 GetValue(unsigned numBits) const
  {
    UInt32 v = ((UInt32)_buf[-1] << 16) | ((UInt32)_buf[-2] << 8) | (UInt32)_buf[-3];
    v >>= (24 - numBits - _bitPos);
    return v & ((1 << numBits) - 1);
  }
  
  void MovePos(unsigned numBits)
  {
    _bitPos += numBits;
    _buf -= (_bitPos >> 3);
    _bitPos &= 7;
  }

  UInt32 ReadBits32(unsigned numBits)
  {
    UInt32 mask = (((UInt32)1 << numBits) - 1);
    numBits += _bitPos;
    const Byte *buf = _buf;
    UInt32 v = GetUi32(buf - 4);
    if (numBits > 32)
    {
      v <<= (numBits - 32);
      v |= (UInt32)buf[-5] >> (40 - numBits);
    }
    else
      v >>= (32 - numBits);
    _buf = buf - (numBits >> 3);
    _bitPos = numBits & 7;
    return v & mask;
  }
};


const unsigned k_NumLitSyms = 256;
const unsigned k_NumLenSyms = 54;
const unsigned k_NumPosSyms = 799;
const unsigned k_NumPowerSyms = 8;

const unsigned k_NumProbBits = 6;
const unsigned k_ProbLimit = 1 << k_NumProbBits;
const unsigned k_InitialProb = 48;
const UInt32 k_InitialHist = 0x55555555;

const unsigned k_NumReps = 3;

const unsigned k_NumMainProbs  = 16;
const unsigned k_NumMatchProbs = 32;
const unsigned k_NumRepProbs   = 64;

const unsigned k_NumHuffmanBits = 15;

template <UInt32 m_NumSyms, UInt32 m_RebuildFreq, unsigned numTableBits>
class CHuffDecoder: public NCompress::NHuffman::CDecoder<k_NumHuffmanBits, m_NumSyms, numTableBits>
{
public:
  UInt32 RebuildRem;
  UInt32 NumSyms;
  UInt32 Freqs[m_NumSyms];

  void Generate() throw()
  {
    UInt32 vals[m_NumSyms];
    Byte levels[m_NumSyms];

    // We need to check that our algorithm is OK, when optimal Huffman tree uses more than 15 levels !!!
    Huffman_Generate(Freqs, vals, levels, NumSyms, k_NumHuffmanBits);

    /*
    for (UInt32 i = NumSyms; i < m_NumSyms; i++)
      levels[i] = 0;
    */
    this->BuildFull(levels, NumSyms);
  }
  
  void Rebuild() throw()
  {
    Generate();
    RebuildRem = m_RebuildFreq;
    UInt32 num = NumSyms;
    for (UInt32 i = 0; i < num; i++)
      Freqs[i] = (Freqs[i] >> 1) + 1;
  }

public:
  void Init(UInt32 numSyms = m_NumSyms) throw()
  {
    RebuildRem = m_RebuildFreq;
    NumSyms = numSyms;
    for (UInt32 i = 0; i < numSyms; i++)
      Freqs[i] = 1;
    // for (; i < m_NumSyms; i++) Freqs[i] = 0;
    Generate();
  }
};


struct CProbEntry
{
  UInt32 Prob;
  UInt64 Hist;

  void Init()
  {
    Prob = k_InitialProb;
    Hist = k_InitialHist;
  }

  UInt32 GetProb() const throw()
  {
    UInt32 prob = Prob;
    if (prob == 0)
      prob = 1;
    else if (prob == k_ProbLimit)
      prob = k_ProbLimit - 1;
    return prob;
  }

  void Update(unsigned bit) throw()
  {
    Prob += (UInt32)((Int32)(Hist >> (k_ProbLimit - 1)) - (Int32)bit);
    Hist = (Hist << 1) | bit;
  }
};


struct CRangeDecoder
{
  UInt32 range;
  UInt32 code;
  const Byte *cur;
  // const Byte *end;

  void Init(const Byte *data, size_t /* size */) throw()
  {
    range = 0xFFFFFFFF;
    code = (((UInt32)GetUi16(data)) << 16) | GetUi16(data + 2);
    cur = data + 4;
    // end = data + size;
  }

  void Normalize()
  {
    if (range <= 0xFFFF)
    {
      range <<= 16;
      code <<= 16;
      // if (cur >= end) throw 1;
      code |= GetUi16(cur);
      cur += 2;
    }
  }

  unsigned Decode(UInt32 *state, UInt32 numStates, struct CProbEntry *probs)
  {
    UInt32 st = *state;
    CProbEntry *entry = &probs[st];
    st = (st << 1) & (numStates - 1);

    UInt32 prob = entry->GetProb();

    if (range <= 0xFFFF)
    {
      range <<= 16;
      code <<= 16;
      // if (cur >= end) throw 1;
      code |= GetUi16(cur);
      cur += 2;
    }

    UInt32 bound = (range >> k_NumProbBits) * prob;
    
    if (code < bound)
    {
      range = bound;
      *state = st;
      entry->Update(0);
      return 0;
    }
    else
    {
      range -= bound;
      code -= bound;
      *state = st | 1;
      entry->Update(1);
      return 1;
    }
  }
};


class CDecoder
{
  // CRangeDecoder _rc;
  // CBitDecoder _bs;
  size_t _pos;

  UInt32 _reps[k_NumReps + 1];
  UInt64 _deltaReps[k_NumReps + 1];

  UInt32 mainState;
  UInt32 matchState;
  UInt32 lzRepStates[k_NumReps];
  UInt32 deltaRepStates[k_NumReps];

  struct CProbEntry mainProbs[k_NumMainProbs];
  struct CProbEntry matchProbs[k_NumMatchProbs];
  
  struct CProbEntry lzRepProbs[k_NumReps][k_NumRepProbs];
  struct CProbEntry deltaRepProbs[k_NumReps][k_NumRepProbs];

  CHuffDecoder<k_NumLitSyms, 1024, 9> m_LitDecoder;
  CHuffDecoder<k_NumPosSyms, 1024, 9> m_PosDecoder;
  CHuffDecoder<k_NumLenSyms, 512, 8> m_LenDecoder;
  CHuffDecoder<k_NumPowerSyms, 512, 6> m_PowerDecoder;
  CHuffDecoder<k_NumPosSyms, 1024, 9> m_DeltaDecoder;

  Int32 *_x86_history;

  HRESULT CodeReal(const Byte *in, size_t inSize, Byte *out, size_t outSize);
public:
  CDecoder();
  ~CDecoder();

  HRESULT Code(const Byte *in, size_t inSize, Byte *out, size_t outSize);
  size_t GetUnpackSize() const { return _pos; }
};

}}

#endif
