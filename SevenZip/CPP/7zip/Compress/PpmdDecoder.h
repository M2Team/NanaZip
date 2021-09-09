// PpmdDecoder.h
// 2020-07-03 : Igor Pavlov : Public domain

#ifndef __COMPRESS_PPMD_DECODER_H
#define __COMPRESS_PPMD_DECODER_H

#include "../../../C/Ppmd7.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/CWrappers.h"

namespace NCompress {
namespace NPpmd {

class CDecoder :
  public ICompressCoder,
  public ICompressSetDecoderProperties2,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  #ifndef NO_READ_FROM_CODER
  public ICompressSetInStream,
  public ICompressSetOutStreamSize,
  public ISequentialInStream,
  #endif
  public CMyUnknownImp
{
  Byte *_outBuf;
  CByteInBufWrap _inStream;
  CPpmd7 _ppmd;

  Byte _order;
  bool  FinishStream;
  bool _outSizeDefined;
  HRESULT _res;
  int _status;
  UInt64 _outSize;
  UInt64 _processedSize;

  HRESULT CodeSpec(Byte *memStream, UInt32 size);

public:

  #ifndef NO_READ_FROM_CODER
  CMyComPtr<ISequentialInStream> InSeqStream;
  #endif

  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)
  MY_QUERYINTERFACE_ENTRY(ICompressSetDecoderProperties2)
  MY_QUERYINTERFACE_ENTRY(ICompressSetFinishMode)
  MY_QUERYINTERFACE_ENTRY(ICompressGetInStreamProcessedSize)
  #ifndef NO_READ_FROM_CODER
  MY_QUERYINTERFACE_ENTRY(ICompressSetInStream)
  MY_QUERYINTERFACE_ENTRY(ICompressSetOutStreamSize)
  MY_QUERYINTERFACE_ENTRY(ISequentialInStream)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE


  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);
  
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);

  #ifndef NO_READ_FROM_CODER
  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  #endif

  CDecoder():
      _outBuf(NULL),
      FinishStream(false),
      _outSizeDefined(false)
  {
    Ppmd7_Construct(&_ppmd);
    _ppmd.rc.dec.Stream = &_inStream.vt;
  }

  ~CDecoder();
};

}}

#endif
