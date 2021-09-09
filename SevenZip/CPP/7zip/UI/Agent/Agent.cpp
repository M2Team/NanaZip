// Agent.cpp

#include "StdAfx.h"

#include <wchar.h>

#include "../../../../C/Sort.h"

#include "../../../Common/ComTry.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariantConv.h"

#ifndef _7ZIP_ST
#include "../../../Windows/Synchronization.h"
#endif

#include "../Common/ArchiveExtractCallback.h"
#include "../FileManager/RegistryUtils.h"

#include "Agent.h"

using namespace NWindows;

CCodecs *g_CodecsObj;

#ifdef EXTERNAL_CODECS
  CExternalCodecs g_ExternalCodecs;
  static CCodecs::CReleaser g_CodecsReleaser;
#else
  extern
  CMyComPtr<IUnknown> g_CodecsRef;
  CMyComPtr<IUnknown> g_CodecsRef;
#endif

#ifndef _7ZIP_ST
static NSynchronization::CCriticalSection g_CriticalSection;
#define MT_LOCK NSynchronization::CCriticalSectionLock lock(g_CriticalSection);
#else
#define MT_LOCK
#endif

void FreeGlobalCodecs()
{
  MT_LOCK

  #ifdef EXTERNAL_CODECS
  if (g_CodecsObj)
  {
    g_CodecsObj->CloseLibs();
  }
  g_CodecsReleaser.Set(NULL);
  g_CodecsObj = NULL;
  g_ExternalCodecs.ClearAndRelease();
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

  #ifdef EXTERNAL_CODECS
  g_ExternalCodecs.GetCodecs = g_CodecsObj;
  g_ExternalCodecs.GetHashers = g_CodecsObj;
  g_CodecsReleaser.Set(g_CodecsObj);
  #else
  g_CodecsRef.Release();
  g_CodecsRef = g_CodecsObj;
  #endif

  RINOK(g_CodecsObj->Load());
  if (g_CodecsObj->Formats.IsEmpty())
  {
    FreeGlobalCodecs();
    return E_NOTIMPL;
  }

  #ifdef EXTERNAL_CODECS
  RINOK(g_ExternalCodecs.Load());
  #endif

  return S_OK;
}

STDMETHODIMP CAgentFolder::GetAgentFolder(CAgentFolder **agentFolder)
{
  *agentFolder = this;
  return S_OK;
}

void CAgentFolder::LoadFolder(unsigned proxyDirIndex)
{
  CProxyItem item;
  item.DirIndex = proxyDirIndex;
  
  if (_proxy2)
  {
    const CProxyDir2 &dir = _proxy2->Dirs[proxyDirIndex];
    FOR_VECTOR (i, dir.Items)
    {
      item.Index = i;
      _items.Add(item);
      const CProxyFile2 &file = _proxy2->Files[dir.Items[i]];
      if (file.DirIndex >= 0)
        LoadFolder(file.DirIndex);
      if (_loadAltStreams && file.AltDirIndex >= 0)
        LoadFolder(file.AltDirIndex);
    }
    return;
  }
  
  const CProxyDir &dir = _proxy->Dirs[proxyDirIndex];
  unsigned i;
  for (i = 0; i < dir.SubDirs.Size(); i++)
  {
    item.Index = i;
    _items.Add(item);
    LoadFolder(dir.SubDirs[i]);
  }
  
  unsigned start = dir.SubDirs.Size();
  for (i = 0; i < dir.SubFiles.Size(); i++)
  {
    item.Index = start + i;
    _items.Add(item);
  }
}

STDMETHODIMP CAgentFolder::LoadItems()
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

STDMETHODIMP CAgentFolder::GetNumberOfItems(UInt32 *numItems)
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

