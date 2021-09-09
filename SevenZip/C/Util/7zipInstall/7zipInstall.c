/* 7zipInstall.c - 7-Zip Installer
2021-02-23 : Igor Pavlov : Public domain */

#include "Precomp.h"

#define SZ_ERROR_ABORT 100

#ifdef _MSC_VER
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

#include <windows.h>
#include <ShlObj.h>

#include "../../7z.h"
#include "../../7zAlloc.h"
#include "../../7zCrc.h"
#include "../../7zFile.h"
#include "../../7zVersion.h"
#include "../../CpuArch.h"
#include "../../DllSecur.h"

#include "resource.h"

#define LLL_(quote) L##quote
#define LLL(quote) LLL_(quote)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define wcscat lstrcatW
#define wcslen lstrlenW
#define wcscpy lstrcpyW
// wcsncpy() and lstrcpynW() work differently. We don't use them.


#define kInputBufSize ((size_t)1 << 18)


#define _7ZIP_CUR_VER ((MY_VER_MAJOR << 16) | MY_VER_MINOR)
#define _7ZIP_DLL_VER_COMPAT ((16 << 16) | 3)

static LPCSTR const k_7zip = "7-Zip";

static LPCWSTR const k_Reg_Software_7zip = L"Software\\7-Zip";

// #define _64BIT_INSTALLER 1

#ifdef _WIN64
  #define _64BIT_INSTALLER 1
#endif

#define k_7zip_with_Ver_base L"7-Zip " LLL(MY_VERSION)

#ifdef _64BIT_INSTALLER

  // #define USE_7ZIP_32_DLL

  #if defined(_M_ARM64) || defined(_M_ARM)
    #define k_Postfix  L" (arm64)"
  #else
    #define k_Postfix  L" (x64)"
    #define USE_7ZIP_32_DLL
  #endif
#else
  #if defined(_M_ARM64) || defined(_M_ARM)
    #define k_Postfix  L" (arm)"
  #else
    // #define k_Postfix  L" (x86)"
    #define k_Postfix
  #endif
#endif

#define k_7zip_with_Ver  k_7zip_with_Ver_base k_Postfix 


static LPCWSTR const k_7zip_with_Ver_str = k_7zip_with_Ver;

static LPCWSTR const k_7zip_Setup = k_7zip_with_Ver L" Setup";

static LPCWSTR const k_Reg_Path = L"Path";

static LPCWSTR const k_Reg_Path32 = L"Path"
  #ifdef _64BIT_INSTALLER
    L"64"
  #else
    L"32"
  #endif
    ;
 
#if defined(_64BIT_INSTALLER) && !defined(_WIN64)
  #define k_Reg_WOW_Flag KEY_WOW64_64KEY
#else
  #define k_Reg_WOW_Flag 0
#endif

#ifdef _WIN64
  #define k_Reg_WOW_Flag_32 KEY_WOW64_32KEY
#else
  #define k_Reg_WOW_Flag_32 0
#endif

#define k_7zip_CLSID L"{23170F69-40C1-278A-1000-000100020000}"

static LPCWSTR const k_Reg_CLSID_7zip = L"CLSID\\" k_7zip_CLSID;
static LPCWSTR const k_Reg_CLSID_7zip_Inproc = L"CLSID\\" k_7zip_CLSID L"\\InprocServer32";

#define g_AllUsers True

static BoolInt g_Install_was_Pressed;
static BoolInt g_Finished;
static BoolInt g_SilentMode;

static HWND g_HWND;
static HWND g_Path_HWND;
static HWND g_InfoLine_HWND;
static HWND g_Progress_HWND;

static DWORD g_TotalSize;

static WCHAR cmd[MAX_PATH + 4];
static WCHAR cmdError[MAX_PATH + 4];
static WCHAR path[MAX_PATH * 2 + 40];


// #define MAKE_CHAR_UPPER(c) ((((c) >= 'a' && (c) <= 'z') ? (c) -= 0x20 : (c)))


static void CpyAscii(wchar_t *dest, const char *s)
{
  for (;;)
  {
    Byte b = (Byte)*s++;
    *dest++ = b;
    if (b == 0)
      return;
  }
}

static void CatAscii(wchar_t *dest, const char *s)
{
  dest += wcslen(dest);
  CpyAscii(dest, s);
}

static void PrintErrorMessage(const char *s1, const wchar_t *s2)
{
  WCHAR m[MAX_PATH + 512];
  m[0] = 0;
  CatAscii(m, "ERROR:");
  if (s1)
  {
    CatAscii(m, "\n");
    CatAscii(m, s1);
  }
  if (s2)
  {
    CatAscii(m, "\n");
    wcscat(m, s2);
  }
  MessageBoxW(g_HWND, m, k_7zip_with_Ver_str, MB_ICONERROR | MB_OK);
}


