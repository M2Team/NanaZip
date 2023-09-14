// HandlerCont.h

#ifndef ZIP7_INC_HANDLER_CONT_H
#define ZIP7_INC_HANDLER_CONT_H

#include "../../Common/MyCom.h"

#include "IArchive.h"

namespace NArchive {

#define Z7_IFACEM_IInArchive_Cont(x) \
  x(Open(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openCallback)) \
  x(Close()) \
  x(GetNumberOfItems(UInt32 *numItems)) \
  x(GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)) \
  /* x(Extract(const UInt32 *indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback)) */ \
  x(GetArchiveProperty(PROPID propID, PROPVARIANT *value)) \
  x(GetNumberOfProperties(UInt32 *numProps)) \
  x(GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \
  x(GetNumberOfArchiveProperties(UInt32 *numProps)) \
  x(GetArchivePropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \


//  #define Z7_COM7F_PUREO(f)     virtual Z7_COM7F_IMF(f)     Z7_override =0;
//  #define Z7_COM7F_PUREO2(t, f) virtual Z7_COM7F_IMF2(t, f) Z7_override =0;

class CHandlerCont:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_2(
      IInArchive,
      IInArchiveGetStream)
  /*
  Z7_IFACEM_IInArchive_Cont(Z7_COM7F_PUREO)
  // Z7_IFACE_COM7_PURE(IInArchive_Cont)
  */
  Z7_COM7F_IMP(Extract(const UInt32 *indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback))
protected:
  Z7_IFACE_COM7_IMP(IInArchiveGetStream)

  CMyComPtr<IInStream> _stream;
  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const = 0;
  // destructor must be virtual for this class
  virtual ~CHandlerCont() {}
};



#define Z7_IFACEM_IInArchive_Img(x) \
  /* x(Open(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openCallback)) */ \
  x(Close()) \
  /* x(GetNumberOfItems(UInt32 *numItems)) */ \
  x(GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)) \
  /* x(Extract(const UInt32 *indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback)) */ \
  x(GetArchiveProperty(PROPID propID, PROPVARIANT *value)) \
  x(GetNumberOfProperties(UInt32 *numProps)) \
  x(GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \
  x(GetNumberOfArchiveProperties(UInt32 *numProps)) \
  x(GetArchivePropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \


class CHandlerImg:
  public IInArchive,
  public IInArchiveGetStream,
  public IInStream,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_3(
      IInArchive,
      IInArchiveGetStream,
      IInStream)

  Z7_COM7F_IMP(Open(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openCallback))
  Z7_COM7F_IMP(GetNumberOfItems(UInt32 *numItems))
  Z7_COM7F_IMP(Extract(const UInt32 *indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback))
  Z7_IFACE_COM7_IMP(IInStream)
  // Z7_IFACEM_IInArchive_Img(Z7_COM7F_PUREO)

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

  void Reset_PosInArc() { _posInArc = (UInt64)0 - 1; }
  void Reset_VirtPos() { _virtPos = (UInt64)0; }

  void ClearStreamVars()
  {
    _stream_unavailData = false;
    _stream_unsupportedMethod = false;
    _stream_dataError = false;
    // _stream_UsePackSize = false;
    // _stream_PackSize = 0;
  }

  void Clear_HandlerImg_Vars(); // it doesn't Release (Stream) var.

  virtual HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openCallback) = 0;
  virtual void CloseAtError();
  
  // returns (true), if Get_PackSizeProcessed() is required in Extract()
  virtual bool Init_PackSizeProcessed()
  {
    return false;
  }
public:
  virtual bool Get_PackSizeProcessed(UInt64 &size)
  {
    size = 0;
    return false;
  }

  CHandlerImg();
  // destructor must be virtual for this class
  virtual ~CHandlerImg() {}
};


HRESULT ReadZeroTail(ISequentialInStream *stream, bool &areThereNonZeros, UInt64 &numZeros, UInt64 maxSize);

}

#endif