void CAgentFolder::GetPrefix(UInt32 index, UString &prefix) const
{
  if (!_flatMode)
  {
    prefix.Empty();
    return;
  }
  
  const CProxyItem &item = _items[index];
  unsigned proxyIndex = item.DirIndex;
  
  if (_proxy2)
  {
    // that code is unused. 7-Zip gets prefix via GetItemPrefix() .

    unsigned len = 0;
    while (proxyIndex != _proxyDirIndex && proxyIndex >= k_Proxy2_NumRootDirs)
    {
      const CProxyFile2 &file = _proxy2->Files[_proxy2->Dirs[proxyIndex].ArcIndex];
      len += file.NameLen + 1;
      proxyIndex = (file.Parent < 0) ? 0 : _proxy2->Files[file.Parent].GetDirIndex(file.IsAltStream);
    }
    
    wchar_t *p = prefix.GetBuf_SetEnd(len) + len;
    proxyIndex = item.DirIndex;
    while (proxyIndex != _proxyDirIndex && proxyIndex >= k_Proxy2_NumRootDirs)
    {
      const CProxyFile2 &file = _proxy2->Files[_proxy2->Dirs[proxyIndex].ArcIndex];
      p--;
      *p = WCHAR_PATH_SEPARATOR;
      p -= file.NameLen;
      wmemcpy(p, file.Name, file.NameLen);
      proxyIndex = (file.Parent < 0) ? 0 : _proxy2->Files[file.Parent].GetDirIndex(file.IsAltStream);
    }
  }
  else
  {
    unsigned len = 0;
    while (proxyIndex != _proxyDirIndex)
    {
      const CProxyDir *dir = &_proxy->Dirs[proxyIndex];
      len += dir->NameLen + 1;
      proxyIndex = dir->ParentDir;
    }
    
    wchar_t *p = prefix.GetBuf_SetEnd(len) + len;
    proxyIndex = item.DirIndex;
    while (proxyIndex != _proxyDirIndex)
    {
      const CProxyDir *dir = &_proxy->Dirs[proxyIndex];
      p--;
      *p = WCHAR_PATH_SEPARATOR;
      p -= dir->NameLen;
      wmemcpy(p, dir->Name, dir->NameLen);
      proxyIndex = dir->ParentDir;
    }
  }
}

UString CAgentFolder::GetFullPrefix(UInt32 index) const
{
  int foldIndex = _proxyDirIndex;
  
  if (_flatMode)
    foldIndex = _items[index].DirIndex;

  if (_proxy2)
    return _proxy2->Dirs[foldIndex].PathPrefix;
  else
    return _proxy->GetDirPath_as_Prefix(foldIndex);
}

STDMETHODIMP_(UInt64) CAgentFolder::GetItemSize(UInt32 index)
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
      arcIndex = item.ArcIndex;
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

STDMETHODIMP CAgentFolder::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
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
      if (item.AltDirIndex >= 0)
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
            RINOK(_agentSpec->GetArchive()->GetProperty(arcIndex, propID, value));
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
          RINOK(_agentSpec->GetArchive()->GetProperty(item.ArcIndex, propID, value));
        }
        if (item.CrcIsDefined && value->vt == VT_EMPTY)
          prop = item.Crc;
        break;
      }
      default:
        if (item.IsLeaf())
          return _agentSpec->GetArchive()->GetProperty(item.ArcIndex, propID, value);
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

STDMETHODIMP CAgentFolder::GetItemName(UInt32 index, const wchar_t **name, unsigned *len)
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

STDMETHODIMP CAgentFolder::GetItemPrefix(UInt32 index, const wchar_t **name, unsigned *len)
{
  *name = 0;
  *len = 0;
  if (!_flatMode)
    return S_OK;

  if (_proxy2)
  {
    const CProxyItem &item = _items[index];
    const UString &s = _proxy2->Dirs[item.DirIndex].PathPrefix;
    unsigned baseLen = _proxy2->Dirs[_proxyDirIndex].PathPrefix.Len();
    if (baseLen <= s.Len())
    {
      *name = (const wchar_t *)s + baseLen;
      *len = s.Len() - baseLen;
    }
    else
    {
      return E_FAIL;
      // throw 111l;
    }
  }
  return S_OK;
}

