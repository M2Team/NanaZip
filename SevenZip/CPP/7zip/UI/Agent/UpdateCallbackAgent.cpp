// UpdateCallbackAgent.h

#include "StdAfx.h"

#include "../../../Common/IntToString.h"

#include "../../../Windows/ErrorMsg.h"

#include "UpdateCallbackAgent.h"

using namespace NWindows;

void CUpdateCallbackAgent::SetCallback(IFolderArchiveUpdateCallback *callback)
{
  Callback = callback;
  _compressProgress.Release();
  Callback2.Release();
  if (Callback)
  {
    Callback.QueryInterface(IID_ICompressProgressInfo, &_compressProgress);
    Callback.QueryInterface(IID_IFolderArchiveUpdateCallback2, &Callback2);
  }
}

HRESULT CUpdateCallbackAgent::SetNumItems(const CArcToDoStat &stat)
{
  if (Callback)
    return Callback->SetNumFiles(stat.Get_NumDataItems_Total());
  return S_OK;
}


HRESULT CUpdateCallbackAgent::WriteSfx(const wchar_t * /* name */, UInt64 /* size */)
{
  return S_OK;
}


HRESULT CUpdateCallbackAgent::SetTotal(UINT64 size)
{
  if (Callback)
    return Callback->SetTotal(size);
  return S_OK;
}

HRESULT CUpdateCallbackAgent::SetCompleted(const UINT64 *completeValue)
{
  if (Callback)
    return Callback->SetCompleted(completeValue);
  return S_OK;
}

HRESULT CUpdateCallbackAgent::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  if (_compressProgress)
    return _compressProgress->SetRatioInfo(inSize, outSize);
  return S_OK;
}

HRESULT CUpdateCallbackAgent::CheckBreak()
{
  return S_OK;
}

/*
HRESULT CUpdateCallbackAgent::Finalize()
{
  return S_OK;
}
*/

HRESULT CUpdateCallbackAgent::OpenFileError(const FString &path, DWORD systemError)
{
  HRESULT hres = HRESULT_FROM_WIN32(systemError);
  // if (systemError == ERROR_SHARING_VIOLATION)
  {
    if (Callback2)
    {
      RINOK(Callback2->OpenFileError(fs2us(path), hres));
      return S_FALSE;
    }
    
    if (Callback)
    {
      UString s ("WARNING: ");
      s += NError::MyFormatMessage(systemError);
      s += ": ";
      s += fs2us(path);
      RINOK(Callback->UpdateErrorMessage(s));
      return S_FALSE;
    }
  }
  // FailedFiles.Add(name);
  return hres;
}

HRESULT CUpdateCallbackAgent::ReadingFileError(const FString &path, DWORD systemError)
{
  HRESULT hres = HRESULT_FROM_WIN32(systemError);

  // if (systemError == ERROR_SHARING_VIOLATION)
  {
    if (Callback2)
    {
      RINOK(Callback2->ReadingFileError(fs2us(path), hres));
    }
    else if (Callback)
    {
      UString s ("ERROR: ");
      s += NError::MyFormatMessage(systemError);
      s += ": ";
      s += fs2us(path);
      RINOK(Callback->UpdateErrorMessage(s));
    }
  }
  // FailedFiles.Add(name);
  return hres;
}

HRESULT CUpdateCallbackAgent::GetStream(const wchar_t *name, bool isDir, bool /* isAnti */, UInt32 mode)
{
  if (Callback2)
    return Callback2->ReportUpdateOperation(mode, name, BoolToInt(isDir));
  if (Callback)
    return Callback->CompressOperation(name);
  return S_OK;
}

HRESULT CUpdateCallbackAgent::SetOperationResult(Int32 operationResult)
{
  if (Callback)
    return Callback->OperationResult(operationResult);
  return S_OK;
}

void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, const wchar_t *fileName, UString &s);

HRESULT CUpdateCallbackAgent::ReportExtractResult(Int32 opRes, Int32 isEncrypted, const wchar_t *name)
{
  if (Callback2)
  {
    return Callback2->ReportExtractResult(opRes, isEncrypted, name);
  }
  /*
  if (mode != NArchive::NExtract::NOperationResult::kOK)
  {
    Int32 encrypted = 0;
    UString s;
    SetExtractErrorMessage(mode, encrypted, name, s);
    // ProgressDialog->Sync.AddError_Message(s);
  }
  */
  return S_OK;
}

HRESULT CUpdateCallbackAgent::ReportUpdateOperation(UInt32 op, const wchar_t *name, bool isDir)
{
  if (Callback2)
  {
    return Callback2->ReportUpdateOperation(op, name, BoolToInt(isDir));
  }
  return S_OK;
}

/*
HRESULT CUpdateCallbackAgent::SetPassword(const UString &
    #ifndef _NO_CRYPTO
    password
    #endif
    )
{
  #ifndef _NO_CRYPTO
  PasswordIsDefined = true;
  Password = password;
  #endif
  return S_OK;
}
*/

HRESULT CUpdateCallbackAgent::CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password)
{
  *password = NULL;
  *passwordIsDefined = BoolToInt(false);
  if (!_cryptoGetTextPassword)
  {
    if (!Callback)
      return S_OK;
    Callback.QueryInterface(IID_ICryptoGetTextPassword2, &_cryptoGetTextPassword);
    if (!_cryptoGetTextPassword)
      return S_OK;
  }
  return _cryptoGetTextPassword->CryptoGetTextPassword2(passwordIsDefined, password);
}

HRESULT CUpdateCallbackAgent::CryptoGetTextPassword(BSTR *password)
{
  *password = NULL;
  CMyComPtr<ICryptoGetTextPassword> getTextPassword;
  Callback.QueryInterface(IID_ICryptoGetTextPassword, &getTextPassword);
  if (!getTextPassword)
    return E_NOTIMPL;
  return getTextPassword->CryptoGetTextPassword(password);
}

HRESULT CUpdateCallbackAgent::ShowDeleteFile(const wchar_t *name, bool /* isDir */)
{
  return Callback->DeleteOperation(name);
}
