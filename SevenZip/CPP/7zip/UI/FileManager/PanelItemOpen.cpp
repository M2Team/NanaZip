// PanelItemOpen.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include <TlHelp32.h>

#include "../../../Common/IntToString.h"

#include "../../../Common/AutoPtr.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ProcessUtils.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#include "../../Common/FileStreams.h"
#include "../../Common/StreamObjects.h"

#include "../Common/ExtractingFilePath.h"

#include "App.h"

#include "FileFolderPluginOpen.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "PropertyNameRes.h"
#include "RegistryUtils.h"
#include "UpdateCallback100.h"

#include "../GUI/ExtractRes.h"

#include "resource.h"

using namespace NWindows;
using namespace NSynchronization;
using namespace NFile;
using namespace NDir;

extern bool g_RAM_Size_Defined;
extern UInt64 g_RAM_Size;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

#define kTempDirPrefix FTEXT("7zO")

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
  #define DEBUG_PRINT(s) OutputDebugStringA(s);
  #define DEBUG_PRINT_W(s) OutputDebugStringW(s);
  #define DEBUG_PRINT_NUM(s, num) { char ttt[32]; ConvertUInt32ToString(num, ttt); OutputDebugStringA(s); OutputDebugStringA(ttt); }
#else
  #define DEBUG_PRINT(s)
  #define DEBUG_PRINT_W(s)
  #define DEBUG_PRINT_NUM(s, num)
#endif



#ifndef UNDER_CE

class CProcessSnapshot
{
  HANDLE _handle;
public:
  CProcessSnapshot(): _handle(INVALID_HANDLE_VALUE) {};
  ~CProcessSnapshot() { Close(); }

  bool Close()
  {
    if (_handle == INVALID_HANDLE_VALUE)
      return true;
    if (!::CloseHandle(_handle))
      return false;
    _handle = INVALID_HANDLE_VALUE;
    return true;
  }

  bool Create()
  {
    _handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    return (_handle != INVALID_HANDLE_VALUE);
  }

  bool GetFirstProcess(PROCESSENTRY32 *pe) { return BOOLToBool(Process32First(_handle, pe)); }
  bool GetNextProcess(PROCESSENTRY32 *pe) { return BOOLToBool(Process32Next(_handle, pe)); }
};

#endif


/*
struct COpenExtProg
{
  const char *Ext;
  const char *Prog;
};

static const COpenExtProg g_Progs[] =
{
  { "jpeg jpg png bmp gif", "Microsoft.Photos.exe" },
  { "html htm pdf", "MicrosoftEdge.exe" },
  // , { "rrr", "notepad.exe" }
};

static bool FindExtProg(const char *exts, const char *ext)
{
  unsigned len = (unsigned)strlen(ext);
  for (;;)
  {
    const char *p = exts;
    for (;; p++)
    {
      const char c = *p;
      if (c == 0 || c == ' ')
        break;
    }
    if (len == (unsigned)(p - exts) && IsString1PrefixedByString2(exts, ext))
      return true;
    if (*p == 0)
      return false;
    exts = p + 1;
  }
}

class CPossibleProgs
{
public:
  AStringVector ProgNames;

  void SetFromExtension(const char *ext) // ext must be low case
  {
    ProgNames.Clear();
    for (unsigned i = 0; i < ARRAY_SIZE(g_Progs); i++)
      if (FindExtProg(g_Progs[i].Ext, ext))
      {
        ProgNames.Add(g_Progs[i].Prog);
      }
  }
  
  bool IsFromList(const UString &progName) const
  {
    FOR_VECTOR (i, ProgNames)
      if (progName.IsEqualTo_Ascii_NoCase(ProgNames[i]))
        return true;
    return false;
  }
};
*/


#ifndef UNDER_CE

EXTERN_C_BEGIN

/*
GetProcessImageFileName
  returns the path in device form, rather than drive letters:
    \Device\HarddiskVolume1\WINDOWS\SysWOW64\notepad.exe

GetModuleFileNameEx works only after Sleep(something). Why?
  returns the path
    C:\WINDOWS\system32\NOTEPAD.EXE
*/

/* Kernel32.dll: Win7, Win2008R2;
   Psapi.dll: (if PSAPI_VERSION=1) on Win7 and Win2008R2;
   Psapi.dll: XP, Win2003, Vista, 2008;
*/

typedef DWORD (WINAPI *Func_GetProcessImageFileNameW)(
    HANDLE hProcess, LPWSTR lpFilename, DWORD nSize);

typedef DWORD (WINAPI *Func_GetModuleFileNameExW)(
    HANDLE hProcess, HMODULE hModule, LPWSTR lpFilename, DWORD nSize);

typedef DWORD (WINAPI *Func_GetProcessId)(HANDLE process);

EXTERN_C_END


static HMODULE g_Psapi_dll_module;

/*
static void My_GetProcessFileName_2(HANDLE hProcess, UString &path)
{
  path.Empty();
  const unsigned maxPath = 1024;
  WCHAR temp[maxPath + 1];
  
  const char *func_name = "GetModuleFileNameExW";
  Func_GetModuleFileNameExW my_func = (Func_GetModuleFileNameExW)
    ::GetProcAddress(::GetModuleHandleA("kernel32.dll"), func_name);
  if (!my_func)
  {
    if (!g_Psapi_dll_module)
      g_Psapi_dll_module = LoadLibraryW(L"Psapi.dll");
    if (g_Psapi_dll_module)
      my_func = (Func_GetModuleFileNameExW)::GetProcAddress(g_Psapi_dll_module, func_name);
  }
  if (my_func)
  {
    // DWORD num = GetModuleFileNameEx(hProcess, NULL, temp, maxPath);
    DWORD num = my_func(hProcess, NULL, temp, maxPath);
    if (num != 0)
      path = temp;
  }
  // FreeLibrary(lib);
}
*/

static void My_GetProcessFileName(HANDLE hProcess, UString &path)
{
  path.Empty();
  const unsigned maxPath = 1024;
  WCHAR temp[maxPath + 1];
  
  const char *func_name = "GetProcessImageFileNameW";
  Func_GetProcessImageFileNameW my_func = (Func_GetProcessImageFileNameW)
    (void *)::GetProcAddress(::GetModuleHandleA("kernel32.dll"), func_name);
  
  if (!my_func)
  {
    if (!g_Psapi_dll_module)
      g_Psapi_dll_module = LoadLibraryW(L"Psapi.dll");
    if (g_Psapi_dll_module)
      my_func = (Func_GetProcessImageFileNameW)(void *)::GetProcAddress(g_Psapi_dll_module, func_name);
  }
  
  if (my_func)
  {
    // DWORD num = GetProcessImageFileNameW(hProcess, temp, maxPath);
    DWORD num = my_func(hProcess, temp, maxPath);
    if (num != 0)
      path = temp;
  }
  // FreeLibrary(lib);
}

