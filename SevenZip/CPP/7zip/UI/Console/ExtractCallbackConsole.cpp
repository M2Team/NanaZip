// ExtractCallbackConsole.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/TimeUtils.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/PropVariantConv.h"

#ifndef _7ZIP_ST
#include "../../../Windows/Synchronization.h"
#endif

#include "../../Common/FilePathAutoRename.h"

#include "../Common/ExtractingFilePath.h"

#include "ConsoleClose.h"
#include "ExtractCallbackConsole.h"
#include "UserInputUtils.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static HRESULT CheckBreak2()
{
  return NConsoleClose::TestBreakSignal() ? E_ABORT : S_OK;
}

static const char * const kError = "ERROR: ";


void CExtractScanConsole::StartScanning()
{
  if (NeedPercents())
    _percent.Command = "Scan";
}

HRESULT CExtractScanConsole::ScanProgress(const CDirItemsStat &st, const FString &path, bool /* isDir */)
{
  if (NeedPercents())
  {
    _percent.Files = st.NumDirs + st.NumFiles;
    _percent.Completed = st.GetTotalBytes();
    _percent.FileName = fs2us(path);
    _percent.Print();
  }

  return CheckBreak2();
}

HRESULT CExtractScanConsole::ScanError(const FString &path, DWORD systemError)
{
  ClosePercentsAndFlush();
  
  if (_se)
  {
    *_se << endl << kError << NError::MyFormatMessage(systemError) << endl;
    _se->NormalizePrint_UString(fs2us(path));
    *_se << endl << endl;
    _se->Flush();
  }
  return HRESULT_FROM_WIN32(systemError);
}


void Print_UInt64_and_String(AString &s, UInt64 val, const char *name);
void Print_UInt64_and_String(AString &s, UInt64 val, const char *name)
{
  char temp[32];
  ConvertUInt64ToString(val, temp);
  s += temp;
  s.Add_Space();
  s += name;
}

void PrintSize_bytes_Smart(AString &s, UInt64 val);
void PrintSize_bytes_Smart(AString &s, UInt64 val)
{
  Print_UInt64_and_String(s, val, "bytes");

  if (val == 0)
    return;

  unsigned numBits = 10;
  char c = 'K';
  char temp[4] = { 'K', 'i', 'B', 0 };
       if (val >= ((UInt64)10 << 30)) { numBits = 30; c = 'G'; }
  else if (val >= ((UInt64)10 << 20)) { numBits = 20; c = 'M'; }
  temp[0] = c;
  s += " (";
  Print_UInt64_and_String(s, ((val + ((UInt64)1 << numBits) - 1) >> numBits), temp);
  s += ')';
}

static void PrintSize_bytes_Smart_comma(AString &s, UInt64 val)
{
  if (val == (UInt64)(Int64)-1)
    return;
  s += ", ";
  PrintSize_bytes_Smart(s, val);
}



void Print_DirItemsStat(AString &s, const CDirItemsStat &st);
void Print_DirItemsStat(AString &s, const CDirItemsStat &st)
{
  if (st.NumDirs != 0)
  {
    Print_UInt64_and_String(s, st.NumDirs, st.NumDirs == 1 ? "folder" : "folders");
    s += ", ";
  }
  Print_UInt64_and_String(s, st.NumFiles, st.NumFiles == 1 ? "file" : "files");
  PrintSize_bytes_Smart_comma(s, st.FilesSize);
  if (st.NumAltStreams != 0)
  {
    s.Add_LF();
    Print_UInt64_and_String(s, st.NumAltStreams, "alternate streams");
    PrintSize_bytes_Smart_comma(s, st.AltStreamsSize);
  }
}


void Print_DirItemsStat2(AString &s, const CDirItemsStat2 &st);
void Print_DirItemsStat2(AString &s, const CDirItemsStat2 &st)
{
  Print_DirItemsStat(s, (CDirItemsStat &)st);
  bool needLF = true;
  if (st.Anti_NumDirs != 0)
  {
    if (needLF)
      s.Add_LF();
    needLF = false;
    Print_UInt64_and_String(s, st.Anti_NumDirs, st.Anti_NumDirs == 1 ? "anti-folder" : "anti-folders");
  }
  if (st.Anti_NumFiles != 0)
  {
    if (needLF)
      s.Add_LF();
    else
      s += ", ";
    needLF = false;
    Print_UInt64_and_String(s, st.Anti_NumFiles, st.Anti_NumFiles == 1 ? "anti-file" : "anti-files");
  }
  if (st.Anti_NumAltStreams != 0)
  {
    if (needLF)
      s.Add_LF();
    else
      s += ", ";
    needLF = false;
    Print_UInt64_and_String(s, st.Anti_NumAltStreams, "anti-alternate-streams");
  }
}


