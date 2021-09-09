// ExtractEngine.h

#include "StdAfx.h"

#ifndef _7ZIP_ST
#include "../../../Windows/Synchronization.h"
#endif

#include "../../../Common/StringConvert.h"

#include "ExtractEngine.h"
#include "FarUtils.h"
#include "Messages.h"
#include "OverwriteDialogFar.h"

using namespace NWindows;
using namespace NFar;

#ifndef _7ZIP_ST
static NSynchronization::CCriticalSection g_CriticalSection;
#define MT_LOCK NSynchronization::CCriticalSectionLock lock(g_CriticalSection);
#else
#define MT_LOCK
#endif


static HRESULT CheckBreak2()
{
  return WasEscPressed() ? E_ABORT : S_OK;
}

extern void PrintMessage(const char *message);

CExtractCallbackImp::~CExtractCallbackImp()
{
}

void CExtractCallbackImp::Init(
    UINT codePage,
    CProgressBox *progressBox,
    bool passwordIsDefined,
    const UString &password)
{
  m_PasswordIsDefined = passwordIsDefined;
  m_Password = password;
  m_CodePage = codePage;
  _percent = progressBox;
}

STDMETHODIMP CExtractCallbackImp::SetTotal(UInt64 size)
{
  MT_LOCK

  if (_percent)
  {
    _percent->Total = size;
    _percent->Print();
  }
  return CheckBreak2();
}

STDMETHODIMP CExtractCallbackImp::SetCompleted(const UInt64 *completeValue)
{
  MT_LOCK

  if (_percent)
  {
    if (completeValue)
      _percent->Completed = *completeValue;
    _percent->Print();
  }
  return CheckBreak2();
}

STDMETHODIMP CExtractCallbackImp::AskOverwrite(
    const wchar_t *existName, const FILETIME *existTime, const UInt64 *existSize,
    const wchar_t *newName, const FILETIME *newTime, const UInt64 *newSize,
    Int32 *answer)
{
  MT_LOCK

  NOverwriteDialog::CFileInfo oldFileInfo, newFileInfo;
  oldFileInfo.TimeIsDefined = (existTime != 0);
  if (oldFileInfo.TimeIsDefined)
    oldFileInfo.Time = *existTime;
  oldFileInfo.SizeIsDefined = (existSize != NULL);
  if (oldFileInfo.SizeIsDefined)
    oldFileInfo.Size = *existSize;
  oldFileInfo.Name = existName;

  newFileInfo.TimeIsDefined = (newTime != 0);
  if (newFileInfo.TimeIsDefined)
    newFileInfo.Time = *newTime;
  newFileInfo.SizeIsDefined = (newSize != NULL);
  if (newFileInfo.SizeIsDefined)
    newFileInfo.Size = *newSize;
  newFileInfo.Name = newName;
  
  NOverwriteDialog::NResult::EEnum result =
    NOverwriteDialog::Execute(oldFileInfo, newFileInfo);
  
  switch (result)
  {
  case NOverwriteDialog::NResult::kCancel:
    // *answer = NOverwriteAnswer::kCancel;
    // break;
    return E_ABORT;
  case NOverwriteDialog::NResult::kNo:
    *answer = NOverwriteAnswer::kNo;
    break;
  case NOverwriteDialog::NResult::kNoToAll:
    *answer = NOverwriteAnswer::kNoToAll;
    break;
  case NOverwriteDialog::NResult::kYesToAll:
    *answer = NOverwriteAnswer::kYesToAll;
    break;
  case NOverwriteDialog::NResult::kYes:
    *answer = NOverwriteAnswer::kYes;
    break;
  case NOverwriteDialog::NResult::kAutoRename:
    *answer = NOverwriteAnswer::kAutoRename;
    break;
  default:
    return E_FAIL;
  }
  
  return CheckBreak2();
}

static const char * const kTestString    =  "Testing";
static const char * const kExtractString =  "Extracting";
static const char * const kSkipString    =  "Skipping";

