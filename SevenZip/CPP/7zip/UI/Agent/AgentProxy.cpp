// AgentProxy.cpp

#include "StdAfx.h"

// #include <stdio.h>
#ifdef _WIN32
#include <wchar.h>
#else
#include <ctype.h>
#endif

#include "../../../../C/Sort.h"
#include "../../../../C/CpuArch.h"

#include "../../../Common/UTFConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#include "AgentProxy.h"

using namespace NWindows;

int CProxyArc::FindSubDir(unsigned dirIndex, const wchar_t *name, unsigned &insertPos) const
{
  const CRecordVector<unsigned> &subDirs = Dirs[dirIndex].SubDirs;
  unsigned left = 0, right = subDirs.Size();
  for (;;)
  {
    if (left == right)
    {
      insertPos = left;
      return -1;
    }
    unsigned mid = (left + right) / 2;
    unsigned dirIndex2 = subDirs[mid];
    int compare = CompareFileNames(name, Dirs[dirIndex2].Name);
    if (compare == 0)
      return dirIndex2;
    if (compare < 0)
      right = mid;
    else
      left = mid + 1;
  }
}

int CProxyArc::FindSubDir(unsigned dirIndex, const wchar_t *name) const
{
  unsigned insertPos;
  return FindSubDir(dirIndex, name, insertPos);
}

static const wchar_t *AllocStringAndCopy(const wchar_t *s, size_t len)
{
  wchar_t *p = new wchar_t[len + 1];
  MyStringCopy(p, s);
  return p;
}

static const wchar_t *AllocStringAndCopy(const UString &s)
{
  return AllocStringAndCopy(s, s.Len());
}
                         
unsigned CProxyArc::AddDir(unsigned dirIndex, int arcIndex, const UString &name)
{
  unsigned insertPos;
  int subDirIndex = FindSubDir(dirIndex, name, insertPos);
  if (subDirIndex >= 0)
  {
    if (arcIndex >= 0)
    {
      CProxyDir &item = Dirs[subDirIndex];
      if (item.ArcIndex < 0)
        item.ArcIndex = arcIndex;
    }
    return subDirIndex;
  }
  subDirIndex = Dirs.Size();
  Dirs[dirIndex].SubDirs.Insert(insertPos, subDirIndex);
  CProxyDir &item = Dirs.AddNew();

  item.NameLen = name.Len();
  item.Name = AllocStringAndCopy(name);

  item.ArcIndex = arcIndex;
  item.ParentDir = dirIndex;
  return subDirIndex;
}

void CProxyDir::Clear()
{
  SubDirs.Clear();
  SubFiles.Clear();
}

void CProxyArc::GetDirPathParts(int dirIndex, UStringVector &pathParts) const
{
  pathParts.Clear();
  while (dirIndex >= 0)
  {
    const CProxyDir &dir = Dirs[dirIndex];
    dirIndex = dir.ParentDir;
    if (dirIndex < 0)
      break;
    pathParts.Insert(0, dir.Name);
  }
}

UString CProxyArc::GetDirPath_as_Prefix(int dirIndex) const
{
  UString s;
  while (dirIndex >= 0)
  {
    const CProxyDir &dir = Dirs[dirIndex];
    dirIndex = dir.ParentDir;
    if (dirIndex < 0)
      break;
    s.InsertAtFront(WCHAR_PATH_SEPARATOR);
    s.Insert(0, dir.Name);
  }
  return s;
}

void CProxyArc::AddRealIndices(unsigned dirIndex, CUIntVector &realIndices) const
{
  const CProxyDir &dir = Dirs[dirIndex];
  if (dir.IsLeaf())
    realIndices.Add(dir.ArcIndex);
  unsigned i;
  for (i = 0; i < dir.SubDirs.Size(); i++)
    AddRealIndices(dir.SubDirs[i], realIndices);
  for (i = 0; i < dir.SubFiles.Size(); i++)
    realIndices.Add(dir.SubFiles[i]);
}

