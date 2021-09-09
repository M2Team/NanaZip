// XzEncoder.h

#ifndef __XZ_ENCODER_H
#define __XZ_ENCODER_H

#include "../../../C/XzEnc.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NXz {


class CEncoder:
  public ICompressCoder,
  public ICompressSetCoderProperties,
  public ICompressSetCoderPropertiesOpt,
  public CMyUnknownImp
{
  CXzEncHandle _encoder;
public:
  CXzProps xzProps;

  MY_UNKNOWN_IMP3(
      ICompressCoder,
      ICompressSetCoderProperties,
      ICompressSetCoderPropertiesOpt)

  void InitCoderProps();
  HRESULT SetCheckSize(UInt32 checkSizeInBytes);
  HRESULT SetCoderProp(PROPID propID, const PROPVARIANT &prop);
  
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  STDMETHOD(SetCoderPropertiesOpt)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);

  CEncoder();
  virtual ~CEncoder();
};

}}

#endif
