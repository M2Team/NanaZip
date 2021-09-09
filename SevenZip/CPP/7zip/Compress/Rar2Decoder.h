// Rar2Decoder.h
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives

#ifndef __COMPRESS_RAR2_DECODER_H
#define __COMPRESS_RAR2_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/InBuffer.h"

#include "BitmDecoder.h"
#include "HuffmanDecoder.h"
#include "LzOutWindow.h"

namespace NCompress {
namespace NRar2 {

const unsigned kNumRepDists = 4;
const unsigned kDistTableSize = 48;

const unsigned kMMTableSize = 256 + 1;

const UInt32 kMainTableSize = 298;
const UInt32 kLenTableSize = 28;

const UInt32 kDistTableStart = kMainTableSize;
const UInt32 kLenTableStart = kDistTableStart + kDistTableSize;

const UInt32 kHeapTablesSizesSum = kMainTableSize + kDistTableSize + kLenTableSize;

const UInt32 kLevelTableSize = 19;

const UInt32 kMMTablesSizesSum = kMMTableSize * 4;

const UInt32 kMaxTableSize = kMMTablesSizesSum;

const UInt32 kTableDirectLevels = 16;
const UInt32 kTableLevelRepNumber = kTableDirectLevels;
const UInt32 kTableLevel0Number = kTableLevelRepNumber + 1;
const UInt32 kTableLevel0Number2 = kTableLevel0Number + 1;

const UInt32 kLevelMask = 0xF;


const UInt32 kRepBothNumber = 256;
const UInt32 kRepNumber = kRepBothNumber + 1;
const UInt32 kLen2Number = kRepNumber + 4;

const UInt32 kLen2NumNumbers = 8;
const UInt32 kReadTableNumber = kLen2Number + kLen2NumNumbers;
const UInt32 kMatchNumber = kReadTableNumber + 1;

const Byte kLenStart     [kLenTableSize] = {0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,80,96,112,128,160,192,224};
const Byte kLenDirectBits[kLenTableSize] = {0,0,0,0,0,0,0,0,1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5};

const UInt32 kDistStart   [kDistTableSize] = {0,1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288,16384,24576,32768U,49152U,65536,98304,131072,196608,262144,327680,393216,458752,524288,589824,655360,720896,786432,851968,917504,983040};
const Byte kDistDirectBits[kDistTableSize] = {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13,    14,    14,   15,   15,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16};

const Byte kLevelDirectBits[kLevelTableSize] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7};

const Byte kLen2DistStarts[kLen2NumNumbers]={0,4,8,16,32,64,128,192};
const Byte kLen2DistDirectBits[kLen2NumNumbers]={2,2,3, 4, 5, 6,  6,  6};

const UInt32 kDistLimit2 = 0x101 - 1;
const UInt32 kDistLimit3 = 0x2000 - 1;
const UInt32 kDistLimit4 = 0x40000 - 1;

const UInt32 kMatchMaxLen = 255 + 2;
const UInt32 kMatchMaxLenMax = 255 + 5;
const UInt32 kNormalMatchMinLen = 3;

namespace NMultimedia {

struct CFilter
{
  int K1,K2,K3,K4,K5;
  int D1,D2,D3,D4;
  int LastDelta;
  UInt32 Dif[11];
  UInt32 ByteCount;
  int LastChar;

  Byte Decode(int &channelDelta, Byte delta);

  void Init() { memset(this, 0, sizeof(*this)); }

};

const unsigned kNumChanelsMax = 4;

class CFilter2
{
public:
  CFilter  m_Filters[kNumChanelsMax];
  int m_ChannelDelta;
  unsigned CurrentChannel;

  void Init() { memset(this, 0, sizeof(*this)); }
  Byte Decode(Byte delta)
  {
    return m_Filters[CurrentChannel].Decode(m_ChannelDelta, delta);
  }

};

}

typedef NBitm::CDecoder<CInBuffer> CBitDecoder;

const unsigned kNumHuffmanBits = 15;

class CDecoder :
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public CMyUnknownImp
{
  CLzOutWindow m_OutWindowStream;
  CBitDecoder m_InBitStream;

  UInt32 m_RepDistPtr;
  UInt32 m_RepDists[kNumRepDists];

  UInt32 m_LastLength;

  bool _isSolid;
  bool _solidAllowed;
  bool m_TablesOK;
  bool m_AudioMode;

  NHuffman::CDecoder<kNumHuffmanBits, kMainTableSize> m_MainDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kDistTableSize> m_DistDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kLenTableSize> m_LenDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kMMTableSize> m_MMDecoders[NMultimedia::kNumChanelsMax];
  NHuffman::CDecoder<kNumHuffmanBits, kLevelTableSize> m_LevelDecoder;

  UInt64 m_PackSize;

  unsigned m_NumChannels;
  NMultimedia::CFilter2 m_MmFilter;

  Byte m_LastLevels[kMaxTableSize];


  void InitStructures();
  UInt32 ReadBits(unsigned numBits);
  bool ReadTables();
  bool ReadLastTables();

  bool DecodeMm(UInt32 pos);
  bool DecodeLz(Int32 pos);

  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

public:
  CDecoder();

  MY_UNKNOWN_IMP1(ICompressSetDecoderProperties2)

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);

};

}}

#endif
