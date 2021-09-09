// UpdateCallbackGUI.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/PropVariant.h"

#include "../FileManager/FormatUtils.h"
#include "../FileManager/LangUtils.h"

#include "../FileManager/resourceGui.h"

#include "resource2.h"

#include "UpdateCallbackGUI.h"

using namespace NWindows;

// CUpdateCallbackGUI::~CUpdateCallbackGUI() {}

void CUpdateCallbackGUI::Init()
{
  CUpdateCallbackGUI2::Init();
  FailedFiles.Clear();
}

void OpenResult_GUI(UString &s, const CCodecs *codecs, const CArchiveLink &arcLink, const wchar_t *name, HRESULT result);

HRESULT CUpdateCallbackGUI::OpenResult(
    const CCodecs *codecs, const CArchiveLink &arcLink, const wchar_t *name, HRESULT result)
{
  UString s;
  OpenResult_GUI(s, codecs, arcLink, name, result);
  if (!s.IsEmpty())
  {
    ProgressDialog->Sync.AddError_Message(s);
  }

  return S_OK;
}

HRESULT CUpdateCallbackGUI::StartScanning()
{
  CProgressSync &sync = ProgressDialog->Sync;
  sync.Set_Status(LangString(IDS_SCANNING));
  return S_OK;
}

HRESULT CUpdateCallbackGUI::ScanError(const FString &path, DWORD systemError)
{
  FailedFiles.Add(path);
  ProgressDialog->Sync.AddError_Code_Name(systemError, fs2us(path));
  return S_OK;
}

HRESULT CUpdateCallbackGUI::FinishScanning(const CDirItemsStat &st)
{
  CProgressSync &sync = ProgressDialog->Sync;
  RINOK(ProgressDialog->Sync.ScanProgress(st.NumFiles + st.NumAltStreams,
      st.GetTotalBytes(), FString(), true));
  sync.Set_Status(L"");
  return S_OK;
}

HRESULT CUpdateCallbackGUI::StartArchive(const wchar_t *name, bool /* updating */)
{
  CProgressSync &sync = ProgressDialog->Sync;
  sync.Set_Status(LangString(IDS_PROGRESS_COMPRESSING));
  sync.Set_TitleFileName(name);
  return S_OK;
}

HRESULT CUpdateCallbackGUI::FinishArchive(const CFinishArchiveStat & /* st */)
{
  CProgressSync &sync = ProgressDialog->Sync;
  sync.Set_Status(L"");
  return S_OK;
}

HRESULT CUpdateCallbackGUI::CheckBreak()
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackGUI::ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)
{
  return ProgressDialog->Sync.ScanProgress(st.NumFiles + st.NumAltStreams,
      st.GetTotalBytes(), path, isDir);
}

/*
HRESULT CUpdateCallbackGUI::Finalize()
{
  return S_OK;
}
*/

HRESULT CUpdateCallbackGUI::SetNumItems(const CArcToDoStat &stat)
{
  ProgressDialog->Sync.Set_NumFilesTotal(stat.Get_NumDataItems_Total());
  return S_OK;
}

HRESULT CUpdateCallbackGUI::SetTotal(UInt64 total)
{
  ProgressDialog->Sync.Set_NumBytesTotal(total);
  return S_OK;
}

HRESULT CUpdateCallbackGUI::SetCompleted(const UInt64 *completed)
{
  return ProgressDialog->Sync.Set_NumBytesCur(completed);
}

HRESULT CUpdateCallbackGUI::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  ProgressDialog->Sync.Set_Ratio(inSize, outSize);
  return CheckBreak();
}

HRESULT CUpdateCallbackGUI::GetStream(const wchar_t *name, bool isDir, bool /* isAnti */, UInt32 mode)
{
  return SetOperation_Base(mode, name, isDir);
}

HRESULT CUpdateCallbackGUI::OpenFileError(const FString &path, DWORD systemError)
{
  FailedFiles.Add(path);
  // if (systemError == ERROR_SHARING_VIOLATION)
  {
    ProgressDialog->Sync.AddError_Code_Name(systemError, fs2us(path));
    return S_FALSE;
  }
  // return systemError;
}

