// SysIconUtils.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../../../Common/StringConvert.h"
#endif

#include "../../../Windows/FileDir.h"

#include "SysIconUtils.h"

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <shlobj.h>
#else
#include <ShlObj.h>
#endif

#ifndef _UNICODE
extern bool g_IsNT;
#endif

int GetIconIndexForCSIDL(int csidl)
{
  LPITEMIDLIST pidl = NULL;
  SHGetSpecialFolderLocation(NULL, csidl, &pidl);
  if (pidl)
  {
    SHFILEINFO shellInfo;
    shellInfo.iIcon = 0;
    const DWORD_PTR res = SHGetFileInfo((LPCTSTR)(const void *)(pidl), FILE_ATTRIBUTE_NORMAL,
        &shellInfo, sizeof(shellInfo),
        SHGFI_PIDL | SHGFI_SYSICONINDEX);
    /*
    IMalloc *pMalloc;
    SHGetMalloc(&pMalloc);
    if (pMalloc)
    {
      pMalloc->Free(pidl);
      pMalloc->Release();
    }
    */
    // we use OLE2.dll function here
    CoTaskMemFree(pidl);
    if (res)
      return shellInfo.iIcon;
  }
  return 0;
}

#ifndef _UNICODE
typedef DWORD_PTR (WINAPI * Func_SHGetFileInfoW)(LPCWSTR pszPath, DWORD attrib, SHFILEINFOW *psfi, UINT cbFileInfo, UINT uFlags);

static struct C_SHGetFileInfo_Init
{
  Func_SHGetFileInfoW f_SHGetFileInfoW;
  C_SHGetFileInfo_Init()
  {
       f_SHGetFileInfoW = Z7_GET_PROC_ADDRESS(
    Func_SHGetFileInfoW, ::GetModuleHandleW(L"shell32.dll"),
        "SHGetFileInfoW");
  }
} g_SHGetFileInfo_Init;
#endif

static DWORD_PTR My_SHGetFileInfoW(LPCWSTR pszPath, DWORD attrib, SHFILEINFOW *psfi, UINT cbFileInfo, UINT uFlags)
{
  #ifdef _UNICODE
  return SHGetFileInfo
  #else
  if (!g_SHGetFileInfo_Init.f_SHGetFileInfoW)
    return 0;
  return g_SHGetFileInfo_Init.f_SHGetFileInfoW
  #endif
  (pszPath, attrib, psfi, cbFileInfo, uFlags);
}

DWORD_PTR GetRealIconIndex(CFSTR path, DWORD attrib, int &iconIndex)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    SHFILEINFO shellInfo;
    const DWORD_PTR res = ::SHGetFileInfo(fs2fas(path), FILE_ATTRIBUTE_NORMAL | attrib, &shellInfo,
      sizeof(shellInfo), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX);
    iconIndex = shellInfo.iIcon;
    return res;
  }
  else
  #endif
  {
    SHFILEINFOW shellInfo;
    const DWORD_PTR res = ::My_SHGetFileInfoW(fs2us(path), FILE_ATTRIBUTE_NORMAL | attrib, &shellInfo,
      sizeof(shellInfo), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX);
    iconIndex = shellInfo.iIcon;
    return res;
  }
}

/*
DWORD_PTR GetRealIconIndex(const UString &fileName, DWORD attrib, int &iconIndex, UString *typeName)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    SHFILEINFO shellInfo;
    shellInfo.szTypeName[0] = 0;
    DWORD_PTR res = ::SHGetFileInfoA(GetSystemString(fileName), FILE_ATTRIBUTE_NORMAL | attrib, &shellInfo,
        sizeof(shellInfo), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_TYPENAME);
    if (typeName)
      *typeName = GetUnicodeString(shellInfo.szTypeName);
    iconIndex = shellInfo.iIcon;
    return res;
  }
  else
  #endif
  {
    SHFILEINFOW shellInfo;
    shellInfo.szTypeName[0] = 0;
    DWORD_PTR res = ::My_SHGetFileInfoW(fileName, FILE_ATTRIBUTE_NORMAL | attrib, &shellInfo,
        sizeof(shellInfo), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_TYPENAME);
    if (typeName)
      *typeName = shellInfo.szTypeName;
    iconIndex = shellInfo.iIcon;
    return res;
  }
}
*/

