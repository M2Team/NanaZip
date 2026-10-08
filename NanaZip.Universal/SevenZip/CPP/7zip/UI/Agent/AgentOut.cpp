// AgentOut.cpp

#include "StdAfx.h"

#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/TimeUtils.h"

#include "../../Compress/CopyCoder.h"

#include "../../Common/FileStreams.h"

#include "../../Archive/Common/ItemNameUtils.h"

#include "Agent.h"
#include "UpdateCallbackAgent.h"

using namespace NWindows;
using namespace NCOM;

Z7_COM7F_IMF(CAgent::SetFolder(IFolderFolder *folder))
{
  _updatePathPrefix.Empty();
  _updatePathPrefix_is_AltFolder = false;
  _agentFolder = NULL;

  if (!folder)
    return S_OK;

  {
    Z7_DECL_CMyComPtr_QI_FROM(
        IArchiveFolderInternal,
        afi, folder)
    if (afi)
    {
      RINOK(afi->GetAgentFolder(&_agentFolder))
    }
    if (!_agentFolder)
      return E_FAIL;
  }

  if (_proxy2)
  {
    _updatePathPrefix = _proxy2->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex);
    _updatePathPrefix_is_AltFolder = _proxy2->IsAltDir(_agentFolder->_proxyDirIndex);
  }
  else
    _updatePathPrefix = _proxy->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex);
  return S_OK;
}

Z7_COM7F_IMF(CAgent::SetFiles(const wchar_t *folderPrefix,
    const wchar_t * const *names, UInt32 numNames))
{
  _folderPrefix = us2fs(folderPrefix);
  _names.ClearAndReserve(numNames);
  for (UInt32 i = 0; i < numNames; i++)
    _names.AddInReserved(us2fs(names[i]));
  return S_OK;
}


static HRESULT EnumerateArchiveItems(const CAgent *agent,
    CObjectVector<CArcItem> &arcItems)
{
  CUIntVector vec;
  CArcItem ai;
  ai.Censored = true; // test it
  unsigned prefixLen = 0;
  unsigned dirIndex = k_Proxy_RootDirIndex; // 0
  unsigned i = 0;
  for (;;)
  {
    const CProxyDir &dir = agent->_proxy->Dirs[dirIndex];

    if (i == dir.SubDirs.Size()) // we fill SubFiles after SubDirs
    {
      ai.IsDir = false;
      FOR_VECTOR (k, dir.SubFiles)
      {
        const unsigned arcIndex = dir.SubFiles[k];
        ai.IndexInServer = arcIndex;
        RINOK(agent->GetArc().GetItem_MTime(arcIndex, ai.MTime))
        RINOK(agent->GetArc().GetItem_Size(arcIndex, ai.Size, ai.Size_Defined))
        RINOK(Archive_IsItem_AltStream(agent->GetArc().Archive, arcIndex, ai.IsAltStream))
        ai.Name.DeleteFrom(prefixLen);
        const CProxyFile &file = agent->_proxy->Files[arcIndex];
        ai.Name += file.Name;
        arcItems.Add(ai);
      }

      const unsigned num = vec.Size();
      if (num < 3)
        return S_OK;
      prefixLen = vec[num - 3];
      dirIndex = vec[num - 2];
      i = vec[num - 1];
      vec.DeleteFrom(num - 3);
      continue;
    }

    const unsigned subDirIndex = dir.SubDirs[i];
    i++;
    const CProxyDir &subDir = agent->_proxy->Dirs[subDirIndex];
    ai.Name.DeleteFrom(prefixLen);
    ai.Name += subDir.Name;
    if (subDir.IsLeaf())
    {
      ai.IndexInServer = (unsigned)subDir.ArcIndex;
      RINOK(agent->GetArc().GetItem_MTime((unsigned)subDir.ArcIndex, ai.MTime))
      ai.IsDir = true;
      ai.IsAltStream = false;
      ai.Size_Defined = false;
      ai.Size = 0;
      arcItems.Add(ai);
    }

    vec.Add(prefixLen);
    vec.Add(dirIndex);
    vec.Add(i);
    ai.Name.Add_PathSepar();
    prefixLen = ai.Name.Len();
    if (prefixLen >= (1 << 15) /* || vec.Size() >= (3 << 14) */ ) // NTFS limit
      return E_NOTIMPL;
    dirIndex = subDirIndex;
    i = 0;
  }
}


