// Agent.cpp

#include "StdAfx.h"

#include <wchar.h>

#include "../../../../C/Sort.h"

#include "../../../Common/ComTry.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariantConv.h"
#include "../../../Windows/System.h"

#ifndef Z7_ST
#include "../../../Windows/Synchronization.h"
#endif

#include "../../Archive/Common/ItemNameUtils.h"

#include "../Common/ArchiveExtractCallback.h"
#include "../FileManager/RegistryUtils.h"

#include "Agent.h"

using namespace NWindows;

CCodecs *g_CodecsObj;

static const bool k_keepEmptyDirPrefixes =
    false; // 22.00
    // true; // 21.07

#ifdef Z7_EXTERNAL_CODECS
  extern
  CExternalCodecs g_ExternalCodecs;
  CExternalCodecs g_ExternalCodecs;
  extern
  const CExternalCodecs *g_ExternalCodecs_Ptr;
  const CExternalCodecs *g_ExternalCodecs_Ptr;
  static CCodecs::CReleaser g_CodecsReleaser;
#else
  extern
  CMyComPtr<IUnknown> g_CodecsRef;
  CMyComPtr<IUnknown> g_CodecsRef;
#endif

#ifndef Z7_ST
static NSynchronization::CCriticalSection g_CriticalSection;
#define MT_LOCK NSynchronization::CCriticalSectionLock lock(g_CriticalSection);
#else
#define MT_LOCK
#endif

void FreeGlobalCodecs()
{
  MT_LOCK

  #ifdef Z7_EXTERNAL_CODECS
  if (g_CodecsObj)
  {
    g_CodecsObj->CloseLibs();
  }
  g_CodecsReleaser.Set(NULL);
  g_CodecsObj = NULL;
  g_ExternalCodecs.ClearAndRelease();
  g_ExternalCodecs_Ptr = NULL;
  #else
  g_CodecsRef.Release();
  #endif
}

HRESULT LoadGlobalCodecs()
{
  MT_LOCK

  if (g_CodecsObj)
    return S_OK;

  g_CodecsObj = new CCodecs;

  #ifdef Z7_EXTERNAL_CODECS
  g_ExternalCodecs.GetCodecs = g_CodecsObj;
  g_ExternalCodecs.GetHashers = g_CodecsObj;
  g_CodecsReleaser.Set(g_CodecsObj);
  #else
  g_CodecsRef.Release();
  g_CodecsRef = g_CodecsObj;
  #endif

  RINOK(g_CodecsObj->Load())
  if (g_CodecsObj->Formats.IsEmpty())
  {
    FreeGlobalCodecs();
    return E_NOTIMPL;
  }

  Codecs_AddHashArcHandler(g_CodecsObj);

  #ifdef Z7_EXTERNAL_CODECS
  RINOK(g_ExternalCodecs.Load())
  g_ExternalCodecs_Ptr = &g_ExternalCodecs;
  #endif

  return S_OK;
}

Z7_COM7F_IMF(CAgentFolder::GetAgentFolder(CAgentFolder **agentFolder))
{
  *agentFolder = this;
  return S_OK;
}

void CAgentFolder::LoadFolder(unsigned dirIndex)
{
  CProxyItem item;

  if (_proxy2)
  {
    CUIntVector vec;
    unsigned i = 0;
    for (;;)
    {
      const CProxyDir2 &dir = _proxy2->Dirs[dirIndex];
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

      item.DirIndex = dirIndex;
      item.Index = i;
      _items.Add(item);
      const unsigned index = dir.Items[i];
      i++;
      const CProxyFile2 &file = _proxy2->Files[index];
      if (_loadAltStreams && file.AltDirIndex != -1)
        LoadFolder((unsigned)file.AltDirIndex);
      if (file.DirIndex != -1)
      {
        vec.Add(dirIndex);
        vec.Add(i);
        dirIndex = (unsigned)file.DirIndex;
        i = 0;
      }
    }
  }

  item.DirIndex = dirIndex;
  const CProxyDir &dir = _proxy->Dirs[dirIndex];
  unsigned i;
  for (i = 0; i < dir.SubDirs.Size(); i++)
  {
    item.Index = i;
    _items.Add(item);
    LoadFolder(dir.SubDirs[i]);
  }

  const unsigned start = dir.SubDirs.Size();
  for (i = 0; i < dir.SubFiles.Size(); i++)
  {
    item.Index = start + i;
    _items.Add(item);
  }
}

Z7_COM7F_IMF(CAgentFolder::LoadItems())
{
  if (!_agentSpec->_archiveLink.IsOpen)
    return E_FAIL;
  _items.Clear();
  if (_flatMode)
  {
    LoadFolder(_proxyDirIndex);
    if (_proxy2 && _loadAltStreams)
    {
      if (_proxyDirIndex == k_Proxy2_RootDirIndex)
        LoadFolder(k_Proxy2_AltRootDirIndex);
    }
  }
  return S_OK;
}

Z7_COM7F_IMF(CAgentFolder::GetNumberOfItems(UInt32 *numItems))
{
  if (_flatMode)
    *numItems = _items.Size();
  else if (_proxy2)
    *numItems = _proxy2->Dirs[_proxyDirIndex].Items.Size();
  else
  {
    const CProxyDir *dir = &_proxy->Dirs[_proxyDirIndex];
    *numItems = dir->SubDirs.Size() + dir->SubFiles.Size();
  }
  return S_OK;
}

#define SET_realIndex_AND_dir \
  unsigned realIndex; const CProxyDir *dir; \
  if (_flatMode) { const CProxyItem &item = _items[index]; dir = &_proxy->Dirs[item.DirIndex]; realIndex = item.Index; } \
  else { dir = &_proxy->Dirs[_proxyDirIndex]; realIndex = index; }

#define SET_realIndex_AND_dir_2 \
  unsigned realIndex; const CProxyDir2 *dir; \
  if (_flatMode) { const CProxyItem &item = _items[index]; dir = &_proxy2->Dirs[item.DirIndex]; realIndex = item.Index; } \
  else { dir = &_proxy2->Dirs[_proxyDirIndex]; realIndex = index; }

UString CAgentFolder::GetName(UInt32 index) const
{
  if (_proxy2)
  {
    SET_realIndex_AND_dir_2
    return _proxy2->Files[dir->Items[realIndex]].Name;
  }
  SET_realIndex_AND_dir
  if (realIndex < dir->SubDirs.Size())
    return _proxy->Dirs[dir->SubDirs[realIndex]].Name;
  return _proxy->Files[dir->SubFiles[realIndex - dir->SubDirs.Size()]].Name;
}


/* called for (_flatMode == true) from:
    CAgentFolder::GetProperty(kpidPrefix)
    CAgentFolder::CompareItems(kpidPrefix)
*/
void CAgentFolder::GetPrefix(const UInt32 index, UString &prefix) const
{
  prefix.Empty();
  if (!_flatMode)
    return;
  const unsigned itemDirIndex = _items[index].DirIndex;
  if (itemDirIndex == _proxyDirIndex)
    return;
  if (_proxy2)
  {
    const unsigned kLenLimit = 1u << 12;
    _proxy2->GetDirPath_as_Prefix_from_Base(itemDirIndex, prefix, _proxyDirIndex, kLenLimit);
  }
  else
    _proxy->GetDirPath_as_Prefix_from_Base(itemDirIndex, prefix, _proxyDirIndex);
}

UString CAgentFolder::GetFullPrefix(const UInt32 index) const
{
  unsigned foldIndex = _proxyDirIndex;
  if (_flatMode)
    foldIndex = _items[index].DirIndex;
  if (_proxy2)
    return _proxy2->GetDirPath_as_Prefix(foldIndex);
  return _proxy->GetDirPath_as_Prefix(foldIndex);
}

