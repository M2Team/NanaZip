// Agent/ArchiveFolderOpen.cpp

#include "StdAfx.h"

// #ifdef NEW_FOLDER_INTERFACE

#include "../../../Common/StringToInt.h"
#include "../../../Windows/DLL.h"
#include "../../../Windows/ResourceString.h"

#include "Agent.h"

extern HINSTANCE g_hInstance;
static const UINT kIconTypesResId = 100;

void CCodecIcons::LoadIcons(HMODULE m)
{
  IconPairs.Clear();
  UString iconTypes;
  NWindows::MyLoadString(m, kIconTypesResId, iconTypes);
  UStringVector pairs;
  SplitString(iconTypes, pairs);
  FOR_VECTOR (i, pairs)
  {
    const UString &s = pairs[i];
    int pos = s.Find(L':');
    CIconPair iconPair;
    iconPair.IconIndex = -1;
    if (pos < 0)
      pos = (int)s.Len();
    else
    {
      const UString num = s.Ptr((unsigned)pos + 1);
      if (!num.IsEmpty())
      {
        const wchar_t *end;
        iconPair.IconIndex = (int)ConvertStringToUInt32(num, &end);
        if (*end != 0)
          continue;
      }
    }
    iconPair.Ext = s.Left((unsigned)pos);
    IconPairs.Add(iconPair);
  }
}

bool CCodecIcons::FindIconIndex(const UString &ext, int &iconIndex) const
{
  iconIndex = -1;
  FOR_VECTOR (i, IconPairs)
  {
    const CIconPair &pair = IconPairs[i];
    if (ext.IsEqualTo_NoCase(pair.Ext))
    {
      iconIndex = pair.IconIndex;
      return true;
    }
  }
  return false;
}


void CArchiveFolderManager::LoadFormats()
{
  if (WasLoaded)
    return;

  LoadGlobalCodecs();

  #ifdef Z7_EXTERNAL_CODECS
  CodecIconsVector.Clear();
  FOR_VECTOR (i, g_CodecsObj->Libs)
  {
    CCodecIcons &ci = CodecIconsVector.AddNew();
    ci.LoadIcons(g_CodecsObj->Libs[i].Lib.Get_HMODULE());
  }
  #endif
  InternalIcons.LoadIcons(g_hInstance);
  WasLoaded = true;
}

/*
int CArchiveFolderManager::FindFormat(const UString &type)
{
  FOR_VECTOR (i, g_CodecsObj->Formats)
    if (type.IsEqualTo_NoCase(g_CodecsObj->Formats[i].Name))
      return (int)i;
  return -1;
}
*/

Z7_COM7F_IMF(CArchiveFolderManager::OpenFolderFile(IInStream *inStream,
    const wchar_t *filePath, const wchar_t *arcFormat,
    IFolderFolder **resultFolder, IProgress *progress))
{
  CMyComPtr<IArchiveOpenCallback> openArchiveCallback;
  if (progress)
  {
    CMyComPtr<IProgress> progressWrapper = progress;
    progressWrapper.QueryInterface(IID_IArchiveOpenCallback, &openArchiveCallback);
  }
  CAgent *agent = new CAgent();
  CMyComPtr<IInFolderArchive> archive = agent;

  const HRESULT res = archive->Open(inStream, filePath, arcFormat, NULL, openArchiveCallback);

  if (res != S_OK)
  {
    if (res != S_FALSE)
      return res;
    /* 20.01: we create folder even for Non-Open cases, if there is NonOpen_ErrorInfo information.
         So we can get error information from that IFolderFolder later. */
    if (!agent->_archiveLink.NonOpen_ErrorInfo.IsThereErrorOrWarning())
      return res;
  }

  agent->_progress_for_Open = progress;
  agent->_progress_ArchiveOpenCallback_for_Open = openArchiveCallback;
  {
    HRESULT hres = E_OUTOFMEMORY;
    // IInFolderArchive::BindToRootFolder() doesn't raise exceptions.
    // so try/catch is optional
    try {
      hres = archive->BindToRootFolder(resultFolder);
    } catch (...) {}
    agent->_progress_for_Open = NULL;
    agent->_progress_ArchiveOpenCallback_for_Open = NULL;
    RINOK(hres)
  }
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
Z7_COM7F_IMF(CArchiveFolderManager::GetExtensions(const wchar_t *type, BSTR *extensions))
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


Z7_COM7F_IMF(CArchiveFolderManager::GetExtensions(BSTR *extensions))
{
  *extensions = NULL;
  LoadFormats();
  UString res;

  #ifdef Z7_EXTERNAL_CODECS
  /*
  FOR_VECTOR (i, g_CodecsObj->Libs)
    AddIconExt(g_CodecsObj->Libs[i].CodecIcons, res);
  */
  FOR_VECTOR (i, CodecIconsVector)
    AddIconExt(CodecIconsVector[i], res);
  #endif

  AddIconExt(
      // g_CodecsObj->
      InternalIcons, res);

  return StringToBstr(res, extensions);
}


Z7_COM7F_IMF(CArchiveFolderManager::GetIconPath(const wchar_t *ext, BSTR *iconPath, Int32 *iconIndex))
{
  *iconPath = NULL;
  *iconIndex = 0;
  LoadFormats();

  #ifdef Z7_EXTERNAL_CODECS
  // FOR_VECTOR (i, g_CodecsObj->Libs)
  FOR_VECTOR (i, CodecIconsVector)
  {
    int ii;
    if (CodecIconsVector[i].FindIconIndex(ext, ii))
    {
      const CCodecLib &lib = g_CodecsObj->Libs[i];
      *iconIndex = ii;
      return StringToBstr(fs2us(lib.Path), iconPath);
    }
  }
  #endif

  int ii;
  if (InternalIcons.FindIconIndex(ext, ii))
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
Z7_COM7F_IMF(CArchiveFolderManager::GetTypes(BSTR *types))
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
Z7_COM7F_IMF(CArchiveFolderManager::CreateFolderFile(const wchar_t * type,
    const wchar_t * filePath, IProgress progress))
{
  return E_NOTIMPL;
}
*/

// #endif