static HRESULT EnumerateArchiveItems2(const CAgent *agent,
    unsigned dirIndex,
    const UString &prefix,
    CObjectVector<CArcItem> &arcItems)
{
  CUIntVector vec;
  CArcItem ai;
  ai.Censored = true; // test it
  ai.Name = prefix;
  unsigned prefixLen = ai.Name.Len();
  unsigned i = 0;
  for (;;)
  {
    const CProxyDir2 &dir = agent->_proxy2->Dirs[dirIndex];
    if (i == dir.Items.Size())
    {
      const unsigned num = vec.Size();
      if (num < 3)
        return S_OK;
      prefixLen = vec[num - 3];
      dirIndex = vec[num - 2];
      i = vec[num - 1];
      vec.DeleteFrom(num - 3);
      continue;
    }

    const unsigned arcIndex = dir.Items[i];
    i++;
    ai.Name.DeleteFrom(prefixLen);
    const CProxyFile2 &file = agent->_proxy2->Files[arcIndex];
    ai.Name += file.Name;
    // v23.03: we normalize slashes:
    NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(
        ai.Name.Ptr_non_const() + (ai.Name.Len() - file.NameLen), file.NameLen);
    ai.IndexInServer = arcIndex;
    RINOK(agent->GetArc().GetItem_MTime(arcIndex, ai.MTime))
    ai.Size = 0;
    ai.Size_Defined = false;
    ai.IsAltStream = file.IsAltStream;
    ai.IsDir = file.IsDir();
    if (!file.IsDir())
      RINOK(agent->GetArc().GetItem_Size(arcIndex, ai.Size, ai.Size_Defined))

    arcItems.Add(ai);

    if (file.AltDirIndex != -1)
      RINOK(EnumerateArchiveItems2(agent, (unsigned)file.AltDirIndex,
          ai.Name + L':', arcItems))

    if (file.IsDir())
    {
      vec.Add(prefixLen);
      vec.Add(dirIndex);
      vec.Add(i);
      ai.Name.Add_PathSepar();
      prefixLen = ai.Name.Len();
      if (prefixLen >= (1 << 15) /* || vec.Size() >= (3 << 14) */ ) // NTFS limit
        return E_NOTIMPL;
      dirIndex = (unsigned)file.DirIndex;
      i = 0;
    }
  }
}


struct CAgUpCallbackImp Z7_final: public IUpdateProduceCallback
{
  const CObjectVector<CArcItem> *_arcItems;
  IFolderArchiveUpdateCallback *_callback;

  CAgUpCallbackImp(const CObjectVector<CArcItem> *a Z7_lifetimebound,
      IFolderArchiveUpdateCallback *callback Z7_lifetimebound):
    _arcItems(a), _callback(callback) {}
  HRESULT ShowDeleteFile(unsigned arcIndex) Z7_override;
};

HRESULT CAgUpCallbackImp::ShowDeleteFile(unsigned arcIndex)
{
  return _callback->DeleteOperation((*_arcItems)[arcIndex].Name);
}


static void SetInArchiveInterfaces(CAgent *agent, CArchiveUpdateCallback *upd)
{
  if (agent->_archiveLink.Arcs.IsEmpty())
    return;
  const CArc &arc = agent->GetArc();
  upd->Arc = &arc;
  upd->Archive = arc.Archive;

  upd->ArcFileName = ExtractFileNameFromPath(arc.Path);
}

struct CDirItemsCallback_AgentOut Z7_final: public IDirItemsCallback
{
  CMyComPtr<IFolderScanProgress> FolderScanProgress;
  IFolderArchiveUpdateCallback *FolderArchiveUpdateCallback;
  HRESULT ErrorCode;

  CDirItemsCallback_AgentOut(): FolderArchiveUpdateCallback(NULL), ErrorCode(S_OK) {}

  HRESULT ScanError(const FString &name, DWORD systemError) Z7_override
  {
    const HRESULT hres = HRESULT_FROM_WIN32(systemError);
    if (FolderArchiveUpdateCallback)
      return FolderScanProgress->ScanError(fs2us(name), hres);
    ErrorCode = hres;
    return ErrorCode;
  }

