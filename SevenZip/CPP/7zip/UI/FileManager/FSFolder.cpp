// FSFolder.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/Defs.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../../PropID.h"

#include "FSDrives.h"
#include "FSFolder.h"

#ifndef UNDER_CE
#include "NetFolder.h"
#endif

#include "SysIconUtils.h"

#if _WIN32_WINNT < 0x0501
#ifdef _APISETFILE_
// Windows SDK 8.1 defines in fileapi.h the function GetCompressedFileSizeW only if _WIN32_WINNT >= 0x0501
// But real support version for that function is NT 3.1 (probably)
// So we must define GetCompressedFileSizeW
EXTERN_C_BEGIN
WINBASEAPI DWORD WINAPI GetCompressedFileSizeW(LPCWSTR lpFileName, LPDWORD lpFileSizeHigh);
EXTERN_C_END
#endif
#endif

using namespace NWindows;
using namespace NFile;
using namespace NFind;
using namespace NDir;
using namespace NName;

#ifndef USE_UNICODE_FSTRING
int CompareFileNames_ForFolderList(const FChar *s1, const FChar *s2)
{
  return CompareFileNames_ForFolderList(fs2us(s1), fs2us(s2));
}
#endif

namespace NFsFolder {

static const Byte kProps[] =
{
  kpidName,
  kpidSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidAttrib,
  kpidPackSize,
  #ifdef FS_SHOW_LINKS_INFO
  kpidINode,
  kpidLinks,
  #endif
  kpidComment,
  kpidNumSubDirs,
  kpidNumSubFiles,
  kpidPrefix
};

HRESULT CFSFolder::Init(const FString &path /* , IFolderFolder *parentFolder */)
{
  // _parentFolder = parentFolder;
  _path = path;

  #ifdef _WIN32

  _findChangeNotification.FindFirst(_path, false,
        FILE_NOTIFY_CHANGE_FILE_NAME
      | FILE_NOTIFY_CHANGE_DIR_NAME
      | FILE_NOTIFY_CHANGE_ATTRIBUTES
      | FILE_NOTIFY_CHANGE_SIZE
      | FILE_NOTIFY_CHANGE_LAST_WRITE
      /*
      | FILE_NOTIFY_CHANGE_LAST_ACCESS
      | FILE_NOTIFY_CHANGE_CREATION
      | FILE_NOTIFY_CHANGE_SECURITY
      */
      );

  if (!_findChangeNotification.IsHandleAllocated())
  {
    DWORD lastError = GetLastError();
    CFindFile findFile;
    CFileInfo fi;
    FString path2 = _path;
    path2 += '*'; // CHAR_ANY_MASK;
    if (!findFile.FindFirst(path2, fi))
      return lastError;
  }

  #endif
  
  return S_OK;
}


HRESULT CFsFolderStat::Enumerate()
{
  if (Progress)
  {
    RINOK(Progress->SetCompleted(NULL));
  }
  Path.Add_PathSepar();
  const unsigned len = Path.Len();
  CEnumerator enumerator;
  enumerator.SetDirPrefix(Path);
  CDirEntry fi;
  while (enumerator.Next(fi))
  {
    if (fi.IsDir())
    {
      NumFolders++;
      Path.DeleteFrom(len);
      Path += fi.Name;
      RINOK(Enumerate());
    }
    else
    {
      NumFiles++;
      Size += fi.Size;
    }
  }
  return S_OK;
}

#ifndef UNDER_CE

bool MyGetCompressedFileSizeW(CFSTR path, UInt64 &size);
bool MyGetCompressedFileSizeW(CFSTR path, UInt64 &size)
{
  DWORD highPart;
  DWORD lowPart = INVALID_FILE_SIZE;
  IF_USE_MAIN_PATH
  {
    lowPart = ::GetCompressedFileSizeW(fs2us(path), &highPart);
    if (lowPart != INVALID_FILE_SIZE || ::GetLastError() == NO_ERROR)
    {
      size = ((UInt64)highPart << 32) | lowPart;
      return true;
    }
  }
  #ifdef WIN_LONG_PATH
  if (USE_SUPER_PATH)
  {
    UString superPath;
    if (GetSuperPath(path, superPath, USE_MAIN_PATH))
    {
      lowPart = ::GetCompressedFileSizeW(superPath, &highPart);
      if (lowPart != INVALID_FILE_SIZE || ::GetLastError() == NO_ERROR)
      {
        size = ((UInt64)highPart << 32) | lowPart;
        return true;
      }
    }
  }
  #endif
  return false;
}

#endif

HRESULT CFSFolder::LoadSubItems(int dirItem, const FString &relPrefix)
{
  const unsigned startIndex = Folders.Size();
  {
    CEnumerator enumerator;
    enumerator.SetDirPrefix(_path + relPrefix);
    CDirItem fi;
    fi.FolderStat_Defined = false;
    fi.NumFolders = 0;
    fi.NumFiles = 0;
    fi.Parent = dirItem;
    
    while (enumerator.Next(fi))
    {
      if (fi.IsDir())
      {
        fi.Size = 0;
        if (_flatMode)
          Folders.Add(relPrefix + fi.Name + FCHAR_PATH_SEPARATOR);
      }
      else
      {
        /*
        fi.PackSize_Defined = true;
        if (!MyGetCompressedFileSizeW(_path + relPrefix + fi.Name, fi.PackSize))
          fi.PackSize = fi.Size;
        */
      }
      
      #ifndef UNDER_CE

      fi.Reparse.Free();
      fi.PackSize_Defined = false;
    
      #ifdef FS_SHOW_LINKS_INFO
      fi.FileInfo_Defined = false;
      fi.FileInfo_WasRequested = false;
      fi.FileIndex = 0;
      fi.NumLinks = 0;
      #endif
      
      fi.PackSize = fi.Size;
      if (fi.HasReparsePoint())
      {
        fi.FileInfo_WasRequested = true;
        BY_HANDLE_FILE_INFORMATION info;
        NIO::GetReparseData(_path + relPrefix + fi.Name, fi.Reparse, &info);
        fi.NumLinks = info.nNumberOfLinks;
        fi.FileIndex = (((UInt64)info.nFileIndexHigh) << 32) + info.nFileIndexLow;
        fi.FileInfo_Defined = true;
      }

      #endif

      /* unsigned fileIndex = */ Files.Add(fi);

      #if defined(_WIN32) && !defined(UNDER_CE)
      /*
      if (_scanAltStreams)
      {
        CStreamEnumerator enumerator(_path + relPrefix + fi.Name);
        CStreamInfo si;
        for (;;)
        {
          bool found;
          if (!enumerator.Next(si, found))
          {
            // if (GetLastError() == ERROR_ACCESS_DENIED)
            //   break;
            // return E_FAIL;
            break;
          }
          if (!found)
            break;
          if (si.IsMainStream())
            continue;
          CAltStream ss;
          ss.Parent = fileIndex;
          ss.Name = si.GetReducedName();
          ss.Size = si.Size;
          ss.PackSize_Defined = false;
          ss.PackSize = si.Size;
          Streams.Add(ss);
        }
      }
      */
      #endif
    }
  }
  if (!_flatMode)
    return S_OK;

  const unsigned endIndex = Folders.Size();
  for (unsigned i = startIndex; i < endIndex; i++)
    LoadSubItems(i, Folders[i]);
  return S_OK;
}

STDMETHODIMP CFSFolder::LoadItems()
{
  Int32 dummy;
  WasChanged(&dummy);
  Clear();
  RINOK(LoadSubItems(-1, FString()));
  _commentsAreLoaded = false;
  return S_OK;
}

static CFSTR const kDescriptionFileName = FTEXT("descript.ion");

bool CFSFolder::LoadComments()
{
  _comments.Clear();
  _commentsAreLoaded = true;
  NIO::CInFile file;
  if (!file.Open(_path + kDescriptionFileName))
    return false;
  UInt64 len;
  if (!file.GetLength(len))
    return false;
  if (len >= (1 << 28))
    return false;
  AString s;
  char *p = s.GetBuf((unsigned)(size_t)len);
  size_t processedSize;
  if (!file.ReadFull(p, (unsigned)(size_t)len, processedSize))
    return false;
  s.ReleaseBuf_CalcLen((unsigned)(size_t)len);
  if (processedSize != len)
    return false;
  file.Close();
  UString unicodeString;
  if (!ConvertUTF8ToUnicode(s, unicodeString))
    return false;
  return _comments.ReadFromString(unicodeString);
}

bool CFSFolder::SaveComments()
{
  AString utf;
  {
    UString unicode;
    _comments.SaveToString(unicode);
    ConvertUnicodeToUTF8(unicode, utf);
  }
  if (!utf.IsAscii())
    utf.Insert(0, "\xEF\xBB\xBF" "\r\n");

  FString path = _path + kDescriptionFileName;
  // We must set same attrib. COutFile::CreateAlways can fail, if file has another attrib.
  DWORD attrib = FILE_ATTRIBUTE_NORMAL;
  {
    CFileInfo fi;
    if (fi.Find(path))
      attrib = fi.Attrib;
  }
  NIO::COutFile file;
  if (!file.CreateAlways(path, attrib))
    return false;
  UInt32 processed;
  file.Write(utf, utf.Len(), processed);
  _commentsAreLoaded = false;
  return true;
}

STDMETHODIMP CFSFolder::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = Files.Size() /* + Streams.Size() */;
  return S_OK;
}

