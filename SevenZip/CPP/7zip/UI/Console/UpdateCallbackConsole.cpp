// UpdateCallbackConsole.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileName.h"

#ifndef _7ZIP_ST
#include "../../../Windows/Synchronization.h"
#endif

#include "ConsoleClose.h"
#include "UserInputUtils.h"
#include "UpdateCallbackConsole.h"

using namespace NWindows;

#ifndef _7ZIP_ST
static NSynchronization::CCriticalSection g_CriticalSection;
#define MT_LOCK NSynchronization::CCriticalSectionLock lock(g_CriticalSection);
#else
#define MT_LOCK
#endif

static const wchar_t * const kEmptyFileAlias = L"[Content]";

static const char * const kOpenArchiveMessage = "Open archive: ";
static const char * const kCreatingArchiveMessage = "Creating archive: ";
static const char * const kUpdatingArchiveMessage = "Updating archive: ";
static const char * const kScanningMessage = "Scanning the drive:";

static const char * const kError = "ERROR: ";
static const char * const kWarning = "WARNING: ";

static HRESULT CheckBreak2()
{
  return NConsoleClose::TestBreakSignal() ? E_ABORT : S_OK;
}

HRESULT Print_OpenArchive_Props(CStdOutStream &so, const CCodecs *codecs, const CArchiveLink &arcLink);
HRESULT Print_OpenArchive_Error(CStdOutStream &so, const CCodecs *codecs, const CArchiveLink &arcLink);

void PrintErrorFlags(CStdOutStream &so, const char *s, UInt32 errorFlags);

void Print_ErrorFormatIndex_Warning(CStdOutStream *_so, const CCodecs *codecs, const CArc &arc);

HRESULT CUpdateCallbackConsole::OpenResult(
    const CCodecs *codecs, const CArchiveLink &arcLink,
    const wchar_t *name, HRESULT result)
{
  ClosePercents2();

  FOR_VECTOR (level, arcLink.Arcs)
  {
    const CArc &arc = arcLink.Arcs[level];
    const CArcErrorInfo &er = arc.ErrorInfo;
    
    UInt32 errorFlags = er.GetErrorFlags();

    if (errorFlags != 0 || !er.ErrorMessage.IsEmpty())
    {
      if (_se)
      {
        *_se << endl;
        if (level != 0)
          *_se << arc.Path << endl;
      }
      
      if (errorFlags != 0)
      {
        if (_se)
          PrintErrorFlags(*_se, "ERRORS:", errorFlags);
      }
      
      if (!er.ErrorMessage.IsEmpty())
      {
        if (_se)
          *_se << "ERRORS:" << endl << er.ErrorMessage << endl;
      }
      
      if (_se)
      {
        *_se << endl;
        _se->Flush();
      }
    }
    
    UInt32 warningFlags = er.GetWarningFlags();

    if (warningFlags != 0 || !er.WarningMessage.IsEmpty())
    {
      if (_so)
      {
        *_so << endl;
        if (level != 0)
          *_so << arc.Path << endl;
      }
      
      if (warningFlags != 0)
      {
        if (_so)
          PrintErrorFlags(*_so, "WARNINGS:", warningFlags);
      }
      
      if (!er.WarningMessage.IsEmpty())
      {
        if (_so)
          *_so << "WARNINGS:" << endl << er.WarningMessage << endl;
      }
      
      if (_so)
      {
        *_so << endl;
        if (NeedFlush)
          _so->Flush();
      }
    }

  
    if (er.ErrorFormatIndex >= 0)
    {
      if (_so)
      {
        Print_ErrorFormatIndex_Warning(_so, codecs, arc);
        if (NeedFlush)
          _so->Flush();
      }
    }
  }

  if (result == S_OK)
  {
    if (_so)
    {
      RINOK(Print_OpenArchive_Props(*_so, codecs, arcLink));
      *_so << endl;
    }
  }
  else
  {
    if (_so)
      _so->Flush();
    if (_se)
    {
      *_se << kError;
      _se->NormalizePrint_wstr(name);
      *_se << endl;
      HRESULT res = Print_OpenArchive_Error(*_se, codecs, arcLink);
      RINOK(res);
      _se->Flush();
    }
  }

  return S_OK;
}

HRESULT CUpdateCallbackConsole::StartScanning()
{
  if (_so)
    *_so << kScanningMessage << endl;
  _percent.Command = "Scan ";
  return S_OK;
}