void CExtractScanConsole::PrintStat(const CDirItemsStat &st)
{
  if (_so)
  {
    AString s;
    Print_DirItemsStat(s, st);
    *_so << s << endl;
  }
}







#ifndef _7ZIP_ST
static NSynchronization::CCriticalSection g_CriticalSection;
#define MT_LOCK NSynchronization::CCriticalSectionLock lock(g_CriticalSection);
#else
#define MT_LOCK
#endif


static const char * const kTestString    =  "T";
static const char * const kExtractString =  "-";
static const char * const kSkipString    =  ".";
static const char * const kReadString    =  "H";

// static const char * const kCantAutoRename = "cannot create file with auto name\n";
// static const char * const kCantRenameFile = "cannot rename existing file\n";
// static const char * const kCantDeleteOutputFile = "cannot delete output file ";

static const char * const kMemoryExceptionMessage = "Can't allocate required memory!";

static const char * const kExtracting = "Extracting archive: ";
static const char * const kTesting = "Testing archive: ";

static const char * const kEverythingIsOk = "Everything is Ok";
static const char * const kNoFiles = "No files to process";

static const char * const kUnsupportedMethod = "Unsupported Method";
static const char * const kCrcFailed = "CRC Failed";
static const char * const kCrcFailedEncrypted = "CRC Failed in encrypted file. Wrong password?";
static const char * const kDataError = "Data Error";
static const char * const kDataErrorEncrypted = "Data Error in encrypted file. Wrong password?";
static const char * const kUnavailableData = "Unavailable data";
static const char * const kUnexpectedEnd = "Unexpected end of data";
static const char * const kDataAfterEnd = "There are some data after the end of the payload data";
static const char * const kIsNotArc = "Is not archive";
static const char * const kHeadersError = "Headers Error";
static const char * const kWrongPassword = "Wrong password";

static const char * const k_ErrorFlagsMessages[] =
{
    "Is not archive"
  , "Headers Error"
  , "Headers Error in encrypted archive. Wrong password?"
  , "Unavailable start of archive"
  , "Unconfirmed start of archive"
  , "Unexpected end of archive"
  , "There are data after the end of archive"
  , "Unsupported method"
  , "Unsupported feature"
  , "Data Error"
  , "CRC Error"
};

STDMETHODIMP CExtractCallbackConsole::SetTotal(UInt64 size)
{
  MT_LOCK

  if (NeedPercents())
  {
    _percent.Total = size;
    _percent.Print();
  }
  return CheckBreak2();
}

STDMETHODIMP CExtractCallbackConsole::SetCompleted(const UInt64 *completeValue)
{
  MT_LOCK

  if (NeedPercents())
  {
    if (completeValue)
      _percent.Completed = *completeValue;
    _percent.Print();
  }
  return CheckBreak2();
}

static const char * const kTab = "  ";

static void PrintFileInfo(CStdOutStream *_so, const wchar_t *path, const FILETIME *ft, const UInt64 *size)
{
  *_so << kTab << "Path:     ";
  _so->NormalizePrint_wstr(path);
  *_so << endl;
  if (size && *size != (UInt64)(Int64)-1)
  {
    AString s;
    PrintSize_bytes_Smart(s, *size);
    *_so << kTab << "Size:     " << s << endl;
  }
  if (ft)
  {
    char temp[64];
    if (ConvertUtcFileTimeToString(*ft, temp, kTimestampPrintLevel_SEC))
      *_so << kTab << "Modified: " << temp << endl;
  }
}