#ifdef USE_UNICODE_FSTRING

STDMETHODIMP CFSFolder::GetItemPrefix(UInt32 index, const wchar_t **name, unsigned *len)
{
  *name = 0;
  *len = 0;
  /*
  if (index >= Files.Size())
    index = Streams[index - Files.Size()].Parent;
  */
  CDirItem &fi = Files[index];
  if (fi.Parent >= 0)
  {
    const FString &fo = Folders[fi.Parent];
    USE_UNICODE_FSTRING
    *name = fo;
    *len = fo.Len();
  }
  return S_OK;
}

STDMETHODIMP CFSFolder::GetItemName(UInt32 index, const wchar_t **name, unsigned *len)
{
  *name = 0;
  *len = 0;
  if (index < Files.Size())
  {
    CDirItem &fi = Files[index];
    *name = fi.Name;
    *len = fi.Name.Len();
    return S_OK;
  }
  else
  {
    // const CAltStream &ss = Streams[index - Files.Size()];
    // *name = ss.Name;
    // *len = ss.Name.Len();
    //
    // change it;
  }
  return S_OK;
}

STDMETHODIMP_(UInt64) CFSFolder::GetItemSize(UInt32 index)
{
  /*
  if (index >= Files.Size())
    return Streams[index - Files.Size()].Size;
  */
  CDirItem &fi = Files[index];
  return fi.IsDir() ? 0 : fi.Size;
}