Z7_COM7F_IMF2(UInt64, CAgentFolder::GetItemSize(const UInt32 index))
{
  unsigned arcIndex;
  if (_proxy2)
  {
    SET_realIndex_AND_dir_2
    arcIndex = dir->Items[realIndex];
    const CProxyFile2 &item = _proxy2->Files[arcIndex];
    if (item.IsDir())
    {
      const CProxyDir2 &itemFolder = _proxy2->Dirs[item.DirIndex];
      if (!_flatMode)
        return itemFolder.Size;
    }
  }
  else
  {
    SET_realIndex_AND_dir
    if (realIndex < dir->SubDirs.Size())
    {
      const CProxyDir &item = _proxy->Dirs[dir->SubDirs[realIndex]];
      if (!_flatMode)
        return item.Size;
      if (!item.IsLeaf())
        return 0;
      arcIndex = (unsigned)item.ArcIndex;
    }
    else
    {
      arcIndex = dir->SubFiles[realIndex - dir->SubDirs.Size()];
    }
  }
  NCOM::CPropVariant prop;
  _agentSpec->GetArchive()->GetProperty(arcIndex, kpidSize, &prop);
  if (prop.vt == VT_UI8)
    return prop.uhVal.QuadPart;
  else
    return 0;
}

Z7_COM7F_IMF(CAgentFolder::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  if (propID == kpidPrefix)
  {
    if (_flatMode)
    {
      UString prefix;
      GetPrefix(index, prefix);
      prop = prefix;
    }
  }
  else if (_proxy2)
  {
    SET_realIndex_AND_dir_2
    unsigned arcIndex = dir->Items[realIndex];
    const CProxyFile2 &item = _proxy2->Files[arcIndex];
    /*
    if (propID == kpidNumAltStreams)
    {
      if (item.AltDirIndex != -1)
        prop = _proxy2->Dirs[item.AltDirIndex].Items.Size();
    }
    else
    */
    if (!item.IsDir())
    {
      switch (propID)
      {
        case kpidIsDir: prop = false; break;
        case kpidName: prop = item.Name; break;
        default: return _agentSpec->GetArchive()->GetProperty(arcIndex, propID, value);
      }
    }
    else
    {
      const CProxyDir2 &itemFolder = _proxy2->Dirs[item.DirIndex];
      if (!_flatMode && propID == kpidSize)
        prop = itemFolder.Size;
      else if (!_flatMode && propID == kpidPackSize)
        prop = itemFolder.PackSize;
      else switch (propID)
      {
        case kpidIsDir: prop = true; break;
        case kpidNumSubDirs: prop = itemFolder.NumSubDirs; break;
        case kpidNumSubFiles: prop = itemFolder.NumSubFiles; break;
        case kpidName: prop = item.Name; break;
        case kpidCRC:
        {
          // if (itemFolder.IsLeaf)
          if (!item.Ignore)
          {
            RINOK(_agentSpec->GetArchive()->GetProperty(arcIndex, propID, value))
          }
          if (itemFolder.CrcIsDefined && value->vt == VT_EMPTY)
            prop = itemFolder.Crc;
          break;
        }
        default:
          // if (itemFolder.IsLeaf)
          if (!item.Ignore)
            return _agentSpec->GetArchive()->GetProperty(arcIndex, propID, value);
      }
    }
  }
  else
  {
  SET_realIndex_AND_dir
  if (realIndex < dir->SubDirs.Size())
  {
    const CProxyDir &item = _proxy->Dirs[dir->SubDirs[realIndex]];
    if (!_flatMode && propID == kpidSize)
      prop = item.Size;
    else if (!_flatMode && propID == kpidPackSize)
      prop = item.PackSize;
    else
    switch (propID)
    {
      case kpidIsDir: prop = true; break;
      case kpidNumSubDirs: prop = item.NumSubDirs; break;
      case kpidNumSubFiles: prop = item.NumSubFiles; break;
      case kpidName: prop = item.Name; break;
      case kpidCRC:
      {
        if (item.IsLeaf())
        {
          RINOK(_agentSpec->GetArchive()->GetProperty((unsigned)item.ArcIndex, propID, value))
        }
        if (item.CrcIsDefined && value->vt == VT_EMPTY)
          prop = item.Crc;
        break;
      }
      default:
        if (item.IsLeaf())
          return _agentSpec->GetArchive()->GetProperty((unsigned)item.ArcIndex, propID, value);
    }
  }
  else
  {
    unsigned arcIndex = dir->SubFiles[realIndex - dir->SubDirs.Size()];
    switch (propID)
    {
      case kpidIsDir: prop = false; break;
      case kpidName: prop = _proxy->Files[arcIndex].Name; break;
      default:
        return _agentSpec->GetArchive()->GetProperty(arcIndex, propID, value);
    }
  }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static UInt64 GetUInt64Prop(IInArchive *archive, UInt32 index, PROPID propID)
{
  NCOM::CPropVariant prop;
  if (archive->GetProperty(index, propID, &prop) != S_OK)
    throw 111233443;
  UInt64 v = 0;
  if (ConvertPropVariantToUInt64(prop, v))
    return v;
  return 0;
}

Z7_COM7F_IMF(CAgentFolder::GetItemName(UInt32 index, const wchar_t **name, unsigned *len))
{
  if (_proxy2)
  {
    SET_realIndex_AND_dir_2
    unsigned arcIndex = dir->Items[realIndex];
    const CProxyFile2 &item = _proxy2->Files[arcIndex];
    *name = item.Name;
    *len = item.NameLen;
    return S_OK;
  }
  else
  {
    SET_realIndex_AND_dir
    if (realIndex < dir->SubDirs.Size())
    {
      const CProxyDir &item = _proxy->Dirs[dir->SubDirs[realIndex]];
      *name = item.Name;
      *len = item.NameLen;
      return S_OK;
    }
    else
    {
      const CProxyFile &item = _proxy->Files[dir->SubFiles[realIndex - dir->SubDirs.Size()]];
      *name = item.Name;
      *len = item.NameLen;
      return S_OK;
    }
  }
}


/* called for (_flatMode == true) from:
    CPanel::RefreshListCtrl() : to check selected items and focused item.
    CPanel::SetItemText() : to show prefix column
*/
Z7_COM7F_IMF(CAgentFolder::GetItemPrefix(UInt32 index, const wchar_t **name, unsigned *len))
{
  *name = NULL;
  *len = 0;
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
  if (!_flatMode)
    return S_OK;

  if (_proxy2)
  {
    const CProxyItem &item = _items[index];
    const CProxyDir2 &dir = _proxy2->Dirs[item.DirIndex];
    const CProxyDir2 &baseDir = _proxy2->Dirs[_proxyDirIndex];
    const UString &s = dir.PathPrefix;
    if (dir.IsLongPath)
    {
      // in case of IsLongPath we can return reduced prefix or return nothing.
      // so caller will call it with full prefix.
#if 1 // 1 - for fast processing of long strings, 0 - for full processing
      *name = (const wchar_t *)s;
      *len = s.Len();
#endif
    }
    else
    {
      if (baseDir.IsLongPath)
        return E_FAIL; // throw 1;
      const unsigned baseLen = baseDir.PathPrefix.Len();
      if (s.Len() < baseLen)
        return E_FAIL; // throw 1;
      *name = (const wchar_t *)s + baseLen;
      *len = s.Len() - baseLen;
    }
  }
#else
  UNUSED_VAR(index)
#endif
  return S_OK;
}

static int CompareRawProps(IArchiveGetRawProps *rawProps, unsigned arcIndex1, unsigned arcIndex2, PROPID propID)
{
  // if (propID == kpidSha1)
  if (rawProps)
  {
    const void *p1, *p2;
    UInt32 size1, size2;
    UInt32 propType1, propType2;
    const HRESULT res1 = rawProps->GetRawProp(arcIndex1, propID, &p1, &size1, &propType1);
    const HRESULT res2 = rawProps->GetRawProp(arcIndex2, propID, &p2, &size2, &propType2);
    if (res1 == S_OK && res2 == S_OK)
    {
      for (UInt32 i = 0; i < size1 && i < size2; i++)
      {
        const Byte b1 = ((const Byte *)p1)[i];
        const Byte b2 = ((const Byte *)p2)[i];
        if (b1 < b2) return -1;
        if (b1 > b2) return 1;
      }
      if (size1 < size2) return -1;
      if (size1 > size2) return 1;
      return 0;
    }
  }
  return 0;
}

// returns pointer to extension including '.'

static const wchar_t *GetExtension(const wchar_t *name Z7_lifetimebound)
{
  for (const wchar_t *dotPtr = NULL;; name++)
  {
    wchar_t c = *name;
    if (c == 0)
      return dotPtr ? dotPtr : name;
    if (c == '.')
      dotPtr = name;
  }
}


int CAgentFolder::CompareItems3(UInt32 index1, UInt32 index2, PROPID propID)
{
  NCOM::CPropVariant prop1, prop2;
  // Name must be first property
  GetProperty(index1, propID, &prop1);
  GetProperty(index2, propID, &prop2);
  if (prop1.vt != prop2.vt)
    return MyCompare(prop1.vt, prop2.vt);
  if (prop1.vt == VT_BSTR)
    return MyStringCompareNoCase(prop1.bstrVal, prop2.bstrVal);
  return prop1.Compare(prop2);
}

int CAgentFolder::ComparePrefixes(const UInt32 index1, const UInt32 index2)
{
  // we use CAgentFolder::_temp1 instead of local vaiables for faster execution.
  // it's not allowed to call ComparePrefixes() from different threads simultaneously.
  // UString _temp1, _temp2;
  GetPrefix(index1, _temp1);
  GetPrefix(index2, _temp2);
  return CompareFileNames_ForFolderList(_temp1, _temp2);
}

int CAgentFolder::CompareItems2(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw)
{
  unsigned realIndex1, realIndex2;
  const CProxyDir2 *dir1, *dir2;

  if (_flatMode)
  {
    const CProxyItem &item1 = _items[index1];
    const CProxyItem &item2 = _items[index2];
    dir1 = &_proxy2->Dirs[item1.DirIndex];
    dir2 = &_proxy2->Dirs[item2.DirIndex];
    realIndex1 = item1.Index;
    realIndex2 = item2.Index;
  }
  else
  {
    dir2 = dir1 = &_proxy2->Dirs[_proxyDirIndex];
    realIndex1 = index1;
    realIndex2 = index2;
  }

  UInt32 arcIndex1;
  UInt32 arcIndex2;
  bool isDir1, isDir2;
  arcIndex1 = dir1->Items[realIndex1];
  arcIndex2 = dir2->Items[realIndex2];
  const CProxyFile2 &prox1 = _proxy2->Files[arcIndex1];
  const CProxyFile2 &prox2 = _proxy2->Files[arcIndex2];

  if (propID == kpidName)
  {
    return CompareFileNames_ForFolderList(prox1.Name, prox2.Name);
  }

  if (propID == kpidPrefix)
  {
    if (!_flatMode)
      return 0;
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
    return CompareFileNames_ForFolderList(
        _proxy2->Dirs[_items[index1].DirIndex].PathPrefix,
        _proxy2->Dirs[_items[index2].DirIndex].PathPrefix);
#else
    return ComparePrefixes(index1, index2);
#endif
  }

  if (propID == kpidExtension)
  {
     return CompareFileNames_ForFolderList(
         GetExtension(prox1.Name),
         GetExtension(prox2.Name));
  }

  isDir1 = prox1.IsDir();
  isDir2 = prox2.IsDir();

  if (propID == kpidIsDir)
  {
    if (isDir1 == isDir2)
      return 0;
    return isDir1 ? -1 : 1;
  }

  const CProxyDir2 *proxFolder1 = NULL;
  const CProxyDir2 *proxFolder2 = NULL;
  if (isDir1) proxFolder1 = &_proxy2->Dirs[prox1.DirIndex];
  if (isDir2) proxFolder2 = &_proxy2->Dirs[prox2.DirIndex];

  if (propID == kpidNumSubDirs)
  {
    UInt32 n1 = 0;
    UInt32 n2 = 0;
    if (isDir1) n1 = proxFolder1->NumSubDirs;
    if (isDir2) n2 = proxFolder2->NumSubDirs;
    return MyCompare(n1, n2);
  }

  if (propID == kpidNumSubFiles)
  {
    UInt32 n1 = 0;
    UInt32 n2 = 0;
    if (isDir1) n1 = proxFolder1->NumSubFiles;
    if (isDir2) n2 = proxFolder2->NumSubFiles;
    return MyCompare(n1, n2);
  }

  if (propID == kpidSize)
  {
    UInt64 n1, n2;
    if (isDir1)
      n1 = _flatMode ? 0 : proxFolder1->Size;
    else
      n1 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex1, kpidSize);
    if (isDir2)
      n2 = _flatMode ? 0 : proxFolder2->Size;
    else
      n2 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex2, kpidSize);
    return MyCompare(n1, n2);
  }

  if (propID == kpidPackSize)
  {
    UInt64 n1, n2;
    if (isDir1)
      n1 = _flatMode ? 0 : proxFolder1->PackSize;
    else
      n1 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex1, kpidPackSize);
    if (isDir2)
      n2 = _flatMode ? 0 : proxFolder2->PackSize;
    else
      n2 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex2, kpidPackSize);
    return MyCompare(n1, n2);
  }

  if (propID == kpidCRC)
  {
    UInt64 n1, n2;
    if (!isDir1 || !prox1.Ignore)
      n1 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex1, kpidCRC);
    else
      n1 = proxFolder1->Crc;
    if (!isDir2 || !prox2.Ignore)
      n2 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex2, kpidCRC);
    else
      n2 = proxFolder2->Crc;
    return MyCompare(n1, n2);
  }

  if (propIsRaw)
    return CompareRawProps(_agentSpec->_archiveLink.GetArchiveGetRawProps(), arcIndex1, arcIndex2, propID);

  return CompareItems3(index1, index2, propID);
}