int CProxyArc::GetRealIndex(unsigned dirIndex, unsigned index) const
{
  const CProxyDir &dir = Dirs[dirIndex];
  unsigned numDirItems = dir.SubDirs.Size();
  if (index < numDirItems)
  {
    const CProxyDir &f = Dirs[dir.SubDirs[index]];
    if (f.IsLeaf())
      return f.ArcIndex;
    return -1;
  }
  return dir.SubFiles[index - numDirItems];
}

void CProxyArc::GetRealIndices(unsigned dirIndex, const UInt32 *indices, UInt32 numItems, CUIntVector &realIndices) const
{
  const CProxyDir &dir = Dirs[dirIndex];
  realIndices.Clear();
  for (UInt32 i = 0; i < numItems; i++)
  {
    UInt32 index = indices[i];
    unsigned numDirItems = dir.SubDirs.Size();
    if (index < numDirItems)
      AddRealIndices(dir.SubDirs[index], realIndices);
    else
      realIndices.Add(dir.SubFiles[index - numDirItems]);
  }
  HeapSort(&realIndices.Front(), realIndices.Size());
}

///////////////////////////////////////////////
// CProxyArc

static bool GetSize(IInArchive *archive, UInt32 index, PROPID propID, UInt64 &size)
{
  size = 0;
  NCOM::CPropVariant prop;
  if (archive->GetProperty(index, propID, &prop) != S_OK)
    throw 20120228;
  return ConvertPropVariantToUInt64(prop, size);
}

void CProxyArc::CalculateSizes(unsigned dirIndex, IInArchive *archive)
{
  CProxyDir &dir = Dirs[dirIndex];
  dir.Size = dir.PackSize = 0;
  dir.NumSubDirs = dir.SubDirs.Size();
  dir.NumSubFiles = dir.SubFiles.Size();
  dir.CrcIsDefined = true;
  dir.Crc = 0;
  
  unsigned i;
  
  for (i = 0; i < dir.SubFiles.Size(); i++)
  {
    UInt32 index = (UInt32)dir.SubFiles[i];
    UInt64 size, packSize;
    bool sizeDefined = GetSize(archive, index, kpidSize, size);
    dir.Size += size;
    GetSize(archive, index, kpidPackSize, packSize);
    dir.PackSize += packSize;
    {
      NCOM::CPropVariant prop;
      if (archive->GetProperty(index, kpidCRC, &prop) == S_OK)
      {
        if (prop.vt == VT_UI4)
          dir.Crc += prop.ulVal;
        else if (prop.vt != VT_EMPTY || size != 0 || !sizeDefined)
          dir.CrcIsDefined = false;
      }
      else
        dir.CrcIsDefined = false;
    }
  }
  
  for (i = 0; i < dir.SubDirs.Size(); i++)
  {
    unsigned subDirIndex = dir.SubDirs[i];
    CalculateSizes(subDirIndex, archive);
    CProxyDir &f = Dirs[subDirIndex];
    dir.Size += f.Size;
    dir.PackSize += f.PackSize;
    dir.NumSubFiles += f.NumSubFiles;
    dir.NumSubDirs += f.NumSubDirs;
    dir.Crc += f.Crc;
    if (!f.CrcIsDefined)
      dir.CrcIsDefined = false;
  }
}