#endif

#ifdef FS_SHOW_LINKS_INFO
bool CFSFolder::ReadFileInfo(CDirItem &di)
{
  di.FileInfo_WasRequested = true;
  BY_HANDLE_FILE_INFORMATION info;
  memset(&info, 0, sizeof(info)); // for vc6-O2
  if (!NIO::CFileBase::GetFileInformation(_path + GetRelPath(di), &info))
    return false;
  di.NumLinks = info.nNumberOfLinks;
  di.FileIndex = (((UInt64)info.nFileIndexHigh) << 32) + info.nFileIndexLow;
  di.FileInfo_Defined = true;
  return true;
}
#endif

STDMETHODIMP CFSFolder::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  /*
  if (index >= (UInt32)Files.Size())
  {
    CAltStream &ss = Streams[index - Files.Size()];
    CDirItem &fi = Files[ss.Parent];
    switch (propID)
    {
      case kpidIsDir: prop = false; break;
      case kpidIsAltStream: prop = true; break;
      case kpidName: prop = fs2us(fi.Name) + ss.Name; break;
      case kpidSize: prop = ss.Size; break;
      case kpidPackSize:
        #ifdef UNDER_CE
        prop = ss.Size;
        #else
        if (!ss.PackSize_Defined)
        {
          ss.PackSize_Defined = true;
          if (!MyGetCompressedFileSizeW(_path + GetRelPath(fi) + us2fs(ss.Name), ss.PackSize))
            ss.PackSize = ss.Size;
        }
        prop = ss.PackSize;
        #endif
        break;
      case kpidComment: break;
      default: index = ss.Parent;
    }
    if (index >= (UInt32)Files.Size())
    {
      prop.Detach(value);
      return S_OK;
    }
  }
  */
  CDirItem &fi = Files[index];
  switch (propID)
  {
    case kpidIsDir: prop = fi.IsDir(); break;
    case kpidIsAltStream: prop = false; break;
    case kpidName: prop = fs2us(fi.Name); break;
    case kpidSize: if (!fi.IsDir() || fi.FolderStat_Defined) prop = fi.Size; break;
    case kpidPackSize:
      #ifdef UNDER_CE
      prop = fi.Size;
      #else
      if (!fi.PackSize_Defined)
      {
        fi.PackSize_Defined = true;
        if (fi.IsDir () || !MyGetCompressedFileSizeW(_path + GetRelPath(fi), fi.PackSize))
          fi.PackSize = fi.Size;
      }
      prop = fi.PackSize;
      #endif
      break;

    #ifdef FS_SHOW_LINKS_INFO

    case kpidLinks:
      #ifdef UNDER_CE
      // prop = fi.NumLinks;
      #else
      if (!fi.FileInfo_WasRequested)
        ReadFileInfo(fi);
      if (fi.FileInfo_Defined)
        prop = fi.NumLinks;
      #endif
      break;
    
    case kpidINode:
      #ifdef UNDER_CE
      // prop = fi.FileIndex;
      #else
      if (!fi.FileInfo_WasRequested)
        ReadFileInfo(fi);
      if (fi.FileInfo_Defined)
        prop = fi.FileIndex;
      #endif
      break;
    
    #endif

    case kpidAttrib: prop = (UInt32)fi.Attrib; break;
    case kpidCTime: prop = fi.CTime; break;
    case kpidATime: prop = fi.ATime; break;
    case kpidMTime: prop = fi.MTime; break;
    case kpidComment:
    {
      if (!_commentsAreLoaded)
        LoadComments();
      UString comment;
      if (_comments.GetValue(fs2us(GetRelPath(fi)), comment))
      {
        int pos = comment.Find((wchar_t)4);
        if (pos >= 0)
          comment.DeleteFrom((unsigned)pos);
        prop = comment;
      }
      break;
    }
    case kpidPrefix:
      if (fi.Parent >= 0)
        prop = Folders[fi.Parent];
      break;
    case kpidNumSubDirs: if (fi.IsDir() && fi.FolderStat_Defined) prop = fi.NumFolders; break;
    case kpidNumSubFiles: if (fi.IsDir() && fi.FolderStat_Defined) prop = fi.NumFiles; break;
  }
  prop.Detach(value);
  return S_OK;
}


