// AgentProxy.cpp

#include "StdAfx.h"

// #include <stdio.h>
#ifdef _WIN32
#include <wchar.h>
#else
#include <ctype.h>
#endif

#include "../../../../C/CpuArch.h"

#include "../../../Common/UTFConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#include "../../Archive/Common/ItemNameUtils.h"

#include "AgentProxy.h"

using namespace NWindows;

#define CHECK_MEM_USAGE { if (MemUsage > MemUsage_Limit) return E_OUTOFMEMORY; }

int CProxyArc::FindSubDir(const unsigned *items, unsigned right,
    const wchar_t *name, unsigned &insertPos) const
{
  unsigned left = 0;
  for (;;)
  {
    if (left == right)
    {
      insertPos = left;
      return -1;
    }
    const unsigned mid = (unsigned)(((size_t)left + (size_t)right) / 2);
    const unsigned dirIndex2 = items[mid];
    const int comp = CompareFileNames(name, Dirs[dirIndex2].Name);
    if (comp == 0)
    {
      insertPos = mid; // not used, but we set it to suppress compiler warnings
      return (int)dirIndex2;
    }
    if (comp < 0)
      right = mid;
    else
      left = mid + 1;
  }
}

int CProxyArc::FindSubDir(unsigned dirIndex, const wchar_t *name) const
{
  unsigned insertPos;
  const CUIntVector &dirs = Dirs[dirIndex].SubDirs;
  return FindSubDir(dirs.ConstData(), dirs.Size(), name, insertPos);
}

static const wchar_t *AllocStringAndCopy(const wchar_t *s, size_t len)
{
  ++len;
  wchar_t *p = new wchar_t[len];
  return wmemcpy(p, s, len);
  // MyStringCopy(p, s); return p;
}

static const wchar_t *AllocStringAndCopy(const UString &s)
{
  return AllocStringAndCopy(s, s.Len());
}


void CProxyArc::MergeSubDirs(CProxyDir &baseDir)
{
  CUIntVector &dirs2 = baseDir.SubDirs2;
  unsigned i = dirs2.Size();
  if (i == 0)
    return;
  CUIntVector &dirs = baseDir.SubDirs;
  unsigned right = dirs.Size();
  unsigned total = right + i;
  dirs.ChangeSize_KeepData_plus_AdditionalReserve(total);
  unsigned * const items = dirs.NonConstData();
  do
  {
    const unsigned index = dirs2[--i];
    unsigned insertPos;
    if (FindSubDir(items, right, Dirs[index].Name, insertPos) != -1)
      throw 1;
    const unsigned numItems = right - insertPos;
    right = insertPos;
    total -= numItems;
    memmove(items + total, items + insertPos, numItems * sizeof(*items));
    items[--total] = index;
  }
  while (i);
  dirs2.Clear();
}


unsigned CProxyArc::AddDir(const unsigned dirIndex, const int arcIndex, const UString &name)
{
  CProxyDir &baseDir = Dirs[dirIndex];
  CUIntVector &dirs = baseDir.SubDirs;
  CUIntVector &dirs2 = baseDir.SubDirs2;
  {
    const unsigned numSmallDirs = dirs2.Size();
    if (numSmallDirs * numSmallDirs > dirs.Size())
      MergeSubDirs(baseDir);
  }
  unsigned insertPos;
  int subDirIndex = FindSubDir(dirs.ConstData(), dirs.Size(), name, insertPos);
  if (subDirIndex == -1)
  {
    unsigned insertPos2;
    subDirIndex = FindSubDir(dirs2.ConstData(), dirs2.Size(), name, insertPos2);
    if (subDirIndex == -1)
    {
      CUIntVector *subDirsWork = &dirs;
      if (dirs.Size() - insertPos >= (1 << 12))
      {
        subDirsWork = &dirs2;
        insertPos = insertPos2;
      }
      subDirIndex = (int)Dirs.Size();
      subDirsWork->Insert(insertPos, (unsigned)subDirIndex);
      CProxyDir &item = Dirs.AddNew();
      item.NameLen = name.Len();
      MemUsage += (size_t)item.NameLen * sizeof(*item.Name)
          + sizeof(CProxyDir) + sizeof(void *) * 3 + 24;
      item.Name = AllocStringAndCopy(name);
      item.ArcIndex = arcIndex;
      item.ParentDir = (int)dirIndex;
      return (unsigned)subDirIndex;
    }
  }

  CProxyDir &item = Dirs[(unsigned)subDirIndex];
  if (item.ArcIndex == -1)
    item.ArcIndex = arcIndex;
  return (unsigned)subDirIndex;
}