static int CompareRawProps(IArchiveGetRawProps *rawProps, int arcIndex1, int arcIndex2, PROPID propID)
{
  // if (propID == kpidSha1)
  if (rawProps)
  {
    const void *p1, *p2;
    UInt32 size1, size2;
    UInt32 propType1, propType2;
    HRESULT res1 = rawProps->GetRawProp(arcIndex1, propID, &p1, &size1, &propType1);
    HRESULT res2 = rawProps->GetRawProp(arcIndex2, propID, &p2, &size2, &propType2);
    if (res1 == S_OK && res2 == S_OK)
    {
      for (UInt32 i = 0; i < size1 && i < size2; i++)
      {
        Byte b1 = ((const Byte *)p1)[i];
        Byte b2 = ((const Byte *)p2)[i];
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

static const wchar_t *GetExtension(const wchar_t *name)
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
    return CompareFileNames_ForFolderList(
        _proxy2->Dirs[_items[index1].DirIndex].PathPrefix,
        _proxy2->Dirs[_items[index2].DirIndex].PathPrefix);
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


STDMETHODIMP_(Int32) CAgentFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw)
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
    UString prefix1, prefix2;
    GetPrefix(index1, prefix1);
    GetPrefix(index2, prefix2);
    return CompareFileNames_ForFolderList(prefix1, prefix2);
  }
  
  UInt32 arcIndex1;
  UInt32 arcIndex2;

  const CProxyDir *proxFolder1 = NULL;
  const CProxyDir *proxFolder2 = NULL;
  
  if (realIndex1 < dir1->SubDirs.Size())
  {
    proxFolder1 = &_proxy->Dirs[dir1->SubDirs[realIndex1]];
    arcIndex1 = proxFolder1->ArcIndex;
  }
  else
    arcIndex1 = dir1->SubFiles[realIndex1 - dir1->SubDirs.Size()];

  if (realIndex2 < dir2->SubDirs.Size())
  {
    proxFolder2 = &_proxy->Dirs[dir2->SubDirs[realIndex2]];
    arcIndex2 = proxFolder2->ArcIndex;
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

STDMETHODIMP CAgentFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder)
{
  COM_TRY_BEGIN
  if (_proxy2)
  {
    SET_realIndex_AND_dir_2
    unsigned arcIndex = dir->Items[realIndex];
    const CProxyFile2 &item = _proxy2->Files[arcIndex];
    if (!item.IsDir())
      return E_INVALIDARG;
    return BindToFolder_Internal(item.DirIndex, resultFolder);
  }
  SET_realIndex_AND_dir
  if (realIndex >= (UInt32)dir->SubDirs.Size())
    return E_INVALIDARG;
  return BindToFolder_Internal(dir->SubDirs[realIndex], resultFolder);
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder)
{
  COM_TRY_BEGIN
  if (_proxy2)
  {
    int index = _proxy2->FindItem(_proxyDirIndex, name, true);
    if (index < 0)
      return E_INVALIDARG;
    return BindToFolder_Internal(_proxy2->Files[_proxy2->Dirs[_proxyDirIndex].Items[index]].DirIndex, resultFolder);
  }
  int index = _proxy->FindSubDir(_proxyDirIndex, name);
  if (index < 0)
    return E_INVALIDARG;
  return BindToFolder_Internal(index, resultFolder);
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

STDMETHODIMP CAgentFolder::BindToAltStreams(UInt32 index, IFolderFolder **resultFolder)
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
        unsigned arcIndex = _proxy2->Dirs[_proxyDirIndex].ArcIndex;
        const CProxyFile2 &item = _proxy2->Files[arcIndex];
        if (item.AltDirIndex < 0)
          return S_OK;
        altDirIndex = item.AltDirIndex;
        // parentFolder = _parentFolder;
      }
      
      CAgentFolder *folderSpec = new CAgentFolder;
      CMyComPtr<IFolderFolder> agentFolder = folderSpec;
      folderSpec->Init(_proxy, _proxy2, altDirIndex, /* parentFolder, */ _agentSpec);
      *resultFolder = agentFolder.Detach();
      return S_OK;
    }

    SET_realIndex_AND_dir_2
    unsigned arcIndex = dir->Items[realIndex];
    const CProxyFile2 &item = _proxy2->Files[arcIndex];
    if (item.AltDirIndex < 0)
      return S_OK;
    return BindToAltStreams_Internal(item.AltDirIndex, resultFolder);
  }
  
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::BindToAltStreams(const wchar_t *name, IFolderFolder **resultFolder)
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
    const CProxyDir2 &dir = _proxy2->Dirs[_proxyDirIndex];
    FOR_VECTOR (i, dir.Items)
    {
      const CProxyFile2 &file = _proxy2->Files[dir.Items[i]];
      if (file.AltDirIndex >= 0)
        if (CompareFileNames(file.Name, name) == 0)
          return BindToAltStreams_Internal(file.AltDirIndex, resultFolder);
    }
    return E_INVALIDARG;
  }
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::AreAltStreamsSupported(UInt32 index, Int32 *isSupported)
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
    arcIndex = _proxy2->Dirs[_proxyDirIndex].ArcIndex;
  }
  else
  {
    SET_realIndex_AND_dir_2
    arcIndex = dir->Items[realIndex];
  }
  
  if (_proxy2->Files[arcIndex].AltDirIndex >= 0)
    *isSupported = BoolToInt(true);
  return S_OK;
}