typedef DWORD (WINAPI * Func_GetFileVersionInfoSizeW)(LPCWSTR lptstrFilename, LPDWORD lpdwHandle);
typedef BOOL (WINAPI * Func_GetFileVersionInfoW)(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
typedef BOOL (WINAPI * Func_VerQueryValueW)(const LPVOID pBlock, LPWSTR lpSubBlock, LPVOID * lplpBuffer, PUINT puLen);

static HMODULE g_version_dll_hModule;

static DWORD GetFileVersion(LPCWSTR s)
{
  DWORD size = 0;
  void *vi = NULL;
  DWORD version = 0;

  Func_GetFileVersionInfoSizeW my_GetFileVersionInfoSizeW;
  Func_GetFileVersionInfoW my_GetFileVersionInfoW;
  Func_VerQueryValueW my_VerQueryValueW;

  if (!g_version_dll_hModule)
  {
    wchar_t buf[MAX_PATH + 100];
    {
      unsigned len = GetSystemDirectoryW(buf, MAX_PATH + 2);
      if (len == 0 || len > MAX_PATH)
        return 0;
    }
    {
      unsigned pos = (unsigned)lstrlenW(buf);
      if (buf[pos - 1] != '\\')
        buf[pos++] = '\\';
      lstrcpyW(buf + pos, L"version.dll");
    }
    g_version_dll_hModule = LoadLibraryW(buf);
    if (!g_version_dll_hModule)
      return 0;
  }

  my_GetFileVersionInfoSizeW = (Func_GetFileVersionInfoSizeW)GetProcAddress(g_version_dll_hModule, "GetFileVersionInfoSizeW");
  my_GetFileVersionInfoW = (Func_GetFileVersionInfoW)GetProcAddress(g_version_dll_hModule, "GetFileVersionInfoW");
  my_VerQueryValueW = (Func_VerQueryValueW)GetProcAddress(g_version_dll_hModule, "VerQueryValueW");

  if (!my_GetFileVersionInfoSizeW
     || !my_GetFileVersionInfoW
     || !my_VerQueryValueW)
    return 0;
  
  size = my_GetFileVersionInfoSizeW(s, NULL);
  if (size == 0)
    return 0;
  
  vi = malloc(size);
  if (!vi)
    return 0;
  
  if (my_GetFileVersionInfoW(s, 0, size, vi))
  {
    VS_FIXEDFILEINFO *fi = NULL;
    UINT fiLen = 0;
    if (my_VerQueryValueW(vi, L"\\", (LPVOID *)&fi, &fiLen))
      version = fi->dwFileVersionMS;
  }
  
  free(vi);
  return version;
}


static WRes MyCreateDir(LPCWSTR name)
{
  return CreateDirectoryW(name, NULL) ? 0 : GetLastError();
}

#define IS_SEPAR(c) (c == WCHAR_PATH_SEPARATOR)
#define IS_LETTER_CHAR(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define IS_DRIVE_PATH(s) (IS_LETTER_CHAR(s[0]) && s[1] == ':' && IS_SEPAR(s[2]))

static int ReverseFind_PathSepar(const wchar_t *s)
{
  int separ = -1;
  int i;
  for (i = 0;; i++)
  {
    wchar_t c = s[i];
    if (c == 0)
      return separ;
    if (IS_SEPAR(c))
      separ = i;
  }
}

static WRes CreateComplexDir()
{
  WCHAR s[MAX_PATH + 10];

  unsigned prefixSize = 0;
  WRes wres;

  {
    size_t len = wcslen(path);
    if (len > MAX_PATH)
      return ERROR_INVALID_NAME;
    wcscpy(s, path);
  }

  if (IS_DRIVE_PATH(s))
    prefixSize = 3;
  else if (IS_SEPAR(s[0]) && IS_SEPAR(s[1]))
    prefixSize = 2;
  else
    return ERROR_INVALID_NAME;

  {
    DWORD attrib = GetFileAttributesW(s);
    if (attrib != INVALID_FILE_ATTRIBUTES)
      return (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0 ? 0 : ERROR_ALREADY_EXISTS;
  }

  wres = MyCreateDir(s);
  if (wres == 0 || wres == ERROR_ALREADY_EXISTS)
    return 0;

  {
    size_t len = wcslen(s);
    {
      int pos = ReverseFind_PathSepar(s);
      if (pos < 0)
        return wres;
      if ((unsigned)pos < prefixSize)
        return wres;
      if ((unsigned)pos == len - 1)
      {
        if (len == 1)
          return 0;
        s[pos] = 0;
        len = pos;
      }
    }

    for (;;)
    {
      int pos;
      wres = MyCreateDir(s);
      if (wres == 0)
        break;
      if (wres == ERROR_ALREADY_EXISTS)
      {
        DWORD attrib = GetFileAttributesW(s);
        if (attrib != INVALID_FILE_ATTRIBUTES)
          if ((attrib & FILE_ATTRIBUTE_DIRECTORY) == 0)
            return ERROR_ALREADY_EXISTS;
        break;
      }
      pos = ReverseFind_PathSepar(s);
      if (pos < 0 || pos == 0 || (unsigned)pos < prefixSize)
        return wres;
      s[pos] = 0;
    }
  
    for (;;)
    {
      size_t pos = wcslen(s);
      if (pos >= len)
        return 0;
      s[pos] = CHAR_PATH_SEPARATOR;
      wres = MyCreateDir(s);
      if (wres != 0)
        return wres;
    }
  }
}


static int MyRegistry_QueryString(HKEY hKey, LPCWSTR name, LPWSTR dest)
{
  DWORD cnt = MAX_PATH * sizeof(name[0]);
  DWORD type = 0;
  LONG res = RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)dest, (DWORD *)&cnt);
  if (type != REG_SZ)
    return False;
  return res == ERROR_SUCCESS;
}

static int MyRegistry_QueryString2(HKEY hKey, LPCWSTR keyName, LPCWSTR valName, LPWSTR dest)
{
  HKEY key = 0;
  LONG res = RegOpenKeyExW(hKey, keyName, 0, KEY_READ | k_Reg_WOW_Flag, &key);
  if (res != ERROR_SUCCESS)
    return False;
  {
    BoolInt res2 = MyRegistry_QueryString(key, valName, dest);
    RegCloseKey(key);
    return res2;
  }
}

static LONG MyRegistry_SetString(HKEY hKey, LPCWSTR name, LPCWSTR val)
{
  return RegSetValueExW(hKey, name, 0, REG_SZ,
      (const BYTE *)val, (DWORD)(wcslen(val) + 1) * sizeof(val[0]));
}

static LONG MyRegistry_SetDWORD(HKEY hKey, LPCWSTR name, DWORD val)
{
  return RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE *)&val, sizeof(DWORD));
}


static LONG MyRegistry_CreateKey(HKEY parentKey, LPCWSTR name, HKEY *destKey)
{
  return RegCreateKeyExW(parentKey, name, 0, NULL,
      REG_OPTION_NON_VOLATILE,
      KEY_ALL_ACCESS | k_Reg_WOW_Flag,
      NULL, destKey, NULL);
}

