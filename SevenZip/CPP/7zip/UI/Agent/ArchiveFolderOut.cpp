// ArchiveFolderOut.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "../../../Windows/FileDir.h"

#include "../../Common/FileStreams.h"
#include "../../Common/LimitedStreams.h"

#include "../../Compress/CopyCoder.h"

#include "../Common/WorkDir.h"

#include "Agent.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

void CAgentFolder::GetPathParts(UStringVector &pathParts, bool &isAltStreamFolder)
{
  if (_proxy2)
    _proxy2->GetDirPathParts(_proxyDirIndex, pathParts, isAltStreamFolder);
  else
    _proxy->GetDirPathParts(_proxyDirIndex, pathParts);
}

static bool Delete_EmptyFolder_And_EmptySubFolders(const FString &path)
{
  {
    const FString pathPrefix = path + FCHAR_PATH_SEPARATOR;
    CObjectVector<FString> names;
    {
      NFind::CDirEntry fileInfo;
      NFind::CEnumerator enumerator;
      enumerator.SetDirPrefix(pathPrefix);
      for (;;)
      {
        bool found;
        if (!enumerator.Next(fileInfo, found))
          return false;
        if (!found)
          break;
        if (fileInfo.IsDir())
          names.Add(fileInfo.Name);
      }
    }
    bool res = true;
    FOR_VECTOR (i, names)
    {
      if (!Delete_EmptyFolder_And_EmptySubFolders(pathPrefix + names[i]))
        res = false;
    }
    if (!res)
      return false;
  }
  // we clear read-only attrib to remove read-only dir
  if (!SetFileAttrib(path, 0))
    return false;
  return RemoveDir(path);
}

