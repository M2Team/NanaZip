// OpenCallback.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../../Common/FileStreams.h"

#include "../Common/ZipRegistry.h"

#include "OpenCallback.h"
#include "PasswordDialog.h"

using namespace NWindows;

HRESULT COpenArchiveCallback::Open_SetTotal(const UInt64 *numFiles, const UInt64 *numBytes)
// Z7_COM7F_IMF(COpenArchiveCallback::SetTotal(const UInt64 *numFiles, const UInt64 *numBytes))
{
  // COM_TRY_BEGIN
  RINOK(ProgressDialog.Sync.CheckStop())
  {
    // NSynchronization::CCriticalSectionLock lock(_criticalSection);
    ProgressDialog.Sync.Set_NumFilesTotal(numFiles ? *numFiles : (UInt64)(Int64)-1);
    // if (numFiles)
    {
      ProgressDialog.Sync.Set_FilesProgressMode(numFiles != NULL);
    }
    if (numBytes)
      ProgressDialog.Sync.Set_NumBytesTotal(*numBytes);
  }
  return S_OK;
  // COM_TRY_END
}

HRESULT COpenArchiveCallback::Open_SetCompleted(const UInt64 *numFiles, const UInt64 *numBytes)
// Z7_COM7F_IMF(COpenArchiveCallback::SetCompleted(const UInt64 *numFiles, const UInt64 *numBytes))
{
  // COM_TRY_BEGIN
  // NSynchronization::CCriticalSectionLock lock(_criticalSection);
  if (numFiles)
    ProgressDialog.Sync.Set_NumFilesCur(*numFiles);
  if (numBytes)
    ProgressDialog.Sync.Set_NumBytesCur(*numBytes);
  return ProgressDialog.Sync.CheckStop();
  // COM_TRY_END
}

HRESULT COpenArchiveCallback::Open_CheckBreak()
{
  return ProgressDialog.Sync.CheckStop();
}

HRESULT COpenArchiveCallback::Open_Finished()
{
  return ProgressDialog.Sync.CheckStop();
}

#ifndef Z7_NO_CRYPTO
HRESULT COpenArchiveCallback::Open_CryptoGetTextPassword(BSTR *password)
// Z7_COM7F_IMF(COpenArchiveCallback::CryptoGetTextPassword(BSTR *password))
{
  // COM_TRY_BEGIN
  PasswordWasAsked = true;
  if (!PasswordIsDefined)
  {
    CPasswordDialog dialog;
    bool showPassword = NExtract::Read_ShowPassword();
    dialog.ShowPassword = showPassword;
   
    ProgressDialog.WaitCreating();
    if (dialog.Create(ProgressDialog) != IDOK)
      return E_ABORT;

    Password = dialog.Password;
    PasswordIsDefined = true;
    if (dialog.ShowPassword != showPassword)
      NExtract::Save_ShowPassword(dialog.ShowPassword);
  }
  return StringToBstr(Password, password);
  // COM_TRY_END
}
#endif