HRESULT CUpdateCallbackConsole::ScanProgress(const CDirItemsStat &st, const FString &path, bool /* isDir */)
{
  if (NeedPercents())
  {
    _percent.Files = st.NumDirs + st.NumFiles + st.NumAltStreams;
    _percent.Completed = st.GetTotalBytes();
    _percent.FileName = fs2us(path);
    _percent.Print();
  }

  return CheckBreak();
}

void CCallbackConsoleBase::CommonError(const FString &path, DWORD systemError, bool isWarning)
{
  ClosePercents2();
  
  if (_se)
  {
    if (_so)
      _so->Flush();

    *_se << endl << (isWarning ? kWarning : kError)
        << NError::MyFormatMessage(systemError)
        << endl;
    _se->NormalizePrint_UString(fs2us(path));
    *_se << endl << endl;
    _se->Flush();
  }
}


HRESULT CCallbackConsoleBase::ScanError_Base(const FString &path, DWORD systemError)
{
  MT_LOCK

  ScanErrors.AddError(path, systemError);
  CommonError(path, systemError, true);

  return S_OK;
}

HRESULT CCallbackConsoleBase::OpenFileError_Base(const FString &path, DWORD systemError)
{
  MT_LOCK
  FailedFiles.AddError(path, systemError);
  NumNonOpenFiles++;
  /*
  if (systemError == ERROR_SHARING_VIOLATION)
  {
  */
    CommonError(path, systemError, true);
    return S_FALSE;
  /*
  }
  return systemError;
  */
}

HRESULT CCallbackConsoleBase::ReadingFileError_Base(const FString &path, DWORD systemError)
{
  MT_LOCK
  CommonError(path, systemError, false);
  return HRESULT_FROM_WIN32(systemError);
}

HRESULT CUpdateCallbackConsole::ScanError(const FString &path, DWORD systemError)
{
  return ScanError_Base(path, systemError);
}


static void PrintPropPair(AString &s, const char *name, UInt64 val)
{
  char temp[32];
  ConvertUInt64ToString(val, temp);
  s += name;
  s += ": ";
  s += temp;
}

void PrintSize_bytes_Smart(AString &s, UInt64 val);
void Print_DirItemsStat(AString &s, const CDirItemsStat &st);
void Print_DirItemsStat2(AString &s, const CDirItemsStat2 &st);

HRESULT CUpdateCallbackConsole::FinishScanning(const CDirItemsStat &st)
{
  if (NeedPercents())
  {
    _percent.ClosePrint(true);
    _percent.ClearCurState();
  }

  if (_so)
  {
    AString s;
    Print_DirItemsStat(s, st);
    *_so << s << endl << endl;
  }
  return S_OK;
}

static const char * const k_StdOut_ArcName = "StdOut";

HRESULT CUpdateCallbackConsole::StartOpenArchive(const wchar_t *name)
{
  if (_so)
  {
    *_so << kOpenArchiveMessage;
    if (name)
      *_so << name;
    else
      *_so << k_StdOut_ArcName;
    *_so << endl;
  }
  return S_OK;
}

HRESULT CUpdateCallbackConsole::StartArchive(const wchar_t *name, bool updating)
{
  if (NeedPercents())
    _percent.ClosePrint(true);
  
  _percent.ClearCurState();
  NumNonOpenFiles = 0;

  if (_so)
  {
    *_so << (updating ? kUpdatingArchiveMessage : kCreatingArchiveMessage);
    if (name)
      _so->NormalizePrint_wstr(name);
    else
      *_so << k_StdOut_ArcName;
   *_so << endl << endl;
  }
  return S_OK;
}

HRESULT CUpdateCallbackConsole::FinishArchive(const CFinishArchiveStat &st)
{
  ClosePercents2();

  if (_so)
  {
    AString s;
    // Print_UInt64_and_String(s, _percent.Files == 1 ? "file" : "files", _percent.Files);
    PrintPropPair(s, "Files read from disk", _percent.Files - NumNonOpenFiles);
    s.Add_LF();
    s += "Archive size: ";
    PrintSize_bytes_Smart(s, st.OutArcFileSize);
    s.Add_LF();
    *_so << endl;
    *_so << s;
    // *_so << endl;
  }

  return S_OK;
}

HRESULT CUpdateCallbackConsole::WriteSfx(const wchar_t *name, UInt64 size)
{
  if (_so)
  {
    *_so << "Write SFX: ";
    *_so << name;
    AString s (" : ");
    PrintSize_bytes_Smart(s, size);
    *_so << s << endl;
  }
  return S_OK;
}


