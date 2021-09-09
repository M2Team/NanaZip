// ArchiveOpenCallback.h

#ifndef __ARCHIVE_OPEN_CALLBACK_H
#define __ARCHIVE_OPEN_CALLBACK_H

#include "../../../Common/MyCom.h"

#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"

#ifndef _NO_CRYPTO
#include "../../IPassword.h"
#endif
#include "../../Archive/IArchive.h"

#ifdef _NO_CRYPTO

#define INTERFACE_IOpenCallbackUI_Crypto(x)

#else

#define INTERFACE_IOpenCallbackUI_Crypto(x) \
  virtual HRESULT Open_CryptoGetTextPassword(BSTR *password) x; \
  /* virtual HRESULT Open_GetPasswordIfAny(bool &passwordIsDefined, UString &password) x; */ \
  /* virtual bool Open_WasPasswordAsked() x; */ \
  /* virtual void Open_Clear_PasswordWasAsked_Flag() x; */  \
  
#endif

#define INTERFACE_IOpenCallbackUI(x) \
  virtual HRESULT Open_CheckBreak() x; \
  virtual HRESULT Open_SetTotal(const UInt64 *files, const UInt64 *bytes) x; \
  virtual HRESULT Open_SetCompleted(const UInt64 *files, const UInt64 *bytes) x; \
  virtual HRESULT Open_Finished() x; \
  INTERFACE_IOpenCallbackUI_Crypto(x)

struct IOpenCallbackUI
{
  INTERFACE_IOpenCallbackUI(=0)
};

class COpenCallbackImp:
  public IArchiveOpenCallback,
  public IArchiveOpenVolumeCallback,
  public IArchiveOpenSetSubArchiveName,
  #ifndef _NO_CRYPTO
  public ICryptoGetTextPassword,
  #endif
  public CMyUnknownImp
{
public:
  MY_QUERYINTERFACE_BEGIN2(IArchiveOpenVolumeCallback)
  MY_QUERYINTERFACE_ENTRY(IArchiveOpenSetSubArchiveName)
  #ifndef _NO_CRYPTO
  MY_QUERYINTERFACE_ENTRY(ICryptoGetTextPassword)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  INTERFACE_IArchiveOpenCallback(;)
  INTERFACE_IArchiveOpenVolumeCallback(;)

  #ifndef _NO_CRYPTO
  STDMETHOD(CryptoGetTextPassword)(BSTR *password);
  #endif

  STDMETHOD(SetSubArchiveName(const wchar_t *name))
  {
    _subArchiveMode = true;
    _subArchiveName = name;
    // TotalSize = 0;
    return S_OK;
  }

private:
  FString _folderPrefix;
  NWindows::NFile::NFind::CFileInfo _fileInfo;
  bool _subArchiveMode;
  UString _subArchiveName;

public:
  UStringVector FileNames;
  CBoolVector FileNames_WasUsed;
  CRecordVector<UInt64> FileSizes;
  
  bool PasswordWasAsked;

  IOpenCallbackUI *Callback;
  CMyComPtr<IArchiveOpenCallback> ReOpenCallback;
  // UInt64 TotalSize;

  COpenCallbackImp(): _subArchiveMode(false), Callback(NULL)  {}
  
  HRESULT Init2(const FString &folderPrefix, const FString &fileName)
  {
    FileNames.Clear();
    FileNames_WasUsed.Clear();
    FileSizes.Clear();
    _subArchiveMode = false;
    // TotalSize = 0;
    PasswordWasAsked = false;
    _folderPrefix = folderPrefix;
    if (!_fileInfo.Find_FollowLink(_folderPrefix + fileName))
    {
      // throw 20121118;
      return GetLastError_noZero_HRESULT();
    }
    return S_OK;
  }

  bool SetSecondFileInfo(CFSTR newName)
  {
    return _fileInfo.Find_FollowLink(newName) && !_fileInfo.IsDir();
  }
};

#endif
