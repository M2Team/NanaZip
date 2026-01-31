// GUI.cpp

#include "StdAfx.h"

#ifdef _WIN32
#include "../../../../C/DllSecur.h"
#endif

#include "../../../Common/MyWindows.h"
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <shlwapi.h>
#else
#include <Shlwapi.h>
#endif

#include "../../../Common/MyInitGuid.h"

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/MyException.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/NtCheck.h"

#include "../Common/ArchiveCommandLine.h"
#include "../Common/ExitCode.h"

#include "../FileManager/StringUtils.h"
#include "../FileManager/LangUtils.h"

#include "BenchmarkDialog.h"
#include "ExtractGUI.h"
#include "HashGUI.h"
#include "UpdateGUI.h"

#include "ExtractRes.h"

using namespace NWindows;

#ifdef Z7_EXTERNAL_CODECS
extern
const CExternalCodecs *g_ExternalCodecs_Ptr;
const CExternalCodecs *g_ExternalCodecs_Ptr;
#endif

extern
HINSTANCE g_hInstance;
HINSTANCE g_hInstance;
extern
bool g_DisableUserQuestions;
bool g_DisableUserQuestions;

#ifndef UNDER_CE

#if !defined(Z7_WIN32_WINNT_MIN) || Z7_WIN32_WINNT_MIN < 0x0500  // win2000
#define Z7_USE_DYN_ComCtl32Version
#endif

#ifdef Z7_USE_DYN_ComCtl32Version
Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION

extern
DWORD g_ComCtl32Version;
DWORD g_ComCtl32Version;

static DWORD GetDllVersion(LPCTSTR dllName)
{
  DWORD dwVersion = 0;
  const HMODULE hmodule = LoadLibrary(dllName);
  if (hmodule)
  {
    const
     DLLGETVERSIONPROC f_DllGetVersion = Z7_GET_PROC_ADDRESS(
     DLLGETVERSIONPROC, hmodule,
    "DllGetVersion");
    if (f_DllGetVersion)
    {
      DLLVERSIONINFO dvi;
      ZeroMemory(&dvi, sizeof(dvi));
      dvi.cbSize = sizeof(dvi);
      const HRESULT hr = (*f_DllGetVersion)(&dvi);
      if (SUCCEEDED(hr))
        dwVersion = (DWORD)MAKELONG(dvi.dwMinorVersion, dvi.dwMajorVersion);
    }
    FreeLibrary(hmodule);
  }
  return dwVersion;
}

#endif
#endif

extern
bool g_LVN_ITEMACTIVATE_Support;
bool g_LVN_ITEMACTIVATE_Support = true;

DECLARE_AND_SET_CLIENT_VERSION_VAR

static void ErrorMessage(LPCWSTR message)
{
  if (!g_DisableUserQuestions)
    // **************** NanaZip Modification Start ****************
    //MessageBoxW(NULL, message, L"7-Zip", MB_ICONERROR | MB_OK);
    MessageBoxW(NULL, message, L"NanaZip", MB_ICONERROR | MB_OK);
    // **************** NanaZip Modification End ****************
}

static void ErrorMessage(const char *s)
{
  ErrorMessage(GetUnicodeString(s));
}

static void ErrorLangMessage(UINT resourceID)
{
  ErrorMessage(LangString(resourceID));
}

// **************** NanaZip Modification Start ****************
//static const char* const kNoFormats = "7-Zip cannot find the code that works with archives.";
static const char* const kNoFormats = "NanaZip cannot find the code that works with archives.";
// **************** NanaZip Modification End ****************

static int ShowMemErrorMessage()
{
  ErrorLangMessage(IDS_MEM_ERROR);
  return NExitCode::kMemoryError;
}

static int ShowSysErrorMessage(HRESULT errorCode)
{
  if (errorCode == E_OUTOFMEMORY)
    return ShowMemErrorMessage();
  ErrorMessage(HResultToMessage(errorCode));
  return NExitCode::kFatalError;
}

static void ThrowException_if_Error(HRESULT res)
{
  if (res != S_OK)
    throw CSystemException(res);
}