HRESULT CUpdateCallbackGUI::SetOperationResult(Int32 /* operationResult */)
{
  NumFiles++;
  ProgressDialog->Sync.Set_NumFilesCur(NumFiles);
  return S_OK;
}

void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, const wchar_t *fileName, UString &s);

HRESULT CUpdateCallbackGUI::ReportExtractResult(Int32 opRes, Int32 isEncrypted, const wchar_t *name)
{
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
  {
    UString s;
    SetExtractErrorMessage(opRes, isEncrypted, name, s);
    ProgressDialog->Sync.AddError_Message(s);
  }
  return S_OK;
}

HRESULT CUpdateCallbackGUI::ReportUpdateOpeartion(UInt32 op, const wchar_t *name, bool isDir)
{
  return SetOperation_Base(op, name, isDir);
}

HRESULT CUpdateCallbackGUI::CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password)
{
  *password = NULL;
  if (passwordIsDefined)
    *passwordIsDefined = BoolToInt(PasswordIsDefined);
  if (!PasswordIsDefined)
  {
    if (AskPassword)
    {
      RINOK(ShowAskPasswordDialog())
    }
  }
  if (passwordIsDefined)
    *passwordIsDefined = BoolToInt(PasswordIsDefined);
  return StringToBstr(Password, password);
}

HRESULT CUpdateCallbackGUI::CryptoGetTextPassword(BSTR *password)
{
  return CryptoGetTextPassword2(NULL, password);
}

/*
It doesn't work, since main stream waits Dialog
HRESULT CUpdateCallbackGUI::CloseProgress()
{
  ProgressDialog->MyClose();
  return S_OK;
}
*/


HRESULT CUpdateCallbackGUI::Open_CheckBreak()
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackGUI::Open_SetTotal(const UInt64 * /* numFiles */, const UInt64 * /* numBytes */)
{
  // if (numFiles != NULL) ProgressDialog->Sync.SetNumFilesTotal(*numFiles);
  return S_OK;
}

HRESULT CUpdateCallbackGUI::Open_SetCompleted(const UInt64 * /* numFiles */, const UInt64 * /* numBytes */)
{
  return ProgressDialog->Sync.CheckStop();
}

#ifndef _NO_CRYPTO

HRESULT CUpdateCallbackGUI::Open_CryptoGetTextPassword(BSTR *password)
{
  PasswordWasAsked = true;
  return CryptoGetTextPassword2(NULL, password);
}

/*
HRESULT CUpdateCallbackGUI::Open_GetPasswordIfAny(bool &passwordIsDefined, UString &password)
{
  passwordIsDefined = PasswordIsDefined;
  password = Password;
  return S_OK;
}

bool CUpdateCallbackGUI::Open_WasPasswordAsked()
{
  return PasswordWasAsked;
}

void CUpdateCallbackGUI::Open_Clear_PasswordWasAsked_Flag()
{
  PasswordWasAsked = false;
}
*/

HRESULT CUpdateCallbackGUI::ShowDeleteFile(const wchar_t *name, bool isDir)
{
  return SetOperation_Base(NUpdateNotifyOp::kDelete, name, isDir);
}

HRESULT CUpdateCallbackGUI::FinishDeletingAfterArchiving()
{
  // ClosePercents2();
  return S_OK;
}

HRESULT CUpdateCallbackGUI::DeletingAfterArchiving(const FString &path, bool isDir)
{
  return ProgressDialog->Sync.Set_Status2(_lang_Removing, fs2us(path), isDir);
}

HRESULT CUpdateCallbackGUI::StartOpenArchive(const wchar_t * /* name */)
{
  return S_OK;
}

HRESULT CUpdateCallbackGUI::ReadingFileError(const FString &path, DWORD systemError)
{
  FailedFiles.Add(path);
  ProgressDialog->Sync.AddError_Code_Name(systemError, fs2us(path));
  return S_OK;
}

HRESULT CUpdateCallbackGUI::WriteSfx(const wchar_t * /* name */, UInt64 /* size */)
{
  CProgressSync &sync = ProgressDialog->Sync;
  sync.Set_Status(L"WriteSfx");
  return S_OK;
}

HRESULT CUpdateCallbackGUI::Open_Finished()
{
  // ClosePercents();
  return S_OK;
}

#endif
