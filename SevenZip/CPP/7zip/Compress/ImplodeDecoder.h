// ImplodeDecoder.h

#ifndef __COMPRESS_IMPLODE_DECODER_H
#define __COMPRESS_IMPLODE_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/InBuffer.h"

#include "BitlDecoder.h"
#include "LzOutWindow.h"

namespace NCompress {
namespace NImplode {
namespace NDecoder {

typedef NBitl::CDecoder<CInBuffer> CInBit;

const unsigned kNumHuffmanBits = 16;
const unsigned kMaxHuffTableSize = 1 << 8;

class CHuffmanDecoder
{
  UInt32 _limits[kNumHuffmanBits + 1];
  UInt32 _poses[kNumHuffmanBits + 1];
  Byte _symbols[kMaxHuffTableSize];
public:
  bool Build(const Byte *lens, unsigned numSymbols) throw();
  UInt32 Decode(CInBit *inStream) const throw();
};


class CCoder:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public CMyUnknownImp
{
  CLzOutWindow _outWindowStream;
  CInBit _inBitStream;
  
  CHuffmanDecoder _litDecoder;
  CHuffmanDecoder _lenDecoder;
  CHuffmanDecoder _distDecoder;

  Byte _flags;
  bool _fullStreamMode;

  bool BuildHuff(CHuffmanDecoder &table, unsigned numSymbols);
  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

public:
  MY_UNKNOWN_IMP3(
      ICompressSetDecoderProperties2,
      ICompressSetFinishMode,
      ICompressGetInStreamProcessedSize)

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);

  CCoder();
};

}}}

#endif
