// LzfseDecoder.h

#ifndef __LZFSE_DECODER_H
#define __LZFSE_DECODER_H

#include "../../Common/MyBuffer.h"
#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/InBuffer.h"

#include "LzOutWindow.h"

namespace NCompress {
namespace NLzfse {

class CDecoder:
  public ICompressCoder,
  public CMyUnknownImp
{
  CLzOutWindow m_OutWindowStream;
  CInBuffer m_InStream;
  CByteBuffer _literals;
  CByteBuffer _buffer;

  class CCoderReleaser
  {
    CDecoder *m_Coder;
  public:
    bool NeedFlush;
    CCoderReleaser(CDecoder *coder): m_Coder(coder), NeedFlush(true) {}
    ~CCoderReleaser()
    {
      if (NeedFlush)
        m_Coder->m_OutWindowStream.Flush();
    }
  };
  friend class CCoderReleaser;

  HRESULT GetUInt32(UInt32 &val);

  HRESULT DecodeUncompressed(UInt32 unpackSize);
  HRESULT DecodeLzvn(UInt32 unpackSize, UInt32 packSize);
  HRESULT DecodeLzfse(UInt32 unpackSize, Byte version);

  STDMETHOD(CodeReal)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
public:
  bool LzvnMode;
  MY_UNKNOWN_IMP

  CDecoder():
    LzvnMode(false)
    {}

  // sizes are checked in Code()
  // UInt64 GetInputProcessedSize() const { return m_InStream.GetProcessedSize(); }
  // UInt64 GetOutputProcessedSize() const { return m_OutWindowStream.GetProcessedSize(); }

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream, const UInt64 *inSize,
      const UInt64 *outSize, ICompressProgressInfo *progress);
};

}}

#endif