void CProxyArc::GetDirPathParts_isChanged(unsigned dirIndex,
    UStringVector &pathParts, bool &isChangedPath) const
{
  isChangedPath = false;
  pathParts.Clear();
  while (dirIndex != k_Proxy_RootDirIndex)
  {
    const CProxyDir &dir = Dirs[dirIndex];
    if (dir.Is_Changed_LongPath)
      isChangedPath = true;
    dirIndex = (unsigned)dir.ParentDir;
    pathParts.Insert(0, dir.Name);
    // 22.00: we normalize name
    NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(pathParts[0]);
  }
}


void CProxyArc::GetDirPath_as_Prefix_from_Base(const unsigned dirIndex,
    UString &prefix, const unsigned baseDirIndex) const
{
  prefix.Empty();
  if (dirIndex == baseDirIndex)
    return;
  unsigned len = 0;
  unsigned i = dirIndex;
  do
  {
    const CProxyDir &dir = Dirs[i];
    len += dir.NameLen + 1;
    i = (unsigned)dir.ParentDir;
  }
  while (i != baseDirIndex);

  wchar_t *p = prefix.GetBuf_SetEnd(len) + len;
  i = dirIndex;
  do
  {
    const CProxyDir &dir = Dirs[i];
    *--p = WCHAR_PATH_SEPARATOR;
    p -= dir.NameLen;
    wmemcpy(p, dir.Name, dir.NameLen);
    i = (unsigned)dir.ParentDir;
  }
  while (i != baseDirIndex);
}

UString CProxyArc::GetDirPath_as_Prefix(const unsigned dirIndex) const
{
  UString s;
  GetDirPath_as_Prefix_from_Base(dirIndex, s, k_Proxy_RootDirIndex);
  return s;
}

void CProxyArc::AddRealIndices(const unsigned dirIndex, CUIntVector &realIndices) const
{
  const CProxyDir &dir = Dirs[dirIndex];
  if (dir.IsLeaf())
    realIndices.Add((unsigned)dir.ArcIndex);
  unsigned i;
  for (i = 0; i < dir.SubDirs.Size(); i++)
    AddRealIndices(dir.SubDirs[i], realIndices);
  for (i = 0; i < dir.SubFiles.Size(); i++)
    realIndices.Add(dir.SubFiles[i]);
}

int CProxyArc::GetRealIndex(const unsigned dirIndex, const unsigned index) const
{
  const CProxyDir &dir = Dirs[dirIndex];
  const unsigned numDirItems = dir.SubDirs.Size();
  if (index < numDirItems)
  {
    const CProxyDir &f = Dirs[dir.SubDirs[index]];
    if (f.IsLeaf())
      return f.ArcIndex;
    return -1;
  }
  return (int)dir.SubFiles[index - numDirItems];
}

void CProxyArc::GetRealIndices_Unsorted(unsigned dirIndex,
    const UInt32 *indices, const UInt32 numItems, CUIntVector &realIndices) const
{
  const CProxyDir &dir = Dirs[dirIndex];
  realIndices.Clear();
  for (UInt32 i = 0; i < numItems; i++)
  {
    const UInt32 index = indices[i];
    const unsigned numDirItems = dir.SubDirs.Size();
    if (index < numDirItems)
      AddRealIndices(dir.SubDirs[index], realIndices);
    else
      realIndices.Add(dir.SubFiles[index - numDirItems]);
  }
  // HeapSort(realIndices.NonConstData(), realIndices.Size());
}


static bool GetSize(IInArchive *archive, UInt32 index, PROPID propID, UInt64 &size)
{
  size = 0;
  NCOM::CPropVariant prop;
  if (archive->GetProperty(index, propID, &prop) != S_OK)
    throw 20120228;
  return ConvertPropVariantToUInt64(prop, size);
}

