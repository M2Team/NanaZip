// OpenCallback.h

#ifndef __OPEN_CALLBACK_H
#define __OPEN_CALLBACK_H

#include "../../../Common/MyCom.h"

#include "../../../Windows/FileFind.h"

#include "../../IPassword.h"

#include "../../Archive/IArchive.h"

#ifdef _SFX
#include "ProgressDialog.h"
#else
#include "ProgressDialog2.h"
#endif


class COpenArchiveCallback:
  public IArchiveOpenCallback,
  public IArchiveOpenVolumeCallback,
  public IArchiveOpenSetSubArchiveName,
  public IProgress,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
  FString _folderPrefix;
  NWindows::NFile::NFind::CFileInfo _fileInfo;
  // NWindows::NSynchronization::CCriticalSection _criticalSection;
  bool _subArchiveMode;
  UString _subArchiveName;

public:
  bool PasswordIsDefined;
  bool PasswordWasAsked;
  UString Password;
  HWND ParentWindow;
  CProgressDialog ProgressDialog;

  MY_UNKNOWN_IMP5(
    IArchiveOpenCallback,
    IArchiveOpenVolumeCallback,
    IArchiveOpenSetSubArchiveName,
    IProgress,
    ICryptoGetTextPassword)

  INTERFACE_IProgress(;)
  INTERFACE_IArchiveOpenCallback(;)
  INTERFACE_IArchiveOpenVolumeCallback(;)

  // ICryptoGetTextPassword
  STDMETHOD(CryptoGetTextPassword)(BSTR *password);

  STDMETHOD(SetSubArchiveName(const wchar_t *name))
  {
    _subArchiveMode = true;
    _subArchiveName = name;
    return S_OK;
  }

  COpenArchiveCallback():
    ParentWindow(0)
  {
    _subArchiveMode = false;
    PasswordIsDefined = false;
    PasswordWasAsked = false;
  }
  /*
  void Init()
  {
    PasswordIsDefined = false;
    _subArchiveMode = false;
  }
  */
  
  HRESULT LoadFileInfo2(const FString &folderPrefix, const FString &fileName)
  {
    _folderPrefix = folderPrefix;
    if (!_fileInfo.Find_FollowLink(_folderPrefix + fileName))
    {
      return GetLastError_noZero_HRESULT();
    }
    return S_OK;
  }

  void ShowMessage(const UInt64 *completed);

  INT_PTR StartProgressDialog(const UString &title, NWindows::CThread &thread)
  {
    return ProgressDialog.Create(title, thread, ParentWindow);
  }
};

#endif
