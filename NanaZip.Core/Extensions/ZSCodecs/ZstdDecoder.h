// (C) 2016 - 2018 Tino Reichardt

#define ZSTD_STATIC_LINKING_ONLY
#include "../../SevenZip/C/Alloc.h"
#include <zstd.h>
#include <zstd_errors.h>

#include "../../SevenZip/CPP/Windows/System.h"
#include "../../SevenZip/CPP/Common/Common.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/ICoder.h"
#include "../../SevenZip/CPP/7zip/Common/StreamUtils.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"
#include "../../SevenZip/CPP/7zip/Common/ProgressMt.h"

/**
 * possible return values @ 7zip:
 * S_OK / S_FALSE
 * E_NOTIMPL
 * E_NOINTERFACE
 * E_ABORT
 * E_FAIL
 * E_OUTOFMEMORY
 * E_INVALIDARG
 */

#define ZSTD_LEVEL_MIN      1
#define ZSTD_LEVEL_MAX     22
#define ZSTD_THREAD_MAX   256

namespace NCompress {
namespace NZSTD {

struct DProps
{
  DProps() { clear (); }
  void clear ()
  {
    memset(this, 0, sizeof (*this));
    _ver_major = ZSTD_VERSION_MAJOR;
    _ver_minor = ZSTD_VERSION_MINOR;
    _level = 3;
  }

  Byte _ver_major;
  Byte _ver_minor;
  Byte _level;
  Byte _reserved[2];
};

class CDecoder:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
#ifndef Z7_NO_READ_FROM_CODER
  public ICompressSetInStream,
#endif
  public ICompressSetCoderMt,
  public CMyUnknownImp
{
  CMyComPtr < ISequentialInStream > _inStream;
  DProps _props;

  ZSTD_DCtx* _ctx;
  void*  _srcBuf;
  void*  _dstBuf;
  size_t _srcBufSize;
  size_t _dstBufSize;

  UInt64 _processedIn;
  UInt64 _processedOut;

  HRESULT CodeSpec(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress);
  HRESULT SetOutStreamSizeResume(const UInt64 *outSize);
public:

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetDecoderProperties2)
#ifndef Z7_NO_READ_FROM_CODER
  Z7_COM_QI_ENTRY(ICompressSetInStream)
#endif
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

public:
  STDMETHOD (Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD (SetDecoderProperties2)(const Byte *data, UInt32 size);
  STDMETHOD (SetOutStreamSize)(const UInt64 *outSize);
  STDMETHOD (SetNumberOfThreads)(UInt32 numThreads);

#ifndef Z7_NO_READ_FROM_CODER
  STDMETHOD (SetInStream)(ISequentialInStream *inStream);
  STDMETHOD (ReleaseInStream)();
  UInt64 GetInputProcessedSize() const { return _processedIn; }
#endif
  HRESULT CodeResume(ISequentialOutStream *outStream, const UInt64 *outSize, ICompressProgressInfo *progress);

  CDecoder();
  virtual ~CDecoder();
};

}}
