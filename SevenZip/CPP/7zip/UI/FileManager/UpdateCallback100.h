// UpdateCallback100.h

#ifndef __UPDATE_CALLBACK100_H
#define __UPDATE_CALLBACK100_H

#include "../../../Common/MyCom.h"

#include "../../IPassword.h"

#include "../Agent/IFolderArchive.h"

#include "../GUI/UpdateCallbackGUI2.h"

#include "ProgressDialog2.h"

class CUpdateCallback100Imp:
  public IFolderArchiveUpdateCallback,
  public IFolderArchiveUpdateCallback2,
  public IFolderScanProgress,
  public ICryptoGetTextPassword2,
  public ICryptoGetTextPassword,
  public IArchiveOpenCallback,
  public ICompressProgressInfo,
  public CUpdateCallbackGUI2,
  public CMyUnknownImp
{
public:

  // CUpdateCallback100Imp() {}

  MY_UNKNOWN_IMP7(
    IFolderArchiveUpdateCallback,
    IFolderArchiveUpdateCallback2,
    IFolderScanProgress,
    ICryptoGetTextPassword2,
    ICryptoGetTextPassword,
    IArchiveOpenCallback,
    ICompressProgressInfo)

  INTERFACE_IProgress(;)
  INTERFACE_IArchiveOpenCallback(;)
  INTERFACE_IFolderArchiveUpdateCallback(;)
  INTERFACE_IFolderArchiveUpdateCallback2(;)
  INTERFACE_IFolderScanProgress(;)

  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);

  STDMETHOD(CryptoGetTextPassword)(BSTR *password);
  STDMETHOD(CryptoGetTextPassword2)(Int32 *passwordIsDefined, BSTR *password);
};

#endif
