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

class CEncoder:
  public ICompressCoder,
  public ICompressSetCoderMt,
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  public CMyUnknownImp
{
  CProps _props;

  UInt64 _processedIn;
  UInt64 _processedOut;
  UInt32 _inputSize;
  UInt32 _numThreads;

  Int32 _Long;
  Int32 _WindowLog;

  BROTLIMT_CCtx *_ctx;

public:

  UInt64 unpackSize;

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_ENTRY(ICompressSetCoderProperties)
  Z7_COM_QI_ENTRY(ICompressWriteCoderProperties)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

public:

  STDMETHOD (Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD (SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  STDMETHOD (WriteCoderProperties)(ISequentialOutStream *outStream);
  STDMETHOD (SetNumberOfThreads)(UInt32 numThreads);

  CEncoder();
  virtual ~CEncoder();
};

}}
#endif