  HRESULT ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir) Z7_override
  {
    if (FolderScanProgress)
      return FolderScanProgress->ScanProgress(st.NumDirs, st.NumFiles + st.NumAltStreams,
          st.GetTotalBytes(), fs2us(path), BoolToInt(isDir));
    if (FolderArchiveUpdateCallback)
      return FolderArchiveUpdateCallback->SetNumFiles(st.NumFiles);
    return S_OK;
  }
};


Z7_COM7F_IMF(CAgent::DoOperation(
    FStringVector *requestedPaths,
    FStringVector *processedPaths,
    CCodecs *codecs,
    int formatIndex,
    ISequentialOutStream *outArchiveStream,
    const Byte *stateActions,
    const wchar_t *sfxModule,
    IFolderArchiveUpdateCallback *updateCallback100))
{
  if (!CanUpdate())
    return E_NOTIMPL;

  NUpdateArchive::CActionSet actionSet;
  {
    for (unsigned i = 0; i < NUpdateArchive::NPairState::kNumValues; i++)
      actionSet.StateActions[i] = (NUpdateArchive::NPairAction::EEnum)stateActions[i];
  }

  CDirItemsCallback_AgentOut enumCallback;
  if (updateCallback100)
  {
    enumCallback.FolderArchiveUpdateCallback = updateCallback100;
    updateCallback100->QueryInterface(IID_IFolderScanProgress, (void **)&enumCallback.FolderScanProgress);
  }

  CDirItems dirItems;
  dirItems.Callback = &enumCallback;

  {
    FString folderPrefix = _folderPrefix;
    if (!NFile::NName::IsAltStreamPrefixWithColon(fs2us(folderPrefix)))
      NFile::NName::NormalizeDirPathPrefix(folderPrefix);

    RINOK(dirItems.EnumerateItems2(folderPrefix, _updatePathPrefix, _names, requestedPaths))

    if (_updatePathPrefix_is_AltFolder)
    {
      FOR_VECTOR(i, dirItems.Items)
      {
        CDirItem &item = dirItems.Items[i];
        if (item.IsDir())
          return E_NOTIMPL;
        item.IsAltStream = true;
      }
    }
  }

  CMyComPtr<IOutArchive> outArchive;

  if (GetArchive())
  {
    RINOK(GetArchive()->QueryInterface(IID_IOutArchive, (void **)&outArchive))
  }
  else
  {
    if (formatIndex < 0)
      return E_FAIL;
    RINOK(codecs->CreateOutArchive((unsigned)formatIndex, outArchive))

    #ifdef Z7_EXTERNAL_CODECS
    {
      CMyComPtr<ISetCompressCodecsInfo> setCompressCodecsInfo;
      outArchive.QueryInterface(IID_ISetCompressCodecsInfo, (void **)&setCompressCodecsInfo);
      if (setCompressCodecsInfo)
      {
        RINOK(setCompressCodecsInfo->SetCompressCodecsInfo(codecs))
      }
    }
    #endif
  }

  NFileTimeType::EEnum fileTimeType = NFileTimeType::kNotDefined;
  UInt32 value;
  RINOK(outArchive->GetFileTimeType(&value))
  // we support any future fileType here.
  // 22.00:
  fileTimeType = (NFileTimeType::EEnum)value;
  /*
  switch (value)
  {
    case NFileTimeType::kWindows:
    case NFileTimeType::kDOS:
    case NFileTimeType::kUnix:
      fileTimeType = NFileTimeType::EEnum(value);
      break;
    default:
    {
      return E_FAIL;
    }
  }
  */


  CObjectVector<CArcItem> arcItems;
  if (GetArchive())
  {
    RINOK(ReadItems())
    if (_proxy2)
    {
      RINOK(EnumerateArchiveItems2(this, k_Proxy2_RootDirIndex, L"", arcItems))
      RINOK(EnumerateArchiveItems2(this, k_Proxy2_AltRootDirIndex, L":", arcItems))
    }
    else
    {
      RINOK(EnumerateArchiveItems(this, arcItems))
    }
  }

  CRecordVector<CUpdatePair2> updatePairs2;

  {
    CRecordVector<CUpdatePair> updatePairs;
    GetUpdatePairInfoList(dirItems, arcItems, fileTimeType, updatePairs);
    CAgUpCallbackImp upCallback(&arcItems, updateCallback100);
    UpdateProduce(updatePairs, actionSet, updatePairs2, &upCallback);
  }

  UInt32 numFiles = 0;
  {
    FOR_VECTOR (i, updatePairs2)
      if (updatePairs2[i].NewData)
        numFiles++;
  }

  if (updateCallback100)
  {
    RINOK(updateCallback100->SetNumFiles(numFiles))
  }

  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CMyComPtr2_Create<IArchiveUpdateCallback, CArchiveUpdateCallback> updateCallback;

  updateCallback->DirItems = &dirItems;
  updateCallback->ArcItems = &arcItems;
  updateCallback->UpdatePairs = &updatePairs2;

  SetInArchiveInterfaces(this, updateCallback.ClsPtr());

  updateCallback->Callback = &updateCallbackAgent;

  CByteBuffer processedItems;
  if (processedPaths)
  {
    unsigned num = dirItems.Items.Size();
    processedItems.Alloc(num);
    for (unsigned i = 0; i < num; i++)
      processedItems[i] = 0;
    updateCallback->ProcessedItemsStatuses = processedItems;
  }

  Z7_DECL_CMyComPtr_QI_FROM(
      ISetProperties,
      setProperties, outArchive)
  if (setProperties)
  {
    if (m_PropNames.Size() == 0)
    {
      RINOK(setProperties->SetProperties(NULL, NULL, 0))
    }
    else
    {
      CRecordVector<const wchar_t *> names;
      FOR_VECTOR (i, m_PropNames)
        names.Add((const wchar_t *)m_PropNames[i]);

      CPropVariant *propValues = new CPropVariant[m_PropValues.Size()];
      try
      {
        FOR_VECTOR (i, m_PropValues)
          propValues[i] = m_PropValues[i];
        RINOK(setProperties->SetProperties(names.ConstData(), propValues, names.Size()))
      }
      catch(...)
      {
        delete []propValues;
        return E_FAIL;
      }
      delete []propValues;
    }
  }
  m_PropNames.Clear();
  m_PropValues.Clear();

  if (sfxModule != NULL)
  {
    CMyComPtr2_Create<IInStream, CInFileStream> sfxStream;
    if (!sfxStream->Open(us2fs(sfxModule)))
      return E_FAIL;
      // throw "Can't open sfx module";
    RINOK(NCompress::CopyStream(sfxStream, outArchiveStream, NULL))
  }

  const HRESULT res = outArchive->UpdateItems(outArchiveStream, updatePairs2.Size(), updateCallback);
  if (res == S_OK && processedPaths)
  {
    {
      /* OutHandler for 7z archives doesn't report compression operation for empty files.
         So we must include these files manually */
      FOR_VECTOR(i, updatePairs2)
      {
        const CUpdatePair2 &up = updatePairs2[i];
        if (up.DirIndex != -1 && up.NewData)
        {
          const CDirItem &di = dirItems.Items[(unsigned)up.DirIndex];
          if (!di.IsDir() && di.Size == 0)
            processedItems[(unsigned)up.DirIndex] = 1;
        }
      }
    }

    FOR_VECTOR (i, dirItems.Items)
      if (processedItems[i] != 0)
        processedPaths->Add(dirItems.GetPhyPath(i));
  }
  return res;
}

