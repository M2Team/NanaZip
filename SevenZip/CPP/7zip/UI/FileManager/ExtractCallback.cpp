// ExtractCallback.cpp

#include "StdAfx.h"


#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/Lang.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/PropVariantConv.h"

#include "../../Common/FilePathAutoRename.h"
#include "../../Common/StreamUtils.h"
#include "../Common/ExtractingFilePath.h"

#ifndef _SFX
#include "../Common/ZipRegistry.h"
#endif

#include "../GUI/ExtractRes.h"
#include "resourceGui.h"

#include "ExtractCallback.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "OverwriteDialog.h"
#ifndef _NO_CRYPTO
#include "PasswordDialog.h"
#endif
#include "PropertyName.h"

using namespace NWindows;
using namespace NFile;
using namespace NFind;

CExtractCallbackImp::~CExtractCallbackImp() {}

void CExtractCallbackImp::Init()
{
  _lang_Extracting = LangString(IDS_PROGRESS_EXTRACTING);
  _lang_Testing = LangString(IDS_PROGRESS_TESTING);
  _lang_Skipping = LangString(IDS_PROGRESS_SKIPPING);
  _lang_Reading = "Reading";

  NumArchiveErrors = 0;
  ThereAreMessageErrors = false;
  #ifndef _SFX
  NumFolders = NumFiles = 0;
  NeedAddFile = false;
  #endif
}

void CExtractCallbackImp::AddError_Message(LPCWSTR s)
{
  ThereAreMessageErrors = true;
  ProgressDialog->Sync.AddError_Message(s);
}

#ifndef _SFX

STDMETHODIMP CExtractCallbackImp::SetNumFiles(UInt64
  #ifndef _SFX
  numFiles
  #endif
  )
{
  #ifndef _SFX
  ProgressDialog->Sync.Set_NumFilesTotal(numFiles);
  #endif
  return S_OK;
}

#endif

STDMETHODIMP CExtractCallbackImp::SetTotal(UInt64 total)
{
  ProgressDialog->Sync.Set_NumBytesTotal(total);
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::SetCompleted(const UInt64 *value)
{
  return ProgressDialog->Sync.Set_NumBytesCur(value);
}

HRESULT CExtractCallbackImp::Open_CheckBreak()
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CExtractCallbackImp::Open_SetTotal(const UInt64 *files, const UInt64 *bytes)
{
  HRESULT res = S_OK;
  if (!MultiArcMode)
  {
    if (files)
    {
      _totalFilesDefined = true;
      // res = ProgressDialog->Sync.Set_NumFilesTotal(*files);
    }
    else
      _totalFilesDefined = false;

    if (bytes)
    {
      _totalBytesDefined = true;
      ProgressDialog->Sync.Set_NumBytesTotal(*bytes);
    }
    else
      _totalBytesDefined = false;
  }

  return res;
}

HRESULT CExtractCallbackImp::Open_SetCompleted(const UInt64 *files, const UInt64 *bytes)
{
  if (!MultiArcMode)
  {
    if (files)
    {
      ProgressDialog->Sync.Set_NumFilesCur(*files);
    }

    if (bytes)
    {
    }
  }

  return ProgressDialog->Sync.CheckStop();
}

HRESULT CExtractCallbackImp::Open_Finished()
{
  return ProgressDialog->Sync.CheckStop();
}

#ifndef _NO_CRYPTO

HRESULT CExtractCallbackImp::Open_CryptoGetTextPassword(BSTR *password)
{
  return CryptoGetTextPassword(password);
}

/*
HRESULT CExtractCallbackImp::Open_GetPasswordIfAny(bool &passwordIsDefined, UString &password)
{
  passwordIsDefined = PasswordIsDefined;
  password = Password;
  return S_OK;
}

bool CExtractCallbackImp::Open_WasPasswordAsked()
{
  return PasswordWasAsked;
}

void CExtractCallbackImp::Open_Clear_PasswordWasAsked_Flag()
{
  PasswordWasAsked = false;
}
*/

#endif


#ifndef _SFX
STDMETHODIMP CExtractCallbackImp::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  ProgressDialog->Sync.Set_Ratio(inSize, outSize);
  return S_OK;
}
#endif