Z7_COM7F_IMF2(Int32, CAgentFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw))
{
  try {
  if (_proxy2)
    return CompareItems2(index1, index2, propID, propIsRaw);

  unsigned realIndex1, realIndex2;
  const CProxyDir *dir1, *dir2;

  if (_flatMode)
  {
    const CProxyItem &item1 = _items[index1];
    const CProxyItem &item2 = _items[index2];
    dir1 = &_proxy->Dirs[item1.DirIndex];
    dir2 = &_proxy->Dirs[item2.DirIndex];
    realIndex1 = item1.Index;
    realIndex2 = item2.Index;
  }
  else
  {
    dir2 = dir1 = &_proxy->Dirs[_proxyDirIndex];
    realIndex1 = index1;
    realIndex2 = index2;
  }

  if (propID == kpidPrefix)
  {
    if (!_flatMode)
      return 0;
    return ComparePrefixes(index1, index2);
  }

  UInt32 arcIndex1;
  UInt32 arcIndex2;

  const CProxyDir *proxFolder1 = NULL;
  const CProxyDir *proxFolder2 = NULL;

  if (realIndex1 < dir1->SubDirs.Size())
  {
    proxFolder1 = &_proxy->Dirs[dir1->SubDirs[realIndex1]];
    arcIndex1 = (unsigned)proxFolder1->ArcIndex;
  }
  else
    arcIndex1 = dir1->SubFiles[realIndex1 - dir1->SubDirs.Size()];

  if (realIndex2 < dir2->SubDirs.Size())
  {
    proxFolder2 = &_proxy->Dirs[dir2->SubDirs[realIndex2]];
    arcIndex2 = (unsigned)proxFolder2->ArcIndex;
  }
  else
    arcIndex2 = dir2->SubFiles[realIndex2 - dir2->SubDirs.Size()];

  if (propID == kpidName)
    return CompareFileNames_ForFolderList(
        proxFolder1 ? proxFolder1->Name : _proxy->Files[arcIndex1].Name,
        proxFolder2 ? proxFolder2->Name : _proxy->Files[arcIndex2].Name);

  if (propID == kpidExtension)
    return CompareFileNames_ForFolderList(
       GetExtension(proxFolder1 ? proxFolder1->Name : _proxy->Files[arcIndex1].Name),
       GetExtension(proxFolder2 ? proxFolder2->Name : _proxy->Files[arcIndex2].Name));

  if (propID == kpidIsDir)
  {
    if (proxFolder1)
      return proxFolder2 ? 0 : -1;
    return proxFolder2 ? 1 : 0;
  }

  if (propID == kpidNumSubDirs)
  {
    UInt32 n1 = 0;
    UInt32 n2 = 0;
    if (proxFolder1) n1 = proxFolder1->NumSubDirs;
    if (proxFolder2) n2 = proxFolder2->NumSubDirs;
    return MyCompare(n1, n2);
  }

  if (propID == kpidNumSubFiles)
  {
    UInt32 n1 = 0;
    UInt32 n2 = 0;
    if (proxFolder1) n1 = proxFolder1->NumSubFiles;
    if (proxFolder2) n2 = proxFolder2->NumSubFiles;
    return MyCompare(n1, n2);
  }

  if (propID == kpidSize)
  {
    UInt64 n1, n2;
    if (proxFolder1)
      n1 = _flatMode ? 0 : proxFolder1->Size;
    else
      n1 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex1, kpidSize);
    if (proxFolder2)
      n2 = _flatMode ? 0 : proxFolder2->Size;
    else
      n2 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex2, kpidSize);
    return MyCompare(n1, n2);
  }

  if (propID == kpidPackSize)
  {
    UInt64 n1, n2;
    if (proxFolder1)
      n1 = _flatMode ? 0 : proxFolder1->PackSize;
    else
      n1 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex1, kpidPackSize);
    if (proxFolder2)
      n2 = _flatMode ? 0 : proxFolder2->PackSize;
    else
      n2 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex2, kpidPackSize);
    return MyCompare(n1, n2);
  }

  if (propID == kpidCRC)
  {
    UInt64 n1, n2;
    if (proxFolder1 && !proxFolder1->IsLeaf())
      n1 = proxFolder1->Crc;
    else
      n1 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex1, kpidCRC);
    if (proxFolder2 && !proxFolder2->IsLeaf())
      n2 = proxFolder2->Crc;
    else
      n2 = GetUInt64Prop(_agentSpec->GetArchive(), arcIndex2, kpidCRC);
    return MyCompare(n1, n2);
  }

  if (propIsRaw)
  {
    bool isVirt1 = (proxFolder1 && !proxFolder1->IsLeaf());
    bool isVirt2 = (proxFolder2 && !proxFolder2->IsLeaf());
    if (isVirt1)
      return isVirt2 ? 0 : -1;
    if (isVirt2)
      return 1;
    return CompareRawProps(_agentSpec->_archiveLink.GetArchiveGetRawProps(), arcIndex1, arcIndex2, propID);
  }

  return CompareItems3(index1, index2, propID);

  } catch(...) { return 0; }
}


