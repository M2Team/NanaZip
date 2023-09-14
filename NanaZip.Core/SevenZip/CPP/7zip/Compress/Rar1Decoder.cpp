// Rar1Decoder.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives
 
#include "StdAfx.h"

#include "Rar1Decoder.h"

namespace NCompress {
namespace NRar1 {

static const unsigned kNumBits = 12;

static const Byte kShortLen1[16 * 3] =
{
  0,0xa0,0xd0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff,0xc0,0x80,0x90,0x98,0x9c,0xb0,0,
  1,3,4,4,5,6,7,8,8,4,4,5,6,6,0,0,
  1,4,4,4,5,6,7,8,8,4,4,5,6,6,4,0
};

static const Byte kShortLen2[16 * 3] =
{
  0,0x40,0x60,0xa0,0xd0,0xe0,0xf0,0xf8,0xfc,0xc0,0x80,0x90,0x98,0x9c,0xb0,0,
  2,3,3,3,4,4,5,6,6,4,4,5,6,6,0,0,
  2,3,3,4,4,4,5,6,6,4,4,5,6,6,4,0
};

static const Byte PosL1[kNumBits + 1]  = { 0,0,2,1,2,2,4,5,4,4,8,0,224 };
static const Byte PosL2[kNumBits + 1]  = { 0,0,0,5,2,2,4,5,4,4,8,2,220 };

static const Byte PosHf0[kNumBits + 1] = { 0,0,0,0,8,8,8,9,0,0,0,0,224 };
static const Byte PosHf1[kNumBits + 1] = { 0,0,0,0,0,4,40,16,16,4,0,47,130 };
static const Byte PosHf2[kNumBits + 1] = { 0,0,0,0,0,2,5,46,64,116,24,0,0 };
static const Byte PosHf3[kNumBits + 1] = { 0,0,0,0,0,0,2,14,202,33,6,0,0 };
static const Byte PosHf4[kNumBits + 1] = { 0,0,0,0,0,0,0,0,255,2,0,0,0 };

static const UInt32 kHistorySize = (1 << 16);

CDecoder::CDecoder():
   _isSolid(false),
   _solidAllowed(false)
   {}

UInt32 CDecoder::ReadBits(unsigned numBits) { return m_InBitStream.ReadBits(numBits); }

HRESULT CDecoder::CopyBlock(UInt32 distance, UInt32 len)
{
  if (len == 0)
    return S_FALSE;
  if (m_UnpackSize < len)
    return S_FALSE;
  m_UnpackSize -= len;
  return m_OutWindowStream.CopyBlock(distance, len) ? S_OK : S_FALSE;
}


UInt32 CDecoder::DecodeNum(const Byte *numTab)
{
  /*
  {
    // we can check that tables are correct
    UInt32 sum = 0;
    for (unsigned i = 0; i <= kNumBits; i++)
      sum += ((UInt32)numTab[i] << (kNumBits - i));
    if (sum != (1 << kNumBits))
      throw 111;
  }
  */

  UInt32 val = m_InBitStream.GetValue(kNumBits);
  UInt32 sum = 0;
  unsigned i = 2;

  for (;;)
  {
    const UInt32 num = numTab[i];
    const UInt32 cur = num << (kNumBits - i);
    if (val < cur)
      break;
    i++;
    val -= cur;
    sum += num;
  }
  m_InBitStream.MovePos(i);
  return ((val >> (kNumBits - i)) + sum);
}


HRESULT CDecoder::ShortLZ()
{
  NumHuf = 0;

  if (LCount == 2)
  {
    if (ReadBits(1))
      return CopyBlock(LastDist, LastLength);
    LCount = 0;
  }

  UInt32 bitField = m_InBitStream.GetValue(8);

  UInt32 len, dist;
  {
    const Byte *xors = (AvrLn1 < 37) ? kShortLen1 : kShortLen2;
    const Byte *lens = xors + 16 + Buf60;
    for (len = 0; ((bitField ^ xors[len]) >> (8 - lens[len])) != 0; len++);
    m_InBitStream.MovePos(lens[len]);
  }
  
  if (len >= 9)
  {
    if (len == 9)
    {
      LCount++;
      return CopyBlock(LastDist, LastLength);
    }

    LCount = 0;

    if (len == 14)
    {
      len = DecodeNum(PosL2) + 5;
      dist = 0x8000 + ReadBits(15) - 1;
      LastLength = len;
      LastDist = dist;
      return CopyBlock(dist, len);
    }

    const UInt32 saveLen = len;
    dist = m_RepDists[(m_RepDistPtr - (len - 9)) & 3];
    
    len = DecodeNum(PosL1);
    
    if (len == 0xff && saveLen == 10)
    {
      Buf60 ^= 16;
      return S_OK;
    }
    if (dist >= 256)
    {
      len++;
      if (dist >= MaxDist3 - 1)
        len++;
    }
  }
  else
  {
    LCount = 0;
    AvrLn1 += len;
    AvrLn1 -= AvrLn1 >> 4;
    
    unsigned distancePlace = DecodeNum(PosHf2) & 0xff;
    
    dist = ChSetA[distancePlace];
    
    if (distancePlace != 0)
    {
      PlaceA[dist]--;
      UInt32 lastDistance = ChSetA[(size_t)distancePlace - 1];
      PlaceA[lastDistance]++;
      ChSetA[distancePlace] = lastDistance;
      ChSetA[(size_t)distancePlace - 1] = dist;
    }
  }

  m_RepDists[m_RepDistPtr++] = dist;
  m_RepDistPtr &= 3;
  len += 2;
  LastLength = len;
  LastDist = dist;
  return CopyBlock(dist, len);
}


HRESULT CDecoder::LongLZ()
{
  UInt32 len;
  UInt32 dist;
  UInt32 distancePlace, newDistancePlace;
  UInt32 oldAvr2, oldAvr3;

  NumHuf = 0;
  Nlzb += 16;
  if (Nlzb > 0xff)
  {
    Nlzb = 0x90;
    Nhfb >>= 1;
  }
  oldAvr2 = AvrLn2;

  if (AvrLn2 >= 64)
    len = DecodeNum(AvrLn2 < 122 ? PosL1 : PosL2);
  else
  {
    UInt32 bitField = m_InBitStream.GetValue(16);
    if (bitField < 0x100)
    {
      len = bitField;
      m_InBitStream.MovePos(16);
    }
    else
    {
      for (len = 0; ((bitField << len) & 0x8000) == 0; len++);
      
      m_InBitStream.MovePos(len + 1);
    }
  }

  AvrLn2 += len;
  AvrLn2 -= AvrLn2 >> 5;

  {
    const Byte *tab;
         if (AvrPlcB >= 0x2900) tab = PosHf2;
    else if (AvrPlcB >= 0x0700) tab = PosHf1;
    else                        tab = PosHf0;
    distancePlace = DecodeNum(tab); // [0, 256]
  }

  AvrPlcB += distancePlace;
  AvrPlcB -= AvrPlcB >> 8;

  distancePlace &= 0xff;
  
  for (;;)
  {
    dist = ChSetB[distancePlace];
    newDistancePlace = NToPlB[dist++ & 0xff]++;
    if (dist & 0xff)
      break;
    CorrHuff(ChSetB,NToPlB);
  }

  ChSetB[distancePlace] = ChSetB[newDistancePlace];
  ChSetB[newDistancePlace] = dist;

  dist = ((dist & 0xff00) >> 1) | ReadBits(7);

  oldAvr3 = AvrLn3;
  
  if (len != 1 && len != 4)
  {
    if (len == 0 && dist <= MaxDist3)
    {
      AvrLn3++;
      AvrLn3 -= AvrLn3 >> 8;
    }
    else if (AvrLn3 > 0)
      AvrLn3--;
  }
  
  len += 3;
  
  if (dist >= MaxDist3)
    len++;
  if (dist <= 256)
    len += 8;
  
  if (oldAvr3 > 0xb0 || (AvrPlc >= 0x2a00 && oldAvr2 < 0x40))
    MaxDist3 = 0x7f00;
  else
    MaxDist3 = 0x2001;
  
  m_RepDists[m_RepDistPtr++] = --dist;
  m_RepDistPtr &= 3;
  LastLength = len;
  LastDist = dist;
  
  return CopyBlock(dist, len);
}


HRESULT CDecoder::HuffDecode()
{
  UInt32 curByte, newBytePlace;
  UInt32 len;
  UInt32 dist;
  unsigned bytePlace;
  {
    const Byte *tab;
    
    if      (AvrPlc >= 0x7600)  tab = PosHf4;
    else if (AvrPlc >= 0x5e00)  tab = PosHf3;
    else if (AvrPlc >= 0x3600)  tab = PosHf2;
    else if (AvrPlc >= 0x0e00)  tab = PosHf1;
    else                        tab = PosHf0;
    
    bytePlace = DecodeNum(tab); // [0, 256]
  }
  
  if (StMode)
  {
    if (bytePlace == 0)
    {
      if (ReadBits(1))
      {
        NumHuf = 0;
        StMode = false;
        return S_OK;
      }
      len = ReadBits(1) + 3;
      dist = DecodeNum(PosHf2);
      dist = (dist << 5) | ReadBits(5);
      if (dist == 0)
        return S_FALSE;
      return CopyBlock(dist - 1, len);
    }
    bytePlace--; // bytePlace is [0, 255]
  }
  else if (NumHuf++ >= 16 && FlagsCnt == 0)
    StMode = true;
  
  bytePlace &= 0xff;
  AvrPlc += bytePlace;
  AvrPlc -= AvrPlc >> 8;
  Nhfb += 16;
  
  if (Nhfb > 0xff)
  {
    Nhfb = 0x90;
    Nlzb >>= 1;
  }

  m_UnpackSize--;
  m_OutWindowStream.PutByte((Byte)(ChSet[bytePlace] >> 8));

  for (;;)
  {
    curByte = ChSet[bytePlace];
    newBytePlace = NToPl[curByte++ & 0xff]++;
    if ((curByte & 0xff) <= 0xa1)
      break;
    CorrHuff(ChSet, NToPl);
  }

  ChSet[bytePlace] = ChSet[newBytePlace];
  ChSet[newBytePlace] = curByte;
  return S_OK;
}


void CDecoder::GetFlagsBuf()
{
  UInt32 flags, newFlagsPlace;
  const UInt32 flagsPlace = DecodeNum(PosHf2); // [0, 256]

  if (flagsPlace >= Z7_ARRAY_SIZE(ChSetC))
    return;

  for (;;)
  {
    flags = ChSetC[flagsPlace];
    FlagBuf = flags >> 8;
    newFlagsPlace = NToPlC[flags++ & 0xff]++;
    if ((flags & 0xff) != 0)
      break;
    CorrHuff(ChSetC, NToPlC);
  }

  ChSetC[flagsPlace] = ChSetC[newFlagsPlace];
  ChSetC[newFlagsPlace] = flags;
}


void CDecoder::CorrHuff(UInt32 *CharSet, UInt32 *NumToPlace)
{
  int i;
  for (i = 7; i >= 0; i--)
    for (unsigned j = 0; j < 32; j++, CharSet++)
      *CharSet = (*CharSet & ~(UInt32)0xff) | (unsigned)i;
  memset(NumToPlace, 0, sizeof(NToPl));
  for (i = 6; i >= 0; i--)
    NumToPlace[i] = (7 - (unsigned)i) * 32;
}



HRESULT CDecoder::CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo * /* progress */)
{
  if (!inSize || !outSize)
    return E_INVALIDARG;

  if (_isSolid && !_solidAllowed)
    return S_FALSE;

  _solidAllowed = false;

  if (!m_OutWindowStream.Create(kHistorySize))
    return E_OUTOFMEMORY;
  if (!m_InBitStream.Create(1 << 20))
    return E_OUTOFMEMORY;

  m_UnpackSize = *outSize;

  m_OutWindowStream.SetStream(outStream);
  m_OutWindowStream.Init(_isSolid);
  m_InBitStream.SetStream(inStream);
  m_InBitStream.Init();

  // InitData

  FlagsCnt = 0;
  FlagBuf = 0;
  StMode = false;
  LCount = 0;

  if (!_isSolid)
  {
    AvrPlcB = AvrLn1 = AvrLn2 = AvrLn3 = NumHuf = Buf60 = 0;
    AvrPlc = 0x3500;
    MaxDist3 = 0x2001;
    Nhfb = Nlzb = 0x80;

    {
      // InitStructures
      for (unsigned i = 0; i < kNumRepDists; i++)
        m_RepDists[i] = 0;
      m_RepDistPtr = 0;
      LastLength = 0;
      LastDist = 0;
    }
    
    // InitHuff
    
    for (UInt32 i = 0; i < 256; i++)
    {
      Place[i] = PlaceA[i] = PlaceB[i] = i;
      UInt32 c = (~i + 1) & 0xff;
      PlaceC[i] = c;
      ChSet[i] = ChSetB[i] = i << 8;
      ChSetA[i] = i;
      ChSetC[i] = c << 8;
    }
    memset(NToPl, 0, sizeof(NToPl));
    memset(NToPlB, 0, sizeof(NToPlB));
    memset(NToPlC, 0, sizeof(NToPlC));
    CorrHuff(ChSetB, NToPlB);
  }
   
  if (m_UnpackSize > 0)
  {
    GetFlagsBuf();
    FlagsCnt = 8;
  }

  while (m_UnpackSize != 0)
  {
    if (!StMode)
    {
      if (--FlagsCnt < 0)
      {
        GetFlagsBuf();
        FlagsCnt = 7;
      }
      
      if (FlagBuf & 0x80)
      {
        FlagBuf <<= 1;
        if (Nlzb > Nhfb)
        {
          RINOK(LongLZ())
          continue;
        }
      }
      else
      {
        FlagBuf <<= 1;
        
        if (--FlagsCnt < 0)
        {
          GetFlagsBuf();
          FlagsCnt = 7;
        }

        if ((FlagBuf & 0x80) == 0)
        {
          FlagBuf <<= 1;
          RINOK(ShortLZ())
          continue;
        }
        
        FlagBuf <<= 1;
        
        if (Nlzb <= Nhfb)
        {
          RINOK(LongLZ())
          continue;
        }
      }
    }

    RINOK(HuffDecode())
  }
  
  _solidAllowed = true;
  return m_OutWindowStream.Flush();
}


Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  try { return CodeReal(inStream, outStream, inSize, outSize, progress); }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size))
{
  if (size < 1)
    return E_INVALIDARG;
  _isSolid = ((data[0] & 1) != 0);
  return S_OK;
}

}}
