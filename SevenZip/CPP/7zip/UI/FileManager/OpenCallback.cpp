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

STDMETHODIMP COpenArchiveCallback::SetTotal(const UInt64 *numFiles, const UInt64 *numBytes)
{
  RINOK(ProgressDialog.Sync.CheckStop());
  {
    // NSynchronization::CCriticalSectionLock lock(_criticalSection);
    ProgressDialog.Sync.Set_NumFilesTotal(numFiles ? *numFiles : (UInt64)(Int64)-1);
    // if (numFiles)
    {
      ProgressDialog.Sync.Set_BytesProgressMode(numFiles == NULL);
    }
    if (numBytes)
      ProgressDialog.Sync.Set_NumBytesTotal(*numBytes);
  }
  return S_OK;
}

STDMETHODIMP COpenArchiveCallback::SetCompleted(const UInt64 *numFiles, const UInt64 *numBytes)
{
  // NSynchronization::CCriticalSectionLock lock(_criticalSection);
  if (numFiles)
    ProgressDialog.Sync.Set_NumFilesCur(*numFiles);
  if (numBytes)
    ProgressDialog.Sync.Set_NumBytesCur(*numBytes);
  return ProgressDialog.Sync.CheckStop();
}

STDMETHODIMP COpenArchiveCallback::SetTotal(const UInt64 total)
{
  RINOK(ProgressDialog.Sync.CheckStop());
  ProgressDialog.Sync.Set_NumBytesTotal(total);
  return S_OK;
}

STDMETHODIMP COpenArchiveCallback::SetCompleted(const UInt64 *completed)
{
  return ProgressDialog.Sync.Set_NumBytesCur(completed);
}

STDMETHODIMP COpenArchiveCallback::GetProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  if (_subArchiveMode)
  {
    switch (propID)
    {
      case kpidName: prop = _subArchiveName; break;
    }
  }
  else
  {
    switch (propID)
    {
      case kpidName:  prop = fs2us(_fileInfo.Name); break;
      case kpidIsDir:  prop = _fileInfo.IsDir(); break;
      case kpidSize:  prop = _fileInfo.Size; break;
      case kpidAttrib:  prop = (UInt32)_fileInfo.Attrib; break;
      case kpidCTime:  prop = _fileInfo.CTime; break;
      case kpidATime:  prop = _fileInfo.ATime; break;
      case kpidMTime:  prop = _fileInfo.MTime; break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP COpenArchiveCallback::GetStream(const wchar_t *name, IInStream **inStream)
{
  COM_TRY_BEGIN
  *inStream = NULL;
  if (_subArchiveMode)
    return S_FALSE;

  FString fullPath;
  if (!NFile::NName::GetFullPath(_folderPrefix, us2fs(name), fullPath))
    return S_FALSE;
  if (!_fileInfo.Find_FollowLink(fullPath))
    return S_FALSE;
  if (_fileInfo.IsDir())
    return S_FALSE;
  CInFileStream *inFile = new CInFileStream;
  CMyComPtr<IInStream> inStreamTemp = inFile;
  if (!inFile->Open(fullPath))
    return ::GetLastError();
  *inStream = inStreamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP COpenArchiveCallback::CryptoGetTextPassword(BSTR *password)
{
  COM_TRY_BEGIN
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
  COM_TRY_END
}