static int Main2()
{
  UStringVector commandStrings;
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), commandStrings);

  #ifndef UNDER_CE
  if (commandStrings.Size() > 0)
    commandStrings.Delete(0);
  #endif
  if (commandStrings.Size() == 0)
  {
    // **************** NanaZip Modification Start ****************
    //MessageBoxW(NULL, L"Specify command", L"7-Zip", 0);
    MessageBoxW(NULL, L"Specify command", L"NanaZip", 0);
    // **************** NanaZip Modification End ****************
    return 0;
  }

  CArcCmdLineOptions options;
  CArcCmdLineParser parser;

  parser.Parse1(commandStrings, options);
  g_DisableUserQuestions = options.YesToAll;
  parser.Parse2(options);

  CREATE_CODECS_OBJECT

  codecs->CaseSensitive_Change = options.CaseSensitive_Change;
  codecs->CaseSensitive = options.CaseSensitive;
  ThrowException_if_Error(codecs->Load());
  Codecs_AddHashArcHandler(codecs);

  #ifdef Z7_EXTERNAL_CODECS
  {
    g_ExternalCodecs_Ptr = &_externalCodecs;
    UString s;
    codecs->GetCodecsErrorMessage(s);
    if (!s.IsEmpty())
    {
      if (!g_DisableUserQuestions)
        // **************** NanaZip Modification Start ****************
        //MessageBoxW(NULL, s, L"7-Zip", MB_ICONERROR);
        MessageBoxW(NULL, s, L"NanaZip", MB_ICONERROR);
        // **************** NanaZip Modification End ****************
    }

  }
  #endif

  const bool isExtractGroupCommand = options.Command.IsFromExtractGroup();

  if (codecs->Formats.Size() == 0 &&
        (isExtractGroupCommand

        || options.Command.IsFromUpdateGroup()))
  {
    #ifdef Z7_EXTERNAL_CODECS
    if (!codecs->MainDll_ErrorPath.IsEmpty())
    {
      // **************** NanaZip Modification Start ****************
      //UString s ("7-Zip cannot load module: ");
      UString s("NanaZip cannot load module: ");
      // **************** NanaZip Modification End ****************
      s += fs2us(codecs->MainDll_ErrorPath);
      throw s;
    }
    #endif
    throw kNoFormats;
  }

  CObjectVector<COpenType> formatIndices;
  if (!ParseOpenTypes(*codecs, options.ArcType, formatIndices))
  {
    ErrorLangMessage(IDS_UNSUPPORTED_ARCHIVE_TYPE);
    return NExitCode::kFatalError;
  }

  CIntVector excludedFormats;
  FOR_VECTOR (k, options.ExcludedArcTypes)
  {
    CIntVector tempIndices;
    if (!codecs->FindFormatForArchiveType(options.ExcludedArcTypes[k], tempIndices)
        || tempIndices.Size() != 1)
    {
      ErrorLangMessage(IDS_UNSUPPORTED_ARCHIVE_TYPE);
      return NExitCode::kFatalError;
    }
    excludedFormats.AddToUniqueSorted(tempIndices[0]);
    // excludedFormats.Sort();
  }

  #ifdef Z7_EXTERNAL_CODECS
  if (isExtractGroupCommand
      || options.Command.IsFromUpdateGroup()
      || options.Command.CommandType == NCommandType::kHash
      || options.Command.CommandType == NCommandType::kBenchmark)
    ThrowException_if_Error(_externalCodecs.Load());
  #endif

  if (options.Command.CommandType == NCommandType::kBenchmark)
  {
    HRESULT res = Benchmark(
        EXTERNAL_CODECS_VARS_L
        options.Properties,
        options.NumIterations_Defined ?
          options.NumIterations :
          k_NumBenchIterations_Default);
    /*
    if (res == S_FALSE)
    {
      stdStream << "\nDecoding Error\n";
      return NExitCode::kFatalError;
    }
    */
    ThrowException_if_Error(res);
  }
  else if (isExtractGroupCommand)
  {
    UStringVector ArchivePathsSorted;
    UStringVector ArchivePathsFullSorted;

    CExtractCallbackImp *ecs = new CExtractCallbackImp;
    CMyComPtr<IFolderArchiveExtractCallback> extractCallback = ecs;

    #ifndef Z7_NO_CRYPTO
    ecs->PasswordIsDefined = options.PasswordEnabled;
    ecs->Password = options.Password;
    #endif

    ecs->Init();

    CExtractOptions eo;
    (CExtractOptionsBase &)eo = options.ExtractOptions;
    eo.StdInMode = options.StdInMode;
    eo.StdOutMode = options.StdOutMode;
    eo.YesToAll = options.YesToAll;
    ecs->YesToAll = options.YesToAll;
    eo.TestMode = options.Command.IsTestCommand();
    ecs->TestMode = eo.TestMode;
    // **************** NanaZip Modification Start ****************
    eo.OpenFolder = options.OpenFolder;
    // **************** NanaZip Modification End ****************

    #ifndef Z7_SFX
    eo.Properties = options.Properties;
    #endif

    bool messageWasDisplayed = false;

    #ifndef Z7_SFX
    CHashBundle hb;
    CHashBundle *hb_ptr = NULL;

    if (!options.HashMethods.IsEmpty())
    {
      hb_ptr = &hb;
      ThrowException_if_Error(hb.SetMethods(EXTERNAL_CODECS_VARS_L options.HashMethods));
    }
    #endif

    {
      CDirItemsStat st;
      HRESULT hresultMain = EnumerateDirItemsAndSort(
          options.arcCensor,
          NWildcard::k_RelatPath,
          UString(), // addPathPrefix
          ArchivePathsSorted,
          ArchivePathsFullSorted,
          st,
          NULL // &scan: change it!!!!
          );
      if (hresultMain != S_OK)
      {
        /*
        if (hresultMain != E_ABORT && messageWasDisplayed)
          return NExitCode::kFatalError;
        */
        throw CSystemException(hresultMain);
      }
    }

    ecs->MultiArcMode = (ArchivePathsSorted.Size() > 1);

    HRESULT result = ExtractGUI(
          // EXTERNAL_CODECS_VARS_L
          codecs,
          formatIndices, excludedFormats,
          ArchivePathsSorted,
          ArchivePathsFullSorted,
          options.Censor.Pairs.Front().Head,
          eo,
          #ifndef Z7_SFX
          hb_ptr,
          #endif
          options.ShowDialog, messageWasDisplayed, ecs);
    if (result != S_OK)
    {
      if (result != E_ABORT && messageWasDisplayed)
        return NExitCode::kFatalError;
      throw CSystemException(result);
    }
    if (!ecs->IsOK())
      return NExitCode::kFatalError;
    // **************** NanaZip Modification Start ****************
    else if (eo.OpenFolder.Val) {
      ShellExecuteW(NULL, NULL, ecs->Stat.OutDir, NULL, NULL, SW_SHOWNORMAL);
    }
    // **************** NanaZip Modification End ****************
  }
  else if (options.Command.IsFromUpdateGroup())
  {
    #ifndef Z7_NO_CRYPTO
    bool passwordIsDefined = options.PasswordEnabled && !options.Password.IsEmpty();
    #endif

    CUpdateCallbackGUI callback;
    // callback.EnablePercents = options.EnablePercents;

    #ifndef Z7_NO_CRYPTO
    callback.PasswordIsDefined = passwordIsDefined;
    callback.AskPassword = options.PasswordEnabled && options.Password.IsEmpty();
    callback.Password = options.Password;
    #endif

    // callback.StdOutMode = options.UpdateOptions.StdOutMode;
    callback.Init();

    if (!options.UpdateOptions.InitFormatIndex(codecs, formatIndices, options.ArchiveName) ||
        !options.UpdateOptions.SetArcPath(codecs, options.ArchiveName))
    {
      ErrorLangMessage(IDS_UPDATE_NOT_SUPPORTED);
      return NExitCode::kFatalError;
    }
    bool messageWasDisplayed = false;
    HRESULT result = UpdateGUI(
        codecs, formatIndices,
        options.ArchiveName,
        options.Censor,
        options.UpdateOptions,
        options.ShowDialog,
        messageWasDisplayed,
        &callback);

    if (result != S_OK)
    {
      if (result != E_ABORT && messageWasDisplayed)
        return NExitCode::kFatalError;
      throw CSystemException(result);
    }
    if (callback.FailedFiles.Size() > 0)
    {
      if (!messageWasDisplayed)
        throw CSystemException(E_FAIL);
      return NExitCode::kWarning;
    }
  }
  else if (options.Command.CommandType == NCommandType::kHash)
  {
    bool messageWasDisplayed = false;
    HRESULT result = HashCalcGUI(EXTERNAL_CODECS_VARS_L
        options.Censor, options.HashOptions, messageWasDisplayed);

    if (result != S_OK)
    {
      if (result != E_ABORT && messageWasDisplayed)
        return NExitCode::kFatalError;
      throw CSystemException(result);
    }
    /*
    if (callback.FailedFiles.Size() > 0)
    {
      if (!messageWasDisplayed)
        throw CSystemException(E_FAIL);
      return NExitCode::kWarning;
    }
    */
  }
  else
  {
    throw "Unsupported command";
  }
  return 0;
}

