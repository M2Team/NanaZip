// Lzma2Decoder.h

#ifndef __LZMA2_DECODER_H
#define __LZMA2_DECODER_H

#include "../../../C/Lzma2DecMt.h"

#include "../Common/CWrappers.h"

namespace NCompress {
namespace NLzma2 {

class CDecoder:
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public ICompressSetBufSize,

  #ifndef NO_READ_FROM_CODER
  public ICompressSetInStream,
  public ICompressSetOutStreamSize,
  public ISequentialInStream,
  #endif

  #ifndef _7ZIP_ST
  public ICompressSetCoderMt,
  public ICompressSetMemLimit,
  #endif

  public CMyUnknownImp
{
  CLzma2DecMtHandle _dec;
  UInt64 _inProcessed;
  Byte _prop;
  int _finishMode;
  UInt32 _inBufSize;
  UInt32 _outStep;

public:
  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)
  MY_QUERYINTERFACE_ENTRY(ICompressSetDecoderProperties2)
  MY_QUERYINTERFACE_ENTRY(ICompressSetFinishMode)
  MY_QUERYINTERFACE_ENTRY(ICompressGetInStreamProcessedSize)
  MY_QUERYINTERFACE_ENTRY(ICompressSetBufSize)
  
  #ifndef NO_READ_FROM_CODER
  MY_QUERYINTERFACE_ENTRY(ICompressSetInStream)
  MY_QUERYINTERFACE_ENTRY(ICompressSetOutStreamSize)
  MY_QUERYINTERFACE_ENTRY(ISequentialInStream)
  #endif
  
  #ifndef _7ZIP_ST
  MY_QUERYINTERFACE_ENTRY(ICompressSetCoderMt)
  MY_QUERYINTERFACE_ENTRY(ICompressSetMemLimit)
  #endif
  
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);
  STDMETHOD(SetInBufSize)(UInt32 streamIndex, UInt32 size);
  STDMETHOD(SetOutBufSize)(UInt32 streamIndex, UInt32 size);

  #ifndef _7ZIP_ST
private:
  int _tryMt;
  UInt32 _numThreads;
  UInt64 _memUsage;
public:
  STDMETHOD(SetNumberOfThreads)(UInt32 numThreads);
  STDMETHOD(SetMemLimit)(UInt64 memUsage);
  #endif

  #ifndef NO_READ_FROM_CODER
private:
  CMyComPtr<ISequentialInStream> _inStream;
  CSeqInStreamWrap _inWrap;
public:
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);
  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  #endif

  CDecoder();
  virtual ~CDecoder();
};

}}

#endif