static LONG MyRegistry_CreateKeyAndVal(HKEY parentKey, LPCWSTR keyName, LPCWSTR valName, LPCWSTR val)
{
  HKEY destKey = 0;
  LONG res = MyRegistry_CreateKey(parentKey, keyName, &destKey);
  if (res == ERROR_SUCCESS)
  {
    res = MyRegistry_SetString(destKey, valName, val);
    /* res = */ RegCloseKey(destKey);
  }
  return res;
}


#ifdef USE_7ZIP_32_DLL

static LONG MyRegistry_CreateKey_32(HKEY parentKey, LPCWSTR name, HKEY *destKey)
{
  return RegCreateKeyExW(parentKey, name, 0, NULL,
      REG_OPTION_NON_VOLATILE,
      KEY_ALL_ACCESS | k_Reg_WOW_Flag_32,
      NULL, destKey, NULL);
}

static LONG MyRegistry_CreateKeyAndVal_32(HKEY parentKey, LPCWSTR keyName, LPCWSTR valName, LPCWSTR val)
{
  HKEY destKey = 0;
  LONG res = MyRegistry_CreateKey_32(parentKey, keyName, &destKey);
  if (res == ERROR_SUCCESS)
  {
    res = MyRegistry_SetString(destKey, valName, val);
    /* res = */ RegCloseKey(destKey);
  }
  return res;
}

#endif



#ifdef UNDER_CE
  #define kBufSize (1 << 13)
#else
  #define kBufSize (1 << 15)
#endif

#define kSignatureSearchLimit (1 << 22)

static BoolInt FindSignature(CSzFile *stream, UInt64 *resPos)
{
  Byte buf[kBufSize];
  size_t numPrevBytes = 0;
  *resPos = 0;
  
  for (;;)
  {
    size_t processed, pos;
    if (*resPos > kSignatureSearchLimit)
      return False;
    processed = kBufSize - numPrevBytes;
    if (File_Read(stream, buf + numPrevBytes, &processed) != 0)
      return False;
    processed += numPrevBytes;
    if (processed < k7zStartHeaderSize ||
        (processed == k7zStartHeaderSize && numPrevBytes != 0))
      return False;
    processed -= k7zStartHeaderSize;
    for (pos = 0; pos <= processed; pos++)
    {
      for (; pos <= processed && buf[pos] != '7'; pos++);
      if (pos > processed)
        break;
      if (memcmp(buf + pos, k7zSignature, k7zSignatureSize) == 0)
        if (CrcCalc(buf + pos + 12, 20) == GetUi32(buf + pos + 8))
        {
          *resPos += pos;
          return True;
        }
    }
    *resPos += processed;
    numPrevBytes = k7zStartHeaderSize;
    memmove(buf, buf + processed, k7zStartHeaderSize);
  }
}

static void HexToString(UInt32 val, WCHAR *s)
{
  UInt64 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 4;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)((val & 0xF));
    val >>= 4;
    s[--i] = (WCHAR)(unsigned)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
  }
  while (i);
}


#ifndef UNDER_CE

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM data)
{
  UNUSED_VAR(lp)
  UNUSED_VAR(data)
  UNUSED_VAR(hwnd)

  switch (uMsg)
  {
    case BFFM_INITIALIZED:
    {
      SendMessage(hwnd, BFFM_SETSELECTIONW, TRUE, data);
      break;
    }
    case BFFM_SELCHANGED:
    {
      // show selected path for BIF_STATUSTEXT
      WCHAR dir[MAX_PATH];
      if (!SHGetPathFromIDListW((LPITEMIDLIST)lp, dir))
        dir[0] = 0;
      SendMessage(hwnd, BFFM_SETSTATUSTEXTW, 0, (LPARAM)dir);
      break;
    }
    default:
      break;
  }
  return 0;
}

static BoolInt MyBrowseForFolder(HWND owner, LPCWSTR title, UINT ulFlags,
    LPCWSTR initialFolder, LPWSTR resultPath)
{
  WCHAR displayName[MAX_PATH];
  BROWSEINFOW browseInfo;

  displayName[0] = 0;
  browseInfo.hwndOwner = owner;
  browseInfo.pidlRoot = NULL;

  // there are Unicode/Astring problems in some WinCE SDK ?
  browseInfo.pszDisplayName = displayName;
  browseInfo.lpszTitle = title;
  browseInfo.ulFlags = ulFlags;
  browseInfo.lpfn = (initialFolder != NULL) ? BrowseCallbackProc : NULL;
  browseInfo.lParam = (LPARAM)initialFolder;
  {
    LPITEMIDLIST idlist = SHBrowseForFolderW(&browseInfo);
    if (idlist)
    {
      SHGetPathFromIDListW(idlist, resultPath);
      // free idlist
      // CoTaskMemFree(idlist);
      return True;
    }
    return False;
  }
}

#endif

static void NormalizePrefix(WCHAR *s)
{
  size_t i = 0;
  
  for (;; i++)
  {
    wchar_t c = s[i];
    if (c == 0)
      break;
    if (c == '/')
      s[i] = WCHAR_PATH_SEPARATOR;
  }
  
  if (i != 0 && s[i - 1] != WCHAR_PATH_SEPARATOR)
  {
    s[i] = WCHAR_PATH_SEPARATOR;
    s[i + 1] = 0;
  }
}

static char MyCharLower_Ascii(char c)
{
  if (c >= 'A' && c <= 'Z')
    return (char)((unsigned char)c + 0x20);
  return c;
}

static wchar_t MyWCharLower_Ascii(wchar_t c)
{
  if (c >= 'A' && c <= 'Z')
    return (wchar_t)(c + 0x20);
  return c;
}

