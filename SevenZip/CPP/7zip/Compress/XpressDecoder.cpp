// XpressDecoder.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "HuffmanDecoder.h"
#include "XpressDecoder.h"

namespace NCompress {
namespace NXpress {

struct CBitStream
{
  UInt32 Value;
  unsigned BitPos;

  UInt32 GetValue(unsigned numBits) const
  {
    return (Value >> (BitPos - numBits)) & ((1 << numBits) - 1);
  }
  
  void MovePos(unsigned numBits)
  {
    BitPos -= numBits;
  }
};

#define BIT_STREAM_NORMALIZE \
    if (bs.BitPos < 16) { \
      if (in >= lim) return S_FALSE; \
      bs.Value = (bs.Value << 16) | GetUi16(in); \
      in += 2; bs.BitPos += 16; }
 
static const unsigned kNumHuffBits = 15;
static const unsigned kNumLenBits = 4;
static const unsigned kLenMask = (1 << kNumLenBits) - 1;
static const unsigned kNumPosSlots = 16;
static const unsigned kNumSyms = 256 + (kNumPosSlots << kNumLenBits);

HRESULT Decode(const Byte *in, size_t inSize, Byte *out, size_t outSize)
{
  NCompress::NHuffman::CDecoder<kNumHuffBits, kNumSyms> huff;
  
  if (inSize < kNumSyms / 2 + 4)
    return S_FALSE;
  {
    Byte levels[kNumSyms];
    for (unsigned i = 0; i < kNumSyms / 2; i++)
    {
      Byte b = in[i];
      levels[(size_t)i * 2] = (Byte)(b & 0xF);
      levels[(size_t)i * 2 + 1] = (Byte)(b >> 4);
    }
    if (!huff.BuildFull(levels))
      return S_FALSE;
  }


  CBitStream bs;

  const Byte *lim = in + inSize - 1;

  in += kNumSyms / 2;
  bs.Value = (GetUi16(in) << 16) | GetUi16(in + 2);
  in += 4;
  bs.BitPos = 32;

  size_t pos = 0;

  for (;;)
  {
    // printf("\n%d", pos);
    UInt32 sym = huff.DecodeFull(&bs);
    // printf(" sym = %d", sym);
    BIT_STREAM_NORMALIZE

    if (pos >= outSize)
      return (sym == 256 && in == lim + 1) ? S_OK : S_FALSE;

    if (sym < 256)
      out[pos++] = (Byte)sym;
    else
    {
      sym -= 256;
      UInt32 dist = sym >> kNumLenBits;
      UInt32 len = sym & kLenMask;
      
      if (len == kLenMask)
      {
        if (in > lim)
          return S_FALSE;
        len = *in++;
        if (len == 0xFF)
        {
          if (in >= lim)
            return S_FALSE;
          len = GetUi16(in);
          in += 2;
        }
        else
          len += kLenMask;
      }

      bs.BitPos -= dist;
      dist = (UInt32)1 << dist;
      dist += ((bs.Value >> bs.BitPos) & (dist - 1));

      BIT_STREAM_NORMALIZE
      
      if (len + 3 > outSize - pos)
        return S_FALSE;
      if (dist > pos)
        return S_FALSE;

      Byte *dest = out + pos;
      const Byte *src = dest - dist;
      pos += len + 3;
      len += 1;
      *dest++ = *src++;
      *dest++ = *src++;
      do
        *dest++ = *src++;
      while (--len);
    }
  }
}

}}
