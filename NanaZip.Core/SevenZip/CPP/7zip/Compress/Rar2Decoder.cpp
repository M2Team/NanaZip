// Rar2Decoder.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives
 
#include "StdAfx.h"

#include <stdlib.h>

#include "Rar2Decoder.h"

namespace NCompress {
namespace NRar2 {

namespace NMultimedia {

#define my_abs(x) (unsigned)abs(x)

Byte CFilter::Decode(int &channelDelta, Byte deltaByte)
{
  D4 = D3;
  D3 = D2;
  D2 = LastDelta - D1;
  D1 = LastDelta;
  const int predictedValue = ((8 * LastChar + K1 * D1 + K2 * D2 + K3 * D3 + K4 * D4 + K5 * channelDelta) >> 3);

  const Byte realValue = (Byte)(predictedValue - deltaByte);
  
  {
    const int i = ((int)(signed char)deltaByte) << 3;

    Dif[0] += my_abs(i);
    Dif[1] += my_abs(i - D1);
    Dif[2] += my_abs(i + D1);
    Dif[3] += my_abs(i - D2);
    Dif[4] += my_abs(i + D2);
    Dif[5] += my_abs(i - D3);
    Dif[6] += my_abs(i + D3);
    Dif[7] += my_abs(i - D4);
    Dif[8] += my_abs(i + D4);
    Dif[9] += my_abs(i - channelDelta);
    Dif[10] += my_abs(i + channelDelta);
  }

  channelDelta = LastDelta = (signed char)(realValue - LastChar);
  LastChar = realValue;

  if (((++ByteCount) & 0x1F) == 0)
  {
    UInt32 minDif = Dif[0];
    UInt32 numMinDif = 0;
    Dif[0] = 0;
    
    for (unsigned i = 1; i < Z7_ARRAY_SIZE(Dif); i++)
    {
      if (Dif[i] < minDif)
      {
        minDif = Dif[i];
        numMinDif = i;
      }
      Dif[i] = 0;
    }
    
    switch (numMinDif)
    {
      case 1: if (K1 >= -16) K1--; break;
      case 2: if (K1 <   16) K1++; break;
      case 3: if (K2 >= -16) K2--; break;
      case 4: if (K2 <   16) K2++; break;
      case 5: if (K3 >= -16) K3--; break;
      case 6: if (K3 <   16) K3++; break;
      case 7: if (K4 >= -16) K4--; break;
      case 8: if (K4 <   16) K4++; break;
      case 9: if (K5 >= -16) K5--; break;
      case 10:if (K5 <   16) K5++; break;
    }
  }
  
  return realValue;
}
}

static const UInt32 kHistorySize = 1 << 20;

// static const UInt32 kWindowReservSize = (1 << 22) + 256;

CDecoder::CDecoder():
  _isSolid(false),
  _solidAllowed(false),
  m_TablesOK(false)
{
}

void CDecoder::InitStructures()
{
  m_MmFilter.Init();
  for (unsigned i = 0; i < kNumReps; i++)
    m_RepDists[i] = 0;
  m_RepDistPtr = 0;
  m_LastLength = 0;
  memset(m_LastLevels, 0, kMaxTableSize);
}

UInt32 CDecoder::ReadBits(unsigned numBits) { return m_InBitStream.ReadBits(numBits); }

#define RIF(x) { if (!(x)) return false; }

static const unsigned kRepBothNumber = 256;
static const unsigned kRepNumber = kRepBothNumber + 1;
static const unsigned kLen2Number = kRepNumber + kNumReps;
static const unsigned kReadTableNumber = kLen2Number + kNumLen2Symbols;
static const unsigned kMatchNumber = kReadTableNumber + 1;

// static const unsigned kDistTableStart = kMainTableSize;
// static const unsigned kLenTableStart = kDistTableStart + kDistTableSize;

static const UInt32 kDistStart   [kDistTableSize] = {0,1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288,16384,24576,32768U,49152U,65536,98304,131072,196608,262144,327680,393216,458752,524288,589824,655360,720896,786432,851968,917504,983040};
static const Byte kDistDirectBits[kDistTableSize] = {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13,    14,    14,   15,   15,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16};

static const Byte kLen2DistStarts    [kNumLen2Symbols]={0,4,8,16,32,64,128,192};
static const Byte kLen2DistDirectBits[kNumLen2Symbols]={2,2,3, 4, 5, 6,  6,  6};

static const UInt32 kDistLimit2 = 0x101 - 1;
static const UInt32 kDistLimit3 = 0x2000 - 1;
static const UInt32 kDistLimit4 = 0x40000 - 1;

// static const UInt32 kMatchMaxLen = 255 + 2;
// static const UInt32 kMatchMaxLenMax = 255 + 5;


bool CDecoder::ReadTables(void)
{
  m_TablesOK = false;

  const unsigned kLevelTableSize = 19;
  Byte levelLevels[kLevelTableSize];
  Byte lens[kMaxTableSize];
  
  m_AudioMode = (ReadBits(1) == 1);

  if (ReadBits(1) == 0)
    memset(m_LastLevels, 0, kMaxTableSize);
  
  unsigned numLevels;
  
  if (m_AudioMode)
  {
    m_NumChannels = ReadBits(2) + 1;
    if (m_MmFilter.CurrentChannel >= m_NumChannels)
      m_MmFilter.CurrentChannel = 0;
    numLevels = m_NumChannels * k_MM_TableSize;
  }
  else
    numLevels = kHeapTablesSizesSum;
 
  unsigned i;
  for (i = 0; i < kLevelTableSize; i++)
    levelLevels[i] = (Byte)ReadBits(4);
  NHuffman::CDecoder256<kNumHufBits, kLevelTableSize, 6> m_LevelDecoder;
  RIF(m_LevelDecoder.Build(levelLevels, NHuffman::k_BuildMode_Full))
  
  i = 0;
  do
  {
    const unsigned sym = m_LevelDecoder.DecodeFull(&m_InBitStream);
    if (sym < 16)
    {
      lens[i] = (Byte)((sym + m_LastLevels[i]) & 15);
      i++;
    }
#if 0
    else if (sym >= kLevelTableSize)
      return false;
#endif
    else
    {
      unsigned num;
      Byte v;
      if (sym == 16)
      {
        if (i == 0)
          return false;
        num = ReadBits(2) + 3;
        v = lens[(size_t)i - 1];
      }
      else
      {
        num = (sym - 17) * 4;
        num += num + 3 + ReadBits(3 + num);
        v = 0;
      }
      num += i;
      if (num > numLevels)
      {
        // return false;
        num = numLevels; // original unRAR
      }
      do
        lens[i++] = v;
      while (i < num);
    }
  }
  while (i < numLevels);

  if (m_InBitStream.ExtraBitsWereRead())
    return false;

  if (m_AudioMode)
    for (i = 0; i < m_NumChannels; i++)
    {
      RIF(m_MMDecoders[i].Build(&lens[(size_t)i * k_MM_TableSize]))
    }
  else
  {
    RIF(m_MainDecoder.Build(&lens[0]))
    RIF(m_DistDecoder.Build(&lens[kMainTableSize]))
    RIF(m_LenDecoder.Build(&lens[kMainTableSize + kDistTableSize]))
  }
  
  memcpy(m_LastLevels, lens, kMaxTableSize);

  m_TablesOK = true;
  return true;
}


bool CDecoder::ReadLastTables()
{
  // it differs a little from pure RAR sources;
  // UInt64 ttt = m_InBitStream.GetProcessedSize() + 2;
  // + 2 works for: return 0xFF; in CInBuffer::ReadByte.
  if (m_InBitStream.GetProcessedSize() + 7 <= m_PackSize) // test it: probably incorrect;
  // if (m_InBitStream.GetProcessedSize() + 2 <= m_PackSize) // test it: probably incorrect;
  {
    if (m_AudioMode)
    {
      const unsigned symbol = m_MMDecoders[m_MmFilter.CurrentChannel].Decode(&m_InBitStream);
      if (symbol == 256)
        return ReadTables();
      if (symbol >= k_MM_TableSize)
        return false;
    }
    else
    {
      const unsigned sym = m_MainDecoder.Decode(&m_InBitStream);
      if (sym == kReadTableNumber)
        return ReadTables();
      if (sym >= kMainTableSize)
        return false;
    }
  }
  return true;
}


bool CDecoder::DecodeMm(UInt32 pos)
{
  while (pos-- != 0)
  {
    const unsigned symbol = m_MMDecoders[m_MmFilter.CurrentChannel].Decode(&m_InBitStream);
    if (m_InBitStream.ExtraBitsWereRead())
      return false;
    if (symbol >= 256)
      return symbol == 256;
    /*
    Byte byPredict = m_Predictor.Predict();
    Byte byReal = (Byte)(byPredict - (Byte)symbol);
    m_Predictor.Update(byReal, byPredict);
    */
    const Byte byReal = m_MmFilter.Decode((Byte)symbol);
    m_OutWindowStream.PutByte(byReal);
    if (++m_MmFilter.CurrentChannel == m_NumChannels)
      m_MmFilter.CurrentChannel = 0;
  }
  return true;
}


typedef unsigned CLenType;

static inline CLenType SlotToLen(CBitDecoder &_bitStream, CLenType slot)
{
  const unsigned numBits = ((unsigned)slot >> 2) - 1;
  return ((4 | (slot & 3)) << numBits) + (CLenType)_bitStream.ReadBits(numBits);
}

bool CDecoder::DecodeLz(Int32 pos)
{
  while (pos > 0)
  {
    unsigned sym = m_MainDecoder.Decode(&m_InBitStream);
    if (m_InBitStream.ExtraBitsWereRead())
      return false;
    UInt32 len, distance;
    if (sym < 256)
    {
      m_OutWindowStream.PutByte(Byte(sym));
      pos--;
      continue;
    }
    else if (sym >= kMatchNumber)
    {
      if (sym >= kMainTableSize)
        return false;
      len = sym - kMatchNumber;
      if (len >= 8)
        len = SlotToLen(m_InBitStream, len);
      len += 3;

      sym = m_DistDecoder.Decode(&m_InBitStream);
      if (sym >= kDistTableSize)
        return false;
      distance = kDistStart[sym] + m_InBitStream.ReadBits(kDistDirectBits[sym]);
      if (distance >= kDistLimit3)
      {
        len += 2 - ((distance - kDistLimit4) >> 31);
        // len++;
        // if (distance >= kDistLimit4)
        //  len++;
      }
    }
    else if (sym == kRepBothNumber)
    {
      len = m_LastLength;
      if (len == 0)
        return false;
      distance = m_RepDists[(m_RepDistPtr + 4 - 1) & 3];
    }
    else if (sym < kLen2Number)
    {
      distance = m_RepDists[(m_RepDistPtr - (sym - kRepNumber + 1)) & 3];
      len = m_LenDecoder.Decode(&m_InBitStream);
      if (len >= kLenTableSize)
        return false;
      if (len >= 8)
        len = SlotToLen(m_InBitStream, len);
      len += 2;

      if (distance >= kDistLimit2)
      {
        len++;
        if (distance >= kDistLimit3)
        {
          len += 2 - ((distance - kDistLimit4) >> 31);
          // len++;
          // if (distance >= kDistLimit4)
          //   len++;
        }
      }
    }
    else if (sym < kReadTableNumber)
    {
      sym -= kLen2Number;
      distance = kLen2DistStarts[sym] +
        m_InBitStream.ReadBits(kLen2DistDirectBits[sym]);
      len = 2;
    }
    else // (sym == kReadTableNumber)
      return true;

    m_RepDists[m_RepDistPtr++ & 3] = distance;
    m_LastLength = len;
    if (!m_OutWindowStream.CopyBlock(distance, len))
      return false;
    pos -= len;
  }
  return true;
}

HRESULT CDecoder::CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
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

