// UpdateCallback.h

#ifndef __UPDATE_CALLBACK_H
#define __UPDATE_CALLBACK_H

#include "../../../Common/MyCom.h"

#include "../../Common/FileStreams.h"

#include "../../IPassword.h"
#include "../../ICoder.h"

#include "../Common/UpdatePair.h"
#include "../Common/UpdateProduce.h"

#include "OpenArchive.h"

struct CArcToDoStat
{
  CDirItemsStat2 NewData;
  CDirItemsStat2 OldData;
  CDirItemsStat2 DeleteData;

  UInt64 Get_NumDataItems_Total() const
  {
    return NewData.Get_NumDataItems2() + OldData.Get_NumDataItems2();
  }
};

#define INTERFACE_IUpdateCallbackUI(x) \
  virtual HRESULT WriteSfx(const wchar_t *name, UInt64 size) x; \
  virtual HRESULT SetTotal(UInt64 size) x; \
  virtual HRESULT SetCompleted(const UInt64 *completeValue) x; \
  virtual HRESULT SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize) x; \
  virtual HRESULT CheckBreak() x; \
  /* virtual HRESULT Finalize() x; */ \
  virtual HRESULT SetNumItems(const CArcToDoStat &stat) x; \
  virtual HRESULT GetStream(const wchar_t *name, bool isDir, bool isAnti, UInt32 mode) x; \
  virtual HRESULT OpenFileError(const FString &path, DWORD systemError) x; \
  virtual HRESULT ReadingFileError(const FString &path, DWORD systemError) x; \
  virtual HRESULT SetOperationResult(Int32 opRes) x; \
  virtual HRESULT ReportExtractResult(Int32 opRes, Int32 isEncrypted, const wchar_t *name) x; \
  virtual HRESULT ReportUpdateOperation(UInt32 op, const wchar_t *name, bool isDir) x; \
  /* virtual HRESULT SetPassword(const UString &password) x; */ \
  virtual HRESULT CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password) x; \
  virtual HRESULT CryptoGetTextPassword(BSTR *password) x; \
  virtual HRESULT ShowDeleteFile(const wchar_t *name, bool isDir) x; \

  /*
  virtual HRESULT ReportProp(UInt32 indexType, UInt32 index, PROPID propID, const PROPVARIANT *value) x; \
  virtual HRESULT ReportRawProp(UInt32 indexType, UInt32 index, PROPID propID, const void *data, UInt32 dataSize, UInt32 propType) x; \
  virtual HRESULT ReportFinished(UInt32 indexType, UInt32 index, Int32 opRes) x; \
  */

  /* virtual HRESULT CloseProgress() { return S_OK; } */

struct IUpdateCallbackUI
{
  INTERFACE_IUpdateCallbackUI(=0)
};

struct CKeyKeyValPair
{
  UInt64 Key1;
  UInt64 Key2;
  unsigned Value;

  int Compare(const CKeyKeyValPair &a) const
  {
    if (Key1 < a.Key1) return -1;
    if (Key1 > a.Key1) return 1;
    return MyCompare(Key2, a.Key2);
  }
};


class CArchiveUpdateCallback:
  public IArchiveUpdateCallback2,
  public IArchiveUpdateCallbackFile,
  // public IArchiveUpdateCallbackArcProp,
  public IArchiveExtractCallbackMessage,
  public IArchiveGetRawProps,
  public IArchiveGetRootProps,
  public ICryptoGetTextPassword2,
  public ICryptoGetTextPassword,
  public ICompressProgressInfo,
  public IInFileStream_Callback,
  public CMyUnknownImp
{
  #if defined(_WIN32) && !defined(UNDER_CE)
  bool _saclEnabled;
  #endif
  CRecordVector<CKeyKeyValPair> _map;

  UInt32 _hardIndex_From;
  UInt32 _hardIndex_To;

  void UpdateProcessedItemStatus(unsigned dirIndex);

public:
  MY_QUERYINTERFACE_BEGIN2(IArchiveUpdateCallback2)
    MY_QUERYINTERFACE_ENTRY(IArchiveUpdateCallbackFile)
    // MY_QUERYINTERFACE_ENTRY(IArchiveUpdateCallbackArcProp)
    MY_QUERYINTERFACE_ENTRY(IArchiveExtractCallbackMessage)
    MY_QUERYINTERFACE_ENTRY(IArchiveGetRawProps)
    MY_QUERYINTERFACE_ENTRY(IArchiveGetRootProps)
    MY_QUERYINTERFACE_ENTRY(ICryptoGetTextPassword2)
    MY_QUERYINTERFACE_ENTRY(ICryptoGetTextPassword)
    MY_QUERYINTERFACE_ENTRY(ICompressProgressInfo)
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE


  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);

  INTERFACE_IArchiveUpdateCallback2(;)
  INTERFACE_IArchiveUpdateCallbackFile(;)
  // INTERFACE_IArchiveUpdateCallbackArcProp(;)
  INTERFACE_IArchiveExtractCallbackMessage(;)
  INTERFACE_IArchiveGetRawProps(;)
  INTERFACE_IArchiveGetRootProps(;)

  STDMETHOD(CryptoGetTextPassword2)(Int32 *passwordIsDefined, BSTR *password);
  STDMETHOD(CryptoGetTextPassword)(BSTR *password);

  CRecordVector<UInt32> _openFiles_Indexes;
  FStringVector _openFiles_Paths;
  // CRecordVector< CInFileStream* > _openFiles_Streams;

  bool AreAllFilesClosed() const { return _openFiles_Indexes.IsEmpty(); }
  virtual HRESULT InFileStream_On_Error(UINT_PTR val, DWORD error);
  virtual void InFileStream_On_Destroy(CInFileStream *stream, UINT_PTR val);

  CRecordVector<UInt64> VolumesSizes;
  FString VolName;
  FString VolExt;
  UString ArcFileName; // without path prefix
  FString VolPrefix;
  FString VolPostfix;
  bool VolNumberAfterExt;
  UInt32 DigitCount;

  IUpdateCallbackUI *Callback;

  const CDirItems *DirItems;
  const CDirItem *ParentDirItem;

  const CArc *Arc;
  CMyComPtr<IInArchive> Archive;
  const CObjectVector<CArcItem> *ArcItems;
  const CRecordVector<CUpdatePair2> *UpdatePairs;
  const UStringVector *NewNames;
  int CommentIndex;
  const UString *Comment;

  bool PreserveATime;
  bool ShareForWrite;
  bool StopAfterOpenError;
  bool StdInMode;

  bool KeepOriginalItemNames;
  bool StoreNtSecurity;
  bool StoreHardLinks;
  bool StoreSymLinks;

  bool StoreOwnerId;
  bool StoreOwnerName;

  /*
  bool Need_ArcMTime_Report;
  bool ArcMTime_WasReported;
  CArcTime Reported_ArcMTime;
  */
  bool Need_LatestMTime;
  bool LatestMTime_Defined;
  CFiTime LatestMTime;

  Byte *ProcessedItemsStatuses;



  CArchiveUpdateCallback();

  bool IsDir(const CUpdatePair2 &up) const
  {
    if (up.DirIndex >= 0)
      return DirItems->Items[(unsigned)up.DirIndex].IsDir();
    else if (up.ArcIndex >= 0)
      return (*ArcItems)[(unsigned)up.ArcIndex].IsDir;
    return false;
  }
};

#endif