HRESULT CAgentFolder::BindToFolder_Internal(unsigned proxyDirIndex, IFolderFolder **resultFolder)
{
  /*
  CMyComPtr<IFolderFolder> parentFolder;

  if (_proxy2)
  {
    const CProxyDir2 &dir = _proxy2->Dirs[proxyDirIndex];
    int par = _proxy2->GetParentFolderOfFile(dir.ArcIndex);
    if (par != (int)_proxyDirIndex)
    {
      RINOK(BindToFolder_Internal(par, &parentFolder));
    }
    else
      parentFolder = this;
  }
  else
  {
    const CProxyDir &dir = _proxy->Dirs[proxyDirIndex];
    if (dir.Parent != (int)_proxyDirIndex)
    {
      RINOK(BindToFolder_Internal(dir.Parent, &parentFolder));
    }
    else
      parentFolder = this;
  }
  */
  CAgentFolder *folderSpec = new CAgentFolder;
  CMyComPtr<IFolderFolder> agentFolder = folderSpec;
  folderSpec->Init(_proxy, _proxy2, proxyDirIndex, /* parentFolder, */ _agentSpec);
  *resultFolder = agentFolder.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CAgentFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  if (_proxy2)
  {
    SET_realIndex_AND_dir_2
    const unsigned arcIndex = dir->Items[realIndex];
    const CProxyFile2 &item = _proxy2->Files[arcIndex];
    if (!item.IsDir())
      return E_INVALIDARG;
    return BindToFolder_Internal((unsigned)item.DirIndex, resultFolder);
  }
  SET_realIndex_AND_dir
  if (realIndex >= (UInt32)dir->SubDirs.Size())
    return E_INVALIDARG;
  return BindToFolder_Internal(dir->SubDirs[realIndex], resultFolder);
  COM_TRY_END
}

Z7_COM7F_IMF(CAgentFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  if (_proxy2)
  {
    const int index = _proxy2->FindItem(_proxyDirIndex, name, true);
    if (index == -1)
      return E_INVALIDARG;
    return BindToFolder_Internal((unsigned)_proxy2->Files[_proxy2->Dirs[_proxyDirIndex].Items[index]].DirIndex, resultFolder);
  }
  const int index = _proxy->FindSubDir(_proxyDirIndex, name);
  if (index == -1)
    return E_INVALIDARG;
  return BindToFolder_Internal((unsigned)index, resultFolder);
  COM_TRY_END
}



// ---------- IFolderAltStreams ----------