STDMETHODIMP CAgentFolder::BindToParentFolder(IFolderFolder **resultFolder)
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
      const CProxyFile2 &file = _proxy2->Files[fold.ArcIndex];
      int parentIndex = file.Parent;
      if (parentIndex < 0)
        proxyDirIndex = k_Proxy2_RootDirIndex;
      else
        proxyDirIndex = _proxy2->Files[parentIndex].DirIndex;
    }
  }
  else
  {
    int parent = _proxy->Dirs[_proxyDirIndex].ParentDir;
    if (parent < 0)
      return S_OK;
    proxyDirIndex = parent;
  }

  CAgentFolder *folderSpec = new CAgentFolder;
  CMyComPtr<IFolderFolder> agentFolder = folderSpec;
  folderSpec->Init(_proxy, _proxy2, proxyDirIndex, /* parentFolder, */ _agentSpec);
  *resultFolder = agentFolder.Detach();

  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::GetStream(UInt32 index, ISequentialInStream **stream)
{
  CMyComPtr<IInArchiveGetStream> getStream;
  _agentSpec->GetArchive()->QueryInterface(IID_IInArchiveGetStream, (void **)&getStream);
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
      arcIndex = item.ArcIndex;
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

STDMETHODIMP CAgentFolder::GetNumberOfProperties(UInt32 *numProps)
{
  COM_TRY_BEGIN
  RINOK(_agentSpec->GetArchive()->GetNumberOfProperties(numProps));
  *numProps += ARRAY_SIZE(kProps);
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

STDMETHODIMP CAgentFolder::GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)
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
      *name = 0;
      return S_OK;
    }
    index--;
  }

  if (index < numProps)
  {
    RINOK(_agentSpec->GetArchive()->GetPropertyInfo(index, name, propID, varType));
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
    *name = 0;
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

STDMETHODIMP CAgentFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN

  NWindows::NCOM::CPropVariant prop;

  if (propID == kpidReadOnly)
  {
    if (_agentSpec->Is_Attrib_ReadOnly())
      prop = true;
    else
      prop = _agentSpec->IsThereReadOnlyArc();
  }
  else if (_proxy2)
  {
    const CProxyDir2 &dir = _proxy2->Dirs[_proxyDirIndex];
    if (propID == kpidName)
    {
      if (dir.ArcIndex >= 0)
        prop = _proxy2->Files[dir.ArcIndex].Name;
    }
    else if (propID == kpidPath)
    {
      bool isAltStreamFolder = false;
      prop = _proxy2->GetDirPath_as_Prefix(_proxyDirIndex, isAltStreamFolder);
    }
    else switch (propID)
    {
      case kpidSize:         prop = dir.Size; break;
      case kpidPackSize:     prop = dir.PackSize; break;
      case kpidNumSubDirs:   prop = dir.NumSubDirs; break;
      case kpidNumSubFiles:  prop = dir.NumSubFiles; break;
        // case kpidName:         prop = dir.Name; break;
      // case kpidPath:         prop = _proxy2->GetFullPathPrefix(_proxyDirIndex); break;
      case kpidType: prop = UString("7-Zip.") + _agentSpec->ArchiveType; break;
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
    case kpidType: prop = UString("7-Zip.") + _agentSpec->ArchiveType; break;
    case kpidCRC: if (dir.CrcIsDefined) prop = dir.Crc; break;
  }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::GetNumberOfFolderProperties(UInt32 *numProps)
{
  *numProps = ARRAY_SIZE(kFolderProps);
  return S_OK;
}

STDMETHODIMP CAgentFolder::GetFolderPropertyInfo IMP_IFolderFolder_GetProp(kFolderProps)

STDMETHODIMP CAgentFolder::GetParent(UInt32 /* index */, UInt32 * /* parent */, UInt32 * /* parentType */)
{
  return E_FAIL;
}


STDMETHODIMP CAgentFolder::GetNumRawProps(UInt32 *numProps)
{
  IArchiveGetRawProps *rawProps = _agentSpec->_archiveLink.GetArchiveGetRawProps();
  if (rawProps)
    return rawProps->GetNumRawProps(numProps);
  *numProps = 0;
  return S_OK;
}

STDMETHODIMP CAgentFolder::GetRawPropInfo(UInt32 index, BSTR *name, PROPID *propID)
{
  IArchiveGetRawProps *rawProps = _agentSpec->_archiveLink.GetArchiveGetRawProps();
  if (rawProps)
    return rawProps->GetRawPropInfo(index, name, propID);
  return E_FAIL;
}

STDMETHODIMP CAgentFolder::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType)
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
        arcIndex = item.ArcIndex;
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

STDMETHODIMP CAgentFolder::GetFolderArcProps(IFolderArcProps **object)
{
  CMyComPtr<IFolderArcProps> temp = _agentSpec;
  *object = temp.Detach();
  return S_OK;
}

#ifdef NEW_FOLDER_INTERFACE

STDMETHODIMP CAgentFolder::SetFlatMode(Int32 flatMode)
{
  _flatMode = IntToBool(flatMode);
  return S_OK;
}

#endif

int CAgentFolder::GetRealIndex(unsigned index) const
{
  if (!_flatMode)
  {
    if (_proxy2)
      return _proxy2->GetRealIndex(_proxyDirIndex, index);
    else
      return _proxy->GetRealIndex(_proxyDirIndex, index);
  }
  {
    const CProxyItem &item = _items[index];
    if (_proxy2)
    {
      const CProxyDir2 *dir = &_proxy2->Dirs[item.DirIndex];
      return dir->Items[item.Index];
    }
    else
    {
      const CProxyDir *dir = &_proxy->Dirs[item.DirIndex];
      unsigned realIndex = item.Index;
      if (realIndex < dir->SubDirs.Size())
      {
        const CProxyDir &f = _proxy->Dirs[dir->SubDirs[realIndex]];
        if (!f.IsLeaf())
          return -1;
        return f.ArcIndex;
      }
      return dir->SubFiles[realIndex - dir->SubDirs.Size()];
    }
  }
}

void CAgentFolder::GetRealIndices(const UInt32 *indices, UInt32 numItems, bool includeAltStreams, bool includeFolderSubItemsInFlatMode, CUIntVector &realIndices) const
{
  if (!_flatMode)
  {
    if (_proxy2)
      _proxy2->GetRealIndices(_proxyDirIndex, indices, numItems, includeAltStreams, realIndices);
    else
      _proxy->GetRealIndices(_proxyDirIndex, indices, numItems, realIndices);
    return;
  }

  realIndices.Clear();
  
  for (UInt32 i = 0; i < numItems; i++)
  {
    const CProxyItem &item = _items[indices[i]];
    if (_proxy2)
    {
      const CProxyDir2 *dir = &_proxy2->Dirs[item.DirIndex];
      _proxy2->AddRealIndices_of_ArcItem(dir->Items[item.Index], includeAltStreams, realIndices);
      continue;
    }
    UInt32 arcIndex;
    {
      const CProxyDir *dir = &_proxy->Dirs[item.DirIndex];
      unsigned realIndex = item.Index;
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
        arcIndex = f.ArcIndex;
      }
      else
        arcIndex = dir->SubFiles[realIndex - dir->SubDirs.Size()];
    }
    realIndices.Add(arcIndex);
  }
  
  HeapSort(&realIndices.Front(), realIndices.Size());
}