static LPCWSTR FindSubString(LPCWSTR s1, const char *s2)
{
  for (;;)
  {
    unsigned i;
    if (*s1 == 0)
      return NULL;
    for (i = 0;; i++)
    {
      Byte b = s2[i];
      if (b == 0)
        return s1;
      if (MyWCharLower_Ascii(s1[i]) != (Byte)MyCharLower_Ascii(b))
      {
        s1++;
        break;
      }
    }
  }
}

static void Set7zipPostfix(WCHAR *s)
{
  NormalizePrefix(s);
  if (FindSubString(s, "7-Zip"))
    return;
  CatAscii(s, "7-Zip\\");
}
    

static int Install(void);

static void OnClose()
{
  if (g_Install_was_Pressed && !g_Finished)
  {
    if (MessageBoxW(g_HWND,
        L"Do you want to cancel " k_7zip_with_Ver L" installation?",
        k_7zip_with_Ver,
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES)
      return;
  }
  DestroyWindow(g_HWND);
  g_HWND = NULL;
}

static INT_PTR CALLBACK MyDlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // UNUSED_VAR(hwnd)
  UNUSED_VAR(lParam)

  switch (message)
  {
    case WM_INITDIALOG:
      g_Path_HWND = GetDlgItem(hwnd, IDE_EXTRACT_PATH);
      g_InfoLine_HWND = GetDlgItem(hwnd, IDT_CUR_FILE);
      g_Progress_HWND = GetDlgItem(hwnd, IDC_PROGRESS);

      SetWindowTextW(hwnd, k_7zip_Setup);
      SetDlgItemTextW(hwnd, IDE_EXTRACT_PATH, path);

      ShowWindow(g_Progress_HWND, SW_HIDE);
      ShowWindow(g_InfoLine_HWND, SW_HIDE);

      break;

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDOK:
        {
          if (g_Finished)
          {
            OnClose();
            break;
          }
          if (!g_Install_was_Pressed)
          {
            SendMessage(hwnd, WM_NEXTDLGCTL, (WPARAM)(void *)GetDlgItem(hwnd, IDCANCEL), TRUE);
            
            EnableWindow(g_Path_HWND, FALSE);
            EnableWindow(GetDlgItem(hwnd, IDB_EXTRACT_SET_PATH), FALSE);
            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
            
            g_Install_was_Pressed = True;
            return TRUE;
          }
          break;
        }
        
        case IDCANCEL:
        {
          OnClose();
          break;
        }
        
        case IDB_EXTRACT_SET_PATH:
        {
          #ifndef UNDER_CE
          
          WCHAR s[MAX_PATH];
          WCHAR s2[MAX_PATH];
          GetDlgItemTextW(hwnd, IDE_EXTRACT_PATH, s, MAX_PATH);
          if (MyBrowseForFolder(hwnd, L"Select the folder for installation:" ,
              0
              | BIF_NEWDIALOGSTYLE // 5.0 of ?.dll ?
              | BIF_RETURNONLYFSDIRS
              // | BIF_STATUSTEXT // doesn't work for BIF_NEWDIALOGSTYLE
              , s, s2))
          {
            Set7zipPostfix(s2);
            SetDlgItemTextW(hwnd, IDE_EXTRACT_PATH, s2);
          }
          
          #endif
          break;
        }
        
        default: return FALSE;
      }
      break;
    
    case WM_CLOSE:
      OnClose();
      break;
    /*
    case WM_DESTROY:
      PostQuitMessage(0);
      return TRUE;
    */
    default:
      return FALSE;
  }
  
  return TRUE;
}



static LONG SetRegKey_Path2(HKEY parentKey)
{
  HKEY destKey = 0;
  LONG res = MyRegistry_CreateKey(parentKey, k_Reg_Software_7zip, &destKey);
  if (res == ERROR_SUCCESS)
  {
    res = MyRegistry_SetString(destKey, k_Reg_Path32, path);
    /* res = */ MyRegistry_SetString(destKey, k_Reg_Path, path);
    /* res = */ RegCloseKey(destKey);
  }
  return res;
}

static void SetRegKey_Path()
{
  SetRegKey_Path2(HKEY_CURRENT_USER);
  SetRegKey_Path2(HKEY_LOCAL_MACHINE);
}


static HRESULT CreateShellLink(LPCWSTR srcPath, LPCWSTR targetPath)
{
  IShellLinkW* sl;
 
  // CoInitialize has already been called.
  HRESULT hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (LPVOID*)&sl);

  if (SUCCEEDED(hres))
  {
    IPersistFile* pf;
    
    sl->lpVtbl->SetPath(sl, targetPath);
    // sl->lpVtbl->SetDescription(sl, description);
    hres = sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (LPVOID*)&pf);
   
    if (SUCCEEDED(hres))
    {
      hres = pf->lpVtbl->Save(pf, srcPath, TRUE);
      pf->lpVtbl->Release(pf);
    }
    sl->lpVtbl->Release(sl);
  }

  return hres;
}

static void SetShellProgramsGroup(HWND hwndOwner)
{
  #ifdef UNDER_CE

  // CpyAscii(link, "\\Program Files\\");
  UNUSED_VAR(hwndOwner)

  #else

  unsigned i = (g_AllUsers ? 0 : 2);

  for (; i < 3; i++)
  {
    BoolInt isOK = True;
    WCHAR link[MAX_PATH + 40];
    WCHAR destPath[MAX_PATH + 40];

    link[0] = 0;
    
    if (SHGetFolderPathW(hwndOwner,
        i == 1 ? CSIDL_COMMON_PROGRAMS : CSIDL_PROGRAMS,
        NULL, SHGFP_TYPE_CURRENT, link) != S_OK)
      continue;

    NormalizePrefix(link);
    CatAscii(link, k_7zip);
    // CatAscii(link, "2");
    
    if (i != 0)
      MyCreateDir(link);
    
    NormalizePrefix(link);
    
    {
      unsigned baseLen = (unsigned)wcslen(link);
      unsigned k;

      for (k = 0; k < 2; k++)
      {
        CpyAscii(link + baseLen, k == 0 ?
            "7-Zip File Manager.lnk" :
            "7-Zip Help.lnk"
           );
        wcscpy(destPath, path);
        CatAscii(destPath, k == 0 ?
            "7zFM.exe" :
            "7-zip.chm");
        
        if (i == 0)
          DeleteFileW(link);
        else if (CreateShellLink(link, destPath) != S_OK)
          isOK = False;
      }
    }

    if (i != 0 && isOK)
      break;
  }

  #endif
}

