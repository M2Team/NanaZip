// Windows/CommonDialog.cpp

#include "StdAfx.h"

#include "../Common/MyWindows.h"

#ifdef UNDER_CE
#include <commdlg.h>
#endif

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "CommonDialog.h"
#include "Defs.h"

// **************** NanaZip Modification Start ****************
#include <shobjidl.h>
#include <K7User.h>
// **************** NanaZip Modification End ****************

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {

#ifndef _UNICODE

class CDoubleZeroStringListA
{
  LPTSTR Buf;
  unsigned Size;
public:
  CDoubleZeroStringListA(LPSTR buf, unsigned size): Buf(buf), Size(size) {}
  bool Add(LPCSTR s) throw();
  void Finish() { *Buf = 0; }
};

bool CDoubleZeroStringListA::Add(LPCSTR s) throw()
{
  unsigned len = MyStringLen(s) + 1;
  if (len >= Size)
    return false;
  MyStringCopy(Buf, s);
  Buf += len;
  Size -= len;
  return true;
}

#endif

class CDoubleZeroStringListW
{
  LPWSTR Buf;
  unsigned Size;
public:
  CDoubleZeroStringListW(LPWSTR buf, unsigned size): Buf(buf), Size(size) {}
  bool Add(LPCWSTR s) throw();
  void Finish() { *Buf = 0; }
};

bool CDoubleZeroStringListW::Add(LPCWSTR s) throw()
{
  unsigned len = MyStringLen(s) + 1;
  if (len >= Size)
    return false;
  MyStringCopy(Buf, s);
  Buf += len;
  Size -= len;
  return true;
}


#ifdef UNDER_CE
#define MY__OFN_PROJECT  0x00400000
#define MY__OFN_SHOW_ALL 0x01000000
#endif

/* if (lpstrFilter == NULL && nFilterIndex == 0)
  MSDN : "the system doesn't show any files",
  but WinXP-64 shows all files. Why ??? */

/*
structures
  OPENFILENAMEW
  OPENFILENAMEA
contain additional members:
#if (_WIN32_WINNT >= 0x0500)
  void *pvReserved;
  DWORD dwReserved;
  DWORD FlagsEx;
#endif

If we compile the source code with (_WIN32_WINNT >= 0x0500), some functions
will not work at NT 4.0, if we use sizeof(OPENFILENAME*).
So we use size of old version of structure. */

#if defined(UNDER_CE) || defined(_WIN64) || (_WIN32_WINNT < 0x0500)
// || !defined(WINVER)
  #ifndef _UNICODE
  #define my_compatib_OPENFILENAMEA_size sizeof(OPENFILENAMEA)
  #endif
  #define my_compatib_OPENFILENAMEW_size sizeof(OPENFILENAMEW)
#else

  // MinGW doesn't support some required macros. So we define them here:
  #ifndef CDSIZEOF_STRUCT
  #define CDSIZEOF_STRUCT(structname, member)  (((int)((LPBYTE)(&((structname*)0)->member) - ((LPBYTE)((structname*)0)))) + sizeof(((structname*)0)->member))
  #endif
  #ifndef _UNICODE
  #ifndef OPENFILENAME_SIZE_VERSION_400A
  #define OPENFILENAME_SIZE_VERSION_400A  CDSIZEOF_STRUCT(OPENFILENAMEA,lpTemplateName)
  #endif
  #endif
  #ifndef OPENFILENAME_SIZE_VERSION_400W
  #define OPENFILENAME_SIZE_VERSION_400W  CDSIZEOF_STRUCT(OPENFILENAMEW,lpTemplateName)
  #endif
  
  #ifndef _UNICODE
  #define my_compatib_OPENFILENAMEA_size OPENFILENAME_SIZE_VERSION_400A
  #endif
  #define my_compatib_OPENFILENAMEW_size OPENFILENAME_SIZE_VERSION_400W
#endif

#ifndef _UNICODE
#define CONV_U_To_A(dest, src, temp) AString temp; if (src) { temp = GetSystemString(src); dest = temp; }
#endif

// **************** NanaZip Modification Start ****************
// IFileOpenDialog support. The GUIDs are defined here because the 7-Zip
// projects don't link uuid.lib, and SHCreateItemFromParsingName is resolved
// dynamically to avoid adding a shlwapi.lib dependency.
namespace NFileOpenDialogInternal {

// {DC1C5A9C-E88A-4DDE-A5A1-60F82A20AEF7}
static const CLSID k_CLSID_FileOpenDialog =
  { 0xDC1C5A9C, 0xE88A, 0x4DDE, { 0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7 } };

// {D57C7288-D4AD-4768-BE02-9D969532D960}
static const IID k_IID_IFileOpenDialog =
  { 0xD57C7288, 0xD4AD, 0x4768, { 0xBE, 0x02, 0x9D, 0x96, 0x95, 0x32, 0xD9, 0x60 } };

// {43826D1E-E718-42EE-BC55-A1E261C37BFE}
static const IID k_IID_IShellItem =
  { 0x43826D1E, 0xE718, 0x42EE, { 0xBC, 0x55, 0xA1, 0xE2, 0x61, 0xC3, 0x7B, 0xFE } };

typedef HRESULT (WINAPI *SHCreateItemFromParsingNameFn)(
    PCWSTR pszPath, IBindCtx* pbc, REFIID riid, void** ppv);

static SHCreateItemFromParsingNameFn GetSHCreateItemFromParsingName()
{
  static SHCreateItemFromParsingNameFn Cached = []() -> SHCreateItemFromParsingNameFn
  {
    HMODULE Module = ::GetModuleHandleW(L"shell32.dll");
    if (!Module)
    {
      Module = ::LoadLibraryW(L"shell32.dll");
    }
    return Module
      ? reinterpret_cast<SHCreateItemFromParsingNameFn>(
          (void*)::GetProcAddress(Module, "SHCreateItemFromParsingName"))
      : nullptr;
  }();
  return Cached;
}

}

#define my_CLSID_FileOpenDialog NWindows::NFileOpenDialogInternal::k_CLSID_FileOpenDialog
#define my_IID_IFileOpenDialog NWindows::NFileOpenDialogInternal::k_IID_IFileOpenDialog
#define my_IID_IShellItem NWindows::NFileOpenDialogInternal::k_IID_IShellItem
#define k_SHCreateItemFromParsingName NWindows::NFileOpenDialogInternal::GetSHCreateItemFromParsingName()
// **************** NanaZip Modification End ****************

bool MyGetOpenFileName(HWND hwnd, LPCWSTR title,
    LPCWSTR initialDir,
    LPCWSTR filePath,
    LPCWSTR filterDescription,
    LPCWSTR filter,
    UString &resPath
    #ifdef UNDER_CE
    , bool openFolder
    #endif
    )
{
  const unsigned kBufSize = MAX_PATH * 2;
  const unsigned kFilterBufSize = MAX_PATH;
  if (!filter)
    filter = L"*.*";
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    CHAR buf[kBufSize];
    MyStringCopy(buf, (const char *)GetSystemString(filePath));
    // OPENFILENAME_NT4A
    OPENFILENAMEA p;
    memset(&p, 0, sizeof(p));
    p.lStructSize = my_compatib_OPENFILENAMEA_size;
    p.hwndOwner = hwnd;
    CHAR filterBuf[kFilterBufSize];
    {
      CDoubleZeroStringListA dz(filterBuf, kFilterBufSize);
      dz.Add(GetSystemString(filterDescription ? filterDescription : filter));
      dz.Add(GetSystemString(filter));
      dz.Finish();
      p.lpstrFilter = filterBuf;
      p.nFilterIndex = 1;
    }
    
    p.lpstrFile = buf;
    p.nMaxFile = kBufSize;
    CONV_U_To_A(p.lpstrInitialDir, initialDir, initialDirA);
    CONV_U_To_A(p.lpstrTitle, title, titleA);
    p.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;

    bool res = BOOLToBool(::GetOpenFileNameA(&p));
    resPath = GetUnicodeString(buf);
    return res;
  }
  else
  #endif
  {
    // **************** NanaZip Modification Start ****************
    // Use IFileOpenDialog instead of GetOpenFileNameW. The legacy
    // GetOpenFileNameW dialog always renders with the light theme on
    // light-mode systems, even when the process forced the dark uxtheme
    // mode, producing an unreadable all-white dialog inside the inverted
    // theme. The modern IFileOpenDialog honors the process dark mode.
    //
    // Suspend the inverted theme for the ENTIRE native dialog lifetime,
    // starting BEFORE the IFileOpenDialog object is created: its DirectUI
    // internals cache the process appearance during creation, so suspending
    // only around Show() leaves the dialog half native / half forced. The
    // RAII guard also covers the result retrieval, the release and the
    // legacy fallback below, and guarantees the theme is resumed on every
    // exit path (including cancel and exceptions).
    struct NK7NativeThemeDialogScope
    {
      NK7NativeThemeDialogScope() { ::K7UserSuspendDarkMode(); }
      ~NK7NativeThemeDialogScope() { ::K7UserResumeDarkMode(); }
    };
    NK7NativeThemeDialogScope NativeThemeDialogScope;

    WCHAR buf[kBufSize];
    MyStringCopy(buf, filePath);

    bool res = false;
    IFileOpenDialog* Dialog = nullptr;
    if (SUCCEEDED(::CoCreateInstance(
        my_CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        my_IID_IFileOpenDialog,
        reinterpret_cast<void**>(&Dialog))) && Dialog)
    {
      DWORD Options = 0;
      if (SUCCEEDED(Dialog->GetOptions(&Options)))
      {
        // The modern dialog has no read-only checkbox, so there is no
        // FOS_HIDEREADONLY (that flag only exists in the legacy OFN API).
        Dialog->SetOptions(Options | FOS_FORCEFILESYSTEM);
      }

      Dialog->SetTitle(title);

      COMDLG_FILTERSPEC FilterSpecs[2];
      FilterSpecs[0].pszName = filterDescription ? filterDescription : filter;
      FilterSpecs[0].pszSpec = filter;
      FilterSpecs[1].pszName = nullptr;
      FilterSpecs[1].pszSpec = nullptr;
      Dialog->SetFileTypes(1, FilterSpecs);
      Dialog->SetFileTypeIndex(1);

      // Split the preset path into folder + file name. The folder must
      // exist for SHCreateItemFromParsingName; the file itself may not.
      WCHAR Drive[MAX_PATH + 1] = {};
      WCHAR Dir[MAX_PATH + 1] = {};
      WCHAR Name[MAX_PATH + 1] = {};
      _wsplitpath_s(buf, Drive, MAX_PATH, Dir, MAX_PATH, Name, MAX_PATH, nullptr, 0);

      WCHAR Folder[MAX_PATH * 2 + 1] = {};
      lstrcpynW(Folder, initialDir ? initialDir : L"", MAX_PATH * 2 + 1);
      if (Drive[0] != L'\0')
      {
        lstrcatW(Folder, Drive);
        lstrcatW(Folder, Dir);
      }

      if (Folder[0] != L'\0')
      {
        IShellItem* FolderItem = nullptr;
        if (SUCCEEDED(k_SHCreateItemFromParsingName(
            Folder,
            nullptr,
            my_IID_IShellItem,
            reinterpret_cast<void**>(&FolderItem))) && FolderItem)
        {
          Dialog->SetFolder(FolderItem);
          FolderItem->Release();
        }
      }

      if (Name[0] != L'\0')
      {
        Dialog->SetFileName(Name);
      }

      // The native theme suspend scope (NativeThemeDialogScope) keeps the
      // whole dialog on the unmodified system appearance for its lifetime.
      const HRESULT ShowResult = Dialog->Show(hwnd);

      if (SUCCEEDED(ShowResult))
      {
        IShellItem* ResultItem = nullptr;
        if (SUCCEEDED(Dialog->GetResult(&ResultItem)) && ResultItem)
        {
          PWSTR ResultPath = nullptr;
          if (SUCCEEDED(ResultItem->GetDisplayName(
              SIGDN_FILESYSPATH, &ResultPath)) && ResultPath)
          {
            resPath = ResultPath;
            res = true;
            ::CoTaskMemFree(ResultPath);
          }
          ResultItem->Release();
        }
      }

      Dialog->Release();
    }

    // Fall back to the legacy dialog only when the modern dialog is truly
    // unavailable (e.g. COM was not initialized on this thread). A user
    // cancel returns a failed HRESULT from Show() as well, and falling back
    // there would immediately pop up a second dialog.
    if (!res && !Dialog)
    {
      // OPENFILENAME_NT4W
      OPENFILENAMEW p;
      memset(&p, 0, sizeof(p));
      p.lStructSize = my_compatib_OPENFILENAMEW_size;
      p.hwndOwner = hwnd;

      WCHAR filterBuf[kFilterBufSize];
      {
        CDoubleZeroStringListW dz(filterBuf, kFilterBufSize);
        dz.Add(filterDescription ? filterDescription : filter);
        dz.Add(filter);
        dz.Finish();
        p.lpstrFilter = filterBuf;
        p.nFilterIndex = 1;
      }

      p.lpstrFile = buf;
      p.nMaxFile = kBufSize;
      p.lpstrInitialDir = initialDir;
      p.lpstrTitle = title;
      p.Flags = OFN_EXPLORER | OFN_HIDEREADONLY
          #ifdef UNDER_CE
          | (openFolder ? (MY__OFN_PROJECT | MY__OFN_SHOW_ALL) : 0)
          #endif
          ;

      res = BOOLToBool(::GetOpenFileNameW(&p));
      resPath = buf;
    }

    return res;
    // **************** NanaZip Modification End ****************
  }
}

}
