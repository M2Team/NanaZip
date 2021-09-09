// Main.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include "../../../Common/MyInitGuid.h"

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/TextConfig.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/NtCheck.h"
#include "../../../Windows/ResourceString.h"

#include "../../UI/Explorer/MyMessages.h"

#include "ExtractEngine.h"

#include "../../../../C/DllSecur.h"

#include "resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

HINSTANCE g_hInstance;

static CFSTR const kTempDirPrefix = FTEXT("7zS");

#define _SHELL_EXECUTE

static bool ReadDataString(CFSTR fileName, LPCSTR startID,
    LPCSTR endID, AString &stringResult)
{
  stringResult.Empty();
  NIO::CInFile inFile;
  if (!inFile.Open(fileName))
    return false;
  const size_t kBufferSize = (1 << 12);

  Byte buffer[kBufferSize];
  const unsigned signatureStartSize = MyStringLen(startID);
  const unsigned signatureEndSize = MyStringLen(endID);
  
  size_t numBytesPrev = 0;
  bool writeMode = false;
  UInt64 posTotal = 0;
  for (;;)
  {
    if (posTotal > (1 << 20))
      return (stringResult.IsEmpty());
    const size_t numReadBytes = kBufferSize - numBytesPrev;
    size_t processedSize;
    if (!inFile.ReadFull(buffer + numBytesPrev, numReadBytes, processedSize))
      return false;
    if (processedSize == 0)
      return true;
    const size_t numBytesInBuffer = numBytesPrev + processedSize;
    UInt32 pos = 0;
    for (;;)
    {
      if (writeMode)
      {
        if (pos + signatureEndSize > numBytesInBuffer)
          break;
        if (memcmp(buffer + pos, endID, signatureEndSize) == 0)
          return true;
        char b = buffer[pos];
        if (b == 0)
          return false;
        stringResult += b;
        pos++;
      }
      else
      {
        if (pos + signatureStartSize > numBytesInBuffer)
          break;
        if (memcmp(buffer + pos, startID, signatureStartSize) == 0)
        {
          writeMode = true;
          pos += signatureStartSize;
        }
        else
          pos++;
      }
    }
    numBytesPrev = numBytesInBuffer - pos;
    posTotal += pos;
    memmove(buffer, buffer + pos, numBytesPrev);
  }
}

static char kStartID[] = { ',','!','@','I','n','s','t','a','l','l','@','!','U','T','F','-','8','!', 0 };
static char kEndID[]   = { ',','!','@','I','n','s','t','a','l','l','E','n','d','@','!', 0 };

struct CInstallIDInit
{
  CInstallIDInit()
  {
    kStartID[0] = ';';
    kEndID[0] = ';';
  };
} g_CInstallIDInit;


#if defined(_WIN32) && defined(_UNICODE) && !defined(_WIN64) && !defined(UNDER_CE)
#define NT_CHECK_FAIL_ACTION ShowErrorMessage(L"Unsupported Windows version"); return 1;
#endif

static void ShowErrorMessageSpec(const UString &name)
{
  UString message = NError::MyFormatMessage(::GetLastError());
  int pos = message.Find(L"%1");
  if (pos >= 0)
  {
    message.Delete(pos, 2);
    message.Insert(pos, name);
  }
  ShowErrorMessage(NULL, message);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
    #ifdef UNDER_CE
    LPWSTR
    #else
    LPSTR
    #endif
    /* lpCmdLine */,int /* nCmdShow */)
{
  g_hInstance = (HINSTANCE)hInstance;

  NT_CHECK

  #ifdef _WIN32
  LoadSecurityDlls();
  #endif

  // InitCommonControls();

  UString archiveName, switches;
  #ifdef _SHELL_EXECUTE
  UString executeFile, executeParameters;
  #endif
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), archiveName, switches);

  FString fullPath;
  NDLL::MyGetModuleFileName(fullPath);

  switches.Trim();
  bool assumeYes = false;
  if (switches.IsPrefixedBy_Ascii_NoCase("-y"))
  {
    assumeYes = true;
    switches = switches.Ptr(2);
    switches.Trim();
  }

  AString config;
  if (!ReadDataString(fullPath, kStartID, kEndID, config))
  {
    if (!assumeYes)
      ShowErrorMessage(L"Can't load config info");
    return 1;
  }

  UString dirPrefix ("." STRING_PATH_SEPARATOR);
  UString appLaunched;
  bool showProgress = true;
  if (!config.IsEmpty())
  {
    CObjectVector<CTextConfigPair> pairs;
    if (!GetTextConfig(config, pairs))
    {
      if (!assumeYes)
        ShowErrorMessage(L"Config failed");
      return 1;
    }
    UString friendlyName = GetTextConfigValue(pairs, "Title");
    UString installPrompt = GetTextConfigValue(pairs, "BeginPrompt");
    UString progress = GetTextConfigValue(pairs, "Progress");
    if (progress.IsEqualTo_Ascii_NoCase("no"))
      showProgress = false;
    int index = FindTextConfigItem(pairs, "Directory");
    if (index >= 0)
      dirPrefix = pairs[index].String;
    if (!installPrompt.IsEmpty() && !assumeYes)
    {
      if (MessageBoxW(0, installPrompt, friendlyName, MB_YESNO |
          MB_ICONQUESTION) != IDYES)
        return 0;
    }
    appLaunched = GetTextConfigValue(pairs, "RunProgram");
    
    #ifdef _SHELL_EXECUTE
    executeFile = GetTextConfigValue(pairs, "ExecuteFile");
    executeParameters = GetTextConfigValue(pairs, "ExecuteParameters");
    #endif
  }

  CTempDir tempDir;
  if (!tempDir.Create(kTempDirPrefix))
  {
    if (!assumeYes)
      ShowErrorMessage(L"Cannot create temp folder archive");
    return 1;
  }

  CCodecs *codecs = new CCodecs;
  CMyComPtr<IUnknown> compressCodecsInfo = codecs;
  {
    HRESULT result = codecs->Load();
    if (result != S_OK)
    {
      ShowErrorMessage(L"Cannot load codecs");
      return 1;
    }
  }

  const FString tempDirPath = tempDir.GetPath();
  // tempDirPath = L"M:\\1\\"; // to test low disk space
  {
    bool isCorrupt = false;
    UString errorMessage;
    HRESULT result = ExtractArchive(codecs, fullPath, tempDirPath, showProgress,
      isCorrupt, errorMessage);
    
    if (result != S_OK)
    {
      if (!assumeYes)
      {
        if (result == S_FALSE || isCorrupt)
        {
          NWindows::MyLoadString(IDS_EXTRACTION_ERROR_MESSAGE, errorMessage);
          result = E_FAIL;
        }
        if (result != E_ABORT)
        {
          if (errorMessage.IsEmpty())
            errorMessage = NError::MyFormatMessage(result);
          ::MessageBoxW(0, errorMessage, NWindows::MyLoadString(IDS_EXTRACTION_ERROR_TITLE), MB_ICONERROR);
        }
      }
      return 1;
    }
  }

  #ifndef UNDER_CE
  CCurrentDirRestorer currentDirRestorer;
  if (!SetCurrentDir(tempDirPath))
    return 1;
  #endif
  
  HANDLE hProcess = 0;