HRESULT CProxyArc::Load(const CArc &arc, IProgress *progress)
{
  // DWORD tickCount = GetTickCount(); for (int ttt = 0; ttt < 1; ttt++) {

  Files.Free();
  Dirs.Clear();

  Dirs.AddNew();
  IInArchive *archive = arc.Archive;

  UInt32 numItems;
  RINOK(archive->GetNumberOfItems(&numItems));
  
  if (progress)
    RINOK(progress->SetTotal(numItems));
  
  Files.Alloc(numItems);

  UString path;
  UString name;
  NCOM::CPropVariant prop;
  
  for (UInt32 i = 0; i < numItems; i++)
  {
    if (progress && (i & 0xFFFF) == 0)
    {
      UInt64 currentItemIndex = i;
      RINOK(progress->SetCompleted(&currentItemIndex));
    }
    
    const wchar_t *s = NULL;
    unsigned len = 0;
    bool isPtrName = false;

    #if WCHAR_PATH_SEPARATOR != L'/'
    wchar_t replaceFromChar = 0;
    #endif

    #if defined(MY_CPU_LE) && defined(_WIN32)
    // it works only if (sizeof(wchar_t) == 2)
    if (arc.GetRawProps)
    {
      const void *p;
      UInt32 size;
      UInt32 propType;
      if (arc.GetRawProps->GetRawProp(i, kpidPath, &p, &size, &propType) == S_OK
          && propType == NPropDataType::kUtf16z
          && size > 2)
      {
        // is (size <= 2), it's empty name, and we call default arc.GetItemPath();
        len = size / 2 - 1;
        s = (const wchar_t *)p;
        isPtrName = true;
        #if WCHAR_PATH_SEPARATOR != L'/'
        replaceFromChar = L'\\';
        #endif
      }
    }
    if (!s)
    #endif
    {
      prop.Clear();
      RINOK(arc.Archive->GetProperty(i, kpidPath, &prop));
      if (prop.vt == VT_BSTR)
      {
        s = prop.bstrVal;
        len = ::SysStringLen(prop.bstrVal);
      }
      else if (prop.vt != VT_EMPTY)
        return E_FAIL;
      if (len == 0)
      {
        RINOK(arc.GetDefaultItemPath(i, path));
        len = path.Len();
        s = path;
      }
      
      /*
      RINOK(arc.GetItemPath(i, path));
      len = path.Len();
      s = path;
      */
    }

    unsigned curItem = 0;

    /*
    if (arc.Ask_Deleted)
    {
      bool isDeleted = false;
      RINOK(Archive_IsItem_Deleted(archive, i, isDeleted));
      if (isDeleted)
        curItem = AddDirSubItem(curItem, (UInt32)(Int32)-1, false, L"[DELETED]");
    }
    */

    unsigned namePos = 0;

    unsigned numLevels = 0;

    for (unsigned j = 0; j < len; j++)
    {
      const wchar_t c = s[j];
      if (c == WCHAR_PATH_SEPARATOR || c == L'/')
      {
        #if WCHAR_PATH_SEPARATOR != L'/'
        if (c == replaceFromChar)
        {
          // s.ReplaceOneCharAtPos(j, WCHAR_IN_FILE_NAME_BACKSLASH_REPLACEMENT);
          continue;
        }
        #endif

        const unsigned kLevelLimit = 1 << 10;
        if (numLevels <= kLevelLimit)
        {
          if (numLevels == kLevelLimit)
            name = "[LONG_PATH]";
          else
            name.SetFrom(s + namePos, j - namePos);
          curItem = AddDir(curItem, -1, name);
        }
        namePos = j + 1;
        numLevels++;
      }
    }

    /*
    that code must be implemeted to hide alt streams in list.
    if (arc.Ask_AltStreams)
    {
      bool isAltStream;
      RINOK(Archive_IsItem_AltStream(archive, i, isAltStream));
      if (isAltStream)
      {

      }
    }
    */

    bool isDir;
    RINOK(Archive_IsItem_Dir(archive, i, isDir));

    CProxyFile &f = Files[i];

    f.NameLen = len - namePos;
    s += namePos;

    if (isPtrName)
      f.Name = s;
    else
    {
      f.Name = AllocStringAndCopy(s, f.NameLen);
      f.NeedDeleteName = true;
    }

    if (isDir)
    {
      name = s;
      AddDir(curItem, (int)i, name);
    }
    else
      Dirs[curItem].SubFiles.Add(i);
  }
  
  CalculateSizes(0, archive);

  // } char s[128]; sprintf(s, "Load archive: %7d ms", GetTickCount() - tickCount); OutputDebugStringA(s);

  return S_OK;
}



