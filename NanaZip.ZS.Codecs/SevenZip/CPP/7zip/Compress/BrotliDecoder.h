// (C) 2017 Tino Reichardt

#define BROTLI_STATIC_LINKING_ONLY
#include "../../../C/Alloc.h"
#include "../../../C/Threads.h"
// **************** NanaZip Modification Start ****************
//#include "../../../C/brotli/decode.h"
#include <brotli/decode.h>
// **************** NanaZip Modification End ****************
#include "../../../C/zstdmt/brotli-mt.h"

#include "../../Windows/System.h"
#include "../../Common/Common.h"
#include "../../Common/MyCom.h"
#include "../ICoder.h"
#include "../Common/StreamUtils.h"
#include "../Common/RegisterCodec.h"
#include "../Common/ProgressMt.h"

struct BrotliStream {
  ISequentialInStream *inStream;
  ISequentialOutStream *outStream;
  ICompressProgressInfo *progress;
  UInt64 *processedIn;
  UInt64 *processedOut;
};

extern int BrotliRead(void *Stream, BROTLIMT_Buffer * in);
extern int BrotliWrite(void *Stream, BROTLIMT_Buffer * in);

namespace NCompress {
namespace NBROTLI {

struct DProps
{
  DProps() { clear (); }
  void clear ()
  {
    memset(this, 0, sizeof (*this));
    _ver_major = BROTLI_VERSION_MAJOR;
    _ver_minor = BROTLI_VERSION_MINOR;
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
  public ICompressSetInStream,
  public CMyUnknownImp
{
  CMyComPtr < ISequentialInStream > _inStream;

public:
  DProps _props;

  UInt64 _processedIn;
  UInt64 _processedOut;
  UInt32 _inputSize;
  UInt32 _numThreads;
  int    _numThreads_WasForced; /* -1 - default for brotli in 7z, 0 - not forced in .br, 1 - forced in .br */

  HRESULT CodeSpec(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress);
  HRESULT CodeResume(ISequentialOutStream * outStream, const UInt64 * outSize, ICompressProgressInfo * progress);
  HRESULT SetOutStreamSizeResume(const UInt64 *outSize);

  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetDecoderProperties2)
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
#ifndef Z7_NO_READ_FROM_CODER
  Z7_COM_QI_ENTRY(ICompressSetInStream)
#endif
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
public:
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
  Z7_COM7F_IMF(SetOutStreamSize(const UInt64 *outSize));
#ifndef Z7_NO_READ_FROM_CODER
  Z7_IFACE_COM7_IMP(ICompressSetInStream)
  UInt64 GetInputProcessedSize() const { return _processedIn; }
#endif

public:
  CDecoder();
  ~CDecoder();
};

}}
