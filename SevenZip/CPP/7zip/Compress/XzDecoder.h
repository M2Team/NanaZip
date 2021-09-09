// XzDecoder.h

#ifndef __XZ_DECODER_H
#define __XZ_DECODER_H

#include "../../../C/Xz.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NXz {

struct CDecoder
{
  CXzDecMtHandle xz;
  int _tryMt;
  UInt32 _numThreads;
  UInt64 _memUsage;

  SRes MainDecodeSRes; // it's not HRESULT
  bool MainDecodeSRes_wasUsed;
  CXzStatInfo Stat;

  CDecoder():
      xz(NULL),
      _tryMt(True),
      _numThreads(1),
      _memUsage((UInt64)(sizeof(size_t)) << 28),
      MainDecodeSRes(SZ_OK),
      MainDecodeSRes_wasUsed(false)
    {}
  
  ~CDecoder()
  {
    if (xz)
      XzDecMt_Destroy(xz);
  }

  /* Decode() can return S_OK, if there is data after good xz streams, and that data is not new xz stream.
     check also (Stat.DataAfterEnd) flag */

  HRESULT Decode(ISequentialInStream *seqInStream, ISequentialOutStream *outStream,
      const UInt64 *outSizeLimit, bool finishStream, ICompressProgressInfo *compressProgress);
};


class CComDecoder:
  public ICompressCoder,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,

  #ifndef _7ZIP_ST
  public ICompressSetCoderMt,
  public ICompressSetMemLimit,
  #endif

  public CMyUnknownImp,
  public CDecoder
{
  bool _finishStream;

public:
  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)

  MY_QUERYINTERFACE_ENTRY(ICompressSetFinishMode)
  MY_QUERYINTERFACE_ENTRY(ICompressGetInStreamProcessedSize)

  #ifndef _7ZIP_ST
  MY_QUERYINTERFACE_ENTRY(ICompressSetCoderMt)
  MY_QUERYINTERFACE_ENTRY(ICompressSetMemLimit)
  #endif
  
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);

  #ifndef _7ZIP_ST
  STDMETHOD(SetNumberOfThreads)(UInt32 numThreads);
  STDMETHOD(SetMemLimit)(UInt64 memUsage);
  #endif

  CComDecoder(): _finishStream(false) {}
};

}}

#endif