struct CSnapshotProcess
{
  DWORD Id;
  DWORD ParentId;
  UString Name;
};

static void GetSnapshot(CObjectVector<CSnapshotProcess> &items)
{
  items.Clear();

  CProcessSnapshot snapshot;
  if (!snapshot.Create())
    return;

  DEBUG_PRINT("snapshot.Create() OK");
  PROCESSENTRY32 pe;
  CSnapshotProcess item;
  memset(&pe, 0, sizeof(pe));
  pe.dwSize = sizeof(pe);
  BOOL res = snapshot.GetFirstProcess(&pe);
  while (res)
  {
    item.Id = pe.th32ProcessID;
    item.ParentId = pe.th32ParentProcessID;
    item.Name = GetUnicodeString(pe.szExeFile);
    items.Add(item);
    res = snapshot.GetNextProcess(&pe);
  }
}

#endif


class CChildProcesses
{
  #ifndef UNDER_CE
  CRecordVector<DWORD> _ids;
  #endif

public:
  // bool ProgsWereUsed;
  CRecordVector<HANDLE> Handles;
  CRecordVector<bool> NeedWait;
  // UStringVector Names;

  #ifndef UNDER_CE
  UString Path;
  #endif

  // CChildProcesses(): ProgsWereUsed(false) {}
  ~CChildProcesses() { CloseAll(); }
  void DisableWait(unsigned index) { NeedWait[index] = false; }
  
  void CloseAll()
  {
    FOR_VECTOR (i, Handles)
    {
      HANDLE h = Handles[i];
      if (h != NULL)
        CloseHandle(h);
    }

    Handles.Clear();
    NeedWait.Clear();
    // Names.Clear();

    #ifndef UNDER_CE
    // Path.Empty();
    _ids.Clear();
    #endif
  }

  void SetMainProcess(HANDLE h)
  {
    #ifndef UNDER_CE

    Func_GetProcessId func = (Func_GetProcessId)(void *)::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "GetProcessId");
    if (func)
    {
      DWORD id = func(h);
      if (id != 0)
        _ids.AddToUniqueSorted(id);
    }
    
    My_GetProcessFileName(h, Path);
    DEBUG_PRINT_W(Path);

    #endif

    Handles.Add(h);
    NeedWait.Add(true);
  }

  #ifndef UNDER_CE

  void Update(bool needFindProcessByPath /* , const CPossibleProgs &progs */)
  {
    /*
    if (_ids.IsEmpty())
      return;
    */

    CObjectVector<CSnapshotProcess> sps;
    GetSnapshot(sps);

    const int separ = Path.ReverseFind_PathSepar();
    const UString mainName = Path.Ptr((unsigned)(separ + 1));
    if (mainName.IsEmpty())
      needFindProcessByPath = false;

    const DWORD currentProcessId = GetCurrentProcessId();

    for (;;)
    {
      bool wasAdded = false;
      
      FOR_VECTOR (i, sps)
      {
        const CSnapshotProcess &sp = sps[i];
        const DWORD id = sp.Id;
        
        if (id == currentProcessId)
          continue;
        if (_ids.FindInSorted(id) >= 0)
          continue;

        bool isSameName = false;
        const UString &name = sp.Name;
        
        if (needFindProcessByPath)
          isSameName = mainName.IsEqualTo_NoCase(name);

        bool needAdd = false;
        // bool isFromProgs = false;
        
        if (isSameName || _ids.FindInSorted(sp.ParentId) >= 0)
          needAdd = true;
        /*
        else if (progs.IsFromList(name))
        {
          needAdd = true;
          isFromProgs = true;
        }
        */

        if (needAdd)
        {
          DEBUG_PRINT("----- OpenProcess -----");
          DEBUG_PRINT_W(name);
          HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, id);
          if (hProcess)
          {
            DEBUG_PRINT("----- OpenProcess OK -----");
            // if (!isFromProgs)
              _ids.AddToUniqueSorted(id);
            Handles.Add(hProcess);
            NeedWait.Add(true);
            // Names.Add(name);
            wasAdded = true;
            // ProgsWereUsed = isFromProgs;
          }
        }
      }
      
      if (!wasAdded)
        break;
    }
  }
  
  #endif
};


struct CTmpProcessInfo: public CTempFileInfo
{
  CChildProcesses Processes;
  HWND Window;
  UString FullPathFolderPrefix;
  bool UsePassword;
  UString Password;

  bool ReadOnly;
  
  CTmpProcessInfo(): UsePassword(false), ReadOnly(false) {}
};


class CTmpProcessInfoRelease
{
  CTmpProcessInfo *_tmpProcessInfo;
public:
  bool _needDelete;
  CTmpProcessInfoRelease(CTmpProcessInfo &tpi):
      _tmpProcessInfo(&tpi), _needDelete(true) {}
  ~CTmpProcessInfoRelease()
  {
    if (_needDelete)
      _tmpProcessInfo->DeleteDirAndFile();
  }
};

void GetFolderError(CMyComPtr<IFolderFolder> &folder, UString &s);


HRESULT CPanel::OpenAsArc(IInStream *inStream,
    const CTempFileInfo &tempFileInfo,
    const UString &virtualFilePath,
    const UString &arcFormat,
    COpenResult &openRes)
{
  openRes.Encrypted = false;
  CFolderLink folderLink;
  (CTempFileInfo &)folderLink = tempFileInfo;
  
  if (inStream)
    folderLink.IsVirtual = true;
  else
  {
    if (!folderLink.FileInfo.Find(folderLink.FilePath))
      return ::GetLastError();
    if (folderLink.FileInfo.IsDir())
      return S_FALSE;
    folderLink.IsVirtual = false;
  }

  folderLink.VirtualPath = virtualFilePath;

  CFfpOpen ffp;
  HRESULT res = ffp.OpenFileFolderPlugin(inStream,
      folderLink.FilePath.IsEmpty() ? us2fs(virtualFilePath) : folderLink.FilePath,
      arcFormat, GetParent());

  openRes.Encrypted = ffp.Encrypted;
  openRes.ErrorMessage = ffp.ErrorMessage;

  RINOK(res);
 
  folderLink.Password = ffp.Password;
  folderLink.UsePassword = ffp.Encrypted;

  if (_folder)
    folderLink.ParentFolderPath = GetFolderPath(_folder);
  else
    folderLink.ParentFolderPath = _currentFolderPrefix;
  
  if (!_parentFolders.IsEmpty())
    folderLink.ParentFolder = _folder;

  _parentFolders.Add(folderLink);
  _parentFolders.Back().Library.Attach(_library.Detach());

  ReleaseFolder();
  _library.Free();
  SetNewFolder(ffp.Folder);
  _library.Attach(ffp.Library.Detach());

  _flatMode = _flatModeForArc;

  _thereAreDeletedItems = false;
  
  if (!openRes.ErrorMessage.IsEmpty())
    MessageBox_Error(openRes.ErrorMessage);
  /*
  UString s;
  GetFolderError(_folder, s);
  if (!s.IsEmpty())
    MessageBox_Error(s);
  */
  // we don't show error here by some reasons:
  // after MessageBox_Warning it throws exception in nested archives in Debug Mode. why ?.
  // MessageBox_Warning(L"test error");

  return S_OK;
}


