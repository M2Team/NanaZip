// Main.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include <Shlwapi.h>

#include "../../../Common/MyInitGuid.h"

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/NtCheck.h"
#include "../../../Windows/ResourceString.h"

#include "../../ICoder.h"
#include "../../IPassword.h"
#include "../../Archive/IArchive.h"
#include "../../UI/Common/Extract.h"
#include "../../UI/Common/ExitCode.h"
#include "../../UI/Explorer/MyMessages.h"
#include "../../UI/FileManager/MyWindowsNew.h"
#include "../../UI/GUI/ExtractGUI.h"
#include "../../UI/GUI/ExtractRes.h"

#include "../../../../C/DllSecur.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

HINSTANCE g_hInstance;

#ifndef UNDER_CE

DWORD g_ComCtl32Version;

static DWORD GetDllVersion(LPCTSTR dllName)
{
  DWORD dwVersion = 0;
  HINSTANCE hinstDll = LoadLibrary(dllName);
  if (hinstDll)
  {
    DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");
    if (pDllGetVersion)
    {
      DLLVERSIONINFO dvi;
      ZeroMemory(&dvi, sizeof(dvi));
      dvi.cbSize = sizeof(dvi);
      HRESULT hr = (*pDllGetVersion)(&dvi);
      if (SUCCEEDED(hr))
        dwVersion = MAKELONG(dvi.dwMinorVersion, dvi.dwMajorVersion);
    }
    FreeLibrary(hinstDll);
  }
  return dwVersion;
}

#endif

bool g_LVN_ITEMACTIVATE_Support = true;

static const wchar_t * const kUnknownExceptionMessage = L"ERROR: Unknown Error!";

static void ErrorMessageForHRESULT(HRESULT res)
{
  ShowErrorMessage(HResultToMessage(res));
}

static int APIENTRY WinMain2()
{
  // OleInitialize is required for ProgressBar in TaskBar.
  #ifndef UNDER_CE
  OleInitialize(NULL);
  #endif

  #ifndef UNDER_CE
  g_ComCtl32Version = ::GetDllVersion(TEXT("comctl32.dll"));
  g_LVN_ITEMACTIVATE_Support = (g_ComCtl32Version >= MAKELONG(71, 4));
  #endif
  
  UString password;
  bool assumeYes = false;
  bool outputFolderDefined = false;
  FString outputFolder;
  UStringVector commandStrings;
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), commandStrings);

  #ifndef UNDER_CE
  if (commandStrings.Size() > 0)
    commandStrings.Delete(0);
  #endif

  FOR_VECTOR (i, commandStrings)
  {
    const UString &s = commandStrings[i];
    if (s.Len() > 1 && s[0] == '-')
    {
      wchar_t c = MyCharLower_Ascii(s[1]);
      if (c == 'y')
      {
        assumeYes = true;
        if (s.Len() != 2)
        {
          ShowErrorMessage(L"Bad command");
          return 1;
        }
      }
      else if (c == 'o')
      {
        outputFolder = us2fs(s.Ptr(2));
        NName::NormalizeDirPathPrefix(outputFolder);
        outputFolderDefined = !outputFolder.IsEmpty();
      }
      else if (c == 'p')
      {
        password = s.Ptr(2);
      }
    }
  }

  FString path;
  NDLL::MyGetModuleFileName(path);

  FString fullPath;
  if (!MyGetFullPathName(path, fullPath))
  {
    ShowErrorMessage(L"Error 1329484");
    return 1;
  }

  CCodecs *codecs = new CCodecs;
  CMyComPtr<IUnknown> compressCodecsInfo = codecs;
  HRESULT result = codecs->Load();
  if (result != S_OK)
  {
    ErrorMessageForHRESULT(result);
    return 1;
  }

  // COpenCallbackGUI openCallback;

  // openCallback.PasswordIsDefined = !password.IsEmpty();
  // openCallback.Password = password;

  CExtractCallbackImp *ecs = new CExtractCallbackImp;
  CMyComPtr<IFolderArchiveExtractCallback> extractCallback = ecs;
  ecs->Init();

  #ifndef _NO_CRYPTO
  ecs->PasswordIsDefined = !password.IsEmpty();
  ecs->Password = password;
  #endif

  CExtractOptions eo;

  FString dirPrefix;
  if (!GetOnlyDirPrefix(path, dirPrefix))
  {
    ShowErrorMessage(L"Error 1329485");
    return 1;
  }

  eo.OutputDir = outputFolderDefined ? outputFolder : dirPrefix;
  eo.YesToAll = assumeYes;
  eo.OverwriteMode = assumeYes ?
      NExtract::NOverwriteMode::kOverwrite :
      NExtract::NOverwriteMode::kAsk;
  eo.PathMode = NExtract::NPathMode::kFullPaths;
  eo.TestMode = false;
  
  UStringVector v1, v2;
  v1.Add(fs2us(fullPath));
  v2.Add(fs2us(fullPath));
  NWildcard::CCensorNode wildcardCensor;
  wildcardCensor.AddItem(true, L"*", true, true, true, true);

  bool messageWasDisplayed = false;
  result = ExtractGUI(codecs,
      CObjectVector<COpenType>(), CIntVector(),
      v1, v2,
      wildcardCensor, eo, (assumeYes ? false: true), messageWasDisplayed, ecs);

  if (result == S_OK)
  {
    if (!ecs->IsOK())
      return NExitCode::kFatalError;
    return 0;
  }
  if (result == E_ABORT)
    return NExitCode::kUserBreak;
  if (!messageWasDisplayed)
  {
    if (result == S_FALSE)
      ShowErrorMessage(L"Error in archive");
    else
      ErrorMessageForHRESULT(result);
  }
  if (result == E_OUTOFMEMORY)
    return NExitCode::kMemoryError;
  return NExitCode::kFatalError;
}

#if defined(_WIN32) && defined(_UNICODE) && !defined(_WIN64) && !defined(UNDER_CE)
#define NT_CHECK_FAIL_ACTION ShowErrorMessage(L"Unsupported Windows version"); return NExitCode::kFatalError;
#endif

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
  #ifdef UNDER_CE
  LPWSTR
  #else
  LPSTR
  #endif
  /* lpCmdLine */, int /* nCmdShow */)
{
  g_hInstance = (HINSTANCE)hInstance;

  NT_CHECK

  try
  {
    #ifdef _WIN32
    LoadSecurityDlls();
    #endif

    return WinMain2();
  }
  catch(const CNewException &)
  {
    ErrorMessageForHRESULT(E_OUTOFMEMORY);
    return NExitCode::kMemoryError;
  }
  catch(...)
  {
    ShowErrorMessage(kUnknownExceptionMessage);
    return NExitCode::kFatalError;
  }
}