void CProxyArc::CalculateSizes(IInArchive *archive)
{
  CUIntVector vec;
  unsigned dirIndex = 0, i = 0;
  for (;;)
  {
    CProxyDir &dir = Dirs[dirIndex];
    if (i == 0)
    {
      dir.Size = dir.PackSize = 0;
      dir.NumSubDirs = dir.SubDirs.Size();
      dir.NumSubFiles = dir.SubFiles.Size();
      dir.CrcIsDefined = true;
      dir.Crc = 0;

      FOR_VECTOR (k, dir.SubFiles)
      {
        const UInt32 index = (UInt32)dir.SubFiles[k];
        UInt64 size, packSize;
        const bool sizeDefined = GetSize(archive, index, kpidSize, size);
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
    }

    if (i == dir.SubDirs.Size())
    {
      const unsigned num = vec.Size();
      if (num < 2)
        return;
      dirIndex = vec[num - 2];
      i = vec[num - 1];
      vec.DeleteFrom(num - 2);

      CProxyDir &dir2 = Dirs[dirIndex];
      dir2.Size += dir.Size;
      dir2.PackSize += dir.PackSize;
      dir2.NumSubFiles += dir.NumSubFiles;
      dir2.NumSubDirs += dir.NumSubDirs;
      dir2.Crc += dir.Crc;
      if (!dir.CrcIsDefined)
        dir2.CrcIsDefined = false;
      continue;
    }

    vec.Add(dirIndex);
    vec.Add(i + 1);
    dirIndex = dir.SubDirs[i];
    i = 0;
  }
}


void CProxyArc::FreeFiles()
{
  const unsigned numFiles = NumFiles;
  NumFiles = 0;
  CProxyFile *f = Files;
  for (unsigned i = 0; i < numFiles; i++, f++)
    if (f->NeedDeleteName)
      delete [](wchar_t *)(void *)f->Name;
  delete []Files;
  Files = NULL;
}


static void SetPath_as_LONG_PATH(UString &s,
    const unsigned fileIndex, const bool isDir)
{
  s = isDir ? "[LONG_PATH_DIR_" : "[LONG_PATH_FILE_";
  s.Add_UInt32((UInt32)fileIndex);
  s.Add_Char(']');
}


static const UInt32 k_NumFiles_Max = 0x7fffffff - 15;

HRESULT CProxyArc::Load(const CArc &arc, IArchiveOpenCallback *progress)
{
  // DWORD tickCount = GetTickCount(); for (int ttt = 0; ttt < 1; ttt++) {
  FreeFiles();
  Dirs.Clear();
  Are_Changed_LongPaths = false;

  Dirs.AddNew();
  IInArchive *archive = arc.Archive;
  UInt32 numItems;
  RINOK(archive->GetNumberOfItems(&numItems))
  if (numItems > k_NumFiles_Max)
    return E_OUTOFMEMORY;
  if (progress)
  {
    const UInt64 numItems64 = numItems;
    RINOK(progress->SetTotal(&numItems64, NULL))
  }

  MemUsage = (UInt64)numItems * sizeof(*Files);
  CHECK_MEM_USAGE
  Z7_ARRAY_NEW(Files, CProxyFile, numItems)
  memset(Files, 0, (size_t)numItems * sizeof(*Files));
  NumFiles = numItems;

  UString path, name;
  NCOM::CPropVariant prop;

  for (UInt32 i = 0; i < numItems; i++)
  {
    CHECK_MEM_USAGE
    if (progress && (i & 0xFFFF) == 0)
    {
      const UInt64 currentItemIndex = i;
      RINOK(progress->SetCompleted(&currentItemIndex, NULL))
    }
    const wchar_t *s = NULL;
    unsigned len = 0;
    bool isPtrName = false;

   #if WCHAR_PATH_SEPARATOR != L'/'
    wchar_t separatorChar = WCHAR_PATH_SEPARATOR;
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
        separatorChar = L'/';  // 0
       #endif
      }
    }
    if (!s)
    #endif
    {
      len = 0;
      prop.Clear();
      RINOK(arc.Archive->GetProperty(i, kpidPath, &prop))
      if (prop.vt == VT_BSTR)
      {
        s = prop.bstrVal;
        len = ::SysStringLen(prop.bstrVal);
      }
      else if (prop.vt != VT_EMPTY)
        return E_FAIL;
      if (len == 0)
      {
        RINOK(arc.GetItem_DefaultPath(i, path))
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
    bool is_Changed_LongPath = false;
    bool isDir;
    RINOK(Archive_IsItem_Dir(archive, i, isDir))

    if (len >= (1 << 15))
    {
      Are_Changed_LongPaths = true;
      is_Changed_LongPath = true;
      SetPath_as_LONG_PATH(path, i, isDir);
      s = path;
      len = path.Len();
      isPtrName = false;
    }
    else for (unsigned j = 0; j < len; j++)
    {
      const wchar_t c = s[j];
      if (c == L'/'
        #if WCHAR_PATH_SEPARATOR != L'/'
          || (c == separatorChar)
        #endif
          )
      {
        const unsigned kLevelLimit = 1 << 10;
        if (numLevels <= kLevelLimit)
        {
          if (numLevels == kLevelLimit)
          {
            name = "[LONG_PATH]";
            Are_Changed_LongPaths = true;
            is_Changed_LongPath = true;
          }
          else
            name.SetFrom(s + namePos, j - namePos);
          // 22.00: we can normalize dir here
          // NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(name);
          curItem = AddDir(curItem, -1, name);
          if (is_Changed_LongPath)
            Dirs[curItem].Is_Changed_LongPath = true;
        }
        namePos = j + 1;
        numLevels++;
      }
    }

    /*
    that code must be implemented to hide alt streams in list.
    if (arc.Ask_AltStreams)
    {
      bool isAltStream;
      RINOK(Archive_IsItem_AltStream(archive, i, isAltStream));
      if (isAltStream){}
    }
    */
    CProxyFile &f = Files[i];
    f.Construct(); // optional because memset() in code above
    f.NameLen = len - namePos;
    s += namePos;

    if (isPtrName)
      f.Name = s;
    else
    {
      MemUsage += 16 + (size_t)f.NameLen * sizeof(*f.Name);
      f.Name = AllocStringAndCopy(s, f.NameLen);
      f.NeedDeleteName = true;
    }

    if (isDir)
    {
      name = s;
      // 22.00: we can normalize dir here
      // NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(name);
      curItem = AddDir(curItem, (int)i, name);
      if (is_Changed_LongPath)
        Dirs[curItem].Is_Changed_LongPath = true;
    }
    else
    {
      MemUsage += Dirs[curItem].SubFiles.IsEmpty() ? 16u : 4u + 1;
      Dirs[curItem].SubFiles.Add(i);
    }
  }

  FOR_VECTOR (k, Dirs)
  {
    MergeSubDirs(Dirs[k]);
    Dirs[k].SubDirs2.ClearAndFree(); // these arryas are small
  }

  CalculateSizes(archive);
  if (progress)
  {
    const UInt64 numItems64 = numItems;
    RINOK(progress->SetCompleted(&numItems64, NULL))
  }
  // } char s[128]; sprintf(s, "Load archive: %7d ms", GetTickCount() - tickCount); OutputDebugStringA(s);
  return S_OK;
}



// ---------- for Tree-mode archive ----------


void SetDirPrefix_as_LONG_PATH(UString &s, unsigned fileIndex,
    const bool asAltDirPrefix, const bool isDir)
{
  SetPath_as_LONG_PATH(s, fileIndex, isDir);
  s.Add_Char(asAltDirPrefix ? ':' : CHAR_PATH_SEPARATOR);
}


bool CProxyArc2::IsAltDir(const unsigned dirIndex) const
{
  if (dirIndex == k_Proxy2_RootDirIndex)
    return false;
  if (dirIndex == k_Proxy2_AltRootDirIndex)
    return true;
  const CProxyDir2 &dir = Dirs[dirIndex];
  if ((unsigned)dir.ArcIndex >= NumFiles) throw 1; // optional
  return (int)dirIndex == Files[(unsigned)dir.ArcIndex].AltDirIndex;
}


/* called from:
    Agent.cpp:
        CAgentFolder::Extract()
    ArchiveFolderOut.cpp:
        CAgentFolder::GetPathParts()
            CAgentFolder::CommonUpdateOperation()
*/
void CProxyArc2::GetDirPathParts(const unsigned dirIndex, UStringVector &pathParts, bool &isAltStreamDir) const
{
  pathParts.Clear();
  isAltStreamDir = false;
  if (dirIndex == k_Proxy2_RootDirIndex)
    return;
  if (dirIndex == k_Proxy2_AltRootDirIndex)
  {
    isAltStreamDir = true;
    return;
  }
  int fileIndex = Dirs[dirIndex].ArcIndex;
  do
  {
    if ((unsigned)fileIndex >= NumFiles) throw 1; // optional
    const CProxyFile2 &file = Files[(unsigned)fileIndex];
    // if (pathParts.Size() == 0) // optional
    if ((int)dirIndex == file.AltDirIndex)
      isAltStreamDir = true;
    // that code is not optimized for big number of loop iterations.
    pathParts.Insert(0, file.Name);
    fileIndex = file.Parent;
  }
  while (fileIndex != -1);
}


// if Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX is defined,
//   { this code is unused mostly. 7-Zip gets prefix via GetItemPrefix(). }
void CProxyArc2::GetDirPath_as_Prefix_from_Base(const unsigned dirIndex,
    UString &prefix, const unsigned baseDirIndex, const unsigned lenLimit) const
{
  prefix.Empty();
  if (dirIndex == baseDirIndex)
    return;
  if (dirIndex == k_Proxy2_AltRootDirIndex)
  {
    // (baseDirIndex == k_Proxy2_RootDirIndex) is expected
    prefix.Add_Colon();
    return;
  }

  const CProxyDir2 &dir = Dirs[dirIndex];
  int arcIndex = dir.ArcIndex;
  if (arcIndex != -1)
  {
    const CProxyDir2 &baseDir = Dirs[baseDirIndex];
    unsigned len = dir.PrefixLen - baseDir.PrefixLen;
    if (len >= lenLimit)
    {
      const CProxyFile2 &file = Files[(unsigned)arcIndex];
      SetDirPrefix_as_LONG_PATH(prefix, (unsigned)arcIndex,
          (int)dirIndex == file.AltDirIndex, // asAltDir,
          file.IsDir());
      return;
    }
    const int baseArcIndex = baseDir.ArcIndex;
    wchar_t * const p = prefix.GetBuf_SetEnd(len);
    do
    {
      const CProxyFile2 &file = Files[(unsigned)arcIndex];
      wchar_t c = CHAR_PATH_SEPARATOR;
      if ((int)dirIndex == file.AltDirIndex)
        c = ':';
      if (len <= file.NameLen)
        break;
      p[--len] = c;
      len -= file.NameLen;
      wmemcpy(p + len, file.Name, file.NameLen);
      // slash normalization can be omitted, if we want max speed of sorting operation:
      NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(p + len, file.NameLen);
      arcIndex = file.Parent;
      if (arcIndex == baseArcIndex)
      {
        if (len)
          break;
        return;
      }
    }
    while (arcIndex != -1);
  }
  throw 1;
}


/* called from:
  Agent.cpp:
    CAgentFolder::GetFullPrefix()
      CAgent::RenameItem() in AgentOut.cpp
    CAgentFolder::GetFolderProperty(kpidPath)
      GetFolderPath() in PanelFolderChange.cpp
        LoadFullPath()
  AgentOut.cpp:
    CAgent::SetFolder()
      CAgentFolder::CommonUpdateOperation() in ArchiveFolderOut.cpp
    CAgent::CreateFolder()
*/
UString CProxyArc2::GetDirPath_as_Prefix(const unsigned dirIndex, const bool canReducePath) const
{
  UString s;
  const unsigned lenLimit = canReducePath ? (1u << 14) : (1u << 28);
  GetDirPath_as_Prefix_from_Base(dirIndex, s, k_Proxy2_RootDirIndex, lenLimit);
  return s;
}


void CProxyArc2::AddRealIndices_of_ArcItem(const unsigned arcIndex,
    const bool includeAltStreams, const bool includeDirSubItems,
    CUIntVector &realIndices) const
{
  const CProxyFile2 &file = Files[arcIndex];
  if (!file.IsAux)
    realIndices.Add(arcIndex);
  if (includeDirSubItems && file.DirIndex != -1)
    AddRealIndices_of_Dir((unsigned)file.DirIndex, includeAltStreams, realIndices);
  if (includeAltStreams && file.AltDirIndex != -1)
    AddRealIndices_of_Dir((unsigned)file.AltDirIndex, includeAltStreams, realIndices);
}


void CProxyArc2::AddRealIndices_of_Dir(unsigned dirIndex,
    const bool includeAltStreams,
    CUIntVector &realIndices) const
{
  CUIntVector vec;
  unsigned i = 0;
  for (;;)
  {
    const CProxyDir2 &dir = Dirs[dirIndex];
    if (i == dir.Items.Size())
    {
      const unsigned num = vec.Size();
      if (num < 2)
        return;
      dirIndex = vec[num - 2];
      i = vec[num - 1];
      vec.DeleteFrom(num - 2);
      continue;
    }

    const unsigned arcIndex = dir.Items[i];
    i++;
    const CProxyFile2 &file = Files[arcIndex];
    if (!file.IsAux)
      realIndices.Add(arcIndex);
    if (includeAltStreams && file.AltDirIndex != -1)
      AddRealIndices_of_Dir((unsigned)file.AltDirIndex, includeAltStreams, realIndices);
    if (file.DirIndex != -1)
    {
      vec.Add(dirIndex);
      vec.Add(i);
      dirIndex = (unsigned)file.DirIndex;
      i = 0;
    }
  }
}


void CProxyArc2::GetRealIndices_Unsorted(const unsigned dirIndex,
    const UInt32 *indices, const UInt32 numItems,
    const bool includeAltStreams,
    CUIntVector &realIndices) const
{
  const CUIntVector &items = Dirs[dirIndex].Items;
  realIndices.Clear();
  for (UInt32 i = 0; i < numItems; i++)
    AddRealIndices_of_ArcItem(items[indices[i]],
        includeAltStreams,
        true, // includeDirSubItems
        realIndices);
  // HeapSort(realIndices.NonConstData(), realIndices.Size());
}


CProxyDir2::CProxyDir2():
  ArcIndex(-1),
  PrefixLen(0),
  Size(0),
  PackSize(0),
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
  IsLongPath(false),
#endif
  CrcIsDefined(true),
  Crc(0),
  NumSubDirs(0),
  NumSubFiles(0)
{}


HRESULT CProxyArc2::CalculateSizes(unsigned dirIndex, IInArchive *archive
    , IProgress *progress, const UInt64 *completed, unsigned &progressCounter)
{
  CUIntVector vec;
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
  UString prefix, name;
#endif
  unsigned i = 0;
  for (;;)
  {
    CProxyDir2 &dir = Dirs[dirIndex];
    // if (i == 0) {} // we can init some (dir) variables here instead of constructor

    if (i == dir.Items.Size())
    {
      const unsigned num = vec.Size();
      if (num < 2)
        return S_OK;
      dirIndex = vec[num - 2];
      i = vec[num - 1];
      vec.DeleteFrom(num - 2);

      CProxyDir2 &dir2 = Dirs[dirIndex];
      dir2.Size += dir.Size;
      dir2.PackSize += dir.PackSize;
      dir2.NumSubFiles += dir.NumSubFiles;
      dir2.NumSubDirs += dir.NumSubDirs;
      dir2.Crc += dir.Crc;
      if (!dir.CrcIsDefined)
        dir2.CrcIsDefined = false;
      continue;
    }

    if (progress && (++progressCounter & 0xfffff) == 0)
      RINOK(progress->SetCompleted(completed))

    const UInt32 index = dir.Items[i];
    i++;
    UInt64 size, packSize;
    const bool sizeDefined = GetSize(archive, index, kpidSize, size);
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

    if (subFile.NameLen >= (1u << 30) - dir.PrefixLen)
      return E_NOTIMPL;
    const unsigned prefixLen = dir.PrefixLen + subFile.NameLen + 1;

#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
    bool isLongPath = dir.IsLongPath;
    if (subFile.AltDirIndex != -1 || subFile.DirIndex != -1)
    {
      // 22.00: we normalize name
      name = subFile.Name;
      NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(name);
      if (!isLongPath)
      {
        prefix = dir.PathPrefix;
        prefix += name;
        if (prefix.Len() >= (1u << 11)) // CProxyDir2::PathPrefix limit
          isLongPath = true;
      }
    }
#endif

    if (subFile.AltDirIndex != -1)
    {
      CProxyDir2 &f = Dirs[subFile.AltDirIndex];
      f.PrefixLen = prefixLen;
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
      f.IsLongPath = isLongPath;
      if (isLongPath)
        SetDirPrefix_as_LONG_PATH(prefix, index,
            true, // asAltDirPrefix
            subFile.IsDir());
      else
        prefix.Add_Colon();
      f.PathPrefix = prefix;
      UpdateMemUsage_with_StringLen(f.PathPrefix.Len());
      CHECK_MEM_USAGE
      prefix.DeleteBack(); // we delete Colon character
#endif
      RINOK(CalculateSizes((unsigned)subFile.AltDirIndex, archive, progress, completed, progressCounter))
    }

    if (subFile.DirIndex == -1)
      dir.NumSubFiles++;
    else
    {
      CProxyDir2 &f = Dirs[subFile.DirIndex];
      f.PrefixLen = prefixLen;
      dir.NumSubDirs++;
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
      f.IsLongPath = isLongPath;
      if (isLongPath)
        SetDirPrefix_as_LONG_PATH(prefix, index,
            false, // asAltDirPrefix
            true); // isDir
      else
        prefix.Add_PathSepar();
      f.PathPrefix = prefix;
      UpdateMemUsage_with_StringLen(f.PathPrefix.Len());
      CHECK_MEM_USAGE
#endif
      if (vec.Size() >= (1u << 28)) // tree levels limit
        return E_NOTIMPL;
      vec.Add(dirIndex);
      vec.Add(i);
      dirIndex = (unsigned)subFile.DirIndex;
      i = 0;
    }
  }
}


void CProxyArc2::FreeFiles()
{
  const unsigned numFiles = NumFiles;
  NumFiles = 0;
  CProxyFile2 *f = Files;
  for (unsigned i = 0; i < numFiles; i++, f++)
    if (f->NeedDeleteName)
      delete [](wchar_t *)(void *)f->Name;
  delete []Files;
  Files = NULL;
}


void CProxyArc2::AddDir(int arcIndex)
{
  Dirs.AddNew().ArcIndex = arcIndex;
  MemUsage += sizeof(Dirs[0]) + sizeof(void *) + 11;
}

HRESULT CProxyArc2::Load(const CArc &arc, IProgress *progress)
{
  if (!arc.GetRawProps)
    return E_FAIL;
  // DWORD tickCount = GetTickCount(); for (int ttt = 0; ttt < 1; ttt++) {

  Dirs.Clear();
  FreeFiles();

  IInArchive *archive = arc.Archive;
  UInt32 numItems;
  RINOK(archive->GetNumberOfItems(&numItems))
  if (numItems > k_NumFiles_Max)
    return E_OUTOFMEMORY;
  if (progress)
    RINOK(progress->SetTotal(numItems))
  Dirs.AddNew(); // Dirs[0] - root dir
  Dirs.AddNew()  // Dirs[1] - for alt streams of root dir
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
    .PathPrefix.Add_Colon() // = ':';
#endif
  ;

  MemUsage = (UInt64)numItems * sizeof(*Files);
  CHECK_MEM_USAGE
  Z7_ARRAY_NEW(Files, CProxyFile2, numItems)
  memset(Files, 0, (size_t)numItems * sizeof(*Files));
  NumFiles = numItems;

  UString fileName;
  UString tempUString;
  AString tempAString;

  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    CHECK_MEM_USAGE
    if (progress && (i & 0xFFFFF) == 0)
    {
      const UInt64 currentItemIndex = i;
      RINOK(progress->SetCompleted(&currentItemIndex))
    }
    CProxyFile2 &file = Files[i];
    file.Construct();

    const void *p;
    UInt32 size;
    UInt32 propType;
    RINOK(arc.GetRawProps->GetRawProp(i, kpidName, &p, &size, &propType))

#ifdef MY_CPU_LE
    if (p && propType == PROP_DATA_TYPE_wchar_t_PTR_Z_LE)
    {
      file.Name = (const wchar_t *)p;
      file.NameLen = 0;
      if (size >= sizeof(wchar_t))
        file.NameLen = size / (unsigned)sizeof(wchar_t) - 1;
    }
    else
#endif
    {
      const wchar_t *s;
      unsigned len;
      if (p && propType == NPropDataType::kUtf8z)
      {
        tempAString = (const char *)p;
        ConvertUTF8ToUnicode(tempAString, tempUString);
        s = tempUString.Ptr();
        len = tempUString.Len();
      }
      else
      {
        NCOM::CPropVariant prop;
        RINOK(arc.Archive->GetProperty(i, kpidName, &prop))
        if (prop.vt == VT_BSTR)
          s = prop.bstrVal;
        else if (prop.vt == VT_EMPTY)
          s = L"[Content]";
        else
          return E_FAIL;
        len = MyStringLen(s);
      }
      file.NameLen = len;
      file.Name = AllocStringAndCopy(s, len);
      UpdateMemUsage_with_StringLen(len);
      file.NeedDeleteName = true;
    }

    UInt32 parent = (UInt32)(Int32)-1;
    UInt32 parentType = 0;
    RINOK(arc.GetRawProps->GetParent(i, &parent, &parentType))
    if (parent >= numItems && parent != (UInt32)(Int32)-1)
      return E_FAIL;
    file.Parent = (Int32)parent;
    /*
    if (arc.Ask_Deleted)
    {
      bool isDeleted = false;
      RINOK(Archive_IsItem_Deleted(archive, i, isDeleted))
      if (isDeleted) {}
    }
    */
    bool isDir;
    RINOK(Archive_IsItem_Dir(archive, i, isDir))
    if (isDir)
    {
      file.DirIndex = (int)Dirs.Size();
      AddDir((int)i); // arcIndex
    }
    if (arc.Ask_AltStream)
      RINOK(Archive_IsItem_AltStream(archive, i, file.IsAltStream))
    // if (file.IsAltStream) file.Parent = -1; // for debug
#if 1 // 1 to use AUX: v24.06 : we will ignore AUX items for AddRealIndices_of_ArcItem() : for CRC calculation.
    if (arc.Ask_Aux)
      RINOK(Archive_IsItem_Aux(archive, i, file.IsAux))
#endif
  }

  for (i = 0; i < numItems; i++)
  {
    CProxyFile2 &file = Files[i];
    int dirIndex;

    if (file.IsAltStream)
    {
      if (file.Parent == -1)
        dirIndex = k_Proxy2_AltRootDirIndex;
      else
      {
        int &folderIndex2 = Files[(unsigned)file.Parent].AltDirIndex;
        if (folderIndex2 == -1)
        {
          folderIndex2 = (int)Dirs.Size();
          AddDir(file.Parent); // arcIndex
        }
        dirIndex = folderIndex2;
      }
    }
    else
    {
      if (file.Parent == -1)
        dirIndex = k_Proxy2_RootDirIndex;
      else
      {
        dirIndex = Files[(unsigned)file.Parent].DirIndex;
        if (dirIndex == -1)
          return E_FAIL;
      }
    }

    MemUsage += Dirs[dirIndex].Items.IsEmpty() ? 16u : 4u + 1;
    CHECK_MEM_USAGE
    Dirs[dirIndex].Items.Add(i);
  }

  unsigned progressCounter = 0;
  for (i = 0; i < k_Proxy2_NumRootDirs; i++)
  {
    const UInt64 numItems64 = numItems;
    RINOK(CalculateSizes(i, archive, progress, &numItems64, progressCounter))
  }
  // OutputDebugStringA("finished");

  // } char s[128]; sprintf(s, "Load archive: %7d ms", GetTickCount() - tickCount); OutputDebugStringA(s);
  return S_OK;
}

int CProxyArc2::FindItem(const unsigned dirIndex, const wchar_t *name, bool foldersOnly) const
{
  int index = -1;
  const CUIntVector &subFiles = Dirs[dirIndex].Items;
  FOR_VECTOR (i, subFiles)
  {
    const CProxyFile2 &file = Files[subFiles[i]];
    if (foldersOnly && file.DirIndex == -1)
      continue;
    if (CompareFileNames(file.Name, name) != 0)
      continue;
    if (MyStringCompare(file.Name, name) == 0)
      return (int)i;
    if (index == -1)
      index = (int)i;
  }
  return index;
}