static LPCWSTR const k_Shell_Approved = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
static LPCWSTR const k_7zip_ShellExtension = L"7-Zip Shell Extension";

static void WriteCLSID()
{
  HKEY destKey;
  LONG res;

  #ifdef USE_7ZIP_32_DLL
 
  MyRegistry_CreateKeyAndVal_32(HKEY_CLASSES_ROOT, k_Reg_CLSID_7zip, NULL, k_7zip_ShellExtension);
  
  res = MyRegistry_CreateKey_32(HKEY_CLASSES_ROOT, k_Reg_CLSID_7zip_Inproc, &destKey);
  
  if (res == ERROR_SUCCESS)
  {
    WCHAR destPath[MAX_PATH + 40];
    wcscpy(destPath, path);
    CatAscii(destPath, "7-zip32.dll");
    /* res = */ MyRegistry_SetString(destKey, NULL, destPath);
    /* res = */ MyRegistry_SetString(destKey, L"ThreadingModel", L"Apartment");
    // DeleteRegValue(destKey, L"InprocServer32");
    /* res = */ RegCloseKey(destKey);
  }

  #endif

  
  MyRegistry_CreateKeyAndVal(HKEY_CLASSES_ROOT, k_Reg_CLSID_7zip, NULL, k_7zip_ShellExtension);
  
  destKey = 0;
  res = MyRegistry_CreateKey(HKEY_CLASSES_ROOT, k_Reg_CLSID_7zip_Inproc, &destKey);
  
  if (res == ERROR_SUCCESS)
  {
    WCHAR destPath[MAX_PATH + 40];
    wcscpy(destPath, path);
    CatAscii(destPath, "7-zip.dll");
    /* res = */ MyRegistry_SetString(destKey, NULL, destPath);
    /* res = */ MyRegistry_SetString(destKey, L"ThreadingModel", L"Apartment");
    // DeleteRegValue(destKey, L"InprocServer32");
    /* res = */ RegCloseKey(destKey);
  }
}

static LPCSTR const k_ShellEx_Items[] =
{
    "*\\shellex\\ContextMenuHandlers"
  , "Directory\\shellex\\ContextMenuHandlers"
  , "Folder\\shellex\\ContextMenuHandlers"
  , "Directory\\shellex\\DragDropHandlers"
  , "Drive\\shellex\\DragDropHandlers"
};

static void WriteShellEx()
{
  unsigned i;
  WCHAR destPath[MAX_PATH + 40];

  for (i = 0; i < ARRAY_SIZE(k_ShellEx_Items); i++)
  {
    CpyAscii(destPath, k_ShellEx_Items[i]);
    CatAscii(destPath, "\\7-Zip");

    #ifdef USE_7ZIP_32_DLL
    MyRegistry_CreateKeyAndVal_32(HKEY_CLASSES_ROOT, destPath, NULL, k_7zip_CLSID);
    #endif
    MyRegistry_CreateKeyAndVal   (HKEY_CLASSES_ROOT, destPath, NULL, k_7zip_CLSID);
  }

  #ifdef USE_7ZIP_32_DLL
  MyRegistry_CreateKeyAndVal_32(HKEY_LOCAL_MACHINE, k_Shell_Approved, k_7zip_CLSID, k_7zip_ShellExtension);
  #endif
  MyRegistry_CreateKeyAndVal   (HKEY_LOCAL_MACHINE, k_Shell_Approved, k_7zip_CLSID, k_7zip_ShellExtension);


  wcscpy(destPath, path);
  CatAscii(destPath, "7zFM.exe");
  
  {
    HKEY destKey = 0;
    LONG res = MyRegistry_CreateKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\7zFM.exe", &destKey);
    if (res == ERROR_SUCCESS)
    {
      MyRegistry_SetString(destKey, NULL, destPath);
      MyRegistry_SetString(destKey, L"Path", path);
      RegCloseKey(destKey);
    }

  }
  
  {
    HKEY destKey = 0;
    LONG res = MyRegistry_CreateKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\7-Zip", &destKey);
    if (res == ERROR_SUCCESS)
    {
      MyRegistry_SetString(destKey, L"DisplayName", k_7zip_with_Ver_str);
      MyRegistry_SetString(destKey, L"DisplayVersion", LLL(MY_VERSION_NUMBERS));
      MyRegistry_SetString(destKey, L"DisplayIcon", destPath);
      MyRegistry_SetString(destKey, L"InstallLocation", path);

      destPath[0] = '\"';
      wcscpy(destPath + 1, path);
      CatAscii(destPath, "Uninstall.exe\"");
      MyRegistry_SetString(destKey, L"UninstallString", destPath);
      
      MyRegistry_SetDWORD(destKey, L"NoModify", 1);
      MyRegistry_SetDWORD(destKey, L"NoRepair", 1);

      MyRegistry_SetDWORD(destKey, L"EstimatedSize", g_TotalSize >> 10);
      
      MyRegistry_SetDWORD(destKey, L"VersionMajor", MY_VER_MAJOR);
      MyRegistry_SetDWORD(destKey, L"VersionMinor", MY_VER_MINOR);
  
      MyRegistry_SetString(destKey, L"Publisher", LLL(MY_AUTHOR_NAME));
      
      // MyRegistry_SetString(destKey, L"HelpLink", L"http://www.7-zip.org/support.html");
      // MyRegistry_SetString(destKey, L"URLInfoAbout", L"http://www.7-zip.org/");
      // MyRegistry_SetString(destKey, L"URLUpdateInfo", L"http://www.7-zip.org/");
      
      RegCloseKey(destKey);
    }
  }
}


