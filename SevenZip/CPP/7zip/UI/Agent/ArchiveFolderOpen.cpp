// Agent/ArchiveFolderOpen.cpp

#include "StdAfx.h"

#include "../../../Windows/DLL.h"

#include "Agent.h"

void CArchiveFolderManager::LoadFormats()
{
  LoadGlobalCodecs();
}

int CArchiveFolderManager::FindFormat(const UString &type)
{
  FOR_VECTOR (i, g_CodecsObj->Formats)
    if (type.IsEqualTo_NoCase(g_CodecsObj->Formats[i].Name))
      return i;
  return -1;
}

STDMETHODIMP CArchiveFolderManager::OpenFolderFile(IInStream *inStream,
    const wchar_t *filePath, const wchar_t *arcFormat,
    IFolderFolder **resultFolder, IProgress *progress)
{
  CMyComPtr<IArchiveOpenCallback> openArchiveCallback;
  if (progress)
  {
    CMyComPtr<IProgress> progressWrapper = progress;
    progressWrapper.QueryInterface(IID_IArchiveOpenCallback, &openArchiveCallback);
  }
  CAgent *agent = new CAgent();
  CMyComPtr<IInFolderArchive> archive = agent;
  
  HRESULT res = agent->Open(inStream, filePath, arcFormat, NULL, openArchiveCallback);
  
  if (res != S_OK)
  {
    if (res != S_FALSE)
      return res;
    /* 20.01: we create folder even for Non-Open cases, if there is NonOpen_ErrorInfo information.
         So we can get error information from that IFolderFolder later. */
    if (!agent->_archiveLink.NonOpen_ErrorInfo.IsThereErrorOrWarning())
      return res;
  }
  
  RINOK(agent->BindToRootFolder(resultFolder));
  return res;
}

/*
HRESULT CAgent::FolderReOpen(
    IArchiveOpenCallback *openArchiveCallback)
{
  return ReOpenArchive(_archive, _archiveFilePath);
}
*/


/*
STDMETHODIMP CArchiveFolderManager::GetExtensions(const wchar_t *type, BSTR *extensions)
{
  *extensions = 0;
  int formatIndex = FindFormat(type);
  if (formatIndex <  0)
    return E_INVALIDARG;
  // Exts[0].Ext;
  return StringToBstr(g_CodecsObj.Formats[formatIndex].GetAllExtensions(), extensions);
}
*/

static void AddIconExt(const CCodecIcons &lib, UString &dest)
{
  FOR_VECTOR (i, lib.IconPairs)
  {
    dest.Add_Space_if_NotEmpty();
    dest += lib.IconPairs[i].Ext;
  }
}

STDMETHODIMP CArchiveFolderManager::GetExtensions(BSTR *extensions)
{
  LoadFormats();
  *extensions = 0;
  UString res;
  
  #ifdef EXTERNAL_CODECS
  
  FOR_VECTOR (i, g_CodecsObj->Libs)
    AddIconExt(g_CodecsObj->Libs[i], res);
  
  #endif
  
  AddIconExt(g_CodecsObj->InternalIcons, res);
  return StringToBstr(res, extensions);
}

STDMETHODIMP CArchiveFolderManager::GetIconPath(const wchar_t *ext, BSTR *iconPath, Int32 *iconIndex)
{
  *iconPath = 0;
  *iconIndex = 0;

  LoadFormats();

  #ifdef EXTERNAL_CODECS

  FOR_VECTOR (i, g_CodecsObj->Libs)
  {
    const CCodecLib &lib = g_CodecsObj->Libs[i];
    int ii;
    if (lib.FindIconIndex(ext, ii))
    {
      *iconIndex = ii;
      return StringToBstr(fs2us(lib.Path), iconPath);
    }
  }
  
  #endif

  int ii;
  if (g_CodecsObj->InternalIcons.FindIconIndex(ext, ii))
  {
    FString path;
    if (NWindows::NDLL::MyGetModuleFileName(path))
    {
      *iconIndex = ii;
      return StringToBstr(fs2us(path), iconPath);
    }
  }
  return S_OK;
}

/*
STDMETHODIMP CArchiveFolderManager::GetTypes(BSTR *types)
{
  LoadFormats();
  UString typesStrings;
  FOR_VECTOR(i, g_CodecsObj.Formats)
  {
    const CArcInfoEx &ai = g_CodecsObj.Formats[i];
    if (ai.AssociateExts.Size() == 0)
      continue;
    if (i != 0)
      typesStrings.Add_Space();
    typesStrings += ai.Name;
  }
  return StringToBstr(typesStrings, types);
}
STDMETHODIMP CArchiveFolderManager::CreateFolderFile(const wchar_t * type,
    const wchar_t * filePath, IProgress progress)
{
  return E_NOTIMPL;
}
*/