STDMETHODIMP CExtractCallbackImp::PrepareOperation(const wchar_t *name, Int32 /* isFolder */, Int32 askExtractMode, const UInt64 * /* position */)
{
  MT_LOCK

  m_CurrentFilePath = name;
  const char *s;

  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract: s = kExtractString; break;
    case NArchive::NExtract::NAskMode::kTest:    s = kTestString; break;
    case NArchive::NExtract::NAskMode::kSkip:    s = kSkipString; break;
    default: s = "???"; // return E_FAIL;
  };

  if (_percent)
  {
    _percent->Command = s;
    _percent->FileName = name;
    _percent->Print();
  }

  return CheckBreak2();
}

STDMETHODIMP CExtractCallbackImp::MessageError(const wchar_t *message)
{
  MT_LOCK

  AString s (UnicodeStringToMultiByte(message, CP_OEMCP));
  if (g_StartupInfo.ShowErrorMessage((const char *)s) == -1)
    return E_ABORT;

  return CheckBreak2();
}

void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, AString &s);
void SetExtractErrorMessage(Int32 opRes, Int32 encrypted, AString &s)
{
  s.Empty();

  switch (opRes)
  {
    case NArchive::NExtract::NOperationResult::kOK:
      return;
    default:
    {
      UINT messageID = 0;
      switch (opRes)
      {
        case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
          messageID = NMessageID::kExtractUnsupportedMethod;
          break;
        case NArchive::NExtract::NOperationResult::kCRCError:
          messageID = encrypted ?
            NMessageID::kExtractCRCFailedEncrypted :
            NMessageID::kExtractCRCFailed;
          break;
        case NArchive::NExtract::NOperationResult::kDataError:
          messageID = encrypted ?
            NMessageID::kExtractDataErrorEncrypted :
            NMessageID::kExtractDataError;
          break;
      }
      if (messageID != 0)
      {
        s = g_StartupInfo.GetMsgString(messageID);
        s.Replace((AString)" '%s'", AString());
      }
      else if (opRes == NArchive::NExtract::NOperationResult::kUnavailable)
        s = "Unavailable data";
      else if (opRes == NArchive::NExtract::NOperationResult::kUnexpectedEnd)
        s = "Unexpected end of data";
      else if (opRes == NArchive::NExtract::NOperationResult::kDataAfterEnd)
        s = "There are some data after the end of the payload data";
      else if (opRes == NArchive::NExtract::NOperationResult::kIsNotArc)
        s = "Is not archive";
      else if (opRes == NArchive::NExtract::NOperationResult::kHeadersError)
        s = "kHeaders Error";
      else if (opRes == NArchive::NExtract::NOperationResult::kWrongPassword)
        s = "Wrong Password";
      else
      {
        s = "Error #";
        s.Add_UInt32(opRes);
      }
    }
  }
}

STDMETHODIMP CExtractCallbackImp::SetOperationResult(Int32 opRes, Int32 encrypted)
{
  MT_LOCK

  if (opRes == NArchive::NExtract::NOperationResult::kOK)
  {
    if (_percent)
    {
      _percent->Command.Empty();
      _percent->FileName.Empty();
      _percent->Files++;
    }
  }
  else
  {
    AString s;
    SetExtractErrorMessage(opRes, encrypted, s);
    if (PrintErrorMessage(s, m_CurrentFilePath) == -1)
      return E_ABORT;
  }
  
  return CheckBreak2();
}


STDMETHODIMP CExtractCallbackImp::ReportExtractResult(Int32 opRes, Int32 encrypted, const wchar_t *name)
{
  MT_LOCK

  if (opRes != NArchive::NExtract::NOperationResult::kOK)
  {
    AString s;
    SetExtractErrorMessage(opRes, encrypted, s);
    if (PrintErrorMessage(s, name) == -1)
      return E_ABORT;
  }
  
  return CheckBreak2();
}

extern HRESULT GetPassword(UString &password);

STDMETHODIMP CExtractCallbackImp::CryptoGetTextPassword(BSTR *password)
{
  MT_LOCK

  if (!m_PasswordIsDefined)
  {
    RINOK(GetPassword(m_Password));
    m_PasswordIsDefined = true;
  }
  return StringToBstr(m_Password, password);
}
