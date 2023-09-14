// TarHandler.h

#ifndef ZIP7_INC_TAR_HANDLER_H
#define ZIP7_INC_TAR_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../../Compress/CopyCoder.h"

#include "../Common/HandlerOut.h"

#include "TarIn.h"

namespace NArchive {
namespace NTar {

Z7_CLASS_IMP_CHandler_IInArchive_4(
    IArchiveOpenSeq
  , IInArchiveGetStream
  , ISetProperties
  , IOutArchive
)
public:
  CObjectVector<CItemEx> _items;
  CMyComPtr<IInStream> _stream;
  CMyComPtr<ISequentialInStream> _seqStream;
private:
  bool _isArc;
  bool _posixMode_WasForced;
  bool _posixMode;
  bool _forceCodePage;
  UInt32 _specifiedCodePage;
  UInt32 _curCodePage;
  UInt32 _openCodePage;
  // CTimeOptions TimeOptions;
  CHandlerTimeOptions _handlerTimeOptions;
  CEncodingCharacts _encodingCharacts;

  UInt32 _curIndex;
  bool _latestIsRead;
  CItemEx _latestItem;

  CArchive _arc;

  NCompress::CCopyCoder *copyCoderSpec;
  CMyComPtr<ICompressCoder> copyCoder;

  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *callback);
  HRESULT SkipTo(UInt32 index);
  void TarStringToUnicode(const AString &s, NWindows::NCOM::CPropVariant &prop, bool toOs = false) const;
public:
  void Init();
  CHandler();
};

}}

#endif