// ---------- for Tree-mode archive ----------

void CProxyArc2::GetDirPathParts(int dirIndex, UStringVector &pathParts, bool &isAltStreamDir) const
{
  pathParts.Clear();
  
  isAltStreamDir = false;
  
  if (dirIndex == (int)k_Proxy2_RootDirIndex)
    return;
  if (dirIndex == (int)k_Proxy2_AltRootDirIndex)
  {
    isAltStreamDir = true;
    return;
  }

  while (dirIndex >= (int)k_Proxy2_NumRootDirs)
  {
    const CProxyDir2 &dir = Dirs[dirIndex];
    const CProxyFile2 &file = Files[dir.ArcIndex];
    if (pathParts.IsEmpty() && dirIndex == file.AltDirIndex)
      isAltStreamDir = true;
    pathParts.Insert(0, file.Name);
    int par = file.Parent;
    if (par < 0)
      break;
    dirIndex = Files[par].DirIndex;
  }
}

bool CProxyArc2::IsAltDir(unsigned dirIndex) const
{
  if (dirIndex == k_Proxy2_RootDirIndex)
    return false;
  if (dirIndex == k_Proxy2_AltRootDirIndex)
    return true;
  const CProxyDir2 &dir = Dirs[dirIndex];
  const CProxyFile2 &file = Files[dir.ArcIndex];
  return ((int)dirIndex == file.AltDirIndex);
}

UString CProxyArc2::GetDirPath_as_Prefix(unsigned dirIndex, bool &isAltStreamDir) const
{
  isAltStreamDir = false;
  const CProxyDir2 &dir = Dirs[dirIndex];
  if (dirIndex == k_Proxy2_AltRootDirIndex)
    isAltStreamDir = true;
  else if (dirIndex >= k_Proxy2_NumRootDirs)
  {
    const CProxyFile2 &file = Files[dir.ArcIndex];
    isAltStreamDir = ((int)dirIndex == file.AltDirIndex);
  }
  return dir.PathPrefix;
}

void CProxyArc2::AddRealIndices_of_ArcItem(unsigned arcIndex, bool includeAltStreams, CUIntVector &realIndices) const
{
  realIndices.Add(arcIndex);
  const CProxyFile2 &file = Files[arcIndex];
  if (file.DirIndex >= 0)
    AddRealIndices_of_Dir(file.DirIndex, includeAltStreams, realIndices);
  if (includeAltStreams && file.AltDirIndex >= 0)
    AddRealIndices_of_Dir(file.AltDirIndex, includeAltStreams, realIndices);
}

void CProxyArc2::AddRealIndices_of_Dir(unsigned dirIndex, bool includeAltStreams, CUIntVector &realIndices) const
{
  const CRecordVector<unsigned> &subFiles = Dirs[dirIndex].Items;
  FOR_VECTOR (i, subFiles)
  {
    AddRealIndices_of_ArcItem(subFiles[i], includeAltStreams, realIndices);
  }
}

unsigned CProxyArc2::GetRealIndex(unsigned dirIndex, unsigned index) const
{
  return Dirs[dirIndex].Items[index];
}

void CProxyArc2::GetRealIndices(unsigned dirIndex, const UInt32 *indices, UInt32 numItems, bool includeAltStreams, CUIntVector &realIndices) const
{
  const CProxyDir2 &dir = Dirs[dirIndex];
  realIndices.Clear();
  for (UInt32 i = 0; i < numItems; i++)
  {
    AddRealIndices_of_ArcItem(dir.Items[indices[i]], includeAltStreams, realIndices);
  }
  HeapSort(&realIndices.Front(), realIndices.Size());
}