// ---------- IArchiveGetRawProps ----------


STDMETHODIMP CFSFolder::GetNumRawProps(UInt32 *numProps)
{
  *numProps = 1;
  return S_OK;
}

STDMETHODIMP CFSFolder::GetRawPropInfo(UInt32 /* index */, BSTR *name, PROPID *propID)
{
  *name = NULL;
  *propID = kpidNtReparse;
  return S_OK;
}

STDMETHODIMP CFSFolder::GetParent(UInt32 /* index */, UInt32 * /* parent */, UInt32 * /* parentType */)
{
  return E_FAIL;
}

STDMETHODIMP CFSFolder::GetRawProp(UInt32
  #ifndef UNDER_CE
  index
  #endif
  , PROPID
  #ifndef UNDER_CE
  propID
  #endif
  , const void **data, UInt32 *dataSize, UInt32 *propType)
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;
  
  #ifndef UNDER_CE
  if (propID == kpidNtReparse)
  {
    const CDirItem &fi = Files[index];
    const CByteBuffer &buf = fi.Reparse;
    if (buf.Size() == 0)
      return S_OK;
    *data = buf;
    *dataSize = (UInt32)buf.Size();
    *propType = NPropDataType::kRaw;
    return S_OK;
  }
  #endif
  
  return S_OK;
}