static const wchar_t *GetCmdParam(const wchar_t *s)
{
  unsigned pos = 0;
  BoolInt quoteMode = False;
  for (;; s++)
  {
    wchar_t c = *s;
    if (c == 0 || (c == L' ' && !quoteMode))
      break;
    if (c == L'\"')
    {
      quoteMode = !quoteMode;
      continue;
    }
    if (pos >= ARRAY_SIZE(cmd) - 1)
      exit(1);
    cmd[pos++] = c;
  }
  cmd[pos] = 0;
  return s;
}


static void RemoveQuotes(wchar_t *s)
{
  const wchar_t *src = s;
  for (;;)
  {
    wchar_t c = *src++;
    if (c == '\"')
      continue;
    *s++ = c;
    if (c == 0)
      return;
  }
}

// #define IS_LIMIT_CHAR(c) (c == 0 || c == ' ')


typedef BOOL (WINAPI *Func_IsWow64Process)(HANDLE, PBOOL);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    #ifdef UNDER_CE
      LPWSTR
    #else
      LPSTR
    #endif
    lpCmdLine, int nCmdShow)
{

  UNUSED_VAR(hPrevInstance)
  UNUSED_VAR(lpCmdLine)
  UNUSED_VAR(nCmdShow)

  #ifndef UNDER_CE
  LoadSecurityDlls();
  CoInitialize(NULL);
  #endif

  CrcGenerateTable();

  {
    const wchar_t *s = GetCommandLineW();
    
    #ifndef UNDER_CE
    s = GetCmdParam(s);
    #endif

    for (;;)
    {
      {
        wchar_t c = *s;
        if (c == 0)
          break;
        if (c == ' ')
        {
          s++;
          continue;
        }
      }

      {
        const wchar_t *s2 = GetCmdParam(s);
        BoolInt error = True;
        if (cmd[0] == '/')
        {
          if (cmd[1] == 'S')
          {
            if (cmd[2] == 0)
            {
              g_SilentMode = True;
              error = False;
            }
          }
          else if (cmd[1] == 'D' && cmd[2] == '=')
          {
            wcscpy(path, cmd + 3);
            // RemoveQuotes(path);
            error = False;
          }
        }
        s = s2;
        if (error && cmdError[0] == 0)
          wcscpy(cmdError, cmd);
      }
    }

    if (cmdError[0] != 0)
    {
      if (!g_SilentMode)
        PrintErrorMessage("Unsupported command:", cmdError);
      return 1;
    }
  }

  #if defined(_64BIT_INSTALLER) && !defined(_WIN64)
  {
    BOOL isWow64 = FALSE;
    Func_IsWow64Process func_IsWow64Process = (Func_IsWow64Process)
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process");
    
    if (func_IsWow64Process)
      func_IsWow64Process(GetCurrentProcess(), &isWow64);

    if (!isWow64)
    {
      if (!g_SilentMode)
        PrintErrorMessage("This installation requires Windows " MY_CPU_NAME, NULL);
      return 1;
    }
  }
  #endif


  if (path[0] == 0)
  {
    HKEY key = 0;
    BoolInt ok = False;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, k_Reg_Software_7zip, 0, KEY_READ | k_Reg_WOW_Flag, &key);
    if (res == ERROR_SUCCESS)
    {
      ok = MyRegistry_QueryString(key, k_Reg_Path32, path);
      // ok = MyRegistry_QueryString(key, k_Reg_Path, path);
      RegCloseKey(key);
    }
    
    // ok = False;
    if (!ok)
    {
      /*
      #ifdef UNDER_CE
        CpyAscii(path, "\\Program Files\\");
      #else
      
        #ifdef _64BIT_INSTALLER
        {
          DWORD ttt = GetEnvironmentVariableW(L"ProgramW6432", path, MAX_PATH);
          if (ttt == 0 || ttt > MAX_PATH)
            CpyAscii(path, "C:\\");
        }
        #else
        if (!SHGetSpecialFolderPathW(0, path, CSIDL_PROGRAM_FILES, FALSE))
          CpyAscii(path, "C:\\");
        #endif
      #endif
      */
      if (!MyRegistry_QueryString2(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir", path))
        CpyAscii(path,
            #ifdef UNDER_CE
              "\\Program Files\\"
            #else
              "C:\\"
            #endif
            );

      Set7zipPostfix(path);
    }
  }

  NormalizePrefix(path);

  if (g_SilentMode)
    return Install();

  {
    int retCode = 1;
    // INT_PTR res = DialogBox(
    g_HWND = CreateDialog(
        hInstance,
        // GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_INSTALL), NULL, MyDlgProc);
    if (!g_HWND)
      return 1;

    {
      HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
      // SendMessage(g_HWND, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon);
      SendMessage(g_HWND, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hIcon);
    }

  
    {
      BOOL bRet;
      MSG msg;
      
      // we need messages for all thread windows (including EDITTEXT window in dialog)
      while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
      {
        if (bRet == -1)
          return retCode;
        if (!g_HWND)
          return retCode;

        if (!IsDialogMessage(g_HWND, &msg))
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
        if (!g_HWND)
          return retCode;

        if (g_Install_was_Pressed && !g_Finished)
        {
          retCode = Install();
          g_Finished = True;
          if (retCode != 0)
            break;
          if (!g_HWND)
            break;
          {
            SetDlgItemTextW(g_HWND, IDOK, L"Close");
            EnableWindow(GetDlgItem(g_HWND, IDOK), TRUE);
            EnableWindow(GetDlgItem(g_HWND, IDCANCEL), FALSE);
            SendMessage(g_HWND, WM_NEXTDLGCTL, (WPARAM)(void *)GetDlgItem(g_HWND, IDOK), TRUE);
          }
        }
      }

      if (g_HWND)
      {
        DestroyWindow(g_HWND);
        g_HWND = NULL;
      }
    }

    return retCode;
  }
}


static BoolInt GetErrorMessage(DWORD errorCode, WCHAR *message)
{
  LPWSTR msgBuf;
  if (FormatMessageW(
          FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, 0, (LPWSTR) &msgBuf, 0, NULL) == 0)
    return False;
  wcscpy(message, msgBuf);
  LocalFree(msgBuf);
  return True;
}



static int Install(void)
{
  CFileInStream archiveStream;
  CLookToRead2 lookStream;
  CSzArEx db;
  
  SRes res = SZ_OK;
  WRes winRes = 0;
  const char *errorMessage = NULL;
  
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  WCHAR sfxPath[MAX_PATH + 2];

  int needRebootLevel = 0;

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;

  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  {
    DWORD len = GetModuleFileNameW(NULL, sfxPath, MAX_PATH);
    if (len == 0 || len > MAX_PATH)
      return 1;
  }

  winRes = InFile_OpenW(&archiveStream.file, sfxPath);

  if (winRes == 0)
  {
    UInt64 pos = 0;
    if (!FindSignature(&archiveStream.file, &pos))
      errorMessage = "Can't find 7z archive";
    else
      winRes = File_Seek(&archiveStream.file, (Int64 *)&pos, SZ_SEEK_SET);
  }

  if (winRes != 0)
    res = SZ_ERROR_FAIL;

  if (errorMessage)
    res = SZ_ERROR_FAIL;

if (res == SZ_OK)
{
  size_t pathLen;
  if (!g_SilentMode)
  {
    GetDlgItemTextW(g_HWND, IDE_EXTRACT_PATH, path, MAX_PATH);
  }

  FileInStream_CreateVTable(&archiveStream);
  LookToRead2_CreateVTable(&lookStream, False);
  lookStream.buf = NULL;
 
  RemoveQuotes(path);
  {
    // Remove post spaces
    unsigned endPos = 0;
    unsigned i = 0;
    
    for (;;)
    {
      wchar_t c = path[i++];
      if (c == 0)
        break;
      if (c != ' ')
        endPos = i;
    }

    path[endPos] = 0;
    if (path[0] == 0)
    {
      PrintErrorMessage("Incorrect path", NULL);
      return 1;
    }
  }

  NormalizePrefix(path);
  winRes = CreateComplexDir();

  if (winRes != 0)
    res = SZ_ERROR_FAIL;

  pathLen = wcslen(path);

  if (res == SZ_OK)
  {
    lookStream.buf = (Byte *)ISzAlloc_Alloc(&allocImp, kInputBufSize);
    if (!lookStream.buf)
      res = SZ_ERROR_MEM;
    else
    {
      lookStream.bufSize = kInputBufSize;
      lookStream.realStream = &archiveStream.vt;
      LookToRead2_Init(&lookStream);
    }
  }

  SzArEx_Init(&db);

  if (res == SZ_OK)
  {
    res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
  }
    
  if (res == SZ_OK)
  {
    UInt32 i;
    UInt32 blockIndex = 0xFFFFFFFF; /* it can have any value before first call, if (!outBuf) */
    Byte *outBuf = NULL; /* it must be NULL before first call for each new archive. */
    size_t outBufSize = 0;  /* it can have any value before first call, if (!outBuf) */
    
    g_TotalSize = 0;

    if (!g_SilentMode)
    {
      ShowWindow(g_Progress_HWND, SW_SHOW);
      ShowWindow(g_InfoLine_HWND, SW_SHOW);
      SendMessage(g_Progress_HWND, PBM_SETRANGE32, 0, db.NumFiles);
    }
    
    for (i = 0; i < db.NumFiles; i++)
    {
      size_t offset = 0;
      size_t outSizeProcessed = 0;
      WCHAR *temp;

      if (!g_SilentMode)
      {
        MSG msg;
        
        // g_HWND
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
          if (!IsDialogMessage(g_HWND, &msg))
          {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
          if (!g_HWND)
            return 1;
        }

        // Sleep(10);
        SendMessage(g_Progress_HWND, PBM_SETPOS, i, 0);
      }

      {
        size_t len = SzArEx_GetFileNameUtf16(&db, i, NULL);
        if (len >= MAX_PATH)
        {
          res = SZ_ERROR_FAIL;
          break;
        }
      }
        
      temp = path + pathLen;
      
      SzArEx_GetFileNameUtf16(&db, i, (UInt16 *)temp);

      if (!g_SilentMode)
        SetWindowTextW(g_InfoLine_HWND, temp);

      {
        res = SzArEx_Extract(&db, &lookStream.vt, i,
            &blockIndex, &outBuf, &outBufSize,
            &offset, &outSizeProcessed,
            &allocImp, &allocTempImp);
        if (res != SZ_OK)
          break;
      }

      {
        CSzFile outFile;
        size_t processedSize;
        size_t j;
        // size_t nameStartPos = 0;
        UInt32 tempIndex = 0;
        int fileLevel = 1 << 2;
        WCHAR origPath[MAX_PATH * 2 + 10];

        for (j = 0; temp[j] != 0; j++)
        {
          if (temp[j] == '/')
          {
            temp[j] = 0;
            MyCreateDir(path);
            temp[j] = CHAR_PATH_SEPARATOR;
            // nameStartPos = j + 1;
          }
        }

        if (SzArEx_IsDir(&db, i))
        {
          MyCreateDir(path);
          continue;
        }

        {
          // BoolInt skipFile = False;
          
          wcscpy(origPath, path);
  
          for (;;)
          {
            WRes openRes;

            if (tempIndex != 0)
            {
              if (tempIndex > 100)
              {
                res = SZ_ERROR_FAIL;
                break;
              }
              wcscpy(path, origPath);
              CatAscii(path, ".tmp");
              if (tempIndex > 1)
                HexToString(tempIndex, path + wcslen(path));
              if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
              {
                tempIndex++;
                continue;
              }
            }
            
            {
              SetFileAttributesW(path, 0);
              openRes = OutFile_OpenW(&outFile, path);
              if (openRes == 0)
                break;
            }

            if (tempIndex != 0)
            {
              tempIndex++;
              continue;
            }
            
            if (FindSubString(temp, "7-zip.dll")
                #ifdef USE_7ZIP_32_DLL
                || FindSubString(temp, "7-zip32.dll")
                #endif
                )
            {
              DWORD ver = GetFileVersion(path);
              fileLevel = ((ver < _7ZIP_DLL_VER_COMPAT || ver > _7ZIP_CUR_VER) ? 2 : 1);
              tempIndex++;
              continue;
            }

            if (g_SilentMode)
            {
              tempIndex++;
              continue;
            }
            {
              WCHAR message[MAX_PATH * 3 + 100];
              int mbRes;

              CpyAscii(message, "Can't open file\n");
              wcscat(message, path);
              CatAscii(message, "\n");
              
              GetErrorMessage(openRes, message + wcslen(message));

              mbRes = MessageBoxW(g_HWND, message, L"Error", MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_DEFBUTTON3);
              if (mbRes == IDABORT)
              {
                res = SZ_ERROR_ABORT;
                tempIndex = 0;
                break;
              }
              if (mbRes == IDIGNORE)
              {
                // skipFile = True;
                tempIndex++;
              }
            }
          }
          
          if (res != SZ_OK)
            break;

          /*
          if (skipFile)
            continue;
          */
        }
  
        // if (res == SZ_OK)
        {
          processedSize = outSizeProcessed;
          winRes = File_Write(&outFile, outBuf + offset, &processedSize);
          if (winRes != 0 || processedSize != outSizeProcessed)
          {
            errorMessage = "Can't write output file";
            res = SZ_ERROR_FAIL;
          }
          
          g_TotalSize += (DWORD)outSizeProcessed;

          #ifdef USE_WINDOWS_FILE
          if (SzBitWithVals_Check(&db.MTime, i))
          {
            const CNtfsFileTime *t = db.MTime.Vals + i;
            FILETIME mTime;
            mTime.dwLowDateTime = t->Low;
            mTime.dwHighDateTime = t->High;
            SetFileTime(outFile.handle, NULL, NULL, &mTime);
          }
          #endif
          
          {
            SRes winRes2 = File_Close(&outFile);
            if (res != SZ_OK)
              break;
            if (winRes2 != 0)
            {
              winRes = winRes2;
              break;
            }
          }
          
          #ifdef USE_WINDOWS_FILE
          if (SzBitWithVals_Check(&db.Attribs, i))
            SetFileAttributesW(path, db.Attribs.Vals[i]);
          #endif
        }

        if (tempIndex != 0)
        {
          // is it supported at win2000 ?
          #ifndef UNDER_CE
          if (!MoveFileExW(path, origPath, MOVEFILE_DELAY_UNTIL_REBOOT | MOVEFILE_REPLACE_EXISTING))
          {
            winRes = GetLastError();
            break;
          }
          needRebootLevel |= fileLevel;
          #endif
        }

      }
    }

    ISzAlloc_Free(&allocImp, outBuf);

    if (!g_SilentMode)
      SendMessage(g_Progress_HWND, PBM_SETPOS, i, 0);

    path[pathLen] = 0;

    if (i == db.NumFiles)
    {
      SetRegKey_Path();
      WriteCLSID();
      WriteShellEx();
      
      SetShellProgramsGroup(g_HWND);
      if (!g_SilentMode)
        SetWindowTextW(g_InfoLine_HWND, k_7zip_with_Ver L" is installed");
    }
  }

  SzArEx_Free(&db, &allocImp);

  ISzAlloc_Free(&allocImp, lookStream.buf);

  File_Close(&archiveStream.file);

}

  if (winRes != 0)
    res = SZ_ERROR_FAIL;

  if (res == SZ_OK)
  {
    if (!g_SilentMode && needRebootLevel > 1)
    {
      if (MessageBoxW(g_HWND, L"You must restart your system to complete the installation.\nRestart now?",
          k_7zip_Setup, MB_YESNO | MB_DEFBUTTON2) == IDYES)
      {
        #ifndef UNDER_CE
        
        // Get a token for this process.
        HANDLE hToken;

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
          TOKEN_PRIVILEGES tkp;
          // Get the LUID for the shutdown privilege.
          LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
          tkp.PrivilegeCount = 1;  // one privilege to set
          tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
          // Get the shutdown privilege for this process.
          AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
          
          if (GetLastError() == ERROR_SUCCESS)
          {
            if (!ExitWindowsEx(EWX_REBOOT, 0))
            {
            }
          }
        }

        #endif
      }
    }
    
    if (res == SZ_OK)
      return 0;
  }

  if (!g_SilentMode)
  {
    if (winRes != 0)
    {
      WCHAR m[MAX_PATH + 100];
      m[0] = 0;
      GetErrorMessage(winRes, m);
      PrintErrorMessage(NULL, m);
    }
    else
    {
      if (res == SZ_ERROR_ABORT)
        return 2;

      if (res == SZ_ERROR_UNSUPPORTED)
        errorMessage = "Decoder doesn't support this archive";
      else if (res == SZ_ERROR_MEM)
        errorMessage = "Can't allocate required memory";
      else if (res == SZ_ERROR_CRC)
        errorMessage = "CRC error";
      else if (res == SZ_ERROR_DATA)
        errorMessage = "Data error";
      
      if (!errorMessage)
        errorMessage = "ERROR";
      PrintErrorMessage(errorMessage, NULL);
    }
  }
  
  return 1;
}
