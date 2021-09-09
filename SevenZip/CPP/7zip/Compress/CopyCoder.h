// Compress/CopyCoder.h

#ifndef __COMPRESS_COPY_CODER_H
#define __COMPRESS_COPY_CODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {

class CCopyCoder:
  public ICompressCoder,
  public ICompressSetInStream,
  public ISequentialInStream,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public CMyUnknownImp
{
  Byte *_buf;
  CMyComPtr<ISequentialInStream> _inStream;
public:
  UInt64 TotalSize;
  
  CCopyCoder(): _buf(0), TotalSize(0) {};
  ~CCopyCoder();

  MY_UNKNOWN_IMP5(
      ICompressCoder,
      ICompressSetInStream,
      ISequentialInStream,
      ICompressSetFinishMode,
      ICompressGetInStreamProcessedSize)

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);
};

HRESULT CopyStream(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress);
HRESULT CopyStream_ExactSize(ISequentialInStream *inStream, ISequentialOutStream *outStream, UInt64 size, ICompressProgressInfo *progress);

}

#endif