Z7_COM7F_IMF(CAgent::DoOperation2(
    FStringVector *requestedPaths,
    FStringVector *processedPaths,
    ISequentialOutStream *outArchiveStream,
    const Byte *stateActions, const wchar_t *sfxModule, IFolderArchiveUpdateCallback *updateCallback100))
{
  return DoOperation(requestedPaths, processedPaths, g_CodecsObj, -1, outArchiveStream, stateActions, sfxModule, updateCallback100);
}

HRESULT CAgent::CommonUpdate(ISequentialOutStream *outArchiveStream,
    unsigned numUpdateItems, IArchiveUpdateCallback *updateCallback)
{
  if (!CanUpdate())
    return E_NOTIMPL;
  CMyComPtr<IOutArchive> outArchive;
  RINOK(GetArchive()->QueryInterface(IID_IOutArchive, (void **)&outArchive))
  return outArchive->UpdateItems(outArchiveStream, numUpdateItems, updateCallback);
}

Z7_COM7F_IMF(CAgent::DeleteItems(ISequentialOutStream *outArchiveStream,
    const UInt32 *indices, UInt32 numItems,
    IFolderArchiveUpdateCallback *updateCallback100))
{
  if (!CanUpdate())
    return E_NOTIMPL;
  CRecordVector<CUpdatePair2> updatePairs;
  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CMyComPtr2_Create<IArchiveUpdateCallback, CArchiveUpdateCallback> updateCallback;

  CUIntVector realIndices;
  _agentFolder->GetRealIndices(indices, numItems,
      true, // includeAltStreams
      false, // includeFolderSubItemsInFlatMode, we don't want to delete subItems in Flat Mode
      realIndices);
  unsigned curIndex = 0;
  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive))

  UString deletePath;

  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    if (curIndex < realIndices.Size())
      if (realIndices[curIndex] == i)
      {
        RINOK(GetArc().GetItem_Path2(i, deletePath))
        RINOK(updateCallback100->DeleteOperation(deletePath))

        curIndex++;
        continue;
      }
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    updatePairs.Add(up2);
  }
  updateCallback->UpdatePairs = &updatePairs;

  SetInArchiveInterfaces(this, updateCallback.ClsPtr());

  updateCallback->Callback = &updateCallbackAgent;
  return CommonUpdate(outArchiveStream, updatePairs.Size(), updateCallback);
}