// returns Position of extension including '.'

static inline CFSTR GetExtensionPtr(const FString &name)
{
  int dotPos = name.ReverseFind_Dot();
  return name.Ptr((dotPos < 0) ? name.Len() : dotPos);
}

STDMETHODIMP_(Int32) CFSFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 /* propIsRaw */)
{
  /*
  const CAltStream *ss1 = NULL;
  const CAltStream *ss2 = NULL;
  if (index1 >= (UInt32)Files.Size()) { ss1 = &Streams[index1 - Files.Size()]; index1 = ss1->Parent; }
  if (index2 >= (UInt32)Files.Size()) { ss2 = &Streams[index2 - Files.Size()]; index2 = ss2->Parent; }
  */
  CDirItem &fi1 = Files[index1];
  CDirItem &fi2 = Files[index2];

  switch (propID)
  {
    case kpidName:
    {
      int comp = CompareFileNames_ForFolderList(fi1.Name, fi2.Name);
      /*
      if (comp != 0)
        return comp;
      if (!ss1)
        return ss2 ? -1 : 0;
      if (!ss2)
        return 1;
      return MyStringCompareNoCase(ss1->Name, ss2->Name);
      */
      return comp;
    }
    case kpidSize:
      return MyCompare(
          /* ss1 ? ss1->Size : */ fi1.Size,
          /* ss2 ? ss2->Size : */ fi2.Size);
    case kpidAttrib: return MyCompare(fi1.Attrib, fi2.Attrib);
    case kpidCTime: return CompareFileTime(&fi1.CTime, &fi2.CTime);
    case kpidATime: return CompareFileTime(&fi1.ATime, &fi2.ATime);
    case kpidMTime: return CompareFileTime(&fi1.MTime, &fi2.MTime);
    case kpidIsDir:
    {
      bool isDir1 = /* ss1 ? false : */ fi1.IsDir();
      bool isDir2 = /* ss2 ? false : */ fi2.IsDir();
      if (isDir1 == isDir2)
        return 0;
      return isDir1 ? -1 : 1;
    }
    case kpidPackSize:
    {
      #ifdef UNDER_CE
      return MyCompare(fi1.Size, fi2.Size);
      #else
      // PackSize can be undefined here
      return MyCompare(
          /* ss1 ? ss1->PackSize : */ fi1.PackSize,
          /* ss2 ? ss2->PackSize : */ fi2.PackSize);
      #endif
    }
    
    #ifdef FS_SHOW_LINKS_INFO
    case kpidINode:
    {
      #ifndef UNDER_CE
      if (!fi1.FileInfo_WasRequested) ReadFileInfo(fi1);
      if (!fi2.FileInfo_WasRequested) ReadFileInfo(fi2);
      return MyCompare(
          fi1.FileIndex,
          fi2.FileIndex);
      #endif
    }
    case kpidLinks:
    {
      #ifndef UNDER_CE
      if (!fi1.FileInfo_WasRequested) ReadFileInfo(fi1);
      if (!fi2.FileInfo_WasRequested) ReadFileInfo(fi2);
      return MyCompare(
          fi1.NumLinks,
          fi2.NumLinks);
      #endif
    }
    #endif

    case kpidComment:
    {
      // change it !
      UString comment1, comment2;
      _comments.GetValue(fs2us(GetRelPath(fi1)), comment1);
      _comments.GetValue(fs2us(GetRelPath(fi2)), comment2);
      return MyStringCompareNoCase(comment1, comment2);
    }
    case kpidPrefix:
      if (fi1.Parent < 0) return (fi2.Parent < 0) ? 0 : -1;
      if (fi2.Parent < 0) return 1;
      return CompareFileNames_ForFolderList(
          Folders[fi1.Parent],
          Folders[fi2.Parent]);
    case kpidExtension:
      return CompareFileNames_ForFolderList(
          GetExtensionPtr(fi1.Name),
          GetExtensionPtr(fi2.Name));
  }
  
  return 0;
}