#ifdef _SHELL_EXECUTE
  if (!executeFile.IsEmpty())
  {
    CSysString filePath (GetSystemString(executeFile));
    SHELLEXECUTEINFO execInfo;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS
      #ifndef UNDER_CE
      | SEE_MASK_FLAG_DDEWAIT
      #endif
      ;
    execInfo.hwnd = NULL;
    execInfo.lpVerb = NULL;
    execInfo.lpFile = filePath;

    if (!switches.IsEmpty())
    {
      executeParameters.Add_Space_if_NotEmpty();
      executeParameters += switches;
    }

    CSysString parametersSys (GetSystemString(executeParameters));
    if (parametersSys.IsEmpty())
      execInfo.lpParameters = NULL;
    else
      execInfo.lpParameters = parametersSys;

    execInfo.lpDirectory = NULL;
    execInfo.nShow = SW_SHOWNORMAL;
    execInfo.hProcess = 0;
    /* BOOL success = */ ::ShellExecuteEx(&execInfo);
    UINT32 result = (UINT32)(UINT_PTR)execInfo.hInstApp;
    if (result <= 32)
    {
      if (!assumeYes)
        ShowErrorMessage(L"Cannot open file");
      return 1;
    }
    hProcess = execInfo.hProcess;
  }
  else
#endif
  {
    if (appLaunched.IsEmpty())
    {
      appLaunched = L"setup.exe";
      if (!NFind::DoesFileExist_FollowLink(us2fs(appLaunched)))
      {
        if (!assumeYes)
          ShowErrorMessage(L"Cannot find setup.exe");
        return 1;
      }
    }
    
    {
      FString s2 = tempDirPath;
      NName::NormalizeDirPathPrefix(s2);
      appLaunched.Replace(L"%%T" WSTRING_PATH_SEPARATOR, fs2us(s2));
    }
    
    UString appNameForError = appLaunched; // actually we need to rtemove parameters also

    appLaunched.Replace(L"%%T", fs2us(tempDirPath));

    if (!switches.IsEmpty())
    {
      appLaunched.Add_Space();
      appLaunched += switches;
    }
    STARTUPINFO startupInfo;
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.lpReserved = 0;
    startupInfo.lpDesktop = 0;
    startupInfo.lpTitle = 0;
    startupInfo.dwFlags = 0;
    startupInfo.cbReserved2 = 0;
    startupInfo.lpReserved2 = 0;
    
    PROCESS_INFORMATION processInformation;
    
    CSysString appLaunchedSys (GetSystemString(dirPrefix + appLaunched));
    
    BOOL createResult = CreateProcess(NULL, (LPTSTR)(LPCTSTR)appLaunchedSys,
      NULL, NULL, FALSE, 0, NULL, NULL /*tempDir.GetPath() */,
      &startupInfo, &processInformation);
    if (createResult == 0)
    {
      if (!assumeYes)
      {
        // we print name of exe file, if error message is
        // ERROR_BAD_EXE_FORMAT: "%1 is not a valid Win32 application".
        ShowErrorMessageSpec(appNameForError);
      }
      return 1;
    }
    ::CloseHandle(processInformation.hThread);
    hProcess = processInformation.hProcess;
  }
  if (hProcess != 0)
  {
    WaitForSingleObject(hProcess, INFINITE);
    ::CloseHandle(hProcess);
  }
  return 0;
}