STDMETHODIMP CExtractCallbackConsole::AskOverwrite(
    const wchar_t *existName, const FILETIME *existTime, const UInt64 *existSize,
    const wchar_t *newName, const FILETIME *newTime, const UInt64 *newSize,
    Int32 *answer)
{
  MT_LOCK
  
  RINOK(CheckBreak2());

  ClosePercentsAndFlush();

  if (_so)
  {
    *_so << endl << "Would you like to replace the existing file:\n";
    PrintFileInfo(_so, existName, existTime, existSize);
    *_so << "with the file from archive:\n";
    PrintFileInfo(_so, newName, newTime, newSize);
  }
  
  NUserAnswerMode::EEnum overwriteAnswer = ScanUserYesNoAllQuit(_so);
  
  switch (overwriteAnswer)
  {
    case NUserAnswerMode::kQuit:  return E_ABORT;
    case NUserAnswerMode::kNo:     *answer = NOverwriteAnswer::kNo; break;
    case NUserAnswerMode::kNoAll:  *answer = NOverwriteAnswer::kNoToAll; break;
    case NUserAnswerMode::kYesAll: *answer = NOverwriteAnswer::kYesToAll; break;
    case NUserAnswerMode::kYes:    *answer = NOverwriteAnswer::kYes; break;
    case NUserAnswerMode::kAutoRenameAll: *answer = NOverwriteAnswer::kAutoRename; break;
    case NUserAnswerMode::kEof:  return E_ABORT;
    case NUserAnswerMode::kError:  return E_FAIL;
    default: return E_FAIL;
  }
  
  if (_so)
  {
    *_so << endl;
    if (NeedFlush)
      _so->Flush();
  }
  
  return CheckBreak2();
}

STDMETHODIMP CExtractCallbackConsole::PrepareOperation(const wchar_t *name, Int32 isFolder, Int32 askExtractMode, const UInt64 *position)
{
  MT_LOCK
  
  _currentName = name;
  
  const char *s;
  unsigned requiredLevel = 1;

  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract: s = kExtractString; break;
    case NArchive::NExtract::NAskMode::kTest:    s = kTestString; break;
    case NArchive::NExtract::NAskMode::kSkip:    s = kSkipString; requiredLevel = 2; break;
    case NArchive::NExtract::NAskMode::kReadExternal: s = kReadString; requiredLevel = 0; break;
    default: s = "???"; requiredLevel = 2;
  };

  bool show2 = (LogLevel >= requiredLevel && _so);

  if (show2)
  {
    ClosePercents_for_so();
    
    _tempA = s;
    if (name)
      _tempA.Add_Space();
    *_so << _tempA;

    _tempU.Empty();
    if (name)
    {
      _tempU = name;
      _so->Normalize_UString(_tempU);
      // 21.04
      if (isFolder)
      {
        if (!_tempU.IsEmpty() && _tempU.Back() != WCHAR_PATH_SEPARATOR)
          _tempU.Add_PathSepar();
      }
    }
    _so->PrintUString(_tempU, _tempA);
    if (position)
      *_so << " <" << *position << ">";
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
        _percent.Command = s;
        if (name)
          _percent.FileName = name;
      }
    }
    _percent.Print();
  }

  return CheckBreak2();
}

STDMETHODIMP CExtractCallbackConsole::MessageError(const wchar_t *message)
{
  MT_LOCK
  
  RINOK(CheckBreak2());

  NumFileErrors_in_Current++;
  NumFileErrors++;

  ClosePercentsAndFlush();
  if (_se)
  {
    *_se << kError << message << endl;
    _se->Flush();
  }

  return CheckBreak2();
}

void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, AString &dest);
void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, AString &dest)
{
  dest.Empty();
    const char *s = NULL;
    
    switch (opRes)
    {
      case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
        s = kUnsupportedMethod;
        break;
      case NArchive::NExtract::NOperationResult::kCRCError:
        s = (encrypted ? kCrcFailedEncrypted : kCrcFailed);
        break;
      case NArchive::NExtract::NOperationResult::kDataError:
        s = (encrypted ? kDataErrorEncrypted : kDataError);
        break;
      case NArchive::NExtract::NOperationResult::kUnavailable:
        s = kUnavailableData;
        break;
      case NArchive::NExtract::NOperationResult::kUnexpectedEnd:
        s = kUnexpectedEnd;
        break;
      case NArchive::NExtract::NOperationResult::kDataAfterEnd:
        s = kDataAfterEnd;
        break;
      case NArchive::NExtract::NOperationResult::kIsNotArc:
        s = kIsNotArc;
        break;
      case NArchive::NExtract::NOperationResult::kHeadersError:
        s = kHeadersError;
        break;
      case NArchive::NExtract::NOperationResult::kWrongPassword:
        s = kWrongPassword;
        break;
    }
    
    dest += kError;
    if (s)
      dest += s;
    else
    {
      dest += "Error #";
      dest.Add_UInt32((UInt32)opRes);
    }
}