HRESULT CFSFolder::BindToFolderSpec(CFSTR name, IFolderFolder **resultFolder)
{
  *resultFolder = 0;
  CFSFolder *folderSpec = new CFSFolder;
  CMyComPtr<IFolderFolder> subFolder = folderSpec;
  RINOK(folderSpec->Init(_path + name + FCHAR_PATH_SEPARATOR));
  *resultFolder = subFolder.Detach();
  return S_OK;
}

/*
void CFSFolder::GetPrefix(const CDirItem &item, FString &prefix) const
{
  if (item.Parent >= 0)
    prefix = Folders[item.Parent];
  else
    prefix.Empty();
}
*/

/*
void CFSFolder::GetPrefix(const CDirItem &item, FString &prefix) const
{
  int parent = item.Parent;

  unsigned len = 0;

  while (parent >= 0)
  {
    const CDirItem &cur = Files[parent];
    len += cur.Name.Len() + 1;
    parent = cur.Parent;
  }

  wchar_t *p = prefix.GetBuf_SetEnd(len) + len;
  parent = item.Parent;

  while (parent >= 0)
  {
    const CDirItem &cur = Files[parent];
    *(--p) = FCHAR_PATH_SEPARATOR;
    p -= cur.Name.Len();
    wmemcpy(p, cur.Name, cur.Name.Len());
    parent = cur.Parent;
  }
}
*/

FString CFSFolder::GetRelPath(const CDirItem &item) const
{
  if (item.Parent < 0)
    return item.Name;
  return Folders[item.Parent] + item.Name;
}

STDMETHODIMP CFSFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder)
{
  *resultFolder = 0;
  const CDirItem &fi = Files[index];
  if (!fi.IsDir())
    return E_INVALIDARG;
  return BindToFolderSpec(GetRelPath(fi), resultFolder);
}

STDMETHODIMP CFSFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder)
{
  return BindToFolderSpec(us2fs(name), resultFolder);
}

STDMETHODIMP CFSFolder::BindToParentFolder(IFolderFolder **resultFolder)
{
  *resultFolder = 0;
  /*
  if (_parentFolder)
  {
    CMyComPtr<IFolderFolder> parentFolder = _parentFolder;
    *resultFolder = parentFolder.Detach();
    return S_OK;
  }
  */
  if (_path.IsEmpty())
    return E_INVALIDARG;
  
  #ifndef UNDER_CE

  if (IsDriveRootPath_SuperAllowed(_path))
  {
    CFSDrives *drivesFolderSpec = new CFSDrives;
    CMyComPtr<IFolderFolder> drivesFolder = drivesFolderSpec;
    drivesFolderSpec->Init(false, IsSuperPath(_path));
    *resultFolder = drivesFolder.Detach();
    return S_OK;
  }
  
  int pos = _path.ReverseFind_PathSepar();
  if (pos < 0 || pos != (int)_path.Len() - 1)
    return E_FAIL;
  FString parentPath = _path.Left(pos);
  pos = parentPath.ReverseFind_PathSepar();
  parentPath.DeleteFrom((unsigned)(pos + 1));

  if (NName::IsDrivePath_SuperAllowed(parentPath))
  {
    CFSFolder *parentFolderSpec = new CFSFolder;
    CMyComPtr<IFolderFolder> parentFolder = parentFolderSpec;
    if (parentFolderSpec->Init(parentPath) == S_OK)
    {
      *resultFolder = parentFolder.Detach();
      return S_OK;
    }
  }
  
  /*
  FString parentPathReduced = parentPath.Left(pos);
  
  pos = parentPathReduced.ReverseFind_PathSepar();
  if (pos == 1)
  {
    if (!IS_PATH_SEPAR_CHAR(parentPath[0]))
      return E_FAIL;
    CNetFolder *netFolderSpec = new CNetFolder;
    CMyComPtr<IFolderFolder> netFolder = netFolderSpec;
    netFolderSpec->Init(fs2us(parentPath));
    *resultFolder = netFolder.Detach();
    return S_OK;
  }
  */
  
  #endif

  return S_OK;
}

STDMETHODIMP CFSFolder::GetNumberOfProperties(UInt32 *numProperties)
{
  *numProperties = ARRAY_SIZE(kProps);
  if (!_flatMode)
    (*numProperties)--;
  return S_OK;
}