HRESULT CAgentFolder::CommonUpdateOperation(
    AGENT_OP operation,
    bool moveMode,
    const wchar_t *newItemName,
    const NUpdateArchive::CActionSet *actionSet,
    const UInt32 *indices, UInt32 numItems,
    IProgress *progress)
{
  if (!_agentSpec->CanUpdate())
    return E_NOTIMPL;

  CMyComPtr<IFolderArchiveUpdateCallback> updateCallback100;
  if (progress)
    progress->QueryInterface(IID_IFolderArchiveUpdateCallback, (void **)&updateCallback100);

  try
  {

  RINOK(_agentSpec->SetFolder(this));

  // ---------- Save FolderItem ----------

  UStringVector pathParts;
  bool isAltStreamFolder = false;
  GetPathParts(pathParts, isAltStreamFolder);

  FStringVector requestedPaths;
  FStringVector processedPaths;

  CWorkDirTempFile tempFile;
  RINOK(tempFile.CreateTempFile(us2fs(_agentSpec->_archiveFilePath)));
  {
    CMyComPtr<IOutStream> tailStream;
    const CArc &arc = *_agentSpec->_archiveLink.GetArc();

    if (arc.ArcStreamOffset == 0)
      tailStream = tempFile.OutStream;
    else
    {
      if (arc.Offset < 0)
        return E_NOTIMPL;
      RINOK(arc.InStream->Seek(0, STREAM_SEEK_SET, NULL));
      RINOK(NCompress::CopyStream_ExactSize(arc.InStream, tempFile.OutStream, arc.ArcStreamOffset, NULL));
      CTailOutStream *tailStreamSpec = new CTailOutStream;
      tailStream = tailStreamSpec;
      tailStreamSpec->Stream = tempFile.OutStream;
      tailStreamSpec->Offset = arc.ArcStreamOffset;
      tailStreamSpec->Init();
    }
    
    HRESULT result;

    switch (operation)
    {
      case AGENT_OP_Delete:
        result = _agentSpec->DeleteItems(tailStream, indices, numItems, updateCallback100);
        break;
      case AGENT_OP_CreateFolder:
        result = _agentSpec->CreateFolder(tailStream, newItemName, updateCallback100);
        break;
      case AGENT_OP_Rename:
        result = _agentSpec->RenameItem(tailStream, indices, numItems, newItemName, updateCallback100);
        break;
      case AGENT_OP_Comment:
        result = _agentSpec->CommentItem(tailStream, indices, numItems, newItemName, updateCallback100);
        break;
      case AGENT_OP_CopyFromFile:
        result = _agentSpec->UpdateOneFile(tailStream, indices, numItems, newItemName, updateCallback100);
        break;
      case AGENT_OP_Uni:
        {
          Byte actionSetByte[NUpdateArchive::NPairState::kNumValues];
          for (unsigned i = 0; i < NUpdateArchive::NPairState::kNumValues; i++)
            actionSetByte[i] = (Byte)actionSet->StateActions[i];
          result = _agentSpec->DoOperation2(
              moveMode ? &requestedPaths : NULL,
              moveMode ? &processedPaths : NULL,
              tailStream, actionSetByte, NULL, updateCallback100);
          break;
        }
      default:
        return E_FAIL;
    }
    
    RINOK(result);
  }

  _agentSpec->KeepModeForNextOpen();
  _agentSpec->Close();
  
  // before 9.26: if there was error for MoveToOriginal archive was closed.
  // now: we reopen archive after close

  // m_FolderItem = NULL;
  
  HRESULT res = tempFile.MoveToOriginal(true);

  // RINOK(res);
  if (res == S_OK)
  {
    if (moveMode)
    {
      unsigned i;
      for (i = 0; i < processedPaths.Size(); i++)
      {
        DeleteFileAlways(processedPaths[i]);
      }
      for (i = 0; i < requestedPaths.Size(); i++)
      {
        const FString &fs = requestedPaths[i];
        if (NFind::DoesDirExist(fs))
          Delete_EmptyFolder_And_EmptySubFolders(fs);
      }
    }
  }

  {
    CMyComPtr<IArchiveOpenCallback> openCallback;
    if (updateCallback100)
      updateCallback100->QueryInterface(IID_IArchiveOpenCallback, (void **)&openCallback);
    RINOK(_agentSpec->ReOpen(openCallback));
  }
   
  // CAgent::ReOpen() deletes _proxy and _proxy2
  _items.Clear();
  _proxy = NULL;
  _proxy2 = NULL;
  _proxyDirIndex = k_Proxy_RootDirIndex;
  _isAltStreamFolder = false;
  
  
  // ---------- Restore FolderItem ----------

  CMyComPtr<IFolderFolder> archiveFolder;
  RINOK(_agentSpec->BindToRootFolder(&archiveFolder));

  // CAgent::BindToRootFolder() changes _proxy and _proxy2
  _proxy = _agentSpec->_proxy;
  _proxy2 = _agentSpec->_proxy2;

  if (_proxy)
  {
    FOR_VECTOR (i, pathParts)
    {
      int next = _proxy->FindSubDir(_proxyDirIndex, pathParts[i]);
      if (next < 0)
        break;
      _proxyDirIndex = next;
    }
  }
  
  if (_proxy2)
  {
    if (pathParts.IsEmpty() && isAltStreamFolder)
    {
      _proxyDirIndex = k_Proxy2_AltRootDirIndex;
    }
    else FOR_VECTOR (i, pathParts)
    {
      bool dirOnly = (i + 1 < pathParts.Size() || !isAltStreamFolder);
      int index = _proxy2->FindItem(_proxyDirIndex, pathParts[i], dirOnly);
      if (index < 0)
        break;
      
      const CProxyFile2 &file = _proxy2->Files[_proxy2->Dirs[_proxyDirIndex].Items[index]];
  
      if (dirOnly)
        _proxyDirIndex = file.DirIndex;
      else
      {
        if (file.AltDirIndex >= 0)
          _proxyDirIndex = file.AltDirIndex;
        break;
      }
    }
  }

  /*
  if (pathParts.IsEmpty() && isAltStreamFolder)
  {
    CMyComPtr<IFolderAltStreams> folderAltStreams;
    archiveFolder.QueryInterface(IID_IFolderAltStreams, &folderAltStreams);
    if (folderAltStreams)
    {
      CMyComPtr<IFolderFolder> newFolder;
      folderAltStreams->BindToAltStreams((UInt32)(Int32)-1, &newFolder);
      if (newFolder)
        archiveFolder = newFolder;
    }
  }

  FOR_VECTOR (i, pathParts)
  {
    CMyComPtr<IFolderFolder> newFolder;
  
    if (isAltStreamFolder && i == pathParts.Size() - 1)
    {
      CMyComPtr<IFolderAltStreams> folderAltStreams;
      archiveFolder.QueryInterface(IID_IFolderAltStreams, &folderAltStreams);
      if (folderAltStreams)
        folderAltStreams->BindToAltStreams(pathParts[i], &newFolder);
    }
    else
      archiveFolder->BindToFolder(pathParts[i], &newFolder);
    
    if (!newFolder)
      break;
    archiveFolder = newFolder;
  }

  CMyComPtr<IArchiveFolderInternal> archiveFolderInternal;
  RINOK(archiveFolder.QueryInterface(IID_IArchiveFolderInternal, &archiveFolderInternal));
  CAgentFolder *agentFolder;
  RINOK(archiveFolderInternal->GetAgentFolder(&agentFolder));
  _proxyDirIndex = agentFolder->_proxyDirIndex;
  // _parentFolder = agentFolder->_parentFolder;
  */
  
  if (_proxy2)
    _isAltStreamFolder = _proxy2->IsAltDir(_proxyDirIndex);

  return res;

  }
  catch(const UString &s)
  {
    if (updateCallback100)
    {
      UString s2 ("Error: ");
      s2 += s;
      RINOK(updateCallback100->UpdateErrorMessage(s2));
      return E_FAIL;
    }
    throw;
  }
}



