// LzmaEncoder.h

#ifndef __LZMA_ENCODER_H
#define __LZMA_ENCODER_H

#include "../../../C/LzmaEnc.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NLzma {

class CEncoder:
  public ICompressCoder,
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  public ICompressSetCoderPropertiesOpt,
  public CMyUnknownImp
{
  CLzmaEncHandle _encoder;
  UInt64 _inputProcessed;
public:
  MY_UNKNOWN_IMP4(
      ICompressCoder,
      ICompressSetCoderProperties,
      ICompressWriteCoderProperties,
      ICompressSetCoderPropertiesOpt)
    
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  STDMETHOD(WriteCoderProperties)(ISequentialOutStream *outStream);
  STDMETHOD(SetCoderPropertiesOpt)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);

  CEncoder();
  virtual ~CEncoder();

  UInt64 GetInputProcessedSize() const { return _inputProcessed; }
  bool IsWriteEndMark() const { return LzmaEnc_IsWriteEndMark(_encoder) != 0; }
};

}}

#endif
