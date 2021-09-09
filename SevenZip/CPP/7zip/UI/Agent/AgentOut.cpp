// AgentOut.cpp

#include "StdAfx.h"

#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/TimeUtils.h"

#include "../../Compress/CopyCoder.h"

#include "../../Common/FileStreams.h"

#include "Agent.h"
#include "UpdateCallbackAgent.h"

using namespace NWindows;
using namespace NCOM;

STDMETHODIMP CAgent::SetFolder(IFolderFolder *folder)
{
  _updatePathPrefix.Empty();
  _updatePathPrefix_is_AltFolder = false;
  _agentFolder = NULL;

  if (!folder)
    return S_OK;

  {
    CMyComPtr<IArchiveFolderInternal> afi;
    RINOK(folder->QueryInterface(IID_IArchiveFolderInternal, (void **)&afi));
    if (afi)
    {
      RINOK(afi->GetAgentFolder(&_agentFolder));
    }
    if (!_agentFolder)
      return E_FAIL;
  }

  if (_proxy2)
    _updatePathPrefix = _proxy2->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex, _updatePathPrefix_is_AltFolder);
  else
    _updatePathPrefix = _proxy->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex);
  return S_OK;
}

STDMETHODIMP CAgent::SetFiles(const wchar_t *folderPrefix,
    const wchar_t * const *names, UInt32 numNames)
{
  _folderPrefix = us2fs(folderPrefix);
  _names.ClearAndReserve(numNames);
  for (UInt32 i = 0; i < numNames; i++)
    _names.AddInReserved(us2fs(names[i]));
  return S_OK;
}

static HRESULT EnumerateArchiveItems(CAgent *agent,
    const CProxyDir &item,
    const UString &prefix,
    CObjectVector<CArcItem> &arcItems)
{
  unsigned i;
  
  for (i = 0; i < item.SubFiles.Size(); i++)
  {
    unsigned arcIndex = item.SubFiles[i];
    const CProxyFile &fileItem = agent->_proxy->Files[arcIndex];
    CArcItem ai;
    RINOK(agent->GetArc().GetItemMTime(arcIndex, ai.MTime, ai.MTimeDefined));
    RINOK(agent->GetArc().GetItemSize(arcIndex, ai.Size, ai.SizeDefined));
    ai.IsDir = false;
    ai.Name = prefix + fileItem.Name;
    ai.Censored = true; // test it
    ai.IndexInServer = arcIndex;
    arcItems.Add(ai);
  }
  
  for (i = 0; i < item.SubDirs.Size(); i++)
  {
    const CProxyDir &dirItem = agent->_proxy->Dirs[item.SubDirs[i]];
    UString fullName = prefix + dirItem.Name;
    if (dirItem.IsLeaf())
    {
      CArcItem ai;
      RINOK(agent->GetArc().GetItemMTime(dirItem.ArcIndex, ai.MTime, ai.MTimeDefined));
      ai.IsDir = true;
      ai.SizeDefined = false;
      ai.Name = fullName;
      ai.Censored = true; // test it
      ai.IndexInServer = dirItem.ArcIndex;
      arcItems.Add(ai);
    }
    RINOK(EnumerateArchiveItems(agent, dirItem, fullName + WCHAR_PATH_SEPARATOR, arcItems));
  }
  
  return S_OK;
}

static HRESULT EnumerateArchiveItems2(const CAgent *agent,
    unsigned dirIndex,
    const UString &prefix,
    CObjectVector<CArcItem> &arcItems)
{
  const CProxyDir2 &dir = agent->_proxy2->Dirs[dirIndex];
  FOR_VECTOR (i, dir.Items)
  {
    unsigned arcIndex = dir.Items[i];
    const CProxyFile2 &file = agent->_proxy2->Files[arcIndex];
    CArcItem ai;
    ai.IndexInServer = arcIndex;
    ai.Name = prefix + file.Name;
    ai.Censored = true; // test it
    RINOK(agent->GetArc().GetItemMTime(arcIndex, ai.MTime, ai.MTimeDefined));
    ai.IsDir = file.IsDir();
    ai.SizeDefined = false;
    ai.IsAltStream = file.IsAltStream;
    if (!ai.IsDir)
    {
      RINOK(agent->GetArc().GetItemSize(arcIndex, ai.Size, ai.SizeDefined));
      ai.IsDir = false;
    }
    arcItems.Add(ai);
    
    if (file.AltDirIndex >= 0)
    {
      RINOK(EnumerateArchiveItems2(agent, file.AltDirIndex, ai.Name + L':', arcItems));
    }
    
    if (ai.IsDir)
    {
      RINOK(EnumerateArchiveItems2(agent, file.DirIndex, ai.Name + WCHAR_PATH_SEPARATOR, arcItems));
    }
  }
  return S_OK;
}