  m_PackSize = *inSize;

  UInt64 pos = 0, unPackSize = *outSize;
  
  m_OutWindowStream.SetStream(outStream);
  m_OutWindowStream.Init(_isSolid);
  m_InBitStream.SetStream(inStream);
  m_InBitStream.Init();

  // CCoderReleaser coderReleaser(this);
  if (!_isSolid)
  {
    InitStructures();
    if (unPackSize == 0)
    {
      if (m_InBitStream.GetProcessedSize() + 2 <= m_PackSize) // test it: probably incorrect;
        if (!ReadTables())
          return S_FALSE;
      _solidAllowed = true;
      return S_OK;
    }
    ReadTables();
  }

  if (!m_TablesOK)
    return S_FALSE;

  const UInt64 startPos = m_OutWindowStream.GetProcessedSize();
  while (pos < unPackSize)
  {
    UInt32 blockSize = 1 << 20;
    if (blockSize > unPackSize - pos)
      blockSize = (UInt32)(unPackSize - pos);
    UInt64 blockStartPos = m_OutWindowStream.GetProcessedSize();
    if (m_AudioMode)
    {
      if (!DecodeMm(blockSize))
        return S_FALSE;
    }
    else
    {
      if (!DecodeLz((Int32)blockSize))
        return S_FALSE;
    }

    if (m_InBitStream.ExtraBitsWereRead())
      return S_FALSE;

    const UInt64 globalPos = m_OutWindowStream.GetProcessedSize();
    pos = globalPos - blockStartPos;
    if (pos < blockSize)
      if (!ReadTables())
        return S_FALSE;
    pos = globalPos - startPos;
    if (progress)
    {
      const UInt64 packSize = m_InBitStream.GetProcessedSize();
      RINOK(progress->SetRatioInfo(&packSize, &pos))
    }
  }
  if (pos > unPackSize)
    return S_FALSE;

  if (!ReadLastTables())
    return S_FALSE;

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