/*
STDMETHODIMP CExtractCallbackImp::SetTotalFiles(UInt64 total)
{
  ProgressDialog->Sync.SetNumFilesTotal(total);
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::SetCompletedFiles(const UInt64 *value)
{
  if (value != NULL)
    ProgressDialog->Sync.SetNumFilesCur(*value);
  return S_OK;
}
*/

STDMETHODIMP CExtractCallbackImp::AskOverwrite(
    const wchar_t *existName, const FILETIME *existTime, const UInt64 *existSize,
    const wchar_t *newName, const FILETIME *newTime, const UInt64 *newSize,
    Int32 *answer)
{
  COverwriteDialog dialog;

  dialog.OldFileInfo.SetTime(existTime);
  dialog.OldFileInfo.SetSize(existSize);
  dialog.OldFileInfo.Name = existName;

  dialog.NewFileInfo.SetTime(newTime);
  dialog.NewFileInfo.SetSize(newSize);
  dialog.NewFileInfo.Name = newName;
  
  ProgressDialog->WaitCreating();
  INT_PTR writeAnswer = dialog.Create(*ProgressDialog);
  
  switch (writeAnswer)
  {
    case IDCANCEL:        *answer = NOverwriteAnswer::kCancel; return E_ABORT;
    case IDYES:           *answer = NOverwriteAnswer::kYes; break;
    case IDNO:            *answer = NOverwriteAnswer::kNo; break;
    case IDB_YES_TO_ALL:  *answer = NOverwriteAnswer::kYesToAll; break;
    case IDB_NO_TO_ALL:   *answer = NOverwriteAnswer::kNoToAll; break;
    case IDB_AUTO_RENAME: *answer = NOverwriteAnswer::kAutoRename; break;
    default: return E_FAIL;
  }
  return S_OK;
}


STDMETHODIMP CExtractCallbackImp::PrepareOperation(const wchar_t *name, Int32 isFolder, Int32 askExtractMode, const UInt64 * /* position */)
{
  _isFolder = IntToBool(isFolder);
  _currentFilePath = name;

  const UString *msg = &_lang_Empty;
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract: msg = &_lang_Extracting; break;
    case NArchive::NExtract::NAskMode::kTest:    msg = &_lang_Testing; break;
    case NArchive::NExtract::NAskMode::kSkip:    msg = &_lang_Skipping; break;
    case NArchive::NExtract::NAskMode::kReadExternal: msg = &_lang_Reading; break;
    // default: s = "Unknown operation";
  }

  return ProgressDialog->Sync.Set_Status2(*msg, name, IntToBool(isFolder));
}

STDMETHODIMP CExtractCallbackImp::MessageError(const wchar_t *s)
{
  AddError_Message(s);
  return S_OK;
}

HRESULT CExtractCallbackImp::MessageError(const char *message, const FString &path)
{
  ThereAreMessageErrors = true;
  ProgressDialog->Sync.AddError_Message_Name(GetUnicodeString(message), fs2us(path));
  return S_OK;
}

#ifndef _SFX

STDMETHODIMP CExtractCallbackImp::ShowMessage(const wchar_t *s)
{
  AddError_Message(s);
  return S_OK;
}

#endif