STDMETHODIMP CFSFolder::GetPropertyInfo IMP_IFolderFolder_GetProp(kProps)

STDMETHODIMP CFSFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "FSFolder"; break;
    case kpidPath: prop = fs2us(_path); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CFSFolder::WasChanged(Int32 *wasChanged)
{
  bool wasChangedMain = false;

  #ifdef _WIN32

  for (;;)
  {
    if (!_findChangeNotification.IsHandleAllocated())
      break;
    DWORD waitResult = ::WaitForSingleObject(_findChangeNotification, 0);
    if (waitResult != WAIT_OBJECT_0)
      break;
    _findChangeNotification.FindNext();
    wasChangedMain = true;
  }

  #endif

  *wasChanged = BoolToInt(wasChangedMain);
  return S_OK;
}
 
STDMETHODIMP CFSFolder::Clone(IFolderFolder **resultFolder)
{
  CFSFolder *fsFolderSpec = new CFSFolder;
  CMyComPtr<IFolderFolder> folderNew = fsFolderSpec;
  fsFolderSpec->Init(_path);
  *resultFolder = folderNew.Detach();
  return S_OK;
}

HRESULT CFSFolder::GetItemsFullSize(const UInt32 *indices, UInt32 numItems, CFsFolderStat &stat)
{
  for (UInt32 i = 0; i < numItems; i++)
  {
    UInt32 index = indices[i];
    /*
    if (index >= Files.Size())
    {
      size += Streams[index - Files.Size()].Size;
      // numFiles++;
      continue;
    }
    */
    const CDirItem &fi = Files[index];
    if (fi.IsDir())
    {
      stat.Path = _path;
      stat.Path += GetRelPath(fi);
      RINOK(stat.Enumerate());
      stat.NumFolders++;
    }
    else
    {
      stat.NumFiles++;
      stat.Size += fi.Size;
    }
  }
  return S_OK;
}

/*
HRESULT CFSFolder::GetItemFullSize(unsigned index, UInt64 &size, IProgress *progress)
{
  if (index >= Files.Size())
  {
    size = Streams[index - Files.Size()].Size;
    return S_OK;
  }
  const CDirItem &fi = Files[index];
  if (fi.IsDir())
  {
    UInt64 numFolders = 0, numFiles = 0;
    size = 0;
    return GetFolderSize(_path + GetRelPath(fi), numFolders, numFiles, size, progress);
  }
  size = fi.Size;
  return S_OK;
}

STDMETHODIMP CFSFolder::GetItemFullSize(UInt32 index, PROPVARIANT *value, IProgress *progress)
{
  NCOM::CPropVariant prop;
  UInt64 size = 0;
  HRESULT result = GetItemFullSize(index, size, progress);
  prop = size;
  prop.Detach(value);
  return result;
}
*/

STDMETHODIMP CFSFolder::CalcItemFullSize(UInt32 index, IProgress *progress)
{
  if (index >= (UInt32)Files.Size())
    return S_OK;
  CDirItem &fi = Files[index];
  if (!fi.IsDir())
    return S_OK;
  CFsFolderStat stat(_path + GetRelPath(fi), progress);
  RINOK(stat.Enumerate());
  fi.Size = stat.Size;
  fi.NumFolders = stat.NumFolders;
  fi.NumFiles = stat.NumFiles;
  fi.FolderStat_Defined = true;
  return S_OK;
}

void CFSFolder::GetAbsPath(const wchar_t *name, FString &absPath)
{
  absPath.Empty();
  if (!IsAbsolutePath(name))
    absPath += _path;
  absPath += us2fs(name);
}

STDMETHODIMP CFSFolder::CreateFolder(const wchar_t *name, IProgress * /* progress */)
{
  FString absPath;
  GetAbsPath(name, absPath);
  if (CreateDir(absPath))
    return S_OK;
  if (::GetLastError() == ERROR_ALREADY_EXISTS)
    return ::GetLastError();
  if (!CreateComplexDir(absPath))
    return ::GetLastError();
  return S_OK;
}

