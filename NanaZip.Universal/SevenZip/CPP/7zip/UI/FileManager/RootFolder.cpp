// RootFolder.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <shlobj.h>
#else
#include <ShlObj.h>
#endif

#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../../PropID.h"

#if defined(_WIN32) && !defined(UNDER_CE)
#define USE_WIN_PATHS
#endif

static const unsigned kNumRootFolderItems =
  #ifdef USE_WIN_PATHS
  4
  #else
  1
  #endif
  ;


#include "FSFolder.h"
#include "LangUtils.h"
#ifdef USE_WIN_PATHS
#include "NetFolder.h"
#include "FSDrives.h"
#include "AltStreamsFolder.h"
#endif
#include "RootFolder.h"
#include "SysIconUtils.h"

#include "resource.h"

using namespace NWindows;

static const Byte  kProps[] =
{
  kpidName
};

UString RootFolder_GetName_Computer(int &iconIndex);
UString RootFolder_GetName_Computer(int &iconIndex)
{
  #ifdef USE_WIN_PATHS
  iconIndex = Shell_GetFileInfo_SysIconIndex_for_CSIDL(CSIDL_DRIVES);
  #else
  iconIndex = Shell_GetFileInfo_SysIconIndex_for_Path(FSTRING_PATH_SEPARATOR, FILE_ATTRIBUTE_DIRECTORY);
  #endif
  return LangString(IDS_COMPUTER);
}

UString RootFolder_GetName_Network(int &iconIndex);
UString RootFolder_GetName_Network(int &iconIndex)
{
  iconIndex = Shell_GetFileInfo_SysIconIndex_for_CSIDL(CSIDL_NETWORK);
  return LangString(IDS_NETWORK);
}

UString RootFolder_GetName_Documents(int &iconIndex);
UString RootFolder_GetName_Documents(int &iconIndex)
{
  iconIndex = Shell_GetFileInfo_SysIconIndex_for_CSIDL(CSIDL_PERSONAL);
  return LangString(IDS_DOCUMENTS);
}

enum
{
  ROOT_INDEX_COMPUTER = 0
  #ifdef USE_WIN_PATHS
  , ROOT_INDEX_DOCUMENTS
  , ROOT_INDEX_NETWORK
  , ROOT_INDEX_VOLUMES
  #endif
};

#ifdef USE_WIN_PATHS
static const char * const kVolPrefix = "\\\\.";
#endif

void CRootFolder::Init()
{
  _names[ROOT_INDEX_COMPUTER] = RootFolder_GetName_Computer(_iconIndices[ROOT_INDEX_COMPUTER]);
  #ifdef USE_WIN_PATHS
  _names[ROOT_INDEX_DOCUMENTS] = RootFolder_GetName_Documents(_iconIndices[ROOT_INDEX_DOCUMENTS]);
  _names[ROOT_INDEX_NETWORK] = RootFolder_GetName_Network(_iconIndices[ROOT_INDEX_NETWORK]);
  _names[ROOT_INDEX_VOLUMES] = kVolPrefix;
  _iconIndices[ROOT_INDEX_VOLUMES] = Shell_GetFileInfo_SysIconIndex_for_CSIDL(CSIDL_DRIVES);
  #endif
}

Z7_COM7F_IMF(CRootFolder::LoadItems())
{
  Init();
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = kNumRootFolderItems;
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::GetProperty(UInt32 itemIndex, PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidIsDir:  prop = true; break;
    case kpidName:  prop = _names[itemIndex]; break;
  }
  prop.Detach(value);
  return S_OK;
}

#if !defined(Z7_WIN32_WINNT_MIN) || Z7_WIN32_WINNT_MIN < 0x0400  // nt4
#define Z7_USE_DYN_SHGetSpecialFolderPath
#endif

#ifdef Z7_USE_DYN_SHGetSpecialFolderPath
typedef BOOL (WINAPI *Func_SHGetSpecialFolderPathW)(HWND hwnd, LPWSTR pszPath, int csidl, BOOL fCreate);
typedef BOOL (WINAPI *Func_SHGetSpecialFolderPathA)(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate);
#endif

