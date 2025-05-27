// (C) 2017 Tino Reichardt

#define BROTLI_STATIC_LINKING_ONLY
#include "../../SevenZip/C/Alloc.h"
#include "../../SevenZip/C/Threads.h"
#include <brotli/encode.h>
#include <brotli-mt.h>

#include "../../SevenZip/CPP/Common/Common.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/ICoder.h"
#include "../../SevenZip/CPP/7zip/Common/StreamUtils.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NBROTLI {

struct CProps
{
  CProps() { clear (); }
  void clear ()
  {
    memset(this, 0, sizeof (*this));
    _ver_major = BROTLI_VERSION_MAJOR;
    _ver_minor = BROTLI_VERSION_MINOR;
    _level = 3;
  }

  Byte _ver_major;
  Byte _ver_minor;
  Byte _level;
};

class CEncoder Z7_final:
  public ICompressCoder,
  public ICompressWriteCoderProperties,
  public ICompressSetCoderMt,
  public ICompressSetCoderProperties,
  public ICompressSetCoderPropertiesOpt,
  public CMyUnknownImp
{
public:
  CProps _props;

  UInt64 _processedIn;
  UInt64 _processedOut;
  UInt32 _inputSize;
  UInt32 _numThreads;

  Int32 _Long;
  Int32 _WindowLog;
  UInt64 unpackSize;

  BROTLIMT_CCtx *_ctx;

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressWriteCoderProperties)
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_ENTRY(ICompressSetCoderProperties)
  Z7_COM_QI_ENTRY(ICompressSetCoderPropertiesOpt)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

public:
  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressWriteCoderProperties)
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
  Z7_IFACE_COM7_IMP(ICompressSetCoderProperties)
  Z7_IFACE_COM7_IMP(ICompressSetCoderPropertiesOpt)
  CEncoder();
  ~CEncoder();
};

}}
#endif
