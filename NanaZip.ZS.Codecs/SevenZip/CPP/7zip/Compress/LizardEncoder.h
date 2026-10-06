// (C) 2017 Tino Reichardt

#define LIZARD_STATIC_LINKING_ONLY
#include "../../../C/Alloc.h"
#include "../../../C/Threads.h"
// **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip ZS Source Code snippet Start ********
#include "../../../C/lizard/lizard_compress.h"
#include "../../../C/lizard/lizard_frame.h"
#endif // ******** Annotated 7-Zip ZS Source Code snippet End ********
#include <lizard_compress.h>
#include <lizard_frame.h>
// **************** NanaZip Modification End ****************
#include "../../../C/zstdmt/lizard-mt.h"

#include "../../Common/Common.h"
#include "../../Common/MyCom.h"
#include "../ICoder.h"
#include "../Common/StreamUtils.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NLIZARD {

struct CProps
{
  CProps() { clear (); }
  void clear ()
  {
    memset(this, 0, sizeof (*this));
    _ver_major = LIZARD_VERSION_MAJOR;
    _ver_minor = LIZARD_VERSION_MINOR;
    _level = LIZARDMT_LEVEL_MIN;
  }

  Byte _ver_major;
  Byte _ver_minor;
  Byte _level;
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

  LIZARDMT_CCtx *_ctx;

  CEncoder();
  ~CEncoder();
};

}}
#endif
