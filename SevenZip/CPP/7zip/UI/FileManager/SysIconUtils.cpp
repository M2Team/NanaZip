// SysIconUtils.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../../../Common/StringConvert.h"
#endif

#include "../../../Windows/FileDir.h"

#include "SysIconUtils.h"

#include <ShlObj.h>

#define MY_CAST_FUNC  (void(*)())
// #define MY_CAST_FUNC

#ifndef _UNICODE
extern bool g_IsNT;
#endif

int GetIconIndexForCSIDL(int csidl)
{
  LPITEMIDLIST pidl = 0;
  SHGetSpecialFolderLocation(NULL, csidl, &pidl);
  if (pidl)
  {
    SHFILEINFO shellInfo;
    SHGetFileInfo((LPCTSTR)(const void *)(pidl), FILE_ATTRIBUTE_NORMAL,
      &shellInfo, sizeof(shellInfo),
      SHGFI_PIDL | SHGFI_SYSICONINDEX);
    IMalloc  *pMalloc;
    SHGetMalloc(&pMalloc);
    if (pMalloc)
    {
      pMalloc->Free(pidl);
      pMalloc->Release();
    }
    return shellInfo.iIcon;
  }
  return 0;
}

#ifndef _UNICODE
typedef int (WINAPI * Func_SHGetFileInfoW)(LPCWSTR pszPath, DWORD attrib, SHFILEINFOW *psfi, UINT cbFileInfo, UINT uFlags);

static struct CSHGetFileInfoInit
{
  Func_SHGetFileInfoW shGetFileInfoW;
  CSHGetFileInfoInit()
  {
    shGetFileInfoW = (Func_SHGetFileInfoW)
        MY_CAST_FUNC
        ::GetProcAddress(::GetModuleHandleW(L"shell32.dll"), "SHGetFileInfoW");
  }
} g_SHGetFileInfoInit;
#endif

static DWORD_PTR MySHGetFileInfoW(LPCWSTR pszPath, DWORD attrib, SHFILEINFOW *psfi, UINT cbFileInfo, UINT uFlags)
{
  #ifdef _UNICODE
  return SHGetFileInfo
  #else
  if (g_SHGetFileInfoInit.shGetFileInfoW == 0)
    return 0;
  return g_SHGetFileInfoInit.shGetFileInfoW
  #endif
  (pszPath, attrib, psfi, cbFileInfo, uFlags);
}

DWORD_PTR GetRealIconIndex(CFSTR path, DWORD attrib, int &iconIndex)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    SHFILEINFO shellInfo;
    DWORD_PTR res = ::SHGetFileInfo(fs2fas(path), FILE_ATTRIBUTE_NORMAL | attrib, &shellInfo,
      sizeof(shellInfo), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX);
    iconIndex = shellInfo.iIcon;
    return res;
  }
  else
  #endif
  {
    SHFILEINFOW shellInfo;
    DWORD_PTR res = ::MySHGetFileInfoW(fs2us(path), FILE_ATTRIBUTE_NORMAL | attrib, &shellInfo,
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
    DWORD_PTR res = ::MySHGetFileInfoW(fileName, FILE_ATTRIBUTE_NORMAL | attrib, &shellInfo,
        sizeof(shellInfo), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_TYPENAME);
    if (typeName)
      *typeName = shellInfo.szTypeName;
    iconIndex = shellInfo.iIcon;
    return res;
  }
}
*/

static int FindInSorted_Attrib(const CRecordVector<CAttribIconPair> &vect, DWORD attrib, int &insertPos)
{
  unsigned left = 0, right = vect.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    DWORD midAttrib = vect[mid].Attrib;
    if (attrib == midAttrib)
      return mid;
    if (attrib < midAttrib)
      right = mid;
    else
      left = mid + 1;
  }
  insertPos = left;
  return -1;
}

static int FindInSorted_Ext(const CObjectVector<CExtIconPair> &vect, const wchar_t *ext, int &insertPos)
{
  unsigned left = 0, right = vect.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    int compare = MyStringCompareNoCase(ext, vect[mid].Ext);
    if (compare == 0)
      return mid;
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
    wchar_t c = fileName[i];
    if (c == 0)
      break;
    if (c == '.')
      dotPos = i;
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
    int insertPos = 0;
    int index = FindInSorted_Attrib(_attribMap, attrib, insertPos);
    if (index >= 0)
    {
      // if (typeName) *typeName = _attribMap[index].TypeName;
      return _attribMap[index].IconIndex;
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
  int insertPos = 0;
  int index = FindInSorted_Ext(_extMap, ext, insertPos);
  if (index >= 0)
  {
    const CExtIconPair &pa = _extMap[index];
    // if (typeName) *typeName = pa.TypeName;
    return pa.IconIndex;
  }

  for (i = 0;; i++)
  {
    wchar_t c = ext[i];
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