STDMETHODIMP CExtractCallbackConsole::SetOperationResult(Int32 opRes, Int32 encrypted)
{
  MT_LOCK
  
  if (opRes == NArchive::NExtract::NOperationResult::kOK)
  {
    if (NeedPercents())
    {
      _percent.Command.Empty();
      _percent.FileName.Empty();
      _percent.Files++;
    }
  }
  else
  {
    NumFileErrors_in_Current++;
    NumFileErrors++;
    
    if (_se)
    {
      ClosePercentsAndFlush();
      
      AString s;
      SetExtractErrorMessage(opRes, encrypted, s);
      
      *_se << s;
      if (!_currentName.IsEmpty())
      {
        *_se << " : ";
        _se->NormalizePrint_UString(_currentName);
      }
      *_se << endl;
      _se->Flush();
    }
  }
  
  return CheckBreak2();
}

STDMETHODIMP CExtractCallbackConsole::ReportExtractResult(Int32 opRes, Int32 encrypted, const wchar_t *name)
{
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
  {
    _currentName = name;
    return SetOperationResult(opRes, encrypted);
  }

  return CheckBreak2();
}



#ifndef _NO_CRYPTO

HRESULT CExtractCallbackConsole::SetPassword(const UString &password)
{
  PasswordIsDefined = true;
  Password = password;
  return S_OK;
}

STDMETHODIMP CExtractCallbackConsole::CryptoGetTextPassword(BSTR *password)
{
  COM_TRY_BEGIN
  MT_LOCK
  return Open_CryptoGetTextPassword(password);
  COM_TRY_END
}

#endif

HRESULT CExtractCallbackConsole::BeforeOpen(const wchar_t *name, bool testMode)
{
  RINOK(CheckBreak2());

  NumTryArcs++;
  ThereIsError_in_Current = false;
  ThereIsWarning_in_Current = false;
  NumFileErrors_in_Current = 0;
  
  ClosePercents_for_so();
  if (_so)
  {
    *_so << endl << (testMode ? kTesting : kExtracting);
    _so->NormalizePrint_wstr(name);
    *_so << endl;
  }

  if (NeedPercents())
    _percent.Command = "Open";
  return S_OK;
}

HRESULT Print_OpenArchive_Props(CStdOutStream &so, const CCodecs *codecs, const CArchiveLink &arcLink);
HRESULT Print_OpenArchive_Error(CStdOutStream &so, const CCodecs *codecs, const CArchiveLink &arcLink);

static AString GetOpenArcErrorMessage(UInt32 errorFlags)
{
  AString s;
  
  for (unsigned i = 0; i < ARRAY_SIZE(k_ErrorFlagsMessages); i++)
  {
    UInt32 f = (1 << i);
    if ((errorFlags & f) == 0)
      continue;
    const char *m = k_ErrorFlagsMessages[i];
    if (!s.IsEmpty())
      s.Add_LF();
    s += m;
    errorFlags &= ~f;
  }
  
  if (errorFlags != 0)
  {
    char sz[16];
    sz[0] = '0';
    sz[1] = 'x';
    ConvertUInt32ToHex(errorFlags, sz + 2);
    if (!s.IsEmpty())
      s.Add_LF();
    s += sz;
  }
  
  return s;
}

void PrintErrorFlags(CStdOutStream &so, const char *s, UInt32 errorFlags);
void PrintErrorFlags(CStdOutStream &so, const char *s, UInt32 errorFlags)
{
  if (errorFlags == 0)
    return;
  so << s << endl << GetOpenArcErrorMessage(errorFlags) << endl;
}

static void Add_Messsage_Pre_ArcType(UString &s, const char *pre, const wchar_t *arcType)
{
  s.Add_LF();
  s += pre;
  s += " as [";
  s += arcType;
  s += "] archive";
}