struct CAgUpCallbackImp: public IUpdateProduceCallback
{
  const CObjectVector<CArcItem> *_arcItems;
  IFolderArchiveUpdateCallback *_callback;
  
  CAgUpCallbackImp(const CObjectVector<CArcItem> *a,
      IFolderArchiveUpdateCallback *callback): _arcItems(a), _callback(callback) {}
  HRESULT ShowDeleteFile(unsigned arcIndex);
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
}

struct CDirItemsCallback_AgentOut: public IDirItemsCallback
{
  CMyComPtr<IFolderScanProgress> FolderScanProgress;
  IFolderArchiveUpdateCallback *FolderArchiveUpdateCallback;
  HRESULT ErrorCode;
  
  CDirItemsCallback_AgentOut(): FolderArchiveUpdateCallback(NULL), ErrorCode(S_OK) {}

  HRESULT ScanError(const FString &name, DWORD systemError)
  {
    HRESULT hres = HRESULT_FROM_WIN32(systemError);
    if (FolderArchiveUpdateCallback)
      return FolderScanProgress->ScanError(fs2us(name), hres);
    ErrorCode = hres;
    return ErrorCode;
  }

  HRESULT ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)
  {
    if (FolderScanProgress)
      return FolderScanProgress->ScanProgress(st.NumDirs, st.NumFiles + st.NumAltStreams,
          st.GetTotalBytes(), fs2us(path), BoolToInt(isDir));
    
    if (FolderArchiveUpdateCallback)
      return FolderArchiveUpdateCallback->SetNumFiles(st.NumFiles);

    return S_OK;
  }
};

STDMETHODIMP CAgent::DoOperation(
    FStringVector *requestedPaths,
    FStringVector *processedPaths,
    CCodecs *codecs,
    int formatIndex,
    ISequentialOutStream *outArchiveStream,
    const Byte *stateActions,
    const wchar_t *sfxModule,
    IFolderArchiveUpdateCallback *updateCallback100)
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
    NFile::NName::NormalizeDirPathPrefix(folderPrefix);
    
    RINOK(dirItems.EnumerateItems2(folderPrefix, _updatePathPrefix, _names, requestedPaths));

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
    RINOK(GetArchive()->QueryInterface(IID_IOutArchive, (void **)&outArchive));
  }
  else
  {
    if (formatIndex < 0)
      return E_FAIL;
    RINOK(codecs->CreateOutArchive(formatIndex, outArchive));
    
    #ifdef EXTERNAL_CODECS
    {
      CMyComPtr<ISetCompressCodecsInfo> setCompressCodecsInfo;
      outArchive.QueryInterface(IID_ISetCompressCodecsInfo, (void **)&setCompressCodecsInfo);
      if (setCompressCodecsInfo)
      {
        RINOK(setCompressCodecsInfo->SetCompressCodecsInfo(codecs));
      }
    }
    #endif
  }

  NFileTimeType::EEnum fileTimeType;
  UInt32 value;
  RINOK(outArchive->GetFileTimeType(&value));

  switch (value)
  {
    case NFileTimeType::kWindows:
    case NFileTimeType::kDOS:
    case NFileTimeType::kUnix:
      fileTimeType = NFileTimeType::EEnum(value);
      break;
    default:
      return E_FAIL;
  }


  CObjectVector<CArcItem> arcItems;
  if (GetArchive())
  {
    RINOK(ReadItems());
    if (_proxy2)
    {
      RINOK(EnumerateArchiveItems2(this, k_Proxy2_RootDirIndex, L"", arcItems));
      RINOK(EnumerateArchiveItems2(this, k_Proxy2_AltRootDirIndex, L":", arcItems));
    }
    else
    {
      RINOK(EnumerateArchiveItems(this, _proxy->Dirs[0], L"", arcItems));
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
    RINOK(updateCallback100->SetNumFiles(numFiles));
  }
  
  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CArchiveUpdateCallback *updateCallbackSpec = new CArchiveUpdateCallback;
  CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec );

  updateCallbackSpec->DirItems = &dirItems;
  updateCallbackSpec->ArcItems = &arcItems;
  updateCallbackSpec->UpdatePairs = &updatePairs2;
  
  SetInArchiveInterfaces(this, updateCallbackSpec);
  
  updateCallbackSpec->Callback = &updateCallbackAgent;

  CByteBuffer processedItems;
  if (processedPaths)
  {
    unsigned num = dirItems.Items.Size();
    processedItems.Alloc(num);
    for (unsigned i = 0; i < num; i++)
      processedItems[i] = 0;
    updateCallbackSpec->ProcessedItemsStatuses = processedItems;
  }

  CMyComPtr<ISetProperties> setProperties;
  if (outArchive->QueryInterface(IID_ISetProperties, (void **)&setProperties) == S_OK)
  {
    if (m_PropNames.Size() == 0)
    {
      RINOK(setProperties->SetProperties(0, 0, 0));
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
        RINOK(setProperties->SetProperties(&names.Front(), propValues, names.Size()));
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
    CInFileStream *sfxStreamSpec = new CInFileStream;
    CMyComPtr<IInStream> sfxStream(sfxStreamSpec);
    if (!sfxStreamSpec->Open(us2fs(sfxModule)))
      return E_FAIL;
      // throw "Can't open sfx module";
    RINOK(NCompress::CopyStream(sfxStream, outArchiveStream, NULL));
  }

  HRESULT res = outArchive->UpdateItems(outArchiveStream, updatePairs2.Size(), updateCallback);
  if (res == S_OK && processedPaths)
  {
    {
      /* OutHandler for 7z archives doesn't report compression operation for empty files.
         So we must include these files manually */
      FOR_VECTOR(i, updatePairs2)
      {
        const CUpdatePair2 &up = updatePairs2[i];
        if (up.DirIndex >= 0 && up.NewData)
        {
          const CDirItem &di = dirItems.Items[up.DirIndex];
          if (!di.IsDir() && di.Size == 0)
            processedItems[up.DirIndex] = 1;
        }
      }
    }

    FOR_VECTOR (i, dirItems.Items)
      if (processedItems[i] != 0)
        processedPaths->Add(dirItems.GetPhyPath(i));
  }
  return res;
}

