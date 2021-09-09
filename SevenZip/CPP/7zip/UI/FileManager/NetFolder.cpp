// NetFolder.cpp

#include "StdAfx.h"

#include "../../../Windows/PropVariant.h"

#include "../../PropID.h"

#include "FSFolder.h"
#include "NetFolder.h"
#include "SysIconUtils.h"

using namespace NWindows;
using namespace NNet;

static const Byte  kProps[] =
{
  kpidName,
  kpidLocalName,
  kpidComment,
  kpidProvider
};

void CNetFolder::Init(const UString &path)
{
  /*
  if (path.Len() > 2)
  {
    if (path[0] == L'\\' && path[1] == L'\\')
    {
      CResource netResource;
      netResource.RemoteName = GetSystemString(path.Left(path.Len() - 1));
      netResource.Scope = RESOURCE_GLOBALNET;
      netResource.Type = RESOURCETYPE_DISK;
      netResource.DisplayType = RESOURCEDISPLAYTYPE_SERVER;
      netResource.Usage = RESOURCEUSAGE_CONTAINER;
      Init(&netResource, 0, path);
      return;
    }
  }
  Init(0, 0 , L"");
  */
  CResourceW resource;
  resource.RemoteNameIsDefined = true;
  if (!path.IsEmpty())
    resource.RemoteName.SetFrom(path, path.Len() - 1);
  resource.ProviderIsDefined = false;
  resource.LocalNameIsDefined = false;
  resource.CommentIsDefined = false;
  resource.Type = RESOURCETYPE_DISK;
  resource.Scope = RESOURCE_GLOBALNET;
  resource.Usage = 0;
  resource.DisplayType = 0;
  CResourceW destResource;
  UString systemPathPart;
  DWORD result = GetResourceInformation(resource, destResource, systemPathPart);
  if (result == NO_ERROR)
    Init(&destResource, 0, path);
  else
    Init(0, 0 , L"");
  return;
}

void CNetFolder::Init(const NWindows::NNet::CResourceW *netResource,
      IFolderFolder *parentFolder, const UString &path)
{
  _path = path;
  if (netResource == 0)
    _netResourcePointer = 0;
  else
  {
    _netResource = *netResource;
    _netResourcePointer = &_netResource;

    // if (_netResource.DisplayType == RESOURCEDISPLAYTYPE_SERVER)
    _path = _netResource.RemoteName;
    
    /* WinXP-64: When we move UP from Network share without _parentFolder chain,
         we can get empty _netResource.RemoteName. Do we need to use Provider there ? */
    if (_path.IsEmpty())
      _path = _netResource.Provider;

    if (!_path.IsEmpty())
      _path.Add_PathSepar();
  }
  _parentFolder = parentFolder;
}

STDMETHODIMP CNetFolder::LoadItems()
{
  _items.Clear();
  CEnum enumerator;

  for (;;)
  {
    DWORD result = enumerator.Open(
      RESOURCE_GLOBALNET,
      RESOURCETYPE_DISK,
      0,        // enumerate all resources
      _netResourcePointer
      );
    if (result == NO_ERROR)
      break;
    if (result != ERROR_ACCESS_DENIED)
      return result;
    if (_netResourcePointer != 0)
    result = AddConnection2(_netResource,
        0, 0, CONNECT_INTERACTIVE);
    if (result != NO_ERROR)
      return result;
  }

  for (;;)
  {
    CResourceEx resource;
    DWORD result = enumerator.Next(resource);
    if (result == NO_ERROR)
    {
      if (!resource.RemoteNameIsDefined) // For Win 98, I don't know what's wrong
        resource.RemoteName = resource.Comment;
      resource.Name = resource.RemoteName;
      int pos = resource.Name.ReverseFind_PathSepar();
      if (pos >= 0)
      {
        // _path = resource.Name.Left(pos + 1);
        resource.Name.DeleteFrontal(pos + 1);
      }
      _items.Add(resource);
    }
    else if (result == ERROR_NO_MORE_ITEMS)
      break;
    else
      return result;
  }

  /*
  It's too slow for some systems.
  if (_netResourcePointer && _netResource.DisplayType == RESOURCEDISPLAYTYPE_SERVER)
  {
    for (char c = 'a'; c <= 'z'; c++)
    {
      CResourceEx resource;
      resource.Name = UString(wchar_t(c)) + L'$';
      resource.RemoteNameIsDefined = true;
      resource.RemoteName = _path + resource.Name;

      NFile::NFind::CFindFile findFile;
      NFile::NFind::CFileInfo fileInfo;
      if (!findFile.FindFirst(us2fs(resource.RemoteName) + FString(FCHAR_PATH_SEPARATOR) + FCHAR_ANY_MASK, fileInfo))
        continue;
      resource.Usage = RESOURCEUSAGE_CONNECTABLE;
      resource.LocalNameIsDefined = false;
      resource.CommentIsDefined = false;
      resource.ProviderIsDefined = false;
      _items.Add(resource);
    }
  }
  */
  return S_OK;
}