void Print_ErrorFormatIndex_Warning(CStdOutStream *_so, const CCodecs *codecs, const CArc &arc);
void Print_ErrorFormatIndex_Warning(CStdOutStream *_so, const CCodecs *codecs, const CArc &arc)
{
  const CArcErrorInfo &er = arc.ErrorInfo;
  
  *_so << "WARNING:\n";
  _so->NormalizePrint_UString(arc.Path);
  UString s;
  if (arc.FormatIndex == er.ErrorFormatIndex)
  {
    s.Add_LF();
    s += "The archive is open with offset";
  }
  else
  {
    Add_Messsage_Pre_ArcType(s, "Cannot open the file", codecs->GetFormatNamePtr(er.ErrorFormatIndex));
    Add_Messsage_Pre_ArcType(s, "The file is open", codecs->GetFormatNamePtr(arc.FormatIndex));
  }
  
  *_so << s << endl << endl;
}
        

HRESULT CExtractCallbackConsole::OpenResult(
    const CCodecs *codecs, const CArchiveLink &arcLink,
    const wchar_t *name, HRESULT result)
{
  ClosePercents();

  if (NeedPercents())
  {
    _percent.Files = 0;
    _percent.Command.Empty();
    _percent.FileName.Empty();
  }

 
  ClosePercentsAndFlush();

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
        {
          _se->NormalizePrint_UString(arc.Path);
          *_se << endl;
        }
      }
      
      if (errorFlags != 0)
      {
        if (_se)
          PrintErrorFlags(*_se, "ERRORS:", errorFlags);
        NumOpenArcErrors++;
        ThereIsError_in_Current = true;
      }
      
      if (!er.ErrorMessage.IsEmpty())
      {
        if (_se)
          *_se << "ERRORS:" << endl << er.ErrorMessage << endl;
        NumOpenArcErrors++;
        ThereIsError_in_Current = true;
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
        {
          _so->NormalizePrint_UString(arc.Path);
          *_so << endl;
        }
      }
      
      if (warningFlags != 0)
      {
        if (_so)
          PrintErrorFlags(*_so, "WARNINGS:", warningFlags);
        NumOpenArcWarnings++;
        ThereIsWarning_in_Current = true;
      }
      
      if (!er.WarningMessage.IsEmpty())
      {
        if (_so)
          *_so << "WARNINGS:" << endl << er.WarningMessage << endl;
        NumOpenArcWarnings++;
        ThereIsWarning_in_Current = true;
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
      ThereIsWarning_in_Current = true;
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
    NumCantOpenArcs++;
    if (_so)
      _so->Flush();
    if (_se)
    {
      *_se << kError;
      _se->NormalizePrint_wstr(name);
      *_se << endl;
      HRESULT res = Print_OpenArchive_Error(*_se, codecs, arcLink);
      RINOK(res);
      if (result == S_FALSE)
      {
      }
      else
      {
        if (result == E_OUTOFMEMORY)
          *_se << "Can't allocate required memory";
        else
          *_se << NError::MyFormatMessage(result);
        *_se << endl;
      }
      _se->Flush();
    }
  }
  
  
  return CheckBreak2();
}
  
HRESULT CExtractCallbackConsole::ThereAreNoFiles()
{
  ClosePercents_for_so();

  if (_so)
  {
    *_so << endl << kNoFiles << endl;
    if (NeedFlush)
      _so->Flush();
  }
  return CheckBreak2();
}

HRESULT CExtractCallbackConsole::ExtractResult(HRESULT result)
{
  MT_LOCK
  
  if (NeedPercents())
  {
    _percent.ClosePrint(true);
    _percent.Command.Empty();
    _percent.FileName.Empty();
  }

  if (_so)
    _so->Flush();

  if (result == S_OK)
  {
    if (NumFileErrors_in_Current == 0 && !ThereIsError_in_Current)
    {
      if (ThereIsWarning_in_Current)
        NumArcsWithWarnings++;
      else
        NumOkArcs++;
      if (_so)
        *_so << kEverythingIsOk << endl;
    }
    else
    {
      NumArcsWithError++;
      if (_so)
      {
        *_so << endl;
        if (NumFileErrors_in_Current != 0)
          *_so << "Sub items Errors: " << NumFileErrors_in_Current << endl;
      }
    }
    if (_so && NeedFlush)
      _so->Flush();
  }
  else
  {
    NumArcsWithError++;
    if (result == E_ABORT
        || result == HRESULT_FROM_WIN32(ERROR_DISK_FULL)
        )
      return result;
    
    if (_se)
    {
      *_se << endl << kError;
      if (result == E_OUTOFMEMORY)
        *_se << kMemoryExceptionMessage;
      else
        *_se << NError::MyFormatMessage(result);
      *_se << endl;
      _se->Flush();
    }
  }

  return CheckBreak2();
}