HRESULT CPanel::OpenAsArc_Msg(IInStream *inStream,
    const CTempFileInfo &tempFileInfo,
    const UString &virtualFilePath,
    const UString &arcFormat
    // , bool &encrypted
    // , bool showErrorMessage
    )
{
  COpenResult opRes;

  HRESULT res = OpenAsArc(inStream, tempFileInfo, virtualFilePath, arcFormat, opRes);
  
  if (res == S_OK)
    return res;
  if (res == E_ABORT)
    return res;

  // if (showErrorMessage)
  if (opRes.Encrypted || res != S_FALSE) // 17.01 : we show message also for (res != S_FALSE)
  {
    UString message;
    if (res == S_FALSE)
    {
      message = MyFormatNew(
          opRes.Encrypted ?
            IDS_CANT_OPEN_ENCRYPTED_ARCHIVE :
            IDS_CANT_OPEN_ARCHIVE,
          virtualFilePath);
    }
    else
      message = HResultToMessage(res);
    MessageBox_Error(message);
  }

  return res;
}


HRESULT CPanel::OpenAsArc_Name(const UString &relPath, const UString &arcFormat
    // , bool &encrypted,
    // , bool showErrorMessage
    )
{
  CTempFileInfo tfi;
  tfi.RelPath = relPath;
  tfi.FolderPath = us2fs(GetFsPath());
  const UString fullPath = GetFsPath() + relPath;
  tfi.FilePath = us2fs(fullPath);
  return OpenAsArc_Msg(NULL, tfi, fullPath, arcFormat /* , encrypted, showErrorMessage */);
}


HRESULT CPanel::OpenAsArc_Index(int index, const wchar_t *type
    // , bool showErrorMessage
    )
{
  CDisableTimerProcessing disableTimerProcessing1(*this);
  CDisableNotify disableNotify(*this);

  HRESULT res = OpenAsArc_Name(GetItemRelPath2(index), type ? type : L"" /* , encrypted, showErrorMessage */);
  if (res != S_OK)
  {
    RefreshTitle(true); // in case of error we must refresh changed title of 7zFM
    return res;
  }
  RefreshListCtrl();
  return S_OK;
}