static int FindInSorted_Attrib(const CRecordVector<CAttribIconPair> &vect, DWORD attrib, unsigned &insertPos)
{
  unsigned left = 0, right = vect.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const DWORD midAttrib = vect[mid].Attrib;
    if (attrib == midAttrib)
      return (int)mid;
    if (attrib < midAttrib)
      right = mid;
    else
      left = mid + 1;
  }
  insertPos = left;
  return -1;
}

static int FindInSorted_Ext(const CObjectVector<CExtIconPair> &vect, const wchar_t *ext, unsigned &insertPos)
{
  unsigned left = 0, right = vect.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const int compare = MyStringCompareNoCase(ext, vect[mid].Ext);
    if (compare == 0)
      return (int)mid;
    if (compare < 0)
      right = mid;
    else
      left = mid + 1;
  }
  insertPos = left;
  return -1;
}

int CExtToIconMap::GetIconIndex(DWORD attrib, const wchar_t *fileName /*, UString *typeName */)
{
  int dotPos = -1;
  unsigned i;
  for (i = 0;; i++)
  {
    const wchar_t c = fileName[i];
    if (c == 0)
      break;
    if (c == '.')
      dotPos = (int)i;
  }

  /*
  if (MyStringCompareNoCase(fileName, L"$Recycle.Bin") == 0)
  {
    char s[256];
    sprintf(s, "SPEC i = %3d, attr = %7x", _attribMap.Size(), attrib);
    OutputDebugStringA(s);
    OutputDebugStringW(fileName);
  }
  */

  if ((attrib & FILE_ATTRIBUTE_DIRECTORY) != 0 || dotPos < 0)
  {
    unsigned insertPos = 0;
    const int index = FindInSorted_Attrib(_attribMap, attrib, insertPos);
    if (index >= 0)
    {
      // if (typeName) *typeName = _attribMap[index].TypeName;
      return _attribMap[(unsigned)index].IconIndex;
    }
    CAttribIconPair pair;
    GetRealIconIndex(
        #ifdef UNDER_CE
        FTEXT("\\")
        #endif
        FTEXT("__DIR__")
        , attrib, pair.IconIndex
        // , pair.TypeName
        );

    /*
    char s[256];
    sprintf(s, "i = %3d, attr = %7x", _attribMap.Size(), attrib);
    OutputDebugStringA(s);
    */

    pair.Attrib = attrib;
    _attribMap.Insert(insertPos, pair);
    // if (typeName) *typeName = pair.TypeName;
    return pair.IconIndex;
  }

  const wchar_t *ext = fileName + dotPos + 1;
  unsigned insertPos = 0;
  const int index = FindInSorted_Ext(_extMap, ext, insertPos);
  if (index >= 0)
  {
    const CExtIconPair &pa = _extMap[index];
    // if (typeName) *typeName = pa.TypeName;
    return pa.IconIndex;
  }

  for (i = 0;; i++)
  {
    const wchar_t c = ext[i];
    if (c == 0)
      break;
    if (c < L'0' || c > L'9')
      break;
  }
  if (i != 0 && ext[i] == 0)
  {
    // GetRealIconIndex is too slow for big number of split extensions: .001, .002, .003
    if (!SplitIconIndex_Defined)
    {
      GetRealIconIndex(
          #ifdef UNDER_CE
          FTEXT("\\")
          #endif
          FTEXT("__FILE__.001"), 0, SplitIconIndex);
      SplitIconIndex_Defined = true;
    }
    return SplitIconIndex;
  }

  CExtIconPair pair;
  pair.Ext = ext;
  GetRealIconIndex(us2fs(fileName + dotPos), attrib, pair.IconIndex);
  _extMap.Insert(insertPos, pair);
  // if (typeName) *typeName = pair.TypeName;
  return pair.IconIndex;
}

/*
int CExtToIconMap::GetIconIndex(DWORD attrib, const UString &fileName)
{
  return GetIconIndex(attrib, fileName, NULL);
}
*/

HIMAGELIST GetSysImageList(bool smallIcons)
{
  SHFILEINFO shellInfo;
  return (HIMAGELIST)SHGetFileInfo(TEXT(""),
      FILE_ATTRIBUTE_NORMAL |
      FILE_ATTRIBUTE_DIRECTORY,
      &shellInfo, sizeof(shellInfo),
      SHGFI_USEFILEATTRIBUTES |
      SHGFI_SYSICONINDEX |
      (smallIcons ? SHGFI_SMALLICON : SHGFI_ICON));
}