HRESULT CUpdateCallbackConsole::DeletingAfterArchiving(const FString &path, bool /* isDir */)
{
  if (LogLevel > 0 && _so)
  {
    ClosePercents_for_so();
      
    if (!DeleteMessageWasShown)
    {
      if (_so)
        *_so << endl << ": Removing files after including to archive" << endl;
    }
   
    {
      {
        _tempA = "Removing";
        _tempA.Add_Space();
        *_so << _tempA;
        _tempU = fs2us(path);
        _so->Normalize_UString(_tempU);
        _so->PrintUString(_tempU, _tempA);
        *_so << endl;
        if (NeedFlush)
          _so->Flush();
      }
    }
  }

  if (!DeleteMessageWasShown)
  {
    if (NeedPercents())
    {
      _percent.ClearCurState();
    }
    DeleteMessageWasShown = true;
  }
  else
  {
    _percent.Files++;
  }

  if (NeedPercents())
  {
    // if (!FullLog)
    {
      _percent.Command = "Removing";
      _percent.FileName = fs2us(path);
    }
    _percent.Print();
  }

  return S_OK;
}


HRESULT CUpdateCallbackConsole::FinishDeletingAfterArchiving()
{
  ClosePercents2();
  if (_so && DeleteMessageWasShown)
    *_so << endl;
  return S_OK;
}

HRESULT CUpdateCallbackConsole::CheckBreak()
{
  return CheckBreak2();
}

/*
HRESULT CUpdateCallbackConsole::Finalize()
{
  // MT_LOCK
  return S_OK;
}
*/


void static PrintToDoStat(CStdOutStream *_so, const CDirItemsStat2 &stat, const char *name)
{
  AString s;
  Print_DirItemsStat2(s, stat);
  *_so << name << ": " << s << endl;
}

HRESULT CUpdateCallbackConsole::SetNumItems(const CArcToDoStat &stat)
{
  if (_so)
  {
    ClosePercents_for_so();
    if (!stat.DeleteData.IsEmpty())
    {
      *_so << endl;
      PrintToDoStat(_so, stat.DeleteData, "Delete data from archive");
    }
    if (!stat.OldData.IsEmpty())
      PrintToDoStat(_so, stat.OldData, "Keep old data in archive");
    // if (!stat.NewData.IsEmpty())
    {
      PrintToDoStat(_so, stat.NewData, "Add new data to archive");
    }
    *_so << endl;
  }
  return S_OK;
}

HRESULT CUpdateCallbackConsole::SetTotal(UInt64 size)
{
  MT_LOCK
  if (NeedPercents())
  {
    _percent.Total = size;
    _percent.Print();
  }
  return S_OK;
}

HRESULT CUpdateCallbackConsole::SetCompleted(const UInt64 *completeValue)
{
  MT_LOCK
  if (completeValue)
  {
    if (NeedPercents())
    {
      _percent.Completed = *completeValue;
      _percent.Print();
    }
  }
  return CheckBreak2();
}

HRESULT CUpdateCallbackConsole::SetRatioInfo(const UInt64 * /* inSize */, const UInt64 * /* outSize */)
{
  return CheckBreak2();
}

HRESULT CCallbackConsoleBase::PrintProgress(const wchar_t *name, bool isDir, const char *command, bool showInLog)
{
  MT_LOCK
  
  bool show2 = (showInLog && _so);

  if (show2)
  {
    ClosePercents_for_so();
    
    _tempA = command;
    if (name)
      _tempA.Add_Space();
    *_so << _tempA;

    _tempU.Empty();
    if (name)
    {
      _tempU = name;
      if (isDir)
        NWindows::NFile::NName::NormalizeDirPathPrefix(_tempU);
      _so->Normalize_UString(_tempU);
    }
    _so->PrintUString(_tempU, _tempA);
    *_so << endl;
    if (NeedFlush)
      _so->Flush();
  }

  if (NeedPercents())
  {
    if (PercentsNameLevel >= 1)
    {
      _percent.FileName.Empty();
      _percent.Command.Empty();
      if (PercentsNameLevel > 1 || !show2)
      {
        _percent.Command = command;
        if (name)
          _percent.FileName = name;
      }
    }
    _percent.Print();
  }
  
  return CheckBreak2();
}