HRESULT CPanel::OpenParentArchiveFolder()
{
  CDisableTimerProcessing disableTimerProcessing(*this);
  CDisableNotify disableNotify(*this);
  if (_parentFolders.Size() < 2)
    return S_OK;
  const CFolderLink &folderLinkPrev = _parentFolders[_parentFolders.Size() - 2];
  const CFolderLink &folderLink = _parentFolders.Back();
  NFind::CFileInfo newFileInfo;
  if (newFileInfo.Find(folderLink.FilePath))
  {
    if (folderLink.WasChanged(newFileInfo))
    {
      UString message = MyFormatNew(IDS_WANT_UPDATE_MODIFIED_FILE, folderLink.RelPath);
      if (::MessageBoxW((HWND)*this, message, L"7-Zip", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
      {
        if (OnOpenItemChanged(folderLink.FileIndex, fs2us(folderLink.FilePath),
            folderLinkPrev.UsePassword, folderLinkPrev.Password) != S_OK)
        {
          ::MessageBoxW((HWND)*this, MyFormatNew(IDS_CANNOT_UPDATE_FILE,
              fs2us(folderLink.FilePath)), L"7-Zip", MB_OK | MB_ICONSTOP);
          return S_OK;
        }
      }
    }
  }
  folderLink.DeleteDirAndFile();
  return S_OK;
}


static const char * const kExeExtensions =
  " exe bat ps1 com"
  " ";

static const char * const kStartExtensions =
  #ifdef UNDER_CE
  " cab"
  #endif
  " exe bat ps1 com"
  " chm"
  " msi doc dot xls ppt pps wps wpt wks xlr wdb vsd pub"

  " docx docm dotx dotm xlsx xlsm xltx xltm xlsb xps"
  " xlam pptx pptm potx potm ppam ppsx ppsm vsdx xsn"
  " mpp"
  " msg"
  " dwf"

  " flv swf"
  
  " epub"
  " odt ods"
  " wb3"
  " pdf"
  " ps"
  " txt"
  " xml xsd xsl xslt hxk hxc htm html xhtml xht mht mhtml htw asp aspx css cgi jsp shtml"
  " h hpp hxx c cpp cxx m mm go swift"
  " awk sed hta js json php php3 php4 php5 phptml pl pm py pyo rb tcl ts vbs"
  " asm"
  " mak clw csproj vcproj sln dsp dsw"
  " ";

static bool FindExt(const char *p, const UString &name)
{
  int dotPos = name.ReverseFind_Dot();
  if (dotPos < 0 || dotPos == (int)name.Len() - 1)
    return false;

  AString s;
  for (unsigned pos = dotPos + 1;; pos++)
  {
    wchar_t c = name[pos];
    if (c == 0)
      break;
    if (c >= 0x80)
      return false;
    s += (char)MyCharLower_Ascii((char)c);
  }
  for (unsigned i = 0; p[i] != 0;)
  {
    unsigned j;
    for (j = i; p[j] != ' '; j++);
    if (s.Len() == j - i && memcmp(p + i, (const char *)s, s.Len()) == 0)
      return true;
    i = j + 1;
  }
  return false;
}

static bool DoItemAlwaysStart(const UString &name)
{
  return FindExt(kStartExtensions, name);
}

UString GetQuotedString(const UString &s);


void SplitCmdLineSmart(const UString &cmd, UString &prg, UString &params);
void SplitCmdLineSmart(const UString &cmd, UString &prg, UString &params)
{
  params.Empty();
  prg = cmd;
  prg.Trim();
  if (prg.Len() >= 2 && prg[0] == L'"')
  {
    int pos = prg.Find(L'"', 1);
    if (pos >= 0)
    {
      if ((unsigned)(pos + 1) == prg.Len() || prg[pos + 1] == ' ')
      {
        params = prg.Ptr((unsigned)(pos + 1));
        params.Trim();
        prg.DeleteFrom((unsigned)pos);
        prg.DeleteFrontal(1);
      }
    }
  }
}


static WRes StartAppWithParams(const UString &cmd, const UStringVector &paramVector, CProcess &process)
{
  UString param;
  UString prg;

  SplitCmdLineSmart(cmd, prg, param);
  
  param.Trim();

  // int pos = params.Find(L"%1");

  FOR_VECTOR (i, paramVector)
  {
    if (!param.IsEmpty() && param.Back() != ' ')
      param.Add_Space();
    param += GetQuotedString(paramVector[i]);
  }
  
  return process.Create(prg, param, NULL);
}


static HRESULT StartEditApplication(const UString &path, bool useEditor, HWND window, CProcess &process)
{
  UString command;
  ReadRegEditor(useEditor, command);
  if (command.IsEmpty())
  {
    #ifdef UNDER_CE
    command = "\\Windows\\";
    #else
    FString winDir;
    if (!GetWindowsDir(winDir))
      return 0;
    NName::NormalizeDirPathPrefix(winDir);
    command = fs2us(winDir);
    #endif
    command += "notepad.exe";
  }

  UStringVector params;
  params.Add(path);

  HRESULT res = StartAppWithParams(command, params, process);
  if (res != SZ_OK)
    ::MessageBoxW(window, LangString(IDS_CANNOT_START_EDITOR), L"7-Zip", MB_OK  | MB_ICONSTOP);
  return res;
}


void CApp::DiffFiles()
{
  const CPanel &panel = GetFocusedPanel();
  
  if (!panel.Is_IO_FS_Folder())
  {
    panel.MessageBox_Error_UnsupportOperation();
    return;
  }

  CRecordVector<UInt32> indices;
  panel.GetSelectedItemsIndices(indices);

  UString path1, path2;
  if (indices.Size() == 2)
  {
    path1 = panel.GetItemFullPath(indices[0]);
    path2 = panel.GetItemFullPath(indices[1]);
  }
  else if (indices.Size() == 1 && NumPanels >= 2)
  {
    const CPanel &destPanel = Panels[1 - LastFocusedPanel];

    if (!destPanel.Is_IO_FS_Folder())
    {
      panel.MessageBox_Error_UnsupportOperation();
      return;
    }

    path1 = panel.GetItemFullPath(indices[0]);
    CRecordVector<UInt32> indices2;
    destPanel.GetSelectedItemsIndices(indices2);
    if (indices2.Size() == 1)
      path2 = destPanel.GetItemFullPath(indices2[0]);
    else
    {
      UString relPath = panel.GetItemRelPath2(indices[0]);
      if (panel._flatMode && !destPanel._flatMode)
        relPath = panel.GetItemName(indices[0]);
      path2 = destPanel._currentFolderPrefix + relPath;
    }
  }
  else
    return;

  DiffFiles(path1, path2);
}

void CApp::DiffFiles(const UString &path1, const UString &path2)
{
  UString command;
  ReadRegDiff(command);
  if (command.IsEmpty())
    return;

  UStringVector params;
  params.Add(path1);
  params.Add(path2);

  HRESULT res;
  {
    CProcess process;
    res = StartAppWithParams(command, params, process);
  }
  if (res == SZ_OK)
    return;
  ::MessageBoxW(_window, LangString(IDS_CANNOT_START_EDITOR), L"7-Zip", MB_OK  | MB_ICONSTOP);
}


#ifndef _UNICODE
typedef BOOL (WINAPI * ShellExecuteExWP)(LPSHELLEXECUTEINFOW lpExecInfo);
#endif

static HRESULT StartApplication(const UString &dir, const UString &path, HWND window, CProcess &process)
{
  UString path2 = path;

  #ifdef _WIN32
  {
    int dot = path2.ReverseFind_Dot();
    int separ = path2.ReverseFind_PathSepar();
    if (dot < 0 || dot < separ)
      path2 += '.';
  }
  #endif

  UINT32 result;
  
  #ifndef _UNICODE
  if (g_IsNT)
  {
    SHELLEXECUTEINFOW execInfo;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_DDEWAIT;
    execInfo.hwnd = NULL;
    execInfo.lpVerb = NULL;
    execInfo.lpFile = path2;
    execInfo.lpParameters = NULL;
    execInfo.lpDirectory = dir.IsEmpty() ? NULL : (LPCWSTR)dir;
    execInfo.nShow = SW_SHOWNORMAL;
    execInfo.hProcess = 0;
    ShellExecuteExWP shellExecuteExW = (ShellExecuteExWP)
    (void *)::GetProcAddress(::GetModuleHandleW(L"shell32.dll"), "ShellExecuteExW");
    if (!shellExecuteExW)
      return 0;
    shellExecuteExW(&execInfo);
    result = (UINT32)(UINT_PTR)execInfo.hInstApp;
    process.Attach(execInfo.hProcess);
  }
  else
  #endif
  {
    SHELLEXECUTEINFO execInfo;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS
      #ifndef UNDER_CE
      | SEE_MASK_FLAG_DDEWAIT
      #endif
      ;
    execInfo.hwnd = NULL;
    execInfo.lpVerb = NULL;
    const CSysString sysPath (GetSystemString(path2));
    const CSysString sysDir (GetSystemString(dir));
    execInfo.lpFile = sysPath;
    execInfo.lpParameters = NULL;
    execInfo.lpDirectory =
      #ifdef UNDER_CE
        NULL
      #else
        sysDir.IsEmpty() ? NULL : (LPCTSTR)sysDir
      #endif
      ;
    execInfo.nShow = SW_SHOWNORMAL;
    execInfo.hProcess = 0;
    ::ShellExecuteEx(&execInfo);
    result = (UINT32)(UINT_PTR)execInfo.hInstApp;
    process.Attach(execInfo.hProcess);
  }
  

  DEBUG_PRINT_NUM("-- ShellExecuteEx -- execInfo.hInstApp = ", result)

  if (result <= 32)
  {
    switch (result)
    {
      case SE_ERR_NOASSOC:
        ::MessageBoxW(window,
          NError::MyFormatMessage(::GetLastError()),
          // L"There is no application associated with the given file name extension",
          L"7-Zip", MB_OK | MB_ICONSTOP);
    }
    
    return E_FAIL; // fixed in 15.13. Can we use it for any Windows version?
  }
  
  return S_OK;
}

static void StartApplicationDontWait(const UString &dir, const UString &path, HWND window)
{
  CProcess process;
  StartApplication(dir, path, window, process);
}

void CPanel::EditItem(int index, bool useEditor)
{
  if (!_parentFolders.IsEmpty())
  {
    OpenItemInArchive(index, false, true, true, useEditor);
    return;
  }
  CProcess process;
  StartEditApplication(GetItemFullPath(index), useEditor, (HWND)*this, process);
}


void CPanel::OpenFolderExternal(int index)
{
  UString prefix = GetFsPath();
  UString path = prefix;

  if (index == kParentIndex)
  {
    if (prefix.IsEmpty())
      return;
    const wchar_t c = prefix.Back();
    if (!IS_PATH_SEPAR(c) && c != ':')
      return;
    prefix.DeleteBack();
    int pos = prefix.ReverseFind_PathSepar();
    if (pos < 0)
      return;
    prefix.DeleteFrom((unsigned)(pos + 1));
    path = prefix;
  }
  else
  {
    path += GetItemRelPath(index);
    path.Add_PathSepar();
  }

  StartApplicationDontWait(prefix, path, (HWND)*this);
}


bool CPanel::IsVirus_Message(const UString &name)
{
  UString name2;

  const wchar_t cRLO = (wchar_t)0x202E;
  bool isVirus = false;
  bool isSpaceError = false;
  name2 = name;
  
  if (name2.Find(cRLO) >= 0)
  {
    const UString badString(cRLO);
    name2.Replace(badString, L"[RLO]");
    isVirus = true;
  }
  {
    const wchar_t * const kVirusSpaces = L"     ";
    // const unsigned kNumSpaces = strlen(kVirusSpaces);
    for (;;)
    {
      int pos = name2.Find(kVirusSpaces);
      if (pos < 0)
        break;
      isVirus = true;
      isSpaceError = true;
      name2.Replace(kVirusSpaces, L" ");
    }
  }

  #ifdef _WIN32
  {
    unsigned i;
    for (i = name2.Len(); i != 0;)
    {
      wchar_t c = name2[i - 1];
      if (c != '.' && c != ' ')
        break;
      i--;
      name2.ReplaceOneCharAtPos(i, '_');
    }
    if (i != name2.Len())
    {
      UString name3 = name2;
      name3.DeleteFrom(i);
      if (FindExt(kExeExtensions, name3))
        isVirus = true;
    }
  }
  #endif
  
  if (!isVirus)
    return false;

  UString s = LangString(IDS_VIRUS);
  
  if (!isSpaceError)
  {
    int pos1 = s.Find(L'(');
    if (pos1 >= 0)
    {
      int pos2 = s.Find(L')', pos1 + 1);
      if (pos2 >= 0)
      {
        s.Delete(pos1, pos2 + 1 - pos1);
        if (pos1 > 0 && s[pos1 - 1] == ' ' && s[pos1] == '.')
          s.Delete(pos1 - 1);
      }
    }
  }

  UString name3 = name;
  name3.Replace(L'\n', L'_');
  name2.Replace(L'\n', L'_');

  s.Add_LF(); s += name2;
  s.Add_LF(); s += name3;

  MessageBox_Error(s);
  return true;
}


void CPanel::OpenItem(int index, bool tryInternal, bool tryExternal, const wchar_t *type)
{
  CDisableTimerProcessing disableTimerProcessing(*this);
  UString name = GetItemRelPath2(index);
  
  if (tryExternal)
    if (IsVirus_Message(name))
      return;

  if (!_parentFolders.IsEmpty())
  {
    OpenItemInArchive(index, tryInternal, tryExternal, false, false, type);
    return;
  }

  CDisableNotify disableNotify(*this);
  UString prefix = GetFsPath();
  UString fullPath = prefix + name;

  if (tryInternal)
    if (!tryExternal || !DoItemAlwaysStart(name))
    {
      HRESULT res = OpenAsArc_Index(index, type
          // , true
          );
      disableNotify.Restore(); // we must restore to allow text notification update
      InvalidateList();
      if (res == S_OK || res == E_ABORT)
        return;
      if (res != S_FALSE)
      {
        MessageBox_Error_HRESULT(res);
        return;
      }
    }
  
  if (tryExternal)
  {
    // SetCurrentDirectory opens HANDLE to folder!!!
    // NDirectory::MySetCurrentDirectory(prefix);
    StartApplicationDontWait(prefix, fullPath, (HWND)*this);
  }
}

class CThreadCopyFrom: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  UString FullPath;
  UInt32 ItemIndex;

  CMyComPtr<IFolderOperations> FolderOperations;
  CMyComPtr<IProgress> UpdateCallback;
  CUpdateCallback100Imp *UpdateCallbackSpec;
};
  
HRESULT CThreadCopyFrom::ProcessVirt()
{
  return FolderOperations->CopyFromFile(ItemIndex, FullPath, UpdateCallback);
}
      
HRESULT CPanel::OnOpenItemChanged(UInt32 index, const wchar_t *fullFilePath,
    bool usePassword, const UString &password)
{
  if (!_folderOperations)
  {
    MessageBox_Error_UnsupportOperation();
    return E_FAIL;
  }

  CThreadCopyFrom t;
  t.UpdateCallbackSpec = new CUpdateCallback100Imp;
  t.UpdateCallback = t.UpdateCallbackSpec;
  t.UpdateCallbackSpec->ProgressDialog = &t;
  t.ItemIndex = index;
  t.FullPath = fullFilePath;
  t.FolderOperations = _folderOperations;

  t.UpdateCallbackSpec->Init();
  t.UpdateCallbackSpec->PasswordIsDefined = usePassword;
  t.UpdateCallbackSpec->Password = password;


  RINOK(t.Create(GetItemName(index), (HWND)*this));
  return t.Result;
}

LRESULT CPanel::OnOpenItemChanged(LPARAM lParam)
{
  // DEBUG_PRINT_NUM("OnOpenItemChanged", GetCurrentThreadId());

  CTmpProcessInfo &tpi = *(CTmpProcessInfo *)lParam;
  if (tpi.FullPathFolderPrefix != _currentFolderPrefix)
    return 0;
  UInt32 fileIndex = tpi.FileIndex;
  UInt32 numItems;
  _folder->GetNumberOfItems(&numItems);
  
  // This code is not 100% OK for cases when there are several files with
  // tpi.RelPath name and there are changes in archive before update.
  // So tpi.FileIndex can point to another file.
 
  if (fileIndex >= numItems || GetItemRelPath(fileIndex) != tpi.RelPath)
  {
    UInt32 i;
    for (i = 0; i < numItems; i++)
      if (GetItemRelPath(i) == tpi.RelPath)
        break;
    if (i == numItems)
      return 0;
    fileIndex = i;
  }

  CSelectedState state;
  SaveSelectedState(state);

  CDisableNotify disableNotify(*this); // do we need it??

  HRESULT result = OnOpenItemChanged(fileIndex, fs2us(tpi.FilePath), tpi.UsePassword, tpi.Password);
  RefreshListCtrl(state);
  if (result != S_OK)
    return 0;
  return 1;
}


CExitEventLauncher g_ExitEventLauncher;

void CExitEventLauncher::Exit(bool hardExit)
{
  if (_needExit)
  {
    _exitEvent.Set();
    _needExit = false;
  }

  if (_numActiveThreads == 0)
    return;
  
  FOR_VECTOR (i, _threads)
  {
    ::CThread &th = _threads[i];
    DWORD wait = (hardExit ? 100 : INFINITE);
    if (Thread_WasCreated(&th))
    {
      DWORD waitResult = WaitForSingleObject(th, wait);
      // Thread_Wait(&th);
      if (waitResult == WAIT_TIMEOUT)
        wait = 1;
      if (!hardExit && waitResult != WAIT_OBJECT_0)
        continue;
      Thread_Close(&th);
      _numActiveThreads--;
    }
  }
}



static THREAD_FUNC_DECL MyThreadFunction(void *param)
{
  DEBUG_PRINT("==== MyThreadFunction ====");

  CMyAutoPtr<CTmpProcessInfo> tmpProcessInfoPtr((CTmpProcessInfo *)param);
  CTmpProcessInfo *tpi = tmpProcessInfoPtr.get();
  CChildProcesses &processes = tpi->Processes;

  bool mainProcessWasSet = !processes.Handles.IsEmpty();

  bool isComplexMode = true;

  if (!processes.Handles.IsEmpty())
  {

  const DWORD startTime = GetTickCount();

  /*
  CPossibleProgs progs;
  {
    const UString &name = tpi->RelPath;
    int slashPos = name.ReverseFind_PathSepar();
    int dotPos = name.ReverseFind_Dot();
    if (dotPos > slashPos)
    {
      const UString ext = name.Ptr(dotPos + 1);
      AString extA = UnicodeStringToMultiByte(ext);
      extA.MakeLower_Ascii();
      progs.SetFromExtension(extA);
    }
  }
  */

  bool firstPass = true;

  for (;;)
  {
    CRecordVector<HANDLE> handles;
    CUIntVector indices;
    
    FOR_VECTOR (i, processes.Handles)
    {
      if (handles.Size() > 60)
        break;
      if (processes.NeedWait[i])
      {
        handles.Add(processes.Handles[i]);
        indices.Add(i);
      }
    }
    
    bool needFindProcessByPath = false;

    if (handles.IsEmpty())
    {
      if (!firstPass)
        break;
    }
    else
    {
      handles.Add(g_ExitEventLauncher._exitEvent);
      
      DWORD waitResult = WaitForMultiObj_Any_Infinite(handles.Size(), &handles.Front());
      
      waitResult -= WAIT_OBJECT_0;
      
      if (waitResult >= handles.Size() - 1)
      {
        processes.CloseAll();
        /*
        if (waitResult == handles.Size() - 1)
        {
          // exit event
          // we want to delete temp files, if progs were used
          if (processes.ProgsWereUsed)
            break;
        }
        */
        return waitResult >= (DWORD)handles.Size() ? 1 : 0;
      }

      if (firstPass && indices.Size() == 1)
      {
        DWORD curTime = GetTickCount() - startTime;

        /*
        if (curTime > 5 * 1000)
          progs.ProgNames.Clear();
        */

        needFindProcessByPath = (curTime < 2 * 1000);

        if (needFindProcessByPath)
        {
          NFind::CFileInfo newFileInfo;
          if (newFileInfo.Find(tpi->FilePath))
            if (tpi->WasChanged(newFileInfo))
              needFindProcessByPath = false;
        }
        
        DEBUG_PRINT_NUM(" -- firstPass -- time = ", curTime)
      }
      
      processes.DisableWait(indices[waitResult]);
    }

    firstPass = false;
    
    // Sleep(300);
    #ifndef UNDER_CE
    processes.Update(needFindProcessByPath /* , progs */);
    #endif
  }


  DWORD curTime = GetTickCount() - startTime;

  DEBUG_PRINT_NUM("after time = ", curTime)

  processes.CloseAll();

  isComplexMode = (curTime < 2 * 1000);

  }

  bool needCheckTimestamp = true;

  for (;;)
  {
    NFind::CFileInfo newFileInfo;
    
    if (!newFileInfo.Find(tpi->FilePath))
      break;

    if (mainProcessWasSet)
    {
      if (tpi->WasChanged(newFileInfo))
      {
        UString m = MyFormatNew(IDS_CANNOT_UPDATE_FILE, fs2us(tpi->FilePath));
        if (tpi->ReadOnly)
        {
          m.Add_LF();
          AddLangString(m, IDS_PROP_READ_ONLY);
          m.Add_LF();
          m += tpi->FullPathFolderPrefix;
          ::MessageBoxW(g_HWND, m, L"7-Zip", MB_OK | MB_ICONSTOP);
          return 0;
        }
        {
          const UString message = MyFormatNew(IDS_WANT_UPDATE_MODIFIED_FILE, tpi->RelPath);
          if (::MessageBoxW(g_HWND, message, L"7-Zip", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
          {
            // DEBUG_PRINT_NUM("SendMessage", GetCurrentThreadId());
            if (SendMessage(tpi->Window, kOpenItemChanged, 0, (LONG_PTR)tpi) != 1)
            {
              ::MessageBoxW(g_HWND, m, L"7-Zip", MB_OK | MB_ICONSTOP);
              return 0;
            }
          }
          needCheckTimestamp = false;
          break;
        }
      }
    
      if (!isComplexMode)
        break;
    }
    
    // DEBUG_PRINT("WaitForSingleObject");
    DWORD waitResult = ::WaitForSingleObject(g_ExitEventLauncher._exitEvent, INFINITE);
    // DEBUG_PRINT("---");

    if (waitResult == WAIT_OBJECT_0)
      break;

    return 1;
  }

  {
    NFind::CFileInfo newFileInfo;
    
    bool finded = newFileInfo.Find(tpi->FilePath);

    if (!needCheckTimestamp || !finded || !tpi->WasChanged(newFileInfo))
    {
      DEBUG_PRINT("Delete Temp file");
      tpi->DeleteDirAndFile();
    }
  }
  
  return 0;
}



#if defined(_WIN32) && !defined(UNDER_CE)
static const FChar * const k_ZoneId_StreamName = FTEXT(":Zone.Identifier");
#endif


#ifndef UNDER_CE

static void ReadZoneFile(CFSTR fileName, CByteBuffer &buf)
{
  buf.Free();
  NIO::CInFile file;
  if (!file.Open(fileName))
    return;
  UInt64 fileSize;
  if (!file.GetLength(fileSize))
    return;
  if (fileSize == 0 || fileSize >= ((UInt32)1 << 20))
    return;
  buf.Alloc((size_t)fileSize);
  size_t processed;
  if (file.ReadFull(buf, (size_t)fileSize, processed) && processed == fileSize)
    return;
  buf.Free();
}

static bool WriteZoneFile(CFSTR fileName, const CByteBuffer &buf)
{
  NIO::COutFile file;
  if (!file.Create(fileName, true))
    return false;
  UInt32 processed;
  if (!file.Write(buf, (UInt32)buf.Size(), processed))
    return false;
  return processed == buf.Size();
}

#endif

/*
class CBufSeqOutStream_WithFile:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  Byte *_buffer;
  size_t _size;
  size_t _pos;


  size_t _fileWritePos;
  bool fileMode;
public:

  bool IsStreamInMem() const { return !fileMode; }
  size_t GetMemStreamWrittenSize() const { return _pos; }

  // ISequentialOutStream *FileStream;
  FString FilePath;
  COutFileStream *outFileStreamSpec;
  CMyComPtr<ISequentialOutStream> outFileStream;

  CBufSeqOutStream_WithFile(): outFileStreamSpec(NULL) {}

  void Init(Byte *buffer, size_t size)
  {
    fileMode = false;
    _buffer = buffer;
    _pos = 0;
    _size = size;
    _fileWritePos = 0;
  }

  HRESULT FlushToFile();
  size_t GetPos() const { return _pos; }

  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

static const UInt32 kBlockSize = ((UInt32)1 << 31);

STDMETHODIMP CBufSeqOutStream_WithFile::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  if (!fileMode)
  {
    if (_size - _pos >= size)
    {
      if (size != 0)
      {
        memcpy(_buffer + _pos, data, size);
        _pos += size;
      }
      if (processedSize)
        *processedSize = (UInt32)size;
      return S_OK;
    }

    fileMode = true;
  }
  RINOK(FlushToFile());
  return outFileStream->Write(data, size, processedSize);
}

HRESULT CBufSeqOutStream_WithFile::FlushToFile()
{
  if (!outFileStream)
  {
    outFileStreamSpec = new COutFileStream;
    outFileStream = outFileStreamSpec;
    if (!outFileStreamSpec->Create(FilePath, false))
    {
      outFileStream.Release();
      return E_FAIL;
      // MessageBoxMyError(UString("Can't create file ") + fs2us(tempFilePath));
    }
  }
  while (_fileWritePos != _pos)
  {
    size_t cur = _pos - _fileWritePos;
    UInt32 curSize = (cur < kBlockSize) ? (UInt32)cur : kBlockSize;
    UInt32 processedSizeLoc = 0;
    HRESULT res = outFileStream->Write(_buffer + _fileWritePos, curSize, &processedSizeLoc);
    _fileWritePos += processedSizeLoc;
    RINOK(res);
    if (processedSizeLoc == 0)
      return E_FAIL;
  }
  return S_OK;
}
*/

/*
static HRESULT GetTime(IFolderFolder *folder, UInt32 index, PROPID propID, FILETIME &filetime, bool &filetimeIsDefined)
{
  filetimeIsDefined = false;
  NCOM::CPropVariant prop;
  RINOK(folder->GetProperty(index, propID, &prop));
  if (prop.vt == VT_FILETIME)
  {
    filetime = prop.filetime;
    filetimeIsDefined = (filetime.dwHighDateTime != 0 || filetime.dwLowDateTime != 0);
  }
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}
*/


/*
tryInternal tryExternal
  false       false      : unused
  false       true       : external
  true        false      : internal
  true        true       : smart based on file extension:
                      !alwaysStart(name) : both
                      alwaysStart(name)  : external
*/

void CPanel::OpenItemInArchive(int index, bool tryInternal, bool tryExternal, bool editMode, bool useEditor, const wchar_t *type)
{
  const UString name = GetItemName(index);
  const UString relPath = GetItemRelPath(index);
  
  if (tryExternal)
    if (IsVirus_Message(name))
      return;

  if (!_folderOperations)
  {
    MessageBox_Error_UnsupportOperation();
    return;
  }

  bool tryAsArchive = tryInternal && (!tryExternal || !DoItemAlwaysStart(name));

  const UString fullVirtPath = _currentFolderPrefix + relPath;

  CTempDir tempDirectory;
  if (!tempDirectory.Create(kTempDirPrefix))
  {
    MessageBox_LastError();
    return;
  }
  
  FString tempDir = tempDirectory.GetPath();
  FString tempDirNorm = tempDir;
  NName::NormalizeDirPathPrefix(tempDirNorm);
  const FString tempFilePath = tempDirNorm + us2fs(Get_Correct_FsFile_Name(name));

  CTempFileInfo tempFileInfo;
  tempFileInfo.FileIndex = index;
  tempFileInfo.RelPath = relPath;
  tempFileInfo.FolderPath = tempDir;
  tempFileInfo.FilePath = tempFilePath;
  tempFileInfo.NeedDelete = true;

  if (tryAsArchive)
  {
    CMyComPtr<IInArchiveGetStream> getStream;
    _folder.QueryInterface(IID_IInArchiveGetStream, &getStream);
    if (getStream)
    {
      CMyComPtr<ISequentialInStream> subSeqStream;
      getStream->GetStream(index, &subSeqStream);
      if (subSeqStream)
      {
        CMyComPtr<IInStream> subStream;
        subSeqStream.QueryInterface(IID_IInStream, &subStream);
        if (subStream)
        {
          HRESULT res = OpenAsArc_Msg(subStream, tempFileInfo, fullVirtPath, type ? type : L""
              // , true // showErrorMessage
              );
          if (res == S_OK)
          {
            tempDirectory.DisableDeleting();
            RefreshListCtrl();
            return;
          }
          if (res == E_ABORT || res != S_FALSE)
            return;
          if (!tryExternal)
            return;
          tryAsArchive = false;
        }
      }
    }
  }


  CRecordVector<UInt32> indices;
  indices.Add(index);

  UStringVector messages;

  bool usePassword = false;
  UString password;
  if (_parentFolders.Size() > 0)
  {
    const CFolderLink &fl = _parentFolders.Back();
    usePassword = fl.UsePassword;
    password = fl.Password;
  }

  #if defined(_WIN32) && !defined(UNDER_CE)
  CByteBuffer zoneBuf;
  #ifndef _UNICODE
  if (g_IsNT)
  #endif
  if (_parentFolders.Size() > 0)
  {
    const CFolderLink &fl = _parentFolders.Front();
    if (!fl.IsVirtual && !fl.FilePath.IsEmpty())
      ReadZoneFile(fl.FilePath + k_ZoneId_StreamName, zoneBuf);
  }
  #endif


  CVirtFileSystem *virtFileSystemSpec = NULL;
  CMyComPtr<ISequentialOutStream> virtFileSystem;

  bool isAltStream = IsItem_AltStream(index);

  CCopyToOptions options;
  options.includeAltStreams = true;
  options.replaceAltStreamChars = isAltStream;
  
  if (tryAsArchive)
  {
    NCOM::CPropVariant prop;
    _folder->GetProperty(index, kpidSize, &prop);
    UInt64 fileLimit = 1 << 22;
    if (g_RAM_Size_Defined)
      fileLimit = g_RAM_Size / 4;

    UInt64 fileSize = 0;
    if (!ConvertPropVariantToUInt64(prop, fileSize))
      fileSize = fileLimit;
    if (fileSize <= fileLimit && fileSize > 0)
    {
      options.streamMode = true;
      virtFileSystemSpec = new CVirtFileSystem;
      virtFileSystem = virtFileSystemSpec;
      // we allow additional total size for small alt streams;
      virtFileSystemSpec->MaxTotalAllocSize = fileSize + (1 << 10);
      
      virtFileSystemSpec->DirPrefix = tempDirNorm;
      virtFileSystemSpec->Init();
      options.VirtFileSystem = virtFileSystem;
      options.VirtFileSystemSpec = virtFileSystemSpec;
    }
  }

  options.folder = fs2us(tempDirNorm);
  options.showErrorMessages = true;

  HRESULT result = CopyTo(options, indices, &messages, usePassword, password);

  if (_parentFolders.Size() > 0)
  {
    CFolderLink &fl = _parentFolders.Back();
    fl.UsePassword = usePassword;
    fl.Password = password;
  }

  if (!messages.IsEmpty())
    return;
  if (result != S_OK)
  {
    if (result != E_ABORT)
      MessageBox_Error_HRESULT(result);
    return;
  }

  if (options.VirtFileSystem)
  {
    if (virtFileSystemSpec->IsStreamInMem())
    {
      const CVirtFile &file = virtFileSystemSpec->Files[0];

      size_t streamSize = (size_t)file.Size;
      CBufInStream *bufInStreamSpec = new CBufInStream;
      CMyComPtr<IInStream> bufInStream = bufInStreamSpec;
      bufInStreamSpec->Init(file.Data, streamSize, virtFileSystem);

      HRESULT res = OpenAsArc_Msg(bufInStream, tempFileInfo, fullVirtPath, type ? type : L""
          // , encrypted
          // , true // showErrorMessage
          );

      if (res == S_OK)
      {
        tempDirectory.DisableDeleting();
        RefreshListCtrl();
        return;
      }

      if (res == E_ABORT || res != S_FALSE)
        return;
      if (!tryExternal)
        return;
      
      tryAsArchive = false;
      if (virtFileSystemSpec->FlushToDisk(true) != S_OK)
        return;
    }
  }


  #if defined(_WIN32) && !defined(UNDER_CE)
  if (zoneBuf.Size() != 0)
  {
    if (NFind::DoesFileExist_Raw(tempFilePath))
    {
      WriteZoneFile(tempFilePath + k_ZoneId_StreamName, zoneBuf);
    }
  }
  #endif


  if (tryAsArchive)
  {
    HRESULT res = OpenAsArc_Msg(NULL, tempFileInfo, fullVirtPath, type ? type : L""
        // , encrypted
        // , true // showErrorMessage
        );
    if (res == S_OK)
    {
      tempDirectory.DisableDeleting();
      RefreshListCtrl();
      return;
    }
    if (res == E_ABORT || res != S_FALSE)
      return;
  }

  if (!tryExternal)
    return;

  CMyAutoPtr<CTmpProcessInfo> tmpProcessInfoPtr(new CTmpProcessInfo());
  CTmpProcessInfo *tpi = tmpProcessInfoPtr.get();
  tpi->FolderPath = tempDir;
  tpi->FilePath = tempFilePath;
  tpi->NeedDelete = true;
  tpi->UsePassword = usePassword;
  tpi->Password = password;
  tpi->ReadOnly = IsThereReadOnlyFolder();

  if (!tpi->FileInfo.Find(tempFilePath))
    return;

  CTmpProcessInfoRelease tmpProcessInfoRelease(*tpi);

  CProcess process;
  HRESULT res;
  if (editMode)
    res = StartEditApplication(fs2us(tempFilePath), useEditor, (HWND)*this, process);
  else
    res = StartApplication(fs2us(tempDirNorm), fs2us(tempFilePath), (HWND)*this, process);

  if ((HANDLE)process == NULL)
  {
    // win7 / win10 work so for some extensions (pdf, html ..);
    DEBUG_PRINT("#### (HANDLE)process == 0");
    // return;
    if (res != SZ_OK)
      return;
  }

  tpi->Window = (HWND)(*this);
  tpi->FullPathFolderPrefix = _currentFolderPrefix;
  tpi->FileIndex = index;
  tpi->RelPath = relPath;
  
  if ((HANDLE)process != 0)
    tpi->Processes.SetMainProcess(process.Detach());

  ::CThread th;
  if (Thread_Create(&th, MyThreadFunction, tpi) != 0)
    throw 271824;
  g_ExitEventLauncher._threads.Add(th);
  g_ExitEventLauncher._numActiveThreads++;

  tempDirectory.DisableDeleting();
  tmpProcessInfoPtr.release();
  tmpProcessInfoRelease._needDelete = false;
}


/*
static const UINT64 kTimeLimit = UINT64(10000000) * 3600 * 24;

static bool CheckDeleteItem(UINT64 currentFileTime, UINT64 folderFileTime)
{
  return (currentFileTime - folderFileTime > kTimeLimit &&
      folderFileTime - currentFileTime > kTimeLimit);
}

void DeleteOldTempFiles()
{
  UString tempPath;
  if (!MyGetTempPath(tempPath))
    throw 1;

  UINT64 currentFileTime;
  NTime::GetCurUtcFileTime(currentFileTime);
  UString searchWildCard = tempPath + kTempDirPrefix + L"*.tmp";
  searchWildCard += WCHAR(NName::kAnyStringWildcard);
  NFind::CEnumeratorW enumerator(searchWildCard);
  NFind::CFileInfo fileInfo;
  while (enumerator.Next(fileInfo))
  {
    if (!fileInfo.IsDir())
      continue;
    const UINT64 &cTime = *(const UINT64 *)(&fileInfo.CTime);
    if (CheckDeleteItem(cTime, currentFileTime))
      RemoveDirectoryWithSubItems(tempPath + fileInfo.Name);
  }
}
*/