static UString GetMyDocsPath()
{
  UString us;
  WCHAR s[MAX_PATH + 1];
#ifdef Z7_USE_DYN_SHGetSpecialFolderPath
#ifdef UNDER_CE
  #define shell_name TEXT("coredll.dll")
#else
  #define shell_name TEXT("shell32.dll")
#endif
  Func_SHGetSpecialFolderPathW getW = Z7_GET_PROC_ADDRESS(
  Func_SHGetSpecialFolderPathW, GetModuleHandle(shell_name),
      "SHGetSpecialFolderPathW");
  if (getW && getW
#else
  if (SHGetSpecialFolderPathW
#endif
      (NULL, s, CSIDL_PERSONAL, FALSE))
    us = s;
  #ifndef _UNICODE
  else
  {
    CHAR s2[MAX_PATH + 1];
#ifdef Z7_USE_DYN_SHGetSpecialFolderPath
    Func_SHGetSpecialFolderPathA getA = Z7_GET_PROC_ADDRESS(
    Func_SHGetSpecialFolderPathA, ::GetModuleHandleA("shell32.dll"),
        "SHGetSpecialFolderPathA");
    if (getA && getA
#else
    if (SHGetSpecialFolderPathA
#endif
      (NULL, s2, CSIDL_PERSONAL, FALSE))
      us = GetUnicodeString(s2);
  }
  #endif
  NFile::NName::NormalizeDirPathPrefix(us);
  return us;
}

Z7_COM7F_IMF(CRootFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  CMyComPtr<IFolderFolder> subFolder;

  #ifdef USE_WIN_PATHS
  if (index == ROOT_INDEX_COMPUTER || index == ROOT_INDEX_VOLUMES)
  {
    CFSDrives *fsDrivesSpec = new CFSDrives;
    subFolder = fsDrivesSpec;
    fsDrivesSpec->Init(index == ROOT_INDEX_VOLUMES);
  }
  else if (index == ROOT_INDEX_NETWORK)
  {
    CNetFolder *netFolderSpec = new CNetFolder;
    subFolder = netFolderSpec;
    netFolderSpec->Init(NULL, NULL, _names[ROOT_INDEX_NETWORK] + WCHAR_PATH_SEPARATOR);
  }
  else if (index == ROOT_INDEX_DOCUMENTS)
  {
    UString s = GetMyDocsPath();
    if (!s.IsEmpty())
    {
      NFsFolder::CFSFolder *fsFolderSpec = new NFsFolder::CFSFolder;
      subFolder = fsFolderSpec;
      RINOK(fsFolderSpec->Init(us2fs(s)))
    }
  }
  #else
  if (index == ROOT_INDEX_COMPUTER)
  {
    NFsFolder::CFSFolder *fsFolder = new NFsFolder::CFSFolder;
    subFolder = fsFolder;
    fsFolder->InitToRoot();
  }
  #endif
  else
    return E_INVALIDARG;

  *resultFolder = subFolder.Detach();
  return S_OK;
}

static bool AreEqualNames(const UString &path, const wchar_t *name)
{
  unsigned len = MyStringLen(name);
  if (len > path.Len() || len + 1 < path.Len())
    return false;
  if (len + 1 == path.Len() && !IS_PATH_SEPAR(path[len]))
    return false;
  return path.IsPrefixedBy(name);
}

Z7_COM7F_IMF(CRootFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  UString name2 = name;
  name2.Trim();

  if (name2.IsEmpty())
  {
    CRootFolder *rootFolderSpec = new CRootFolder;
    CMyComPtr<IFolderFolder> rootFolder = rootFolderSpec;
    rootFolderSpec->Init();
    *resultFolder = rootFolder.Detach();
    return S_OK;
  }

  for (unsigned i = 0; i < kNumRootFolderItems; i++)
    if (AreEqualNames(name2, _names[i]))
      return BindToFolder((UInt32)i, resultFolder);

  #ifdef USE_WIN_PATHS
  if (AreEqualNames(name2, L"My Documents") ||
      AreEqualNames(name2, L"Documents"))
    return BindToFolder((UInt32)ROOT_INDEX_DOCUMENTS, resultFolder);
  #else
  if (name2.IsEqualTo(STRING_PATH_SEPARATOR))
    return BindToFolder((UInt32)ROOT_INDEX_COMPUTER, resultFolder);
  #endif

  if (AreEqualNames(name2, L"My Computer") ||
      AreEqualNames(name2, L"Computer"))
    return BindToFolder((UInt32)ROOT_INDEX_COMPUTER, resultFolder);

  if (name2.IsEqualTo(STRING_PATH_SEPARATOR))
  {
    CMyComPtr<IFolderFolder> subFolder = this;
    *resultFolder = subFolder.Detach();
    return S_OK;
  }

  if (name2.Len() < 2)
    return E_INVALIDARG;

  CMyComPtr<IFolderFolder> subFolder;

  #ifdef USE_WIN_PATHS
  if (name2.IsPrefixedBy_Ascii_NoCase(kVolPrefix))
  {
    CFSDrives *folderSpec = new CFSDrives;
    subFolder = folderSpec;
    folderSpec->Init(true);
  }
  else if (name2.IsEqualTo(NFile::NName::kSuperPathPrefix))
  {
    CFSDrives *folderSpec = new CFSDrives;
    subFolder = folderSpec;
    folderSpec->Init(false, true);
  }
  else if (name2.Back() == ':'
      && (name2.Len() != 2 || !NFile::NName::IsDrivePath2(name2)))
  {
    NAltStreamsFolder::CAltStreamsFolder *folderSpec = new NAltStreamsFolder::CAltStreamsFolder;
    subFolder = folderSpec;
    if (folderSpec->Init(us2fs(name2)) != S_OK)
      return E_INVALIDARG;
  }
  else
  #endif
  {
    NFile::NName::NormalizeDirPathPrefix(name2);
    NFsFolder::CFSFolder *fsFolderSpec = new NFsFolder::CFSFolder;
    subFolder = fsFolderSpec;
    if (fsFolderSpec->Init(us2fs(name2)) != S_OK)
    {
      #ifdef USE_WIN_PATHS
      if (IS_PATH_SEPAR(name2[0]))
      {
        CNetFolder *netFolderSpec = new CNetFolder;
        subFolder = netFolderSpec;
        netFolderSpec->Init(name2);
      }
      else
      #endif
        return E_INVALIDARG;
    }
  }

  *resultFolder = subFolder.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::BindToParentFolder(IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  return S_OK;
}

IMP_IFolderFolder_Props(CRootFolder)

Z7_COM7F_IMF(CRootFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "RootFolder"; break;
    case kpidPath: prop = ""; break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::GetSystemIconIndex(UInt32 index, Int32 *iconIndex))
{
  *iconIndex = _iconIndices[index];
  return S_OK;
}