void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, const wchar_t *fileName, UString &s);
void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, const wchar_t *fileName, UString &s)
{
  s.Empty();

  if (opRes == NArchive::NExtract::NOperationResult::kOK)
    return;

  UINT messageID = 0;
  UINT id = 0;

  switch (opRes)
  {
    case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
      messageID = IDS_EXTRACT_MESSAGE_UNSUPPORTED_METHOD;
      id = IDS_EXTRACT_MSG_UNSUPPORTED_METHOD;
      break;
    case NArchive::NExtract::NOperationResult::kDataError:
      messageID = encrypted ?
          IDS_EXTRACT_MESSAGE_DATA_ERROR_ENCRYPTED:
          IDS_EXTRACT_MESSAGE_DATA_ERROR;
      id = IDS_EXTRACT_MSG_DATA_ERROR;
      break;
    case NArchive::NExtract::NOperationResult::kCRCError:
      messageID = encrypted ?
          IDS_EXTRACT_MESSAGE_CRC_ERROR_ENCRYPTED:
          IDS_EXTRACT_MESSAGE_CRC_ERROR;
      id = IDS_EXTRACT_MSG_CRC_ERROR;
      break;
    case NArchive::NExtract::NOperationResult::kUnavailable:
      id = IDS_EXTRACT_MSG_UNAVAILABLE_DATA;
      break;
    case NArchive::NExtract::NOperationResult::kUnexpectedEnd:
      id = IDS_EXTRACT_MSG_UEXPECTED_END;
      break;
    case NArchive::NExtract::NOperationResult::kDataAfterEnd:
      id = IDS_EXTRACT_MSG_DATA_AFTER_END;
      break;
    case NArchive::NExtract::NOperationResult::kIsNotArc:
      id = IDS_EXTRACT_MSG_IS_NOT_ARC;
      break;
    case NArchive::NExtract::NOperationResult::kHeadersError:
      id = IDS_EXTRACT_MSG_HEADERS_ERROR;
      break;
    case NArchive::NExtract::NOperationResult::kWrongPassword:
      id = IDS_EXTRACT_MSG_WRONG_PSW_CLAIM;
      break;
    /*
    default:
      messageID = IDS_EXTRACT_MESSAGE_UNKNOWN_ERROR;
      break;
    */
  }

  UString msg;
  UString msgOld;

  #ifndef _SFX
  if (id != 0)
    LangString_OnlyFromLangFile(id, msg);
  if (messageID != 0 && msg.IsEmpty())
    LangString_OnlyFromLangFile(messageID, msgOld);
  #endif

  if (msg.IsEmpty() && !msgOld.IsEmpty())
    s = MyFormatNew(msgOld, fileName);
  else
  {
    if (msg.IsEmpty() && id != 0)
      LangString(id, msg);
    if (!msg.IsEmpty())
      s += msg;
    else
    {
      s += "Error #";
      s.Add_UInt32(opRes);
    }

    if (encrypted && opRes != NArchive::NExtract::NOperationResult::kWrongPassword)
    {
      // s += " : ";
      // AddLangString(s, IDS_EXTRACT_MSG_ENCRYPTED);
      s += " : ";
      AddLangString(s, IDS_EXTRACT_MSG_WRONG_PSW_GUESS);
    }
    s += " : ";
    s += fileName;
  }
}