STDMETHODIMP CAgent::DoOperation2(
    FStringVector *requestedPaths,
    FStringVector *processedPaths,
    ISequentialOutStream *outArchiveStream,
    const Byte *stateActions, const wchar_t *sfxModule, IFolderArchiveUpdateCallback *updateCallback100)
{
  return DoOperation(requestedPaths, processedPaths, g_CodecsObj, -1, outArchiveStream, stateActions, sfxModule, updateCallback100);
}

HRESULT CAgent::CommonUpdate(ISequentialOutStream *outArchiveStream,
    unsigned numUpdateItems, IArchiveUpdateCallback *updateCallback)
{
  if (!CanUpdate())
    return E_NOTIMPL;
  CMyComPtr<IOutArchive> outArchive;
  RINOK(GetArchive()->QueryInterface(IID_IOutArchive, (void **)&outArchive));
  return outArchive->UpdateItems(outArchiveStream, numUpdateItems, updateCallback);
}

STDMETHODIMP CAgent::DeleteItems(ISequentialOutStream *outArchiveStream,
    const UInt32 *indices, UInt32 numItems,
    IFolderArchiveUpdateCallback *updateCallback100)
{
  if (!CanUpdate())
    return E_NOTIMPL;
  CRecordVector<CUpdatePair2> updatePairs;
  CUpdateCallbackAgent updateCallbackAgent;
  updateCallbackAgent.SetCallback(updateCallback100);
  CArchiveUpdateCallback *updateCallbackSpec = new CArchiveUpdateCallback;
  CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec);
  
  CUIntVector realIndices;
  _agentFolder->GetRealIndices(indices, numItems,
      true, // includeAltStreams
      false, // includeFolderSubItemsInFlatMode, we don't want to delete subItems in Flat Mode
      realIndices);
  unsigned curIndex = 0;
  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive));

  UString deletePath;

  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    if (curIndex < realIndices.Size())
      if (realIndices[curIndex] == i)
      {
        RINOK(GetArc().GetItemPath2(i, deletePath));
        RINOK(updateCallback100->DeleteOperation(deletePath));
        
        curIndex++;
        continue;
      }
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    updatePairs.Add(up2);
  }
  updateCallbackSpec->UpdatePairs = &updatePairs;

  SetInArchiveInterfaces(this, updateCallbackSpec);

  updateCallbackSpec->Callback = &updateCallbackAgent;
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
  CArchiveUpdateCallback *updateCallbackSpec = new CArchiveUpdateCallback;
  CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec);

  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive));
  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    updatePairs.Add(up2);
  }
  CUpdatePair2 up2;
  up2.NewData = up2.NewProps = true;
  up2.UseArcProps = false;
  up2.DirIndex = 0;

  updatePairs.Add(up2);

  updatePairs.ReserveDown();

  CDirItem di;

  di.Attrib = FILE_ATTRIBUTE_DIRECTORY;
  di.Size = 0;
  bool isAltStreamFolder = false;
  if (_proxy2)
    di.Name = _proxy2->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex, isAltStreamFolder);
  else
    di.Name = _proxy->GetDirPath_as_Prefix(_agentFolder->_proxyDirIndex);
  di.Name += folderName;

  FILETIME ft;
  NTime::GetCurUtcFileTime(ft);
  di.CTime = di.ATime = di.MTime = ft;

  dirItems.Items.Add(di);

  updateCallbackSpec->Callback = &updateCallbackAgent;
  updateCallbackSpec->DirItems = &dirItems;
  updateCallbackSpec->UpdatePairs = &updatePairs;
  
  SetInArchiveInterfaces(this, updateCallbackSpec);
  
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
  CArchiveUpdateCallback *updateCallbackSpec = new CArchiveUpdateCallback;
  CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec);
  
  CUIntVector realIndices;
  _agentFolder->GetRealIndices(indices, numItems,
      true, // includeAltStreams
      true, // includeFolderSubItemsInFlatMode
      realIndices);

  int mainRealIndex = _agentFolder->GetRealIndex(indices[0]);

  UString fullPrefix = _agentFolder->GetFullPrefix(indices[0]);
  UString oldItemPath = fullPrefix + _agentFolder->GetName(indices[0]);
  UString newItemPath = fullPrefix + newItemName;

  UStringVector newNames;

  unsigned curIndex = 0;
  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive));
  
  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    if (curIndex < realIndices.Size())
      if (realIndices[curIndex] == i)
      {
        up2.NewProps = true;
        RINOK(GetArc().IsItemAnti(i, up2.IsAnti)); // it must work without that line too.

        UString oldFullPath;
        RINOK(GetArc().GetItemPath2(i, oldFullPath));

        if (!IsPath1PrefixedByPath2(oldFullPath, oldItemPath))
          return E_INVALIDARG;

        up2.NewNameIndex = newNames.Add(newItemPath + oldFullPath.Ptr(oldItemPath.Len()));
        up2.IsMainRenameItem = (mainRealIndex == (int)i);
        curIndex++;
      }
    updatePairs.Add(up2);
  }
  
  updateCallbackSpec->Callback = &updateCallbackAgent;
  updateCallbackSpec->UpdatePairs = &updatePairs;
  updateCallbackSpec->NewNames = &newNames;

  SetInArchiveInterfaces(this, updateCallbackSpec);

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
  CArchiveUpdateCallback *updateCallbackSpec = new CArchiveUpdateCallback;
  CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec);
  
  const int mainRealIndex = _agentFolder->GetRealIndex(indices[0]);

  if (mainRealIndex < 0)
    return E_NOTIMPL;

  UInt32 numItemsInArchive;
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive));

  UString newName = newItemName;
  
  for (UInt32 i = 0; i < numItemsInArchive; i++)
  {
    CUpdatePair2 up2;
    up2.SetAs_NoChangeArcItem(i);
    if ((int)i == mainRealIndex)
      up2.NewProps = true;
    updatePairs.Add(up2);
  }
  
  updateCallbackSpec->Callback = &updateCallbackAgent;
  updateCallbackSpec->UpdatePairs = &updatePairs;
  updateCallbackSpec->CommentIndex = mainRealIndex;
  updateCallbackSpec->Comment = &newName;

  SetInArchiveInterfaces(this, updateCallbackSpec);

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
  CArchiveUpdateCallback *updateCallbackSpec = new CArchiveUpdateCallback;
  CMyComPtr<IArchiveUpdateCallback> updateCallback(updateCallbackSpec);
  
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
  RINOK(GetArchive()->GetNumberOfItems(&numItemsInArchive));
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
  updateCallbackSpec->DirItems = &dirItems;
  updateCallbackSpec->Callback = &updateCallbackAgent;
  updateCallbackSpec->UpdatePairs = &updatePairs;
 
  SetInArchiveInterfaces(this, updateCallbackSpec);
  
  updateCallbackSpec->KeepOriginalItemNames = true;
  return CommonUpdate(outArchiveStream, updatePairs.Size(), updateCallback);
}

STDMETHODIMP CAgent::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
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