#if defined(_UNICODE) && !defined(_WIN64) && !defined(UNDER_CE)
#define NT_CHECK_FAIL_ACTION ErrorMessage("Unsupported Windows version"); return NExitCode::kFatalError;
#endif

// **************** NanaZip Modification Start ****************
#include <K7Base.h>
#include <K7User.h>

void NanaZipInitialize()
{
    if (MO_RESULT_SUCCESS_OK != ::K7BaseInitialize())
    {
        ::ErrorMessage(L"K7BaseInitialize Failed");
        ::ExitProcess(1);
    }

    if (MO_RESULT_SUCCESS_OK != ::K7UserInitializeDarkModeSupport())
    {
        ::ErrorMessage(L"K7UserInitializeDarkModeSupport Failed");
    }

    if (MO_RESULT_SUCCESS_OK != ::K7BaseDisableDynamicCodeGeneration())
    {
        ::ErrorMessage(L"K7BaseDisableDynamicCodeGeneration Failed");
    }

    if (MO_RESULT_SUCCESS_OK != ::K7BaseDisableChildProcessCreation())
    {
        ::ErrorMessage(L"K7BaseDisableChildProcessCreation Failed");
    }
}
// **************** NanaZip Modification End ****************

int APIENTRY WinMain(HINSTANCE  hInstance, HINSTANCE /* hPrevInstance */,
  #ifdef UNDER_CE
  LPWSTR
  #else
  LPSTR
  #endif
  /* lpCmdLine */, int /* nCmdShow */)
{
  // **************** NanaZip Modification Start ****************
  ::NanaZipInitialize();
  // **************** NanaZip Modification End ****************

  g_hInstance = hInstance;

  #ifdef _WIN32
  NT_CHECK
  #endif

  InitCommonControls();

#ifdef Z7_USE_DYN_ComCtl32Version
  g_ComCtl32Version = ::GetDllVersion(TEXT("comctl32.dll"));
  g_LVN_ITEMACTIVATE_Support = (g_ComCtl32Version >= MAKELONG(71, 4));
#endif

  // OleInitialize is required for ProgressBar in TaskBar.
  #ifndef UNDER_CE
  OleInitialize(NULL);
  #endif

  #ifdef Z7_LANG
  LoadLangOneTime();
  #endif

  // setlocale(LC_COLLATE, ".ACP");
  try
  {
    #ifdef _WIN32
    My_SetDefaultDllDirectories();
    #endif

    return Main2();
  }
  catch(const CNewException &)
  {
    return ShowMemErrorMessage();
  }
  catch(const CMessagePathException &e)
  {
    ErrorMessage(e);
    return NExitCode::kUserError;
  }
  catch(const CSystemException &systemError)
  {
    if (systemError.ErrorCode == E_ABORT)
      return NExitCode::kUserBreak;
    return ShowSysErrorMessage(systemError.ErrorCode);
  }
  catch(const UString &s)
  {
    ErrorMessage(s);
    return NExitCode::kFatalError;
  }
  catch(const AString &s)
  {
    ErrorMessage(s);
    return NExitCode::kFatalError;
  }
  catch(const wchar_t *s)
  {
    ErrorMessage(s);
    return NExitCode::kFatalError;
  }
  catch(const char *s)
  {
    ErrorMessage(s);
    return NExitCode::kFatalError;
  }
  catch(int v)
  {
    AString e ("Error: ");
    e.Add_UInt32((unsigned)v);
    ErrorMessage(e);
    return NExitCode::kFatalError;
  }
  catch(...)
  {
    ErrorMessage("Unknown error");
    return NExitCode::kFatalError;
  }
}