STDMETHODIMP CAgentFolder::Extract(const UInt32 *indices,
    UInt32 numItems,
    Int32 includeAltStreams,
    Int32 replaceAltStreamColon,
    NExtract::NPathMode::EEnum pathMode,
    NExtract::NOverwriteMode::EEnum overwriteMode,
    const wchar_t *path,
    Int32 testMode,
    IFolderArchiveExtractCallback *extractCallback2)
{
  COM_TRY_BEGIN
  CArchiveExtractCallback *extractCallbackSpec = new CArchiveExtractCallback;
  CMyComPtr<IArchiveExtractCallback> extractCallback = extractCallbackSpec;
  UStringVector pathParts;
  bool isAltStreamFolder = false;
  if (_proxy2)
    _proxy2->GetDirPathParts(_proxyDirIndex, pathParts, isAltStreamFolder);
  else
    _proxy->GetDirPathParts(_proxyDirIndex, pathParts);

  /*
  if (_flatMode)
    pathMode = NExtract::NPathMode::kNoPathnames;
  */

  extractCallbackSpec->InitForMulti(
      false, // multiArchives
      pathMode,
      overwriteMode,
      true  // keepEmptyDirPrefixes
      );

  if (extractCallback2)
    extractCallback2->SetTotal(_agentSpec->GetArc().GetEstmatedPhySize());

  FString pathU;
  if (path)
  {
    pathU = us2fs(path);
    if (!pathU.IsEmpty())
    {
      NFile::NName::NormalizeDirPathPrefix(pathU);
      NFile::NDir::CreateComplexDir(pathU);
    }
  }

  CExtractNtOptions extractNtOptions;
  extractNtOptions.AltStreams.Val = IntToBool(includeAltStreams); // change it!!!
  extractNtOptions.AltStreams.Def = true;

  extractNtOptions.ReplaceColonForAltStream = IntToBool(replaceAltStreamColon);
  
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
    extractCallbackSpec->SetBaseParentFolderIndex(_proxy2->Dirs[_proxyDirIndex].ArcIndex);

  CUIntVector realIndices;
  GetRealIndices(indices, numItems, IntToBool(includeAltStreams),
      false, // includeFolderSubItemsInFlatMode
      realIndices); //

  #ifdef SUPPORT_LINKS

  if (!testMode)
  {
    RINOK(extractCallbackSpec->PrepareHardLinks(&realIndices));
  }
    
  #endif

  {
    CArchiveExtractCallback_Closer ecsCloser(extractCallbackSpec);
    
    HRESULT res = _agentSpec->GetArchive()->Extract(&realIndices.Front(),
        realIndices.Size(), testMode, extractCallback);
    
    HRESULT res2 = ecsCloser.Close();
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
    _isDeviceFile(false)
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

STDMETHODIMP CAgent::Open(
    IInStream *inStream,
    const wchar_t *filePath,
    const wchar_t *arcFormat,
    BSTR *archiveType,
    IArchiveOpenCallback *openArchiveCallback)
{
  COM_TRY_BEGIN
  _archiveFilePath = filePath;
  _attrib = 0;
  NFile::NFind::CFileInfo fi;
  _isDeviceFile = false;
  if (!inStream)
  {
    if (!fi.Find(us2fs(_archiveFilePath)))
      return ::GetLastError();
    if (fi.IsDir())
      return E_FAIL;
    _attrib = fi.Attrib;
    _isDeviceFile = fi.IsDevice;
  }
  CArcInfoEx archiverInfo0, archiverInfo1;

  RINOK(LoadGlobalCodecs());

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
      arc.MTimeDefined = !fi.IsDevice;
      arc.MTime = fi.MTime;
    }
    
    ArchiveType = GetTypeOfArc(arc);
    if (archiveType)
    {
      RINOK(StringToBstr(ArchiveType, archiveType));
    }
  }

  return res;

  COM_TRY_END
}


