// Compress/CopyCoder.h

#ifndef ZIP7_INC_COMPRESS_COPY_CODER_H
#define ZIP7_INC_COMPRESS_COPY_CODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {

class CCopyCoderBase
{
  Byte *_buf;
public:
  UInt64 TotalSize;
  
  CCopyCoderBase(): _buf(NULL), TotalSize(0) {}
  ~CCopyCoderBase() throw();

  HRESULT Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *outSize, ICompressProgressInfo *progress) throw();
};

Z7_CLASS_IMP_COM_5(
  CCopyCoder
  , ICompressCoder
  , ICompressSetInStream
  , ISequentialInStream
  , ICompressSetFinishMode
  , ICompressGetInStreamProcessedSize
)
  CCopyCoderBase Coder;
  CMyComPtr<ISequentialInStream> _inStream;
};

HRESULT CopyStream(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress) throw();
HRESULT CopyStream_ExactSize(ISequentialInStream *inStream, ISequentialOutStream *outStream, UInt64 size, ICompressProgressInfo *progress) throw();

}

#endif
