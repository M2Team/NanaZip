// Compress/HuffmanDecoder.h

#ifndef __COMPRESS_HUFFMAN_DECODER_H
#define __COMPRESS_HUFFMAN_DECODER_H

#include "../../Common/MyTypes.h"

namespace NCompress {
namespace NHuffman {

const unsigned kNumPairLenBits = 4;
const unsigned kPairLenMask = (1 << kNumPairLenBits) - 1;

template <unsigned kNumBitsMax, UInt32 m_NumSymbols, unsigned kNumTableBits = 9>
class CDecoder
{
public:
  UInt32 _limits[kNumBitsMax + 2];
  UInt32 _poses[kNumBitsMax + 1];
  UInt16 _lens[1 << kNumTableBits];
  UInt16 _symbols[m_NumSymbols];

  bool Build(const Byte *lens) throw()
  {
    UInt32 counts[kNumBitsMax + 1];
    
    unsigned i;
    for (i = 0; i <= kNumBitsMax; i++)
      counts[i] = 0;
    
    UInt32 sym;
    
    for (sym = 0; sym < m_NumSymbols; sym++)
      counts[lens[sym]]++;
    
    const UInt32 kMaxValue = (UInt32)1 << kNumBitsMax;

    _limits[0] = 0;
    
    UInt32 startPos = 0;
    UInt32 sum = 0;
    
    for (i = 1; i <= kNumBitsMax; i++)
    {
      const UInt32 cnt = counts[i];
      startPos += cnt << (kNumBitsMax - i);
      if (startPos > kMaxValue)
        return false;
      _limits[i] = startPos;
      counts[i] = sum;
      _poses[i] = sum;
      sum += cnt;
    }

    counts[0] = sum;
    _poses[0] = sum;
    _limits[kNumBitsMax + 1] = kMaxValue;
    
    for (sym = 0; sym < m_NumSymbols; sym++)
    {
      unsigned len = lens[sym];
      if (len == 0)
        continue;

      unsigned offset = counts[len]++;
      _symbols[offset] = (UInt16)sym;

      if (len <= kNumTableBits)
      {
        offset -= _poses[len];
        UInt32 num = (UInt32)1 << (kNumTableBits - len);
        UInt16 val = (UInt16)((sym << kNumPairLenBits) | len);
        UInt16 *dest = _lens + (_limits[(size_t)len - 1] >> (kNumBitsMax - kNumTableBits)) + (offset << (kNumTableBits - len));
        for (UInt32 k = 0; k < num; k++)
          dest[k] = val;
      }
    }
    
    return true;
  }


