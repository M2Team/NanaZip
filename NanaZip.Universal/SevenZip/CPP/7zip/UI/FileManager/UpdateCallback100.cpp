// UpdateCallback100.cpp

#include "StdAfx.h"

#include "../../../Windows/ErrorMsg.h"

#include "../GUI/resource3.h"

#include "LangUtils.h"
#include "UpdateCallback100.h"

Z7_COM7F_IMF(CUpdateCallback100Imp::ScanProgress(UInt64 /* numFolders */, UInt64 numFiles, UInt64 totalSize, const wchar_t *path, Int32 /* isDir */))
{
  return ProgressDialog->Sync.ScanProgress(numFiles, totalSize, us2fs(path));
}

Z7_COM7F_IMF(CUpdateCallback100Imp::ScanError(const wchar_t *path, HRESULT errorCode))
{
  ProgressDialog->Sync.AddError_Code_Name(errorCode, path);
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::SetNumFiles(UInt64 numFiles))
{
  return ProgressDialog->Sync.Set_NumFilesTotal(numFiles);
}

Z7_COM7F_IMF(CUpdateCallback100Imp::SetTotal(UInt64 size))
{
  ProgressDialog->Sync.Set_NumBytesTotal(size);
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::SetCompleted(const UInt64 *completed))
{
  return ProgressDialog->Sync.Set_NumBytesCur(completed);
}

Z7_COM7F_IMF(CUpdateCallback100Imp::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize))
{
  ProgressDialog->Sync.Set_Ratio(inSize, outSize);
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::CompressOperation(const wchar_t *name))
{
  return SetOperation_Base(NUpdateNotifyOp::kAdd, name, false);
}

Z7_COM7F_IMF(CUpdateCallback100Imp::DeleteOperation(const wchar_t *name))
{
  return SetOperation_Base(NUpdateNotifyOp::kDelete, name, false);
}

Z7_COM7F_IMF(CUpdateCallback100Imp::OperationResult(Int32 /* operationResult */))
{
  ProgressDialog->Sync.Set_NumFilesCur(++NumFiles);
  return S_OK;
}

void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, const wchar_t *fileName, UString &s);

Z7_COM7F_IMF(CUpdateCallback100Imp::ReportExtractResult(Int32 opRes, Int32 isEncrypted, const wchar_t *name))
{
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
  {
    UString s;
    SetExtractErrorMessage(opRes, isEncrypted, name, s);
    ProgressDialog->Sync.AddError_Message(s);
  }
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::ReportUpdateOperation(UInt32 notifyOp, const wchar_t *name, Int32 isDir))
{
  return SetOperation_Base(notifyOp, name, IntToBool(isDir));
}

Z7_COM7F_IMF(CUpdateCallback100Imp::UpdateErrorMessage(const wchar_t *message))
{
  ProgressDialog->Sync.AddError_Message(message);
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::OpenFileError(const wchar_t *path, HRESULT errorCode))
{
  ProgressDialog->Sync.AddError_Code_Name(errorCode, path);
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::ReadingFileError(const wchar_t *path, HRESULT errorCode))
{
  ProgressDialog->Sync.AddError_Code_Name(errorCode, path);
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password))
{
  *password = NULL;
  *passwordIsDefined = BoolToInt(PasswordIsDefined);
  if (!PasswordIsDefined)
    return S_OK;
  return StringToBstr(Password, password);
}

Z7_COM7F_IMF(CUpdateCallback100Imp::SetTotal(const UInt64 * /* files */, const UInt64 * /* bytes */))
{
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback100Imp::SetCompleted(const UInt64 * /* files */, const UInt64 * /* bytes */))
{
  return ProgressDialog->Sync.CheckStop();
}


Z7_COM7F_IMF(CUpdateCallback100Imp::MoveArc_Start(const wchar_t *srcTempPath, const wchar_t *destFinalPath, UInt64 size, Int32 updateMode))
{
  return MoveArc_Start_Base(srcTempPath, destFinalPath, size, updateMode);
}

Z7_COM7F_IMF(CUpdateCallback100Imp::MoveArc_Progress(UInt64 totalSize, UInt64 currentSize))
{
  return MoveArc_Progress_Base(totalSize, currentSize);
}

Z7_COM7F_IMF(CUpdateCallback100Imp::MoveArc_Finish())
{
  return MoveArc_Finish_Base();
}

Z7_COM7F_IMF(CUpdateCallback100Imp::Before_ArcReopen())
{
  ProgressDialog->Sync.Clear_Stop_Status();
  return S_OK;
}


Z7_COM7F_IMF(CUpdateCallback100Imp::CryptoGetTextPassword(BSTR *password))
{
  *password = NULL;
  if (!PasswordIsDefined)
  {
    RINOK(ShowAskPasswordDialog())
  }
  return StringToBstr(Password, password);
}
