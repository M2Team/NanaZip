// (C) 2016 - 2018 Tino Reichardt

#define ZSTD_STATIC_LINKING_ONLY
#include "../../SevenZip/C/Alloc.h"
#include "../../SevenZip/C/Threads.h"
#include <zstd.h>

#include "../../SevenZip/CPP/Common/Common.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/ICoder.h"
#include "../../SevenZip/CPP/7zip/Common/StreamUtils.h"

#ifndef Z7_EXTRACT_ONLY
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

Z7_CLASS_IMP_COM_5(
  CEncoder,
  ICompressCoder,
  ICompressSetCoderMt,
  ICompressSetCoderProperties,
  ICompressSetCoderPropertiesOpt,
  ICompressWriteCoderProperties
)
public:
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
  bool  _Max;
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

  int dictIDFlag;
  int checksumFlag;
  UInt64 unpackSize;

  CEncoder();
  ~CEncoder();
};

}}
#endif