STDMETHODIMP CAgentFolder::CopyFrom(Int32 moveMode,
    const wchar_t *fromFolderPath, // test it
    const wchar_t * const *itemsPaths,
    UInt32 numItems,
    IProgress *progress)
{
  COM_TRY_BEGIN
  {
    RINOK(_agentSpec->SetFiles(fromFolderPath, itemsPaths, numItems));
    return CommonUpdateOperation(AGENT_OP_Uni, (moveMode != 0), NULL,
        &NUpdateArchive::k_ActionSet_Add,
        NULL, 0, progress);
  }
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::CopyFromFile(UInt32 destIndex, const wchar_t *itemPath, IProgress *progress)
{
  COM_TRY_BEGIN
  return CommonUpdateOperation(AGENT_OP_CopyFromFile, false, itemPath,
      &NUpdateArchive::k_ActionSet_Add,
      &destIndex, 1, progress);
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::Delete(const UInt32 *indices, UInt32 numItems, IProgress *progress)
{
  COM_TRY_BEGIN
  return CommonUpdateOperation(AGENT_OP_Delete, false, NULL,
      &NUpdateArchive::k_ActionSet_Delete, indices, numItems, progress);
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::CreateFolder(const wchar_t *name, IProgress *progress)
{
  COM_TRY_BEGIN
  
  if (_isAltStreamFolder)
    return E_NOTIMPL;

  if (_proxy2)
  {
    if (_proxy2->IsThere_SubDir(_proxyDirIndex, name))
      return ERROR_ALREADY_EXISTS;
  }
  else
  {
    if (_proxy->FindSubDir(_proxyDirIndex, name) >= 0)
      return ERROR_ALREADY_EXISTS;
  }
  
  return CommonUpdateOperation(AGENT_OP_CreateFolder, false, name, NULL, NULL, 0, progress);
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::Rename(UInt32 index, const wchar_t *newName, IProgress *progress)
{
  COM_TRY_BEGIN
  return CommonUpdateOperation(AGENT_OP_Rename, false, newName, NULL,
      &index, 1, progress);
  COM_TRY_END
}

STDMETHODIMP CAgentFolder::CreateFile(const wchar_t * /* name */, IProgress * /* progress */)
{
  return E_NOTIMPL;
}

STDMETHODIMP CAgentFolder::SetProperty(UInt32 index, PROPID propID,
    const PROPVARIANT *value, IProgress *progress)
{
  COM_TRY_BEGIN
  if (propID != kpidComment || value->vt != VT_BSTR)
    return E_NOTIMPL;
  if (!_agentSpec || !_agentSpec->GetTypeOfArc(_agentSpec->GetArc()).IsEqualTo_Ascii_NoCase("zip"))
    return E_NOTIMPL;
  
  return CommonUpdateOperation(AGENT_OP_Comment, false, value->bstrVal, NULL,
      &index, 1, progress);
  COM_TRY_END
}
