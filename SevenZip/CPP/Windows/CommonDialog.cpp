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
    WCHAR buf[kBufSize];
    MyStringCopy(buf, filePath);
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

    bool res = BOOLToBool(::GetOpenFileNameW(&p));
    resPath = buf;
    return res;
  }
}

}
