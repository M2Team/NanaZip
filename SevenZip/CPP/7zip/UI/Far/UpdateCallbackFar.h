// UpdateCallbackFar.h

#ifndef __UPDATE_CALLBACK_FAR_H
#define __UPDATE_CALLBACK_FAR_H

#include "../../../Common/MyCom.h"

#include "../../IPassword.h"

#include "../Agent/IFolderArchive.h"

#include "ProgressBox.h"

class CUpdateCallback100Imp:
  public IFolderArchiveUpdateCallback,
  public IFolderArchiveUpdateCallback2,
  public IFolderScanProgress,
  public ICryptoGetTextPassword2,
  public ICryptoGetTextPassword,
  public IArchiveOpenCallback,
  public CMyUnknownImp
{
  // CMyComPtr<IInFolderArchive> _archiveHandler;
  CProgressBox *_percent;
  UInt64 _total;

public:

  bool PasswordIsDefined;
  UString Password;

  MY_UNKNOWN_IMP6(
      IFolderArchiveUpdateCallback,
      IFolderArchiveUpdateCallback2,
      IFolderScanProgress,
      ICryptoGetTextPassword2,
      ICryptoGetTextPassword,
      IArchiveOpenCallback
      )

  INTERFACE_IProgress(;)
  INTERFACE_IFolderArchiveUpdateCallback(;)
  INTERFACE_IFolderArchiveUpdateCallback2(;)
  INTERFACE_IFolderScanProgress(;)
  INTERFACE_IArchiveOpenCallback(;)

  STDMETHOD(CryptoGetTextPassword)(BSTR *password);
  STDMETHOD(CryptoGetTextPassword2)(Int32 *passwordIsDefined, BSTR *password);

  CUpdateCallback100Imp(): _total(0) {}
  void Init(/* IInFolderArchive *archiveHandler, */ CProgressBox *progressBox)
  {
    // _archiveHandler = archiveHandler;
    _percent = progressBox;
    PasswordIsDefined = false;
    Password.Empty();
  }
};

#endif