void CProxyArc2::CalculateSizes(unsigned dirIndex, IInArchive *archive)
{
  CProxyDir2 &dir = Dirs[dirIndex];
  dir.Size = dir.PackSize = 0;
  dir.NumSubDirs = 0; // dir.SubDirs.Size();
  dir.NumSubFiles = 0; // dir.Files.Size();
  dir.CrcIsDefined = true;
  dir.Crc = 0;
  
  FOR_VECTOR (i, dir.Items)
  {
    UInt32 index = dir.Items[i];
    UInt64 size, packSize;
    bool sizeDefined = GetSize(archive, index, kpidSize, size);
    dir.Size += size;
    GetSize(archive, index, kpidPackSize, packSize);
    dir.PackSize += packSize;
    {
      NCOM::CPropVariant prop;
      if (archive->GetProperty(index, kpidCRC, &prop) == S_OK)
      {
        if (prop.vt == VT_UI4)
          dir.Crc += prop.ulVal;
        else if (prop.vt != VT_EMPTY || size != 0 || !sizeDefined)
          dir.CrcIsDefined = false;
      }
      else
        dir.CrcIsDefined = false;
    }

    const CProxyFile2 &subFile = Files[index];
    if (subFile.DirIndex < 0)
    {
      dir.NumSubFiles++;
    }
    else
    {
      dir.NumSubDirs++;
      CProxyDir2 &f = Dirs[subFile.DirIndex];
      f.PathPrefix = dir.PathPrefix + subFile.Name + WCHAR_PATH_SEPARATOR;
      CalculateSizes(subFile.DirIndex, archive);
      dir.Size += f.Size;
      dir.PackSize += f.PackSize;
      dir.NumSubFiles += f.NumSubFiles;
      dir.NumSubDirs += f.NumSubDirs;
      dir.Crc += f.Crc;
      if (!f.CrcIsDefined)
        dir.CrcIsDefined = false;
    }

    if (subFile.AltDirIndex < 0)
    {
      // dir.NumSubFiles++;
    }
    else
    {
      // dir.NumSubDirs++;
      CProxyDir2 &f = Dirs[subFile.AltDirIndex];
      f.PathPrefix = dir.PathPrefix + subFile.Name + L':';
      CalculateSizes(subFile.AltDirIndex, archive);
    }
  }
}


bool CProxyArc2::IsThere_SubDir(unsigned dirIndex, const UString &name) const
{
  const CRecordVector<unsigned> &subFiles = Dirs[dirIndex].Items;
  FOR_VECTOR (i, subFiles)
  {
    const CProxyFile2 &file = Files[subFiles[i]];
    if (file.IsDir())
      if (CompareFileNames(name, file.Name) == 0)
        return true;
  }
  return false;
}

