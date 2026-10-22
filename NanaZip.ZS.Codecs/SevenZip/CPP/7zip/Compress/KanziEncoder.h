// KanziEncoder.h

#ifndef ZIP7_INC_COMPRESS_KANZI_ENCODER_H
#define ZIP7_INC_COMPRESS_KANZI_ENCODER_H

#include "../../Common/Common.h"
#include "../../Common/MyCom.h"

#include "../ICoder.h"
#include "../Common/StreamUtils.h"

#include "KanziCommon.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NKANZI {

class CEncoder Z7_final:
  public ICompressCoder,
  public ICompressWriteCoderProperties,
  public ICompressSetCoderMt,
  public ICompressSetCoderProperties,
  public ICompressSetCoderPropertiesOpt,
  public CMyUnknownImp
{
  CProps _props;
  UInt64 _processedIn;
  UInt64 _processedOut;
  UInt64 _inputSize;
  UInt32 _numThreads;
  bool _blockSizeWasSet;

public:
  CEncoder();

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressWriteCoderProperties)
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_ENTRY(ICompressSetCoderProperties)
  Z7_COM_QI_ENTRY(ICompressSetCoderPropertiesOpt)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressWriteCoderProperties)
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
  Z7_IFACE_COM7_IMP(ICompressSetCoderProperties)
  Z7_IFACE_COM7_IMP(ICompressSetCoderPropertiesOpt)
};

}}
#endif

#endif