HRESULT CAgent::CreateFolder(ISequentialOutStream *outArchiveStream,
    const wchar_t *folderName, IFolderArchiveUpdateCallback *updateCallback100)
{
  if (!CanUpdate())
    return E_NOTIMPL;
  CRecordVector<CUpdatePair2> updatePairs;
  CDirItems dirItems;
  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CMyComPtr2_Create<IArchiveUpdateCallback, CArchiveUpdateCallback> updateCallback;

  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive))
  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    updatePairs.Add(up2);
  }
  CUpdatePair2 up2;
  up2.Construct();
  up2.NewData = up2.NewProps = true;
  up2.UseArcProps = false;
  up2.DirIndex = 0;

  updatePairs.Add(up2);

  updatePairs.ReserveDown();

  CDirItem di;

  di.Attrib = FILE_ATTRIBUTE_DIRECTORY;
  di.Size = 0;
  if (_proxy2)
    di.Name = _proxy2->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex /* , isAltStreamFolder */);
  else
    di.Name = _proxy->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex);
  di.Name += folderName;

  FILETIME ft;
  NTime::GetCurUtcFileTime(ft);
  di.CTime = di.ATime = di.MTime = ft;

  dirItems.Items.Add(di);

  updateCallback->Callback = &updateCallbackAgent;
  updateCallback->DirItems = &dirItems;
  updateCallback->UpdatePairs = &updatePairs;

  SetInArchiveInterfaces(this, updateCallback.ClsPtr());

  return CommonUpdate(outArchiveStream, updatePairs.Size(), updateCallback);
}


HRESULT CAgent::RenameItem(ISequentialOutStream *outArchiveStream,
    const UInt32 *indices, UInt32 numItems, const wchar_t *newItemName,
    IFolderArchiveUpdateCallback *updateCallback100)
{
  if (!CanUpdate())
    return E_NOTIMPL;
  if (numItems != 1)
    return E_INVALIDARG;
  if (!_archiveLink.IsOpen)
    return E_FAIL;
  CRecordVector<CUpdatePair2> updatePairs;
  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CMyComPtr2_Create<IArchiveUpdateCallback, CArchiveUpdateCallback> updateCallback;

  CUIntVector realIndices;
  _agentFolder->GetRealIndices(indices, numItems,
      true, // includeAltStreams
      true, // includeFolderSubItemsInFlatMode
      realIndices);

  const UInt32 ind0 = indices[0];
  const int mainRealIndex = _agentFolder->GetRealIndex(ind0);
  const UString fullPrefix = _agentFolder->GetFullPrefix(ind0);
  UString name = _agentFolder->GetName(ind0);
  // 22.00 : we normalize name
  NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(name);
  const UString oldItemPath = fullPrefix + name;
  const UString newItemPath = fullPrefix + newItemName;

  UStringVector newNames;

  unsigned curIndex = 0;
  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive))

  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    if (curIndex < realIndices.Size())
      if (realIndices[curIndex] == i)
      {
        up2.NewProps = true;
        RINOK(GetArc().IsItem_Anti(i, up2.IsAnti)) // it must work without that line too.

        UString oldFullPath;
        RINOK(GetArc().GetItem_Path2(i, oldFullPath))

        if (!IsPath1PrefixedByPath2(oldFullPath, oldItemPath))
          return E_INVALIDARG;

        up2.NewNameIndex = (int)newNames.Add(newItemPath + oldFullPath.Ptr(oldItemPath.Len()));
        up2.IsMainRenameItem = (mainRealIndex == (int)i);
        curIndex++;
      }
    updatePairs.Add(up2);
  }

  updateCallback->Callback = &updateCallbackAgent;
  updateCallback->UpdatePairs = &updatePairs;
  updateCallback->NewNames = &newNames;

  SetInArchiveInterfaces(this, updateCallback.ClsPtr());

  return CommonUpdate(outArchiveStream, updatePairs.Size(), updateCallback);
}