HRESULT CUpdateCallbackConsole::GetStream(const wchar_t *name, bool isDir, bool isAnti, UInt32 mode)
{
  if (StdOutMode)
    return S_OK;
  
  if (!name || name[0] == 0)
    name = kEmptyFileAlias;

  unsigned requiredLevel = 1;
  
  const char *s;
  if (mode == NUpdateNotifyOp::kAdd ||
      mode == NUpdateNotifyOp::kUpdate)
  {
    if (isAnti)
      s = "Anti";
    else if (mode == NUpdateNotifyOp::kAdd)
      s = "+";
    else
      s = "U";
  }
  else
  {
    requiredLevel = 3;
    if (mode == NUpdateNotifyOp::kAnalyze)
      s = "A";
    else
      s = "Reading";
  }
  
  return PrintProgress(name, isDir, s, LogLevel >= requiredLevel);
}

HRESULT CUpdateCallbackConsole::OpenFileError(const FString &path, DWORD systemError)
{
  return OpenFileError_Base(path, systemError);
}

HRESULT CUpdateCallbackConsole::ReadingFileError(const FString &path, DWORD systemError)
{
  return ReadingFileError_Base(path, systemError);
}

HRESULT CUpdateCallbackConsole::SetOperationResult(Int32)
{
  MT_LOCK
  _percent.Files++;
  return S_OK;
}

void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, AString &dest);

HRESULT CUpdateCallbackConsole::ReportExtractResult(Int32 opRes, Int32 isEncrypted, const wchar_t *name)
{
  // if (StdOutMode) return S_OK;

  if (opRes != NArchive::NExtract::NOperationResult::kOK)
  {
    ClosePercents2();
    
    if (_se)
    {
      if (_so)
        _so->Flush();

      AString s;
      SetExtractErrorMessage(opRes, isEncrypted, s);
      *_se << s << " : " << endl;
      _se->NormalizePrint_wstr(name);
      *_se << endl << endl;
      _se->Flush();
    }
    return S_OK;
  }
  return S_OK;
}


HRESULT CUpdateCallbackConsole::ReportUpdateOperation(UInt32 op, const wchar_t *name, bool isDir)
{
  // if (StdOutMode) return S_OK;

  char temp[16];
  const char *s;
  
  unsigned requiredLevel = 1;
  
  switch (op)
  {
    case NUpdateNotifyOp::kAdd:       s = "+"; break;
    case NUpdateNotifyOp::kUpdate:    s = "U"; break;
    case NUpdateNotifyOp::kAnalyze:   s = "A"; requiredLevel = 3; break;
    case NUpdateNotifyOp::kReplicate: s = "="; requiredLevel = 3; break;
    case NUpdateNotifyOp::kRepack:    s = "R"; requiredLevel = 2; break;
    case NUpdateNotifyOp::kSkip:      s = "."; requiredLevel = 2; break;
    case NUpdateNotifyOp::kDelete:    s = "D"; requiredLevel = 3; break;
    case NUpdateNotifyOp::kHeader:    s = "Header creation"; requiredLevel = 100; break;
    default:
    {
      temp[0] = 'o';
      temp[1] = 'p';
      ConvertUInt64ToString(op, temp + 2);
      s = temp;
    }
  }

  return PrintProgress(name, isDir, s, LogLevel >= requiredLevel);
}

/*
HRESULT CUpdateCallbackConsole::SetPassword(const UString &
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

HRESULT CUpdateCallbackConsole::CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password)
{
  COM_TRY_BEGIN

  *password = NULL;

  #ifdef _NO_CRYPTO

  *passwordIsDefined = false;
  return S_OK;
  
  #else
  
  if (!PasswordIsDefined)
  {
    if (AskPassword)
    {
      RINOK(GetPassword_HRESULT(_so, Password));
      PasswordIsDefined = true;
    }
  }
  *passwordIsDefined = BoolToInt(PasswordIsDefined);
  return StringToBstr(Password, password);
  
  #endif

  COM_TRY_END
}

HRESULT CUpdateCallbackConsole::CryptoGetTextPassword(BSTR *password)
{
  COM_TRY_BEGIN
  
  *password = NULL;

  #ifdef _NO_CRYPTO

  return E_NOTIMPL;
  
  #else
  
  if (!PasswordIsDefined)
  {
    {
      RINOK(GetPassword_HRESULT(_so, Password))
      PasswordIsDefined = true;
    }
  }
  return StringToBstr(Password, password);
  
  #endif
  COM_TRY_END
}

HRESULT CUpdateCallbackConsole::ShowDeleteFile(const wchar_t *name, bool isDir)
{
  if (StdOutMode)
    return S_OK;
  
  if (LogLevel > 7)
  {
    if (!name || name[0] == 0)
      name = kEmptyFileAlias;
    return PrintProgress(name, isDir, "D", true);
  }
  return S_OK;
}
