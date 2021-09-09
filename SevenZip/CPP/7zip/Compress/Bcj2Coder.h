// Bcj2Coder.h

#ifndef __COMPRESS_BCJ2_CODER_H
#define __COMPRESS_BCJ2_CODER_H

#include "../../../C/Bcj2.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NBcj2 {

class CBaseCoder
{
protected:
  Byte *_bufs[BCJ2_NUM_STREAMS + 1];
  UInt32 _bufsCurSizes[BCJ2_NUM_STREAMS + 1];
  UInt32 _bufsNewSizes[BCJ2_NUM_STREAMS + 1];

  HRESULT Alloc(bool allocForOrig = true);
public:
  CBaseCoder();
  ~CBaseCoder();
};


#ifndef EXTRACT_ONLY

class CEncoder:
  public ICompressCoder2,
  public ICompressSetCoderProperties,
  public ICompressSetBufSize,
  public CMyUnknownImp,
  public CBaseCoder
{
  UInt32 _relatLim;

  HRESULT CodeReal(ISequentialInStream * const *inStreams, const UInt64 * const *inSizes, UInt32 numInStreams,
      ISequentialOutStream * const *outStreams, const UInt64 * const *outSizes, UInt32 numOutStreams,
      ICompressProgressInfo *progress);

public:
  MY_UNKNOWN_IMP3(ICompressCoder2, ICompressSetCoderProperties, ICompressSetBufSize)

  STDMETHOD(Code)(ISequentialInStream * const *inStreams, const UInt64 * const *inSizes, UInt32 numInStreams,
      ISequentialOutStream * const *outStreams, const UInt64 * const *outSizes, UInt32 numOutStreams,
      ICompressProgressInfo *progress);

  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  
  STDMETHOD(SetInBufSize)(UInt32 streamIndex, UInt32 size);
  STDMETHOD(SetOutBufSize)(UInt32 streamIndex, UInt32 size);
  
  CEncoder();
  ~CEncoder();
};

#endif

class CDecoder:
  public ICompressCoder2,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize2,
  public ICompressSetInStream2,
  public ISequentialInStream,
  public ICompressSetOutStreamSize,
  public ICompressSetBufSize,
  public CMyUnknownImp,
  public CBaseCoder
{
  unsigned _extraReadSizes[BCJ2_NUM_STREAMS];
  UInt64 _inStreamsProcessed[BCJ2_NUM_STREAMS];
  HRESULT _readRes[BCJ2_NUM_STREAMS];
  CMyComPtr<ISequentialInStream> _inStreams[BCJ2_NUM_STREAMS];

  bool _finishMode;
  bool _outSizeDefined;
  UInt64 _outSize;
  UInt64 _outSize_Processed;
  CBcj2Dec dec;

  void InitCommon();
  // HRESULT ReadSpec();
  
public:
  MY_UNKNOWN_IMP7(
    ICompressCoder2,
    ICompressSetFinishMode,
    ICompressGetInStreamProcessedSize2,
    ICompressSetInStream2,
    ISequentialInStream,
    ICompressSetOutStreamSize,
    ICompressSetBufSize
    );
  
  STDMETHOD(Code)(ISequentialInStream * const *inStreams, const UInt64 * const *inSizes, UInt32 numInStreams,
      ISequentialOutStream * const *outStreams, const UInt64 * const *outSizes, UInt32 numOutStreams,
      ICompressProgressInfo *progress);

  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize2)(UInt32 streamIndex, UInt64 *value);

  STDMETHOD(SetInStream2)(UInt32 streamIndex, ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream2)(UInt32 streamIndex);

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);

  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);

  STDMETHOD(SetInBufSize)(UInt32 streamIndex, UInt32 size);
  STDMETHOD(SetOutBufSize)(UInt32 streamIndex, UInt32 size);

  CDecoder();
};

}}

#endif