STDMETHODIMP CAgent::ReOpen(IArchiveOpenCallback *openArchiveCallback)
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

  RINOK(_archiveLink.ReOpen(options));
  return ReadItems();
  COM_TRY_END
}

STDMETHODIMP CAgent::Close()
{
  COM_TRY_BEGIN
  return _archiveLink.Close();
  COM_TRY_END
}

/*
STDMETHODIMP CAgent::EnumProperties(IEnumSTATPROPSTG **EnumProperties)
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
      RINOK(arc.Archive->GetPropertyInfo(i, &name, &propID, &varType));
      if (propID == kpidPath)
        ThereIsPathProp = true;
      /*
      if (propID == kpidIsAltStream)
        ThereIsAltStreamProp = true;
      */
    }
  }

  if (_proxy2)
    return _proxy2->Load(GetArc(), NULL);
  return _proxy->Load(GetArc(), NULL);
}

STDMETHODIMP CAgent::BindToRootFolder(IFolderFolder **resultFolder)
{
  COM_TRY_BEGIN
  if (!_archiveLink.Arcs.IsEmpty())
  {
    RINOK(ReadItems());
  }
  CAgentFolder *folderSpec = new CAgentFolder;
  CMyComPtr<IFolderFolder> rootFolder = folderSpec;
  folderSpec->Init(_proxy, _proxy2, k_Proxy_RootDirIndex, /* NULL, */ this);
  *resultFolder = rootFolder.Detach();
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CAgent::Extract(
    NExtract::NPathMode::EEnum pathMode,
    NExtract::NOverwriteMode::EEnum overwriteMode,
    const wchar_t *path,
    Int32 testMode,
    IFolderArchiveExtractCallback *extractCallback2)
{
  COM_TRY_BEGIN
  CArchiveExtractCallback *extractCallbackSpec = new CArchiveExtractCallback;
  CMyComPtr<IArchiveExtractCallback> extractCallback = extractCallbackSpec;
  extractCallbackSpec->InitForMulti(
      false, // multiArchives
      pathMode,
      overwriteMode,
      true  // keepEmptyDirPrefixes
      );

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

  #ifdef SUPPORT_LINKS

  if (!testMode)
  {
    RINOK(extractCallbackSpec->PrepareHardLinks(NULL)); // NULL means all items
  }
    
  #endif

  return GetArchive()->Extract(0, (UInt32)(Int32)-1, testMode, extractCallback);
  COM_TRY_END
}

STDMETHODIMP CAgent::GetNumberOfProperties(UInt32 *numProps)
{
  COM_TRY_BEGIN
  return GetArchive()->GetNumberOfProperties(numProps);
  COM_TRY_END
}

STDMETHODIMP CAgent::GetPropertyInfo(UInt32 index,
      BSTR *name, PROPID *propID, VARTYPE *varType)
{
  COM_TRY_BEGIN
  RINOK(GetArchive()->GetPropertyInfo(index, name, propID, varType));
  if (*propID == kpidPath)
    *propID = kpidName;
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CAgent::GetArcNumLevels(UInt32 *numLevels)
{
  *numLevels = _archiveLink.Arcs.Size();
  return S_OK;
}

STDMETHODIMP CAgent::GetArcProp(UInt32 level, PROPID propID, PROPVARIANT *value)
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
        UInt32 flags = arc.ErrorInfo.GetErrorFlags();
        if (flags != 0)
          prop = flags;
        break;
      }
      case kpidWarningFlags:
      {
        UInt32 flags = arc.ErrorInfo.GetWarningFlags();
        if (flags != 0)
          prop = flags;
        break;
      }
      case kpidOffset:
      {
        Int64 v = arc.GetGlobalOffset();
        if (v != 0)
          prop = v;
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

STDMETHODIMP CAgent::GetArcNumProps(UInt32 level, UInt32 *numProps)
{
  return _archiveLink.Arcs[level].Archive->GetNumberOfArchiveProperties(numProps);
}

STDMETHODIMP CAgent::GetArcPropInfo(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)
{
  return _archiveLink.Arcs[level].Archive->GetArchivePropertyInfo(index, name, propID, varType);
}

// MainItemProperty
STDMETHODIMP CAgent::GetArcProp2(UInt32 level, PROPID propID, PROPVARIANT *value)
{
  return _archiveLink.Arcs[level - 1].Archive->GetProperty(_archiveLink.Arcs[level].SubfileIndex, propID, value);
}

STDMETHODIMP CAgent::GetArcNumProps2(UInt32 level, UInt32 *numProps)
{
  return _archiveLink.Arcs[level - 1].Archive->GetNumberOfProperties(numProps);
}

STDMETHODIMP CAgent::GetArcPropInfo2(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)
{
  return _archiveLink.Arcs[level - 1].Archive->GetPropertyInfo(index, name, propID, varType);
}