  bool BuildFull(const Byte *lens, UInt32 numSymbols = m_NumSymbols) throw()
  {
    UInt32 counts[kNumBitsMax + 1];
    
    unsigned i;
    for (i = 0; i <= kNumBitsMax; i++)
      counts[i] = 0;
    
    UInt32 sym;
    
    for (sym = 0; sym < numSymbols; sym++)
      counts[lens[sym]]++;
    
    const UInt32 kMaxValue = (UInt32)1 << kNumBitsMax;

    _limits[0] = 0;
    
    UInt32 startPos = 0;
    UInt32 sum = 0;
    
    for (i = 1; i <= kNumBitsMax; i++)
    {
      const UInt32 cnt = counts[i];
      startPos += cnt << (kNumBitsMax - i);
      if (startPos > kMaxValue)
        return false;
      _limits[i] = startPos;
      counts[i] = sum;
      _poses[i] = sum;
      sum += cnt;
    }

    counts[0] = sum;
    _poses[0] = sum;
    _limits[kNumBitsMax + 1] = kMaxValue;
    
    for (sym = 0; sym < numSymbols; sym++)
    {
      unsigned len = lens[sym];
      if (len == 0)
        continue;

      unsigned offset = counts[len]++;
      _symbols[offset] = (UInt16)sym;

      if (len <= kNumTableBits)
      {
        offset -= _poses[len];
        UInt32 num = (UInt32)1 << (kNumTableBits - len);
        UInt16 val = (UInt16)((sym << kNumPairLenBits) | len);
        UInt16 *dest = _lens + (_limits[(size_t)len - 1] >> (kNumBitsMax - kNumTableBits)) + (offset << (kNumTableBits - len));
        for (UInt32 k = 0; k < num; k++)
          dest[k] = val;
      }
    }
    
    return startPos == kMaxValue;
  }

  
  template <class TBitDecoder>
  MY_FORCE_INLINE
  UInt32 Decode(TBitDecoder *bitStream) const
  {
    UInt32 val = bitStream->GetValue(kNumBitsMax);
    
    if (val < _limits[kNumTableBits])
    {
      UInt32 pair = _lens[val >> (kNumBitsMax - kNumTableBits)];
      bitStream->MovePos((unsigned)(pair & kPairLenMask));
      return pair >> kNumPairLenBits;
    }

    unsigned numBits;
    for (numBits = kNumTableBits + 1; val >= _limits[numBits]; numBits++);
    
    if (numBits > kNumBitsMax)
      return 0xFFFFFFFF;

    bitStream->MovePos(numBits);
    UInt32 index = _poses[numBits] + ((val - _limits[(size_t)numBits - 1]) >> (kNumBitsMax - numBits));
    return _symbols[index];
  }

  
  template <class TBitDecoder>
  MY_FORCE_INLINE
  UInt32 DecodeFull(TBitDecoder *bitStream) const
  {
    UInt32 val = bitStream->GetValue(kNumBitsMax);
    
    if (val < _limits[kNumTableBits])
    {
      UInt32 pair = _lens[val >> (kNumBitsMax - kNumTableBits)];
      bitStream->MovePos((unsigned)(pair & kPairLenMask));
      return pair >> kNumPairLenBits;
    }

    unsigned numBits;
    for (numBits = kNumTableBits + 1; val >= _limits[numBits]; numBits++);
    
    bitStream->MovePos(numBits);
    UInt32 index = _poses[numBits] + ((val - _limits[(size_t)numBits - 1]) >> (kNumBitsMax - numBits));
    return _symbols[index];
  }
};



template <UInt32 m_NumSymbols>
class CDecoder7b
{
  Byte _lens[1 << 7];
public:

  bool Build(const Byte *lens) throw()
  {
    const unsigned kNumBitsMax = 7;
    
    UInt32 counts[kNumBitsMax + 1];
    UInt32 _poses[kNumBitsMax + 1];
    UInt32 _limits[kNumBitsMax + 1];
      
    unsigned i;
    for (i = 0; i <= kNumBitsMax; i++)
      counts[i] = 0;
    
    UInt32 sym;
    
    for (sym = 0; sym < m_NumSymbols; sym++)
      counts[lens[sym]]++;
    
    const UInt32 kMaxValue = (UInt32)1 << kNumBitsMax;

    _limits[0] = 0;
    
    UInt32 startPos = 0;
    UInt32 sum = 0;
    
    for (i = 1; i <= kNumBitsMax; i++)
    {
      const UInt32 cnt = counts[i];
      startPos += cnt << (kNumBitsMax - i);
      if (startPos > kMaxValue)
        return false;
      _limits[i] = startPos;
      counts[i] = sum;
      _poses[i] = sum;
      sum += cnt;
    }

    counts[0] = sum;
    _poses[0] = sum;

    for (sym = 0; sym < m_NumSymbols; sym++)
    {
      unsigned len = lens[sym];
      if (len == 0)
        continue;

      unsigned offset = counts[len]++;

      {
        offset -= _poses[len];
        UInt32 num = (UInt32)1 << (kNumBitsMax - len);
        Byte val = (Byte)((sym << 3) | len);
        Byte *dest = _lens + (_limits[(size_t)len - 1]) + (offset << (kNumBitsMax - len));
        for (UInt32 k = 0; k < num; k++)
          dest[k] = val;
      }
    }

    {
      UInt32 limit = _limits[kNumBitsMax];
      UInt32 num = ((UInt32)1 << kNumBitsMax) - limit;
      Byte *dest = _lens + limit;
      for (UInt32 k = 0; k < num; k++)
        dest[k] = (Byte)(0x1F << 3);
    }
    
    return true;
  }

  template <class TBitDecoder>
  UInt32 Decode(TBitDecoder *bitStream) const
  {
    UInt32 val = bitStream->GetValue(7);
    UInt32 pair = _lens[val];
    bitStream->MovePos((unsigned)(pair & 0x7));
    return pair >> 3;
  }
};

}}

#endif