HRESULT CAgentFolder::BindToAltStreams_Internal(unsigned proxyDirIndex, IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  if (!_proxy2)
    return S_OK;

  /*
  CMyComPtr<IFolderFolder> parentFolder;

  int par = _proxy2->GetParentFolderOfFile(_proxy2->Dirs[proxyDirIndex].ArcIndex);
  if (par != (int)_proxyDirIndex)
  {
    RINOK(BindToFolder_Internal(par, &parentFolder));
    if (!parentFolder)
      return S_OK;
  }
  else
    parentFolder = this;
  */

  CAgentFolder *folderSpec = new CAgentFolder;
  CMyComPtr<IFolderFolder> agentFolder = folderSpec;
  folderSpec->Init(_proxy, _proxy2, proxyDirIndex, /* parentFolder, */ _agentSpec);
  *resultFolder = agentFolder.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CAgentFolder::BindToAltStreams(UInt32 index, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN

  *resultFolder = NULL;

  if (!_proxy2)
    return S_OK;

  if (_proxy2->IsAltDir(_proxyDirIndex))
    return S_OK;

  {
    if (index == (UInt32)(Int32)-1)
    {
      unsigned altDirIndex;
      // IFolderFolder *parentFolder;

      if (_proxyDirIndex == k_Proxy2_RootDirIndex)
      {
        altDirIndex = k_Proxy2_AltRootDirIndex;
        // parentFolder = this; // we want to use Root dir as parent for alt root
      }
      else
      {
        const unsigned arcIndex = (unsigned)_proxy2->Dirs[_proxyDirIndex].ArcIndex;
        const CProxyFile2 &item = _proxy2->Files[arcIndex];
        if (item.AltDirIndex == -1)
          return S_OK;
        altDirIndex = (unsigned)item.AltDirIndex;
        // parentFolder = _parentFolder;
      }

      CAgentFolder *folderSpec = new CAgentFolder;
      CMyComPtr<IFolderFolder> agentFolder = folderSpec;
      folderSpec->Init(_proxy, _proxy2, altDirIndex, /* parentFolder, */ _agentSpec);
      *resultFolder = agentFolder.Detach();
      return S_OK;
    }

    SET_realIndex_AND_dir_2
    const unsigned arcIndex = dir->Items[realIndex];
    const CProxyFile2 &item = _proxy2->Files[arcIndex];
    if (item.AltDirIndex == -1)
      return S_OK;
    return BindToAltStreams_Internal((unsigned)item.AltDirIndex, resultFolder);
  }

  COM_TRY_END
}

Z7_COM7F_IMF(CAgentFolder::BindToAltStreams(const wchar_t *name, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN

  *resultFolder = NULL;

  if (!_proxy2)
    return S_OK;

  if (_proxy2->IsAltDir(_proxyDirIndex))
    return S_OK;

  if (name[0] == 0)
    return BindToAltStreams((UInt32)(Int32)-1, resultFolder);

  {
    const CUIntVector &subFiles = _proxy2->Dirs[_proxyDirIndex].Items;
    FOR_VECTOR (i, subFiles)
    {
      const CProxyFile2 &file = _proxy2->Files[subFiles[i]];
      if (file.AltDirIndex != -1)
        if (CompareFileNames(file.Name, name) == 0)
          return BindToAltStreams_Internal((unsigned)file.AltDirIndex, resultFolder);
    }
    return E_INVALIDARG;
  }
  COM_TRY_END
}

Z7_COM7F_IMF(CAgentFolder::AreAltStreamsSupported(UInt32 index, Int32 *isSupported))
{
  *isSupported = BoolToInt(false);

  if (!_proxy2)
    return S_OK;

  if (_proxy2->IsAltDir(_proxyDirIndex))
    return S_OK;

  unsigned arcIndex;

  if (index == (UInt32)(Int32)-1)
  {
    if (_proxyDirIndex == k_Proxy2_RootDirIndex)
    {
      *isSupported = BoolToInt(true);
      return S_OK;
    }
    arcIndex = (unsigned)_proxy2->Dirs[_proxyDirIndex].ArcIndex;
  }
  else
  {
    SET_realIndex_AND_dir_2
    arcIndex = dir->Items[realIndex];
  }

  if (_proxy2->Files[arcIndex].AltDirIndex != -1)
    *isSupported = BoolToInt(true);
  return S_OK;
}


Z7_COM7F_IMF(CAgentFolder::BindToParentFolder(IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  /*
  CMyComPtr<IFolderFolder> parentFolder = _parentFolder;
  *resultFolder = parentFolder.Detach();
  */
  *resultFolder = NULL;

  unsigned proxyDirIndex;

  if (_proxy2)
  {
    if (_proxyDirIndex == k_Proxy2_RootDirIndex)
      return S_OK;
    if (_proxyDirIndex == k_Proxy2_AltRootDirIndex)
      proxyDirIndex = k_Proxy2_RootDirIndex;
    else
    {
      const CProxyDir2 &fold = _proxy2->Dirs[_proxyDirIndex];
      const CProxyFile2 &file = _proxy2->Files[(unsigned)fold.ArcIndex];
      const int parentIndex = file.Parent;
      if (parentIndex == -1)
        proxyDirIndex = k_Proxy2_RootDirIndex;
      else
        proxyDirIndex = (unsigned)_proxy2->Files[(unsigned)parentIndex].DirIndex;
    }
  }
  else
  {
    const int parent = _proxy->Dirs[_proxyDirIndex].ParentDir;
    if (parent == -1)
      return S_OK;
    proxyDirIndex = (unsigned)parent;
  }

  CAgentFolder *folderSpec = new CAgentFolder;
  CMyComPtr<IFolderFolder> agentFolder = folderSpec;
  folderSpec->Init(_proxy, _proxy2, proxyDirIndex, /* parentFolder, */ _agentSpec);
  *resultFolder = agentFolder.Detach();

  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CAgentFolder::GetStream(UInt32 index, ISequentialInStream **stream))
{
  Z7_DECL_CMyComPtr_QI_FROM(
      IInArchiveGetStream,
      getStream, _agentSpec->GetArchive())
  if (!getStream)
    return S_OK;

  UInt32 arcIndex;
  if (_proxy2)
  {
    SET_realIndex_AND_dir_2
    arcIndex = dir->Items[realIndex];
  }
  else
  {
    SET_realIndex_AND_dir

    if (realIndex < dir->SubDirs.Size())
    {
      const CProxyDir &item = _proxy->Dirs[dir->SubDirs[realIndex]];
      if (!item.IsLeaf())
        return S_OK;
      arcIndex = (unsigned)item.ArcIndex;
    }
    else
      arcIndex = dir->SubFiles[realIndex - dir->SubDirs.Size()];
  }
  return getStream->GetStream(arcIndex, stream);
}

// static const unsigned k_FirstOptionalProp = 2;

static const PROPID kProps[] =
{
  kpidNumSubDirs,
  kpidNumSubFiles,

  // kpidNumAltStreams,
  kpidPrefix
};

struct CArchiveItemPropertyTemp
{
  UString Name;
  PROPID ID;
  VARTYPE Type;
};

Z7_COM7F_IMF(CAgentFolder::GetNumberOfProperties(UInt32 *numProps))
{
  COM_TRY_BEGIN
  RINOK(_agentSpec->GetArchive()->GetNumberOfProperties(numProps))
  *numProps += Z7_ARRAY_SIZE(kProps);
  if (!_flatMode)
    (*numProps)--;
  /*
  if (!_agentSpec->ThereIsAltStreamProp)
    (*numProps)--;
  */
  /*
  bool thereIsPathProp = _proxy2 ?
    _agentSpec->_proxy2->ThereIsPathProp :
    _agentSpec->_proxy->ThereIsPathProp;
  */

  // if there is kpidPath, we change kpidPath to kpidName
  // if there is no kpidPath, we add kpidName.
  if (!_agentSpec->ThereIsPathProp)
    (*numProps)++;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CAgentFolder::GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType))
{
  COM_TRY_BEGIN
  UInt32 numProps;
  _agentSpec->GetArchive()->GetNumberOfProperties(&numProps);

  /*
  bool thereIsPathProp = _proxy2 ?
    _agentSpec->_proxy2->ThereIsPathProp :
    _agentSpec->_proxy->ThereIsPathProp;
  */

  if (!_agentSpec->ThereIsPathProp)
  {
    if (index == 0)
    {
      *propID = kpidName;
      *varType = VT_BSTR;
      *name = NULL;
      return S_OK;
    }
    index--;
  }

  if (index < numProps)
  {
    RINOK(_agentSpec->GetArchive()->GetPropertyInfo(index, name, propID, varType))
    if (*propID == kpidPath)
      *propID = kpidName;
  }
  else
  {
    index -= numProps;
    /*
    if (index >= k_FirstOptionalProp)
    {
      if (!_agentSpec->ThereIsAltStreamProp)
        index++;
    }
    */
    *propID = kProps[index];
    *varType = k7z_PROPID_To_VARTYPE[(unsigned)*propID];
    *name = NULL;
  }
  return S_OK;
  COM_TRY_END
}

static const PROPID kFolderProps[] =
{
  kpidSize,
  kpidPackSize,
  kpidNumSubDirs,
  kpidNumSubFiles,
  kpidCRC
};

Z7_COM7F_IMF(CAgentFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN

  NWindows::NCOM::CPropVariant prop;

  if (propID == kpidReadOnly)
  {
    if ((_agentSpec->_proxy && _agentSpec->_proxy->Are_Changed_LongPaths)
        || _agentSpec->Is_Attrib_ReadOnly())
      prop = true;
    else
      prop = _agentSpec->IsThere_ReadOnlyArc();
  }
  else if (propID == kpidIsHash)
  {
    prop = _agentSpec->_isHashHandler;
  }
  else if (_proxy2)
  {
    const CProxyDir2 &dir = _proxy2->Dirs[_proxyDirIndex];
    if (propID == kpidName)
    {
      if (dir.ArcIndex != -1)
        prop = _proxy2->Files[(unsigned)dir.ArcIndex].Name;
    }
    else if (propID == kpidPath)
    {
      // Here we allow the return of reduced path:
      prop = _proxy2->GetDirPath_as_Prefix(_proxyDirIndex, true); // canReducePath
    }
    else switch (propID)
    {
      case kpidSize:         prop = dir.Size; break;
      case kpidPackSize:     prop = dir.PackSize; break;
      case kpidNumSubDirs:   prop = dir.NumSubDirs; break;
      case kpidNumSubFiles:  prop = dir.NumSubFiles; break;
        // case kpidName:         prop = dir.Name; break;
      // case kpidPath:         prop = _proxy2->GetFullPathPrefix(_proxyDirIndex); break;
      // **************** NanaZip Modification Start ****************
      // case kpidType: prop = UString("7-Zip.") + _agentSpec->ArchiveType; break;
      case kpidType: prop = UString("NanaZip.") + _agentSpec->ArchiveType; break;
      // **************** NanaZip Modification End ****************
      case kpidCRC: if (dir.CrcIsDefined) { prop = dir.Crc; } break;
    }

  }
  else
  {
  const CProxyDir &dir = _proxy->Dirs[_proxyDirIndex];
  switch (propID)
  {
    case kpidSize:         prop = dir.Size; break;
    case kpidPackSize:     prop = dir.PackSize; break;
    case kpidNumSubDirs:   prop = dir.NumSubDirs; break;
    case kpidNumSubFiles:  prop = dir.NumSubFiles; break;
    case kpidName:         prop = dir.Name; break;
    case kpidPath:         prop = _proxy->GetDirPath_as_Prefix(_proxyDirIndex); break;
    // **************** NanaZip Modification Start ****************
    // case kpidType: prop = UString("7-Zip.") + _agentSpec->ArchiveType; break;
    case kpidType: prop = UString("NanaZip.") + _agentSpec->ArchiveType; break;
    // **************** NanaZip Modification End ****************
    case kpidCRC: if (dir.CrcIsDefined) prop = dir.Crc; break;
  }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CAgentFolder::GetNumberOfFolderProperties(UInt32 *numProps))
{
  *numProps = Z7_ARRAY_SIZE(kFolderProps);
  return S_OK;
}

IMP_IFolderFolder_GetProp(
  CAgentFolder::GetFolderPropertyInfo,
  kFolderProps)

Z7_COM7F_IMF(CAgentFolder::GetParent(UInt32 /* index */, UInt32 * /* parent */, UInt32 * /* parentType */))
{
  return E_FAIL;
}


Z7_COM7F_IMF(CAgentFolder::GetNumRawProps(UInt32 *numProps))
{
  IArchiveGetRawProps *rawProps = _agentSpec->_archiveLink.GetArchiveGetRawProps();
  if (rawProps)
    return rawProps->GetNumRawProps(numProps);
  *numProps = 0;
  return S_OK;
}

Z7_COM7F_IMF(CAgentFolder::GetRawPropInfo(UInt32 index, BSTR *name, PROPID *propID))
{
  IArchiveGetRawProps *rawProps = _agentSpec->_archiveLink.GetArchiveGetRawProps();
  if (rawProps)
    return rawProps->GetRawPropInfo(index, name, propID);
  return E_FAIL;
}

Z7_COM7F_IMF(CAgentFolder::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType))
{
  IArchiveGetRawProps *rawProps = _agentSpec->_archiveLink.GetArchiveGetRawProps();
  if (rawProps)
  {
    unsigned arcIndex;
    if (_proxy2)
    {
      SET_realIndex_AND_dir_2
      arcIndex = dir->Items[realIndex];
    }
    else
    {
      SET_realIndex_AND_dir
      if (realIndex < dir->SubDirs.Size())
      {
        const CProxyDir &item = _proxy->Dirs[dir->SubDirs[realIndex]];
        if (!item.IsLeaf())
        {
          *data = NULL;
          *dataSize = 0;
          *propType = 0;
          return S_OK;
        }
        arcIndex = (unsigned)item.ArcIndex;
      }
      else
        arcIndex = dir->SubFiles[realIndex - dir->SubDirs.Size()];
    }
    return rawProps->GetRawProp(arcIndex, propID, data, dataSize, propType);
  }
  *data = NULL;
  *dataSize = 0;
  *propType = 0;
  return S_OK;
}

Z7_COM7F_IMF(CAgentFolder::GetFolderArcProps(IFolderArcProps **object))
{
  CMyComPtr<IFolderArcProps> temp = _agentSpec;
  *object = temp.Detach();
  return S_OK;
}


Z7_COM7F_IMF(CAgentFolder::SetFlatMode(Int32 flatMode))
{
  _flatMode = IntToBool(flatMode);
  return S_OK;
}


int CAgentFolder::GetRealIndex(unsigned index) const
{
  if (!_flatMode)
  {
    if (_proxy2)
      return (int)_proxy2->GetRealIndex(_proxyDirIndex, index);
    else
      return _proxy->GetRealIndex(_proxyDirIndex, index);
  }
  {
    const CProxyItem &item = _items[index];
    if (_proxy2)
    {
      const CProxyDir2 *dir = &_proxy2->Dirs[item.DirIndex];
      return (int)dir->Items[item.Index];
    }
    else
    {
      const CProxyDir *dir = &_proxy->Dirs[item.DirIndex];
      const unsigned realIndex = item.Index;
      if (realIndex < dir->SubDirs.Size())
      {
        const CProxyDir &f = _proxy->Dirs[dir->SubDirs[realIndex]];
        if (!f.IsLeaf())
          return -1;
        return f.ArcIndex;
      }
      return (int)dir->SubFiles[realIndex - dir->SubDirs.Size()];
    }
  }
}

void CAgentFolder::GetRealIndices(
    const UInt32 *indices, const UInt32 numItems,
    const bool includeAltStreams, const bool includeFolderSubItemsInFlatMode,
    CUIntVector &realIndices) const
{
  if (!_flatMode)
  {
    if (_proxy2)
      _proxy2->GetRealIndices_Unsorted(_proxyDirIndex, indices, numItems, includeAltStreams, realIndices);
    else
      _proxy->GetRealIndices_Unsorted(_proxyDirIndex, indices, numItems, realIndices);
  }
  else
  {
  realIndices.Clear();

  for (UInt32 i = 0; i < numItems; i++)
  {
    const CProxyItem &item = _items[indices[i]];
    if (_proxy2)
    {
      const CProxyDir2 *dir = &_proxy2->Dirs[item.DirIndex];
      _proxy2->AddRealIndices_of_ArcItem(dir->Items[item.Index],
          includeAltStreams, includeFolderSubItemsInFlatMode, realIndices);
      continue;
    }
    UInt32 arcIndex;
    {
      const CProxyDir *dir = &_proxy->Dirs[item.DirIndex];
      const unsigned realIndex = item.Index;
      if (realIndex < dir->SubDirs.Size())
      {
        if (includeFolderSubItemsInFlatMode)
        {
          _proxy->AddRealIndices(dir->SubDirs[realIndex], realIndices);
          continue;
        }
        const CProxyDir &f = _proxy->Dirs[dir->SubDirs[realIndex]];
        if (!f.IsLeaf())
          continue;
        arcIndex = (unsigned)f.ArcIndex;
      }
      else
        arcIndex = dir->SubFiles[realIndex - dir->SubDirs.Size()];
    }
    realIndices.Add(arcIndex);
  }
  }

  HeapSort(realIndices.NonConstData(), realIndices.Size());
}

Z7_COM7F_IMF(CAgentFolder::Extract(const UInt32 *indices,
    UInt32 numItems,
    Int32 includeAltStreams,
    Int32 replaceAltStreamColon,
    NExtract::NPathMode::EEnum pathMode,
    NExtract::NOverwriteMode::EEnum overwriteMode,
    const wchar_t *path,
    Int32 testMode,
    IFolderArchiveExtractCallback *extractCallback2))
{
  COM_TRY_BEGIN

  if (!testMode && _agentSpec->_isHashHandler)
    return E_NOTIMPL;

  CArchiveExtractCallback *extractCallbackSpec = new CArchiveExtractCallback;
  CMyComPtr<IArchiveExtractCallback> extractCallback = extractCallbackSpec;
  UStringVector pathParts;
  bool isAltStreamFolder = false;
  bool isChangedPath = false;
  if (_proxy2)
    _proxy2->GetDirPathParts(_proxyDirIndex, pathParts, isAltStreamFolder);
  else
    _proxy->GetDirPathParts_isChanged(_proxyDirIndex, pathParts, isChangedPath);
  if (isChangedPath)
    return E_NOTIMPL;
  /*
  if (_flatMode)
    pathMode = NExtract::NPathMode::kNoPathnames;
  */

  extractCallbackSpec->InitForMulti(
      false, // multiArchives
      pathMode,
      overwriteMode,
      _zoneMode,
      k_keepEmptyDirPrefixes);

  if (extractCallback2)
    extractCallback2->SetTotal(_agentSpec->GetArc().GetEstmatedPhySize());

  FString pathU;
  if (path)
  {
    pathU = us2fs(path);
    if (!pathU.IsEmpty()
      && !NFile::NName::IsAltStreamPrefixWithColon(path))
    {
      NFile::NName::NormalizeDirPathPrefix(pathU);
      NFile::NDir::CreateComplexDir(pathU);
    }
  }

  CExtractNtOptions extractNtOptions;
  extractNtOptions.AltStreams.Val = IntToBool(includeAltStreams); // change it!!!
  extractNtOptions.AltStreams.Def = true;

  extractNtOptions.ReplaceColonForAltStream = IntToBool(replaceAltStreamColon);

  extractCallbackSpec->InitBeforeNewArchive();

  #if defined(_WIN32) && !defined(UNDER_CE)
    if (_zoneMode != NExtract::NZoneIdMode::kNone)
    {
      ReadZoneFile_Of_BaseFile(us2fs(_agentSpec->_archiveFilePath), extractCallbackSpec->ZoneBuf);
      if (_zoneBuf.Size() != 0)
        extractCallbackSpec->ZoneBuf = _zoneBuf;
    }
  #endif

  extractCallbackSpec->Init(
      extractNtOptions,
      NULL, &_agentSpec->GetArc(),
      extractCallback2,
      false, // stdOutMode
      IntToBool(testMode),
      pathU,
      pathParts, isAltStreamFolder,
      (UInt64)(Int64)-1);

  if (_proxy2)
    extractCallbackSpec->SetBaseParentFolderIndex((unsigned)_proxy2->Dirs[_proxyDirIndex].ArcIndex);

  // do we need another base folder for subfolders ?
  extractCallbackSpec->DirPathPrefix_for_HashFiles = _agentSpec->_hashBaseFolderPrefix;

  CUIntVector realIndices;
  GetRealIndices(indices, numItems,
      (_loadAltStreams && _flatMode) ? false : IntToBool(includeAltStreams), // v26.03
      false, // includeFolderSubItemsInFlatMode
      realIndices); //

  #ifdef SUPPORT_LINKS

  if (!testMode)
  {
    RINOK(extractCallbackSpec->PrepareHardLinks(&realIndices))
  }

  #endif

  {
    CArchiveExtractCallback_Closer ecsCloser(extractCallbackSpec);

    HRESULT res = _agentSpec->GetArchive()->Extract(realIndices.ConstData(),
        realIndices.Size(), testMode, extractCallback);

    const HRESULT res2 = ecsCloser.Close();
    if (res == S_OK)
      res = res2;
    return res;
  }

  COM_TRY_END
}

/////////////////////////////////////////
// CAgent

CAgent::CAgent():
    _proxy(NULL),
    _proxy2(NULL),
    _updatePathPrefix_is_AltFolder(false),
    _isDeviceFile(false),
    _isHashHandler(false),
    _progress_ArchiveOpenCallback_for_Open(NULL),
    _progress_for_Open(NULL)
{
}

CAgent::~CAgent()
{
  if (_proxy)
    delete _proxy;
  if (_proxy2)
    delete _proxy2;
}

bool CAgent::CanUpdate() const
{
  if (_proxy && _proxy->Are_Changed_LongPaths)
    return false;
  // FAR plugin uses empty agent to create new archive !!!
  if (_archiveLink.Arcs.Size() == 0)
    return true;
  if (_isDeviceFile)
    return false;
  if (_archiveLink.Arcs.Size() != 1)
    return false;
  if (_archiveLink.Arcs[0].ErrorInfo.ThereIsTail)
    return false;
  return true;
}

Z7_COM7F_IMF(CAgent::Open(
    IInStream *inStream,
    const wchar_t *filePath,
    const wchar_t *arcFormat,
    BSTR *archiveType,
    IArchiveOpenCallback *openArchiveCallback))
{
  COM_TRY_BEGIN
  _archiveFilePath = filePath;
  _hashBaseFolderPrefix.Empty();
  _attrib = 0;
  _isDeviceFile = false;
  _isHashHandler = false;
  NFile::NFind::CFileInfo fi;
  if (!inStream)
  {
    if (!fi.Find(us2fs(_archiveFilePath)))
      return GetLastError_noZero_HRESULT();
    if (fi.IsDir())
      return E_FAIL;
    _attrib = fi.Attrib;
    _isDeviceFile = fi.IsDevice;
    FString dirPrefix, fileName;
    if (NFile::NDir::GetFullPathAndSplit(us2fs(_archiveFilePath), dirPrefix, fileName))
    {
      NFile::NName::NormalizeDirPathPrefix(dirPrefix);
      _hashBaseFolderPrefix = dirPrefix;
    }
  }
  CArcInfoEx archiverInfo0, archiverInfo1;

  RINOK(LoadGlobalCodecs())

  CObjectVector<COpenType> types;
  if (!ParseOpenTypes(*g_CodecsObj, arcFormat, types))
    return S_FALSE;

  /*
  CObjectVector<COptionalOpenProperties> optProps;
  if (Read_ShowDeleted())
  {
    COptionalOpenProperties &optPair = optProps.AddNew();
    optPair.FormatName = "ntfs";
    // optPair.Props.AddNew().Name = "LS";
    optPair.Props.AddNew().Name = "LD";
  }
  */

  COpenOptions options;
  options.props = NULL;
  options.codecs = g_CodecsObj;
  options.types = &types;
  CIntVector exl;
  options.excludedFormats = &exl;
  options.stdInMode = false;
  options.stream = inStream;
  options.filePath = _archiveFilePath;
  options.callback = openArchiveCallback;

  HRESULT res = _archiveLink.Open(options);

  if (!_archiveLink.Arcs.IsEmpty())
  {
    CArc &arc = _archiveLink.Arcs.Back();
    if (!inStream)
    {
      arc.MTime.Set_From_FiTime(fi.MTime);
      arc.MTime.Def = !fi.IsDevice;
    }

    ArchiveType = GetTypeOfArc(arc);
    if (archiveType)
    {
      RINOK(StringToBstr(ArchiveType, archiveType))
    }

    if (arc.IsHashHandler(options))
      _isHashHandler = true;
  }

  return res;

  COM_TRY_END
}


Z7_COM7F_IMF(CAgent::ReOpen(IArchiveOpenCallback *openArchiveCallback))
{
  COM_TRY_BEGIN
  if (_proxy2)
  {
    delete _proxy2;
    _proxy2 = NULL;
  }
  if (_proxy)
  {
    delete _proxy;
    _proxy = NULL;
  }

  CObjectVector<COpenType> incl;
  CIntVector exl;

  COpenOptions options;
  options.props = NULL;
  options.codecs = g_CodecsObj;
  options.types = &incl;
  options.excludedFormats = &exl;
  options.stdInMode = false;
  options.filePath = _archiveFilePath;
  options.callback = openArchiveCallback;

  RINOK(_archiveLink.ReOpen(options))
  return ReadItems();
  COM_TRY_END
}

Z7_COM7F_IMF(CAgent::Close())
{
  COM_TRY_BEGIN
  return _archiveLink.Close();
  COM_TRY_END
}

/*
Z7_COM7F_IMF(CAgent::EnumProperties(IEnumSTATPROPSTG **EnumProperties)
{
  return _archive->EnumProperties(EnumProperties);
}
*/

HRESULT CAgent::ReadItems()
{
  if (_proxy || _proxy2)
    return S_OK;

  const CArc &arc = GetArc();
  bool useProxy2 = (arc.GetRawProps && arc.IsTree);

  // useProxy2 = false;

  if (useProxy2)
    _proxy2 = new CProxyArc2();
  else
    _proxy = new CProxyArc();

  size_t ramSize;
  if (NWindows::NSystem::GetRamSize(ramSize))
  {
    ramSize = ramSize / 4 * 3;
    if (_proxy2)
      _proxy2->MemUsage_Limit = ramSize;
    else
      _proxy->MemUsage_Limit = ramSize;
  }

  {
    ThereIsPathProp = false;
    // ThereIsAltStreamProp = false;
    UInt32 numProps;
    arc.Archive->GetNumberOfProperties(&numProps);
    for (UInt32 i = 0; i < numProps; i++)
    {
      CMyComBSTR name;
      PROPID propID;
      VARTYPE varType;
      RINOK(arc.Archive->GetPropertyInfo(i, &name, &propID, &varType))
      if (propID == kpidPath)
        ThereIsPathProp = true;
      /*
      if (propID == kpidIsAltStream)
        ThereIsAltStreamProp = true;
      */
    }
  }

  if (_proxy2)
    return _proxy2->Load(GetArc(), _progress_for_Open);
  return _proxy->Load(GetArc(), _progress_ArchiveOpenCallback_for_Open);
}

Z7_COM7F_IMF(CAgent::BindToRootFolder(IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  if (!_archiveLink.Arcs.IsEmpty())
  {
    RINOK(ReadItems())
  }
  CAgentFolder *folderSpec = new CAgentFolder;
  CMyComPtr<IFolderFolder> rootFolder = folderSpec;
  folderSpec->Init(_proxy, _proxy2, k_Proxy_RootDirIndex, /* NULL, */ this);
  *resultFolder = rootFolder.Detach();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CAgent::Extract(
    NExtract::NPathMode::EEnum pathMode,
    NExtract::NOverwriteMode::EEnum overwriteMode,
    const wchar_t *path,
    Int32 testMode,
    IFolderArchiveExtractCallback *extractCallback2))
{
  COM_TRY_BEGIN

  if (!testMode && _isHashHandler)
    return E_NOTIMPL;

  CArchiveExtractCallback *extractCallbackSpec = new CArchiveExtractCallback;
  CMyComPtr<IArchiveExtractCallback> extractCallback = extractCallbackSpec;
  extractCallbackSpec->InitForMulti(
      false, // multiArchives
      pathMode,
      overwriteMode,
      // **************** NanaZip Modification Start ****************
      // NExtract::NZoneIdMode::kNone,
      NExtract::NZoneIdMode::Default,
      // **************** NanaZip Modification End ****************
      k_keepEmptyDirPrefixes);

  CExtractNtOptions extractNtOptions;
  extractNtOptions.AltStreams.Val = true; // change it!!!
  extractNtOptions.AltStreams.Def = true; // change it!!!
  extractNtOptions.ReplaceColonForAltStream = false; // change it!!!

  extractCallbackSpec->Init(
      extractNtOptions,
      NULL, &GetArc(),
      extractCallback2,
      false, // stdOutMode
      IntToBool(testMode),
      us2fs(path),
      UStringVector(), false,
      (UInt64)(Int64)-1);

  extractCallbackSpec->DirPathPrefix_for_HashFiles = _hashBaseFolderPrefix;

  #ifdef SUPPORT_LINKS

  if (!testMode)
  {
    RINOK(extractCallbackSpec->PrepareHardLinks(NULL)) // NULL means all items
  }

  #endif

  return GetArchive()->Extract(NULL, (UInt32)(Int32)-1, testMode, extractCallback);
  COM_TRY_END
}

Z7_COM7F_IMF(CAgent::GetNumberOfProperties(UInt32 *numProps))
{
  COM_TRY_BEGIN
  return GetArchive()->GetNumberOfProperties(numProps);
  COM_TRY_END
}

Z7_COM7F_IMF(CAgent::GetPropertyInfo(UInt32 index,
      BSTR *name, PROPID *propID, VARTYPE *varType))
{
  COM_TRY_BEGIN
  RINOK(GetArchive()->GetPropertyInfo(index, name, propID, varType))
  if (*propID == kpidPath)
    *propID = kpidName;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CAgent::GetArcNumLevels(UInt32 *numLevels))
{
  *numLevels = _archiveLink.Arcs.Size();
  return S_OK;
}

Z7_COM7F_IMF(CAgent::GetArcProp(UInt32 level, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  if (level > (UInt32)_archiveLink.Arcs.Size())
    return E_INVALIDARG;
  if (level == (UInt32)_archiveLink.Arcs.Size())
  {
    switch (propID)
    {
      case kpidPath:
        if (!_archiveLink.NonOpen_ArcPath.IsEmpty())
          prop = _archiveLink.NonOpen_ArcPath;
        break;
      case kpidErrorType:
        if (_archiveLink.NonOpen_ErrorInfo.ErrorFormatIndex >= 0)
          prop = g_CodecsObj->Formats[_archiveLink.NonOpen_ErrorInfo.ErrorFormatIndex].Name;
        break;
      case kpidErrorFlags:
      {
        UInt32 flags = _archiveLink.NonOpen_ErrorInfo.GetErrorFlags();
        if (flags != 0)
          prop = flags;
        break;
      }
      case kpidWarningFlags:
      {
        UInt32 flags = _archiveLink.NonOpen_ErrorInfo.GetWarningFlags();
        if (flags != 0)
          prop = flags;
        break;
      }
    }
  }
  else
  {
    const CArc &arc = _archiveLink.Arcs[level];
    switch (propID)
    {
      case kpidType: prop = GetTypeOfArc(arc); break;
      case kpidPath: prop = arc.Path; break;
      case kpidErrorType:
        if (arc.ErrorInfo.ErrorFormatIndex >= 0)
          prop = g_CodecsObj->Formats[arc.ErrorInfo.ErrorFormatIndex].Name;
        break;
      case kpidErrorFlags:
      {
        const UInt32 flags = arc.ErrorInfo.GetErrorFlags();
        if (flags != 0)
          prop = flags;
        break;
      }
      case kpidWarningFlags:
      {
        const UInt32 flags = arc.ErrorInfo.GetWarningFlags();
        if (flags != 0)
          prop = flags;
        break;
      }
      case kpidOffset:
      {
        const Int64 v = arc.GetGlobalOffset();
        if (v != 0)
          prop.Set_Int64(v);
        break;
      }
      case kpidTailSize:
      {
        if (arc.ErrorInfo.TailSize != 0)
          prop = arc.ErrorInfo.TailSize;
        break;
      }
      default: return arc.Archive->GetArchiveProperty(propID, value);
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CAgent::GetArcNumProps(UInt32 level, UInt32 *numProps))
{
  return _archiveLink.Arcs[level].Archive->GetNumberOfArchiveProperties(numProps);
}

Z7_COM7F_IMF(CAgent::GetArcPropInfo(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType))
{
  return _archiveLink.Arcs[level].Archive->GetArchivePropertyInfo(index, name, propID, varType);
}

// MainItemProperty
Z7_COM7F_IMF(CAgent::GetArcProp2(UInt32 level, PROPID propID, PROPVARIANT *value))
{
  return _archiveLink.Arcs[level - 1].Archive->GetProperty(_archiveLink.Arcs[level].SubfileIndex, propID, value);
}

Z7_COM7F_IMF(CAgent::GetArcNumProps2(UInt32 level, UInt32 *numProps))
{
  return _archiveLink.Arcs[level - 1].Archive->GetNumberOfProperties(numProps);
}

Z7_COM7F_IMF(CAgent::GetArcPropInfo2(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType))
{
  return _archiveLink.Arcs[level - 1].Archive->GetPropertyInfo(index, name, propID, varType);
}