STDMETHODIMP CExtractCallbackImp::SetOperationResult(Int32 opRes, Int32 encrypted)
{
  switch (opRes)
  {
    case NArchive::NExtract::NOperationResult::kOK:
      break;
    default:
    {
      UString s;
      SetExtractErrorMessage(opRes, encrypted, _currentFilePath, s);
      Add_ArchiveName_Error();
      AddError_Message(s);
    }
  }
  
  #ifndef _SFX
  if (_isFolder)
    NumFolders++;
  else
    NumFiles++;
  ProgressDialog->Sync.Set_NumFilesCur(NumFiles);
  #endif
  
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::ReportExtractResult(Int32 opRes, Int32 encrypted, const wchar_t *name)
{
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
  {
    UString s;
    SetExtractErrorMessage(opRes, encrypted, name, s);
    Add_ArchiveName_Error();
    AddError_Message(s);
  }
  return S_OK;
}

////////////////////////////////////////
// IExtractCallbackUI

HRESULT CExtractCallbackImp::BeforeOpen(const wchar_t *name, bool /* testMode */)
{
  #ifndef _SFX
  RINOK(ProgressDialog->Sync.CheckStop());
  ProgressDialog->Sync.Set_TitleFileName(name);
  #endif
  _currentArchivePath = name;
  return S_OK;
}

HRESULT CExtractCallbackImp::SetCurrentFilePath2(const wchar_t *path)
{
  _currentFilePath = path;
  #ifndef _SFX
  ProgressDialog->Sync.Set_FilePath(path);
  #endif
  return S_OK;
}

#ifndef _SFX

HRESULT CExtractCallbackImp::SetCurrentFilePath(const wchar_t *path)
{
  #ifndef _SFX
  if (NeedAddFile)
    NumFiles++;
  NeedAddFile = true;
  ProgressDialog->Sync.Set_NumFilesCur(NumFiles);
  #endif
  return SetCurrentFilePath2(path);
}

#endif

UString HResultToMessage(HRESULT errorCode);

static const UInt32 k_ErrorFlagsIds[] =
{
  IDS_EXTRACT_MSG_IS_NOT_ARC,
  IDS_EXTRACT_MSG_HEADERS_ERROR,
  IDS_EXTRACT_MSG_HEADERS_ERROR,
  IDS_OPEN_MSG_UNAVAILABLE_START,
  IDS_OPEN_MSG_UNCONFIRMED_START,
  IDS_EXTRACT_MSG_UEXPECTED_END,
  IDS_EXTRACT_MSG_DATA_AFTER_END,
  IDS_EXTRACT_MSG_UNSUPPORTED_METHOD,
  IDS_OPEN_MSG_UNSUPPORTED_FEATURE,
  IDS_EXTRACT_MSG_DATA_ERROR,
  IDS_EXTRACT_MSG_CRC_ERROR
};

static void AddNewLineString(UString &s, const UString &m)
{
  s += m;
  s.Add_LF();
}

UString GetOpenArcErrorMessage(UInt32 errorFlags);
UString GetOpenArcErrorMessage(UInt32 errorFlags)
{
  UString s;

  for (unsigned i = 0; i < ARRAY_SIZE(k_ErrorFlagsIds); i++)
  {
    UInt32 f = ((UInt32)1 << i);
    if ((errorFlags & f) == 0)
      continue;
    UInt32 id = k_ErrorFlagsIds[i];
    UString m = LangString(id);
    if (m.IsEmpty())
      continue;
    if (f == kpv_ErrorFlags_EncryptedHeadersError)
    {
      m += " : ";
      AddLangString(m, IDS_EXTRACT_MSG_WRONG_PSW_GUESS);
    }
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

static void ErrorInfo_Print(UString &s, const CArcErrorInfo &er)
{
  UInt32 errorFlags = er.GetErrorFlags();
  UInt32 warningFlags = er.GetWarningFlags();

  if (errorFlags != 0)
    AddNewLineString(s, GetOpenArcErrorMessage(errorFlags));
      
  if (!er.ErrorMessage.IsEmpty())
    AddNewLineString(s, er.ErrorMessage);
  
  if (warningFlags != 0)
  {
    s += GetNameOfProperty(kpidWarningFlags, L"Warnings");
    s += ":";
    s.Add_LF();
    AddNewLineString(s, GetOpenArcErrorMessage(warningFlags));
  }
  
  if (!er.WarningMessage.IsEmpty())
  {
    s += GetNameOfProperty(kpidWarning, L"Warning");
    s += ": ";
    s += er.WarningMessage;
    s.Add_LF();
  }
}

static UString GetBracedType(const wchar_t *type)
{
  UString s ('[');
  s += type;
  s += ']';
  return s;
}

void OpenResult_GUI(UString &s, const CCodecs *codecs, const CArchiveLink &arcLink, const wchar_t *name, HRESULT result);
void OpenResult_GUI(UString &s, const CCodecs *codecs, const CArchiveLink &arcLink, const wchar_t *name, HRESULT result)
{
  FOR_VECTOR (level, arcLink.Arcs)
  {
    const CArc &arc = arcLink.Arcs[level];
    const CArcErrorInfo &er = arc.ErrorInfo;

    if (!er.IsThereErrorOrWarning() && er.ErrorFormatIndex < 0)
      continue;

    if (s.IsEmpty())
    {
      s += name;
      s.Add_LF();
    }
    
    if (level != 0)
    {
      AddNewLineString(s, arc.Path);
    }
      
    ErrorInfo_Print(s, er);

    if (er.ErrorFormatIndex >= 0)
    {
      AddNewLineString(s, GetNameOfProperty(kpidWarning, L"Warning"));
      if (arc.FormatIndex == er.ErrorFormatIndex)
      {
        AddNewLineString(s, LangString(IDS_IS_OPEN_WITH_OFFSET));
      }
      else
      {
        AddNewLineString(s, MyFormatNew(IDS_CANT_OPEN_AS_TYPE, GetBracedType(codecs->GetFormatNamePtr(er.ErrorFormatIndex))));
        AddNewLineString(s, MyFormatNew(IDS_IS_OPEN_AS_TYPE, GetBracedType(codecs->GetFormatNamePtr(arc.FormatIndex))));
      }
    }
  }

  if (arcLink.NonOpen_ErrorInfo.ErrorFormatIndex >= 0 || result != S_OK)
  {
    s += name;
    s.Add_LF();
    if (!arcLink.Arcs.IsEmpty())
      AddNewLineString(s, arcLink.NonOpen_ArcPath);
    
    if (arcLink.NonOpen_ErrorInfo.ErrorFormatIndex >= 0 || result == S_FALSE)
    {
      UINT id = IDS_CANT_OPEN_ARCHIVE;
      UString param;
      if (arcLink.PasswordWasAsked)
        id = IDS_CANT_OPEN_ENCRYPTED_ARCHIVE;
      else if (arcLink.NonOpen_ErrorInfo.ErrorFormatIndex >= 0)
      {
        id = IDS_CANT_OPEN_AS_TYPE;
        param = GetBracedType(codecs->GetFormatNamePtr(arcLink.NonOpen_ErrorInfo.ErrorFormatIndex));
      }
      UString s2 = MyFormatNew(id, param);
      s2.Replace(L" ''", L"");
      s2.Replace(L"''", L"");
      s += s2;
    }
    else
      s += HResultToMessage(result);

    s.Add_LF();
    ErrorInfo_Print(s, arcLink.NonOpen_ErrorInfo);
  }

  if (!s.IsEmpty() && s.Back() == '\n')
    s.DeleteBack();
}

HRESULT CExtractCallbackImp::OpenResult(const CCodecs *codecs, const CArchiveLink &arcLink, const wchar_t *name, HRESULT result)
{
  _currentArchivePath = name;
  _needWriteArchivePath = true;

  UString s;
  OpenResult_GUI(s, codecs, arcLink, name, result);
  if (!s.IsEmpty())
  {
    NumArchiveErrors++;
    AddError_Message(s);
    _needWriteArchivePath = false;
  }

  return S_OK;
}

HRESULT CExtractCallbackImp::ThereAreNoFiles()
{
  return S_OK;
}

void CExtractCallbackImp::Add_ArchiveName_Error()
{
  if (_needWriteArchivePath)
  {
    if (!_currentArchivePath.IsEmpty())
      AddError_Message(_currentArchivePath);
    _needWriteArchivePath = false;
  }
}

HRESULT CExtractCallbackImp::ExtractResult(HRESULT result)
{
  if (result == S_OK)
    return result;
  NumArchiveErrors++;
  if (result == E_ABORT
      || result == HRESULT_FROM_WIN32(ERROR_DISK_FULL)
      )
    return result;

  Add_ArchiveName_Error();
  if (!_currentFilePath.IsEmpty())
    MessageError(_currentFilePath);
  MessageError(NError::MyFormatMessage(result));
  return S_OK;
}

#ifndef _NO_CRYPTO

HRESULT CExtractCallbackImp::SetPassword(const UString &password)
{
  PasswordIsDefined = true;
  Password = password;
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::CryptoGetTextPassword(BSTR *password)
{
  PasswordWasAsked = true;
  if (!PasswordIsDefined)
  {
    CPasswordDialog dialog;
    #ifndef _SFX
    bool showPassword = NExtract::Read_ShowPassword();
    dialog.ShowPassword = showPassword;
    #endif
    ProgressDialog->WaitCreating();
    if (dialog.Create(*ProgressDialog) != IDOK)
      return E_ABORT;
    Password = dialog.Password;
    PasswordIsDefined = true;
    #ifndef _SFX
    if (dialog.ShowPassword != showPassword)
      NExtract::Save_ShowPassword(dialog.ShowPassword);
    #endif
  }
  return StringToBstr(Password, password);
}

#endif

#ifndef _SFX

STDMETHODIMP CExtractCallbackImp::AskWrite(
    const wchar_t *srcPath, Int32 srcIsFolder,
    const FILETIME *srcTime, const UInt64 *srcSize,
    const wchar_t *destPath,
    BSTR *destPathResult,
    Int32 *writeAnswer)
{
  UString destPathResultTemp = destPath;

  // RINOK(StringToBstr(destPath, destPathResult));

  *destPathResult = 0;
  *writeAnswer = BoolToInt(false);

  FString destPathSys = us2fs(destPath);
  bool srcIsFolderSpec = IntToBool(srcIsFolder);
  CFileInfo destFileInfo;
  
  if (destFileInfo.Find(destPathSys))
  {
    if (srcIsFolderSpec)
    {
      if (!destFileInfo.IsDir())
      {
        RINOK(MessageError("Cannot replace file with folder with same name", destPathSys));
        return E_ABORT;
      }
      *writeAnswer = BoolToInt(false);
      return S_OK;
    }
  
    if (destFileInfo.IsDir())
    {
      RINOK(MessageError("Cannot replace folder with file with same name", destPathSys));
      *writeAnswer = BoolToInt(false);
      return S_OK;
    }

    switch (OverwriteMode)
    {
      case NExtract::NOverwriteMode::kSkip:
        return S_OK;
      case NExtract::NOverwriteMode::kAsk:
      {
        Int32 overwriteResult;
        UString destPathSpec = destPath;
        int slashPos = destPathSpec.ReverseFind_PathSepar();
        destPathSpec.DeleteFrom((unsigned)(slashPos + 1));
        destPathSpec += fs2us(destFileInfo.Name);

        RINOK(AskOverwrite(
            destPathSpec,
            &destFileInfo.MTime, &destFileInfo.Size,
            srcPath,
            srcTime, srcSize,
            &overwriteResult));
        
        switch (overwriteResult)
        {
          case NOverwriteAnswer::kCancel: return E_ABORT;
          case NOverwriteAnswer::kNo: return S_OK;
          case NOverwriteAnswer::kNoToAll: OverwriteMode = NExtract::NOverwriteMode::kSkip; return S_OK;
          case NOverwriteAnswer::kYes: break;
          case NOverwriteAnswer::kYesToAll: OverwriteMode = NExtract::NOverwriteMode::kOverwrite; break;
          case NOverwriteAnswer::kAutoRename: OverwriteMode = NExtract::NOverwriteMode::kRename; break;
          default:
            return E_FAIL;
        }
        break;
      }
      default:
        break;
    }
    
    if (OverwriteMode == NExtract::NOverwriteMode::kRename)
    {
      if (!AutoRenamePath(destPathSys))
      {
        RINOK(MessageError("Cannot create name for file", destPathSys));
        return E_ABORT;
      }
      destPathResultTemp = fs2us(destPathSys);
    }
    else
    {
      if (NFind::DoesFileExist_Raw(destPathSys))
      if (!NDir::DeleteFileAlways(destPathSys))
      if (GetLastError() != ERROR_FILE_NOT_FOUND)
      {
        RINOK(MessageError("Cannot delete output file", destPathSys));
        return E_ABORT;
      }
    }
  }
  *writeAnswer = BoolToInt(true);
  return StringToBstr(destPathResultTemp, destPathResult);
}


STDMETHODIMP CExtractCallbackImp::UseExtractToStream(Int32 *res)
{
  *res = BoolToInt(StreamMode);
  return S_OK;
}

static HRESULT GetTime(IGetProp *getProp, PROPID propID, FILETIME &ft, bool &ftDefined)
{
  ftDefined = false;
  NCOM::CPropVariant prop;
  RINOK(getProp->GetProp(propID, &prop));
  if (prop.vt == VT_FILETIME)
  {
    ft = prop.filetime;
    ftDefined = (ft.dwHighDateTime != 0 || ft.dwLowDateTime != 0);
  }
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}


static HRESULT GetItemBoolProp(IGetProp *getProp, PROPID propID, bool &result)
{
  NCOM::CPropVariant prop;
  result = false;
  RINOK(getProp->GetProp(propID, &prop));
  if (prop.vt == VT_BOOL)
    result = VARIANT_BOOLToBool(prop.boolVal);
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}


STDMETHODIMP CExtractCallbackImp::GetStream7(const wchar_t *name,
    Int32 isDir,
    ISequentialOutStream **outStream, Int32 askExtractMode,
    IGetProp *getProp)
{
  COM_TRY_BEGIN
  *outStream = 0;
  _newVirtFileWasAdded = false;
  _hashStreamWasUsed = false;
  _needUpdateStat = false;

  if (_hashStream)
    _hashStreamSpec->ReleaseStream();

  GetItemBoolProp(getProp, kpidIsAltStream, _isAltStream);

  if (!ProcessAltStreams && _isAltStream)
    return S_OK;

  _filePath = name;
  _isFolder = IntToBool(isDir);
  _curSize = 0;
  _curSizeDefined = false;

  UInt64 size = 0;
  bool sizeDefined;
  {
    NCOM::CPropVariant prop;
    RINOK(getProp->GetProp(kpidSize, &prop));
    sizeDefined = ConvertPropVariantToUInt64(prop, size);
  }

  if (sizeDefined)
  {
    _curSize = size;
    _curSizeDefined = true;
  }

  if (askExtractMode != NArchive::NExtract::NAskMode::kExtract &&
      askExtractMode != NArchive::NExtract::NAskMode::kTest)
    return S_OK;

  _needUpdateStat = true;
  
  CMyComPtr<ISequentialOutStream> outStreamLoc;
  
  if (VirtFileSystem && askExtractMode == NArchive::NExtract::NAskMode::kExtract)
  {
    CVirtFile &file = VirtFileSystemSpec->AddNewFile();
    _newVirtFileWasAdded = true;
    file.Name = name;
    file.IsDir = IntToBool(isDir);
    file.IsAltStream = _isAltStream;
    file.Size = 0;

    RINOK(GetTime(getProp, kpidCTime, file.CTime, file.CTimeDefined));
    RINOK(GetTime(getProp, kpidATime, file.ATime, file.ATimeDefined));
    RINOK(GetTime(getProp, kpidMTime, file.MTime, file.MTimeDefined));

    NCOM::CPropVariant prop;
    RINOK(getProp->GetProp(kpidAttrib, &prop));
    if (prop.vt == VT_UI4)
    {
      file.Attrib = prop.ulVal;
      file.AttribDefined = true;
    }
    // else if (isDir) file.Attrib = FILE_ATTRIBUTE_DIRECTORY;

    file.ExpectedSize = 0;
    if (sizeDefined)
      file.ExpectedSize = size;
    outStreamLoc = VirtFileSystem;
  }

  if (_hashStream)
  {
    {
      _hashStreamSpec->SetStream(outStreamLoc);
      outStreamLoc = _hashStream;
      _hashStreamSpec->Init(true);
      _hashStreamWasUsed = true;
    }
  }

  if (outStreamLoc)
    *outStream = outStreamLoc.Detach();
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CExtractCallbackImp::PrepareOperation7(Int32 askExtractMode)
{
  COM_TRY_BEGIN
  _needUpdateStat = (
         askExtractMode == NArchive::NExtract::NAskMode::kExtract
      || askExtractMode == NArchive::NExtract::NAskMode::kTest
      || askExtractMode == NArchive::NExtract::NAskMode::kReadExternal
      );

  /*
  _extractMode = false;
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract:
      if (_testMode)
        askExtractMode = NArchive::NExtract::NAskMode::kTest;
      else
        _extractMode = true;
      break;
  };
  */
  return SetCurrentFilePath2(_filePath);
  COM_TRY_END
}

STDMETHODIMP CExtractCallbackImp::SetOperationResult8(Int32 opRes, Int32 encrypted, UInt64 size)
{
  COM_TRY_BEGIN
  if (VirtFileSystem && _newVirtFileWasAdded)
  {
    // FIXME: probably we must request file size from VirtFileSystem
    // _curSize = VirtFileSystem->GetLastFileSize()
    // _curSizeDefined = true;
    RINOK(VirtFileSystemSpec->CloseMemFile());
  }
  if (_hashStream && _hashStreamWasUsed)
  {
    _hashStreamSpec->_hash->Final(_isFolder, _isAltStream, _filePath);
    _curSize = _hashStreamSpec->GetSize();
    _curSizeDefined = true;
    _hashStreamSpec->ReleaseStream();
    _hashStreamWasUsed = false;
  }
  else if (_hashCalc && _needUpdateStat)
  {
    _hashCalc->SetSize(size); // (_curSize) before 21.04
    _hashCalc->Final(_isFolder, _isAltStream, _filePath);
  }
  return SetOperationResult(opRes, encrypted);
  COM_TRY_END
}



// static const UInt32 kBlockSize = ((UInt32)1 << 31);

STDMETHODIMP CVirtFileSystem::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;
  if (!_fileMode)
  {
    CVirtFile &file = Files.Back();
    size_t rem = file.Data.Size() - (size_t)file.Size;
    bool useMem = true;
    if (rem < size)
    {
      UInt64 b = 0;
      if (file.Data.Size() == 0)
        b = file.ExpectedSize;
      UInt64 a = file.Size + size;
      if (b < a)
        b = a;
      a = (UInt64)file.Data.Size() * 2;
      if (b < a)
        b = a;
      useMem = false;
      const size_t b_sizet = (size_t)b;
      if (b == b_sizet && b <= MaxTotalAllocSize)
        useMem = file.Data.ReAlloc_KeepData(b_sizet, (size_t)file.Size);
    }
    if (useMem)
    {
      memcpy(file.Data + file.Size, data, size);
      file.Size += size;
      if (processedSize)
        *processedSize = (UInt32)size;
      return S_OK;
    }
    _fileMode = true;
  }
  RINOK(FlushToDisk(false));
  return _outFileStream->Write(data, size, processedSize);
}

HRESULT CVirtFileSystem::FlushToDisk(bool closeLast)
{
  if (!_outFileStream)
  {
    _outFileStreamSpec = new COutFileStream;
    _outFileStream = _outFileStreamSpec;
  }
  while (_numFlushed < Files.Size())
  {
    const CVirtFile &file = Files[_numFlushed];
    const FString path = DirPrefix + us2fs(Get_Correct_FsFile_Name(file.Name));
    if (!_fileIsOpen)
    {
      if (!_outFileStreamSpec->Create(path, false))
      {
        _outFileStream.Release();
        return E_FAIL;
        // MessageBoxMyError(UString("Can't create file ") + fs2us(tempFilePath));
      }
      _fileIsOpen = true;
      RINOK(WriteStream(_outFileStream, file.Data, (size_t)file.Size));
    }
    if (_numFlushed == Files.Size() - 1 && !closeLast)
      break;
    if (file.CTimeDefined ||
        file.ATimeDefined ||
        file.MTimeDefined)
      _outFileStreamSpec->SetTime(
          file.CTimeDefined ? &file.CTime : NULL,
          file.ATimeDefined ? &file.ATime : NULL,
          file.MTimeDefined ? &file.MTime : NULL);
    _outFileStreamSpec->Close();
    _numFlushed++;
    _fileIsOpen = false;
    if (file.AttribDefined)
      NDir::SetFileAttrib_PosixHighDetect(path, file.Attrib);
  }
  return S_OK;
}

#endif
