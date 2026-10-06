// (C) 2016 Tino Reichardt

#define LZ5_STATIC_LINKING_ONLY
#include "../../../C/Alloc.h"
#include "../../../C/Threads.h"
// **************** NanaZip Modification Start ****************
//#include "../../../C/lz5/lz5.h"
#include <lz5.h>
// **************** NanaZip Modification End ****************
#include "../../../C/zstdmt/lz5-mt.h"

#include "../../Common/Common.h"
#include "../../Common/MyCom.h"
#include "../ICoder.h"
#include "../Common/StreamUtils.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NLZ5 {

struct CProps
{
  CProps() { clear (); }
  void clear ()
  {
    memset(this, 0, sizeof (*this));
    _ver_major = LZ5_VERSION_MAJOR;
    _ver_minor = LZ5_VERSION_MINOR;
    _level = 3;
  }

  Byte _ver_major;
  Byte _ver_minor;
  Byte _level;
  Byte _reserved[2];
};

Z7_CLASS_IMP_COM_4(
  CEncoder,
  ICompressCoder,
  ICompressSetCoderMt,
  ICompressSetCoderProperties,
  ICompressWriteCoderProperties
)
public:
  CProps _props;

  UInt64 _processedIn;
  UInt64 _processedOut;
  UInt32 _inputSize;
  UInt32 _numThreads;

  LZ5MT_CCtx *_ctx;

  CEncoder();
  ~CEncoder();
};

}}
#endif