STDMETHODIMP CFSFolder::CreateFile(const wchar_t *name, IProgress * /* progress */)
{
  FString absPath;
  GetAbsPath(name, absPath);
  NIO::COutFile outFile;
  if (!outFile.Create(absPath, false))
    return ::GetLastError();
  return S_OK;
}

STDMETHODIMP CFSFolder::Rename(UInt32 index, const wchar_t *newName, IProgress * /* progress */)
{
  if (index >= (UInt32)Files.Size())
    return E_NOTIMPL;
  const CDirItem &fi = Files[index];
  // FString prefix;
  // GetPrefix(fi, prefix);
  FString fullPrefix = _path;
  if (fi.Parent >= 0)
    fullPrefix += Folders[fi.Parent];
  if (!MyMoveFile(fullPrefix + fi.Name, fullPrefix + us2fs(newName)))
    return GetLastError();
  return S_OK;
}

STDMETHODIMP CFSFolder::Delete(const UInt32 *indices, UInt32 numItems,IProgress *progress)
{
  RINOK(progress->SetTotal(numItems));
  // int prevDeletedFileIndex = -1;
  for (UInt32 i = 0; i < numItems; i++)
  {
    // Sleep(200);
    UInt32 index = indices[i];
    bool result = true;
    /*
    if (index >= (UInt32)Files.Size())
    {
      const CAltStream &ss = Streams[index - (UInt32)Files.Size()];
      if (prevDeletedFileIndex != ss.Parent)
      {
        const CDirItem &fi = Files[ss.Parent];
        result = DeleteFileAlways(_path + GetRelPath(fi) + us2fs(ss.Name));
      }
    }
    else
    */
    {
      const CDirItem &fi = Files[index];
      const FString fullPath = _path + GetRelPath(fi);
      // prevDeletedFileIndex = index;
      if (fi.IsDir())
        result = RemoveDirWithSubItems(fullPath);
      else
        result = DeleteFileAlways(fullPath);
    }
    if (!result)
      return GetLastError();
    UInt64 completed = i;
    RINOK(progress->SetCompleted(&completed));
  }
  return S_OK;
}

STDMETHODIMP CFSFolder::SetProperty(UInt32 index, PROPID propID,
    const PROPVARIANT *value, IProgress * /* progress */)
{
  if (index >= (UInt32)Files.Size())
    return E_INVALIDARG;
  CDirItem &fi = Files[index];
  if (fi.Parent >= 0)
    return E_NOTIMPL;
  switch (propID)
  {
    case kpidComment:
    {
      UString filename = fs2us(fi.Name);
      filename.Trim();
      if (value->vt == VT_EMPTY)
        _comments.DeletePair(filename);
      else if (value->vt == VT_BSTR)
      {
        CTextPair pair;
        pair.ID = filename;
        pair.ID.Trim();
        pair.Value.SetFromBstr(value->bstrVal);
        pair.Value.Trim();
        if (pair.Value.IsEmpty())
          _comments.DeletePair(filename);
        else
          _comments.AddPair(pair);
      }
      else
        return E_INVALIDARG;
      SaveComments();
      break;
    }
    default:
      return E_NOTIMPL;
  }
  return S_OK;
}

STDMETHODIMP CFSFolder::GetSystemIconIndex(UInt32 index, Int32 *iconIndex)
{
  if (index >= (UInt32)Files.Size())
    return E_INVALIDARG;
  const CDirItem &fi = Files[index];
  *iconIndex = 0;
  int iconIndexTemp;
  if (GetRealIconIndex(_path + GetRelPath(fi), fi.Attrib, iconIndexTemp) != 0)
  {
    *iconIndex = iconIndexTemp;
    return S_OK;
  }
  return GetLastError();
}

STDMETHODIMP CFSFolder::SetFlatMode(Int32 flatMode)
{
  _flatMode = IntToBool(flatMode);
  return S_OK;
}

/*
STDMETHODIMP CFSFolder::SetShowNtfsStreamsMode(Int32 showStreamsMode)
{
  _scanAltStreams = IntToBool(showStreamsMode);
  return S_OK;
}
*/

}