HRESULT CProxyArc2::Load(const CArc &arc, IProgress *progress)
{
  if (!arc.GetRawProps)
    return E_FAIL;

  // DWORD tickCount = GetTickCount(); for (int ttt = 0; ttt < 1; ttt++) {

  Dirs.Clear();
  Files.Free();

  IInArchive *archive = arc.Archive;

  UInt32 numItems;
  RINOK(archive->GetNumberOfItems(&numItems));
  if (progress)
    RINOK(progress->SetTotal(numItems));
  UString fileName;


  {
    // Dirs[0] - root dir
    /* CProxyDir2 &dir = */ Dirs.AddNew();
  }

  {
    // Dirs[1] - for alt streams of root dir
    CProxyDir2 &dir = Dirs.AddNew();
    dir.PathPrefix = ':';
  }

  Files.Alloc(numItems);

  UString tempUString;
  AString tempAString;

  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    if (progress && (i & 0xFFFFF) == 0)
    {
      UInt64 currentItemIndex = i;
      RINOK(progress->SetCompleted(&currentItemIndex));
    }
    
    CProxyFile2 &file = Files[i];
    
    const void *p;
    UInt32 size;
    UInt32 propType;
    RINOK(arc.GetRawProps->GetRawProp(i, kpidName, &p, &size, &propType));
    
    #ifdef MY_CPU_LE
    if (p && propType == PROP_DATA_TYPE_wchar_t_PTR_Z_LE)
    {
      file.Name = (const wchar_t *)p;
      file.NameLen = 0;
      if (size >= sizeof(wchar_t))
        file.NameLen = size / sizeof(wchar_t) - 1;
    }
    else
    #endif
    if (p && propType == NPropDataType::kUtf8z)
    {
      tempAString = (const char *)p;
      ConvertUTF8ToUnicode(tempAString, tempUString);
      file.NameLen = tempUString.Len();
      file.Name = AllocStringAndCopy(tempUString);
      file.NeedDeleteName = true;
    }
    else
    {
      NCOM::CPropVariant prop;
      RINOK(arc.Archive->GetProperty(i, kpidName, &prop));
      const wchar_t *s;
      if (prop.vt == VT_BSTR)
        s = prop.bstrVal;
      else if (prop.vt == VT_EMPTY)
        s = L"[Content]";
      else
        return E_FAIL;
      file.NameLen = MyStringLen(s);
      file.Name = AllocStringAndCopy(s, file.NameLen);
      file.NeedDeleteName = true;
    }
    
    UInt32 parent = (UInt32)(Int32)-1;
    UInt32 parentType = 0;
    RINOK(arc.GetRawProps->GetParent(i, &parent, &parentType));
    file.Parent = (Int32)parent;

    if (arc.Ask_Deleted)
    {
      bool isDeleted = false;
      RINOK(Archive_IsItem_Deleted(archive, i, isDeleted));
      if (isDeleted)
      {
        // continue;
        // curItem = AddDirSubItem(curItem, (UInt32)(Int32)-1, false, L"[DELETED]");
      }
    }

    bool isDir;
    RINOK(Archive_IsItem_Dir(archive, i, isDir));
    
    if (isDir)
    {
      file.DirIndex = Dirs.Size();
      CProxyDir2 &dir = Dirs.AddNew();
      dir.ArcIndex = i;
    }
    if (arc.Ask_AltStream)
      RINOK(Archive_IsItem_AltStream(archive, i, file.IsAltStream));
  }

  for (i = 0; i < numItems; i++)
  {
    CProxyFile2 &file = Files[i];
    int dirIndex;
    
    if (file.IsAltStream)
    {
      if (file.Parent < 0)
        dirIndex = k_Proxy2_AltRootDirIndex;
      else
      {
        int &folderIndex2 = Files[file.Parent].AltDirIndex;
        if (folderIndex2 < 0)
        {
          folderIndex2 = Dirs.Size();
          CProxyDir2 &dir = Dirs.AddNew();
          dir.ArcIndex = file.Parent;
        }
        dirIndex = folderIndex2;
      }
    }
    else
    {
      if (file.Parent < 0)
        dirIndex = k_Proxy2_RootDirIndex;
      else
      {
        dirIndex = Files[file.Parent].DirIndex;
        if (dirIndex < 0)
          return E_FAIL;
      }
    }
    
    Dirs[dirIndex].Items.Add(i);
  }
  
  for (i = 0; i < k_Proxy2_NumRootDirs; i++)
    CalculateSizes(i, archive);

  // } char s[128]; sprintf(s, "Load archive: %7d ms", GetTickCount() - tickCount); OutputDebugStringA(s);

  return S_OK;
}

int CProxyArc2::FindItem(unsigned dirIndex, const wchar_t *name, bool foldersOnly) const
{
  const CProxyDir2 &dir = Dirs[dirIndex];
  FOR_VECTOR (i, dir.Items)
  {
    const CProxyFile2 &file = Files[dir.Items[i]];
    if (foldersOnly && file.DirIndex < 0)
      continue;
    if (CompareFileNames(file.Name, name) == 0)
      return i;
  }
  return -1;
}
