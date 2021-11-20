// (C) 2016 - 2018 Tino Reichardt

#define ZSTD_STATIC_LINKING_ONLY
#include "../../../C/Alloc.h"
#include "../../../C/Threads.h"
#include "../../../C/zstd/zstd.h"

#include "../../Common/Common.h"
#include "../../Common/MyCom.h"
#include "../ICoder.h"
#include "../Common/StreamUtils.h"

#ifndef EXTRACT_ONLY
namespace NCompress {
namespace NZSTD {

struct CProps
{
  CProps() { clear (); }
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

class CEncoder:
  public ICompressCoder,
  public ICompressSetCoderMt,
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  public CMyUnknownImp
{
  CProps _props;

  ZSTD_CCtx* _ctx;
  void*  _srcBuf;
  void*  _dstBuf;
  size_t _srcBufSize;
  size_t _dstBufSize;

  UInt64 _processedIn;
  UInt64 _processedOut;
  UInt32 _numThreads;

  /* zstd advanced compression options */
  Int32 _Long;
  Int32 _Level;
  Int32 _Strategy;
  Int32 _WindowLog;
  Int32 _HashLog;
  Int32 _ChainLog;
  Int32 _SearchLog;
  Int32 _MinMatch;
  Int32 _TargetLen;
  Int32 _OverlapLog;
  Int32 _LdmHashLog;
  Int32 _LdmMinMatch;
  Int32 _LdmBucketSizeLog;
  Int32 _LdmHashRateLog;

public:
  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)
  MY_QUERYINTERFACE_ENTRY(ICompressSetCoderMt)
  MY_QUERYINTERFACE_ENTRY(ICompressSetCoderProperties)
  MY_QUERYINTERFACE_ENTRY(ICompressWriteCoderProperties)
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  STDMETHOD (Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD (SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  STDMETHOD (WriteCoderProperties)(ISequentialOutStream *outStream);
  STDMETHOD (SetNumberOfThreads)(UInt32 numThreads);

  CEncoder();
  virtual ~CEncoder();
};

}}
#endif
