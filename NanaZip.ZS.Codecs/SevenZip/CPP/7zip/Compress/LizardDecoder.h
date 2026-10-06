// (C) 2017 Tino Reichardt

#define LIZARD_STATIC_LINKING_ONLY
#include "../../../C/Alloc.h"
#include "../../../C/Threads.h"
// **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip ZS Source Code snippet Start ********
#include "../../../C/lizard/lizard_compress.h"
#include "../../../C/lizard/lizard_decompress.h"
#include "../../../C/lizard/lizard_frame.h"
#endif // ******** Annotated 7-Zip ZS Source Code snippet End ********
#include <lizard_compress.h>
#include <lizard_decompress.h>
#include <lizard_frame.h>
// **************** NanaZip Modification End ****************
#include "../../../C/zstdmt/lizard-mt.h"

#include "../../Windows/System.h"
#include "../../Common/Common.h"
#include "../../Common/MyCom.h"
#include "../ICoder.h"
#include "../Common/StreamUtils.h"
#include "../Common/RegisterCodec.h"
#include "../Common/ProgressMt.h"

struct LizardStream {
  ISequentialInStream *inStream;
  ISequentialOutStream *outStream;
  ICompressProgressInfo *progress;
  UInt64 *processedIn;
  UInt64 *processedOut;
};

extern int LizardRead(void *Stream, LIZARDMT_Buffer * in);
extern int LizardWrite(void *Stream, LIZARDMT_Buffer * in);

namespace NCompress {
namespace NLIZARD {

struct DProps
{
  DProps() { clear (); }
  void clear ()
  {
    memset(this, 0, sizeof (*this));
    _ver_major = LIZARD_VERSION_MAJOR;
    _ver_minor = LIZARD_VERSION_MINOR;
    _level = 1;
  }

  Byte _ver_major;
  Byte _ver_minor;
  Byte _level;
};

class CDecoder Z7_final:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressSetCoderMt,
  public CMyUnknownImp
{
  CMyComPtr < ISequentialInStream > _inStream;

public:
  DProps _props;

  UInt64 _processedIn;
  UInt64 _processedOut;
  UInt32 _inputSize;
  UInt32 _numThreads;

  HRESULT CodeSpec(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress);
  HRESULT CodeResume(ISequentialOutStream * outStream, const UInt64 * outSize, ICompressProgressInfo * progress);
  HRESULT SetOutStreamSizeResume(const UInt64 *outSize);

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetDecoderProperties2)
#ifndef Z7_NO_READ_FROM_CODER
  //Z7_COM_QI_ENTRY(ICompressSetInStream)
#endif
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
public:
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
  Z7_COM7F_IMF(SetOutStreamSize(const UInt64 *outSize));
#ifndef Z7_NO_READ_FROM_CODER
  Z7_COM7F_IMF(SetInStream(ISequentialInStream *inStream));
  Z7_COM7F_IMF(ReleaseInStream());
  UInt64 GetInputProcessedSize() const { return _processedIn; }
#endif

public:
  CDecoder();
  ~CDecoder();
};

}}