STDMETHODIMP CNetFolder::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _items.Size();
  return S_OK;
}

STDMETHODIMP CNetFolder::GetProperty(UInt32 itemIndex, PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  const CResourceEx &item = _items[itemIndex];
  switch (propID)
  {
    case kpidIsDir:  prop = true; break;
    case kpidName:
      // if (item.RemoteNameIsDefined)
        prop = item.Name;
      break;
    case kpidLocalName:  if (item.LocalNameIsDefined) prop = item.LocalName; break;
    case kpidComment: if (item.CommentIsDefined) prop = item.Comment; break;
    case kpidProvider: if (item.ProviderIsDefined) prop = item.Provider; break;
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CNetFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder)
{
  *resultFolder = 0;
  const CResourceEx &resource = _items[index];
  if (resource.Usage == RESOURCEUSAGE_CONNECTABLE ||
      resource.DisplayType == RESOURCEDISPLAYTYPE_SHARE)
  {
    NFsFolder::CFSFolder *fsFolderSpec = new NFsFolder::CFSFolder;
    CMyComPtr<IFolderFolder> subFolder = fsFolderSpec;
    RINOK(fsFolderSpec->Init(us2fs(resource.RemoteName + WCHAR_PATH_SEPARATOR))); // , this
    *resultFolder = subFolder.Detach();
  }
  else
  {
    CNetFolder *netFolder = new CNetFolder;
    CMyComPtr<IFolderFolder> subFolder = netFolder;
    netFolder->Init(&resource, this, resource.Name + WCHAR_PATH_SEPARATOR);
    *resultFolder = subFolder.Detach();
  }
  return S_OK;
}

STDMETHODIMP CNetFolder::BindToFolder(const wchar_t * /* name */, IFolderFolder ** /* resultFolder */)
{
  return E_NOTIMPL;
}

STDMETHODIMP CNetFolder::BindToParentFolder(IFolderFolder **resultFolder)
{
  *resultFolder = 0;
  if (_parentFolder)
  {
    CMyComPtr<IFolderFolder> parentFolder = _parentFolder;
    *resultFolder = parentFolder.Detach();
    return S_OK;
  }
  if (_netResourcePointer != 0)
  {
    CResourceW resourceParent;
    DWORD result = GetResourceParent(_netResource, resourceParent);
    if (result != NO_ERROR)
      return result;
    if (!_netResource.RemoteNameIsDefined)
      return S_OK;

    CNetFolder *netFolder = new CNetFolder;
    CMyComPtr<IFolderFolder> subFolder = netFolder;
    netFolder->Init(&resourceParent, 0, WSTRING_PATH_SEPARATOR);
    *resultFolder = subFolder.Detach();
  }
  return S_OK;
}

IMP_IFolderFolder_Props(CNetFolder)

STDMETHODIMP CNetFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value)
{
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "NetFolder"; break;
    case kpidPath: prop = _path; break;
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CNetFolder::GetSystemIconIndex(UInt32 index, Int32 *iconIndex)
{
  if (index >= (UInt32)_items.Size())
    return E_INVALIDARG;
  *iconIndex = 0;
  const CResourceW &resource = _items[index];
  int iconIndexTemp;
  if (resource.DisplayType == RESOURCEDISPLAYTYPE_SERVER ||
      resource.Usage == RESOURCEUSAGE_CONNECTABLE)
  {
    if (GetRealIconIndex(us2fs(resource.RemoteName), 0, iconIndexTemp))
    {
      *iconIndex = iconIndexTemp;
      return S_OK;
    }
  }
  else
  {
    if (GetRealIconIndex(FTEXT(""), FILE_ATTRIBUTE_DIRECTORY, iconIndexTemp))
    {
      *iconIndex = iconIndexTemp;
      return S_OK;
    }
    // *anIconIndex = GetRealIconIndex(0, L"\\\\HOME");
  }
  return GetLastError();
}