HRESULT CAgent::CommentItem(ISequentialOutStream *outArchiveStream,
    const UInt32 *indices, UInt32 numItems, const wchar_t *newItemName,
    IFolderArchiveUpdateCallback *updateCallback100)
{
  if (!CanUpdate())
    return E_NOTIMPL;
  if (numItems != 1)
    return E_INVALIDARG;
  if (!_archiveLink.IsOpen)
    return E_FAIL;

  CRecordVector<CUpdatePair2> updatePairs;
  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CMyComPtr2_Create<IArchiveUpdateCallback, CArchiveUpdateCallback> updateCallback;

  const int mainRealIndex = _agentFolder->GetRealIndex(indices[0]);

  if (mainRealIndex < 0)
    return E_NOTIMPL;

  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive))

  UString newName = newItemName;

  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    if ((int)i == mainRealIndex)
      up2.NewProps = true;
    updatePairs.Add(up2);
  }

  updateCallback->Callback = &updateCallbackAgent;
  updateCallback->UpdatePairs = &updatePairs;
  updateCallback->CommentIndex = mainRealIndex;
  updateCallback->Comment = &newName;

  SetInArchiveInterfaces(this, updateCallback.ClsPtr());

  return CommonUpdate(outArchiveStream, updatePairs.Size(), updateCallback);
}



HRESULT CAgent::UpdateOneFile(ISequentialOutStream *outArchiveStream,
    const UInt32 *indices, UInt32 numItems, const wchar_t *diskFilePath,
    IFolderArchiveUpdateCallback *updateCallback100)
{
  if (!CanUpdate())
    return E_NOTIMPL;
  CRecordVector<CUpdatePair2> updatePairs;
  CDirItems dirItems;
  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CMyComPtr2_Create<IArchiveUpdateCallback, CArchiveUpdateCallback> updateCallback;

  UInt32 realIndex;
  {
    CUIntVector realIndices;
    _agentFolder->GetRealIndices(indices, numItems,
        false, // includeAltStreams // we update only main stream of file
        false, // includeFolderSubItemsInFlatMode
        realIndices);
    if (realIndices.Size() != 1)
      return E_FAIL;
    realIndex = realIndices[0];
  }

  {
    FStringVector filePaths;
    filePaths.Add(us2fs(diskFilePath));
    dirItems.EnumerateItems2(FString(), UString(), filePaths, NULL);
    if (dirItems.Items.Size() != 1)
      return E_FAIL;
  }

  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive))
  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    if (realIndex == i)
    {
      up2.DirIndex = 0;
      up2.NewData = true;
      up2.NewProps = true;
      up2.UseArcProps = false;
    }
    updatePairs.Add(up2);
  }
  updateCallback->DirItems = &dirItems;
  updateCallback->Callback = &updateCallbackAgent;
  updateCallback->UpdatePairs = &updatePairs;

  SetInArchiveInterfaces(this, updateCallback.ClsPtr());

  updateCallback->KeepOriginalItemNames = true;
  return CommonUpdate(outArchiveStream, updatePairs.Size(), updateCallback);
}

Z7_COM7F_IMF(CAgent::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps))
{
  m_PropNames.Clear();
  m_PropValues.Clear();
  for (UInt32 i = 0; i < numProps; i++)
  {
    m_PropNames.Add(names[i]);
    m_PropValues.Add(values[i]);
  }
  return S_OK;
}
