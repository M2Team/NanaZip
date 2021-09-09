// HandlerCont.h

#ifndef __HANDLER_CONT_H
#define __HANDLER_CONT_H

#include "../../Common/MyCom.h"

#include "IArchive.h"

namespace NArchive {

#define INTERFACE_IInArchive_Cont(x) \
  STDMETHOD(Open)(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openCallback) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(Close)() MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfItems)(UInt32 *numItems) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value) MY_NO_THROW_DECL_ONLY x; \
  /* STDMETHOD(Extract)(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback) MY_NO_THROW_DECL_ONLY x; */ \
  STDMETHOD(GetArchiveProperty)(PROPID propID, PROPVARIANT *value) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfProperties)(UInt32 *numProps) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetPropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfArchiveProperties)(UInt32 *numProps) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetArchivePropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) MY_NO_THROW_DECL_ONLY x; \


class CHandlerCont:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
protected:
  CMyComPtr<IInStream> _stream;

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const = 0;

public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive_Cont(PURE)

  STDMETHOD(Extract)(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback) MY_NO_THROW_DECL_ONLY;
  
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);

  // destructor must be virtual for this class
  virtual ~CHandlerCont() {}
};



#define INTERFACE_IInArchive_Img(x) \
  /* STDMETHOD(Open)(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openCallback) MY_NO_THROW_DECL_ONLY x; */ \
  STDMETHOD(Close)() MY_NO_THROW_DECL_ONLY x; \
  /* STDMETHOD(GetNumberOfItems)(UInt32 *numItems) MY_NO_THROW_DECL_ONLY x; */ \
  STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value) MY_NO_THROW_DECL_ONLY x; \
  /* STDMETHOD(Extract)(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback) MY_NO_THROW_DECL_ONLY x; */ \
  STDMETHOD(GetArchiveProperty)(PROPID propID, PROPVARIANT *value) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfProperties)(UInt32 *numProps) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetPropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfArchiveProperties)(UInt32 *numProps) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetArchivePropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) MY_NO_THROW_DECL_ONLY x; \


class CHandlerImg:
  public IInStream,
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
protected:
  UInt64 _virtPos;
  UInt64 _posInArc;
  UInt64 _size;
  CMyComPtr<IInStream> Stream;
  const char *_imgExt;
  
  bool _stream_unavailData;
  bool _stream_unsupportedMethod;
  bool _stream_dataError;
  // bool _stream_UsePackSize;
  // UInt64 _stream_PackSize;

  void ClearStreamVars()
  {
    _stream_unavailData = false;
    _stream_unsupportedMethod = false;
    _stream_dataError = false;
    // _stream_UsePackSize = false;
    // _stream_PackSize = 0;
  }


  virtual HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openCallback) = 0;
  virtual void CloseAtError();
public:
  MY_UNKNOWN_IMP3(IInArchive, IInArchiveGetStream, IInStream)
  INTERFACE_IInArchive_Img(PURE)

  STDMETHOD(Open)(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openCallback);
  STDMETHOD(GetNumberOfItems)(UInt32 *numItems);
  STDMETHOD(Extract)(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback);

  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream) = 0;
  
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize) = 0;
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);

  CHandlerImg();
  // destructor must be virtual for this class
  virtual ~CHandlerImg() {}
};


HRESULT ReadZeroTail(ISequentialInStream *stream, bool &areThereNonZeros, UInt64 &numZeros, UInt64 maxSize);

}

#endif
