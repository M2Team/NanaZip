// NetFolder.h

#ifndef __NET_FOLDER_H
#define __NET_FOLDER_H

#include "../../../Common/MyCom.h"

#include "../../../Windows/Net.h"

#include "IFolder.h"

struct CResourceEx: public NWindows::NNet::CResourceW
{
  UString Name;
};

class CNetFolder:
  public IFolderFolder,
  public IFolderGetSystemIconIndex,
  public CMyUnknownImp
{
  NWindows::NNet::CResourceW _netResource;
  NWindows::NNet::CResourceW *_netResourcePointer;

  CObjectVector<CResourceEx> _items;

  CMyComPtr<IFolderFolder> _parentFolder;
  UString _path;
public:
  MY_UNKNOWN_IMP1(IFolderGetSystemIconIndex)
  INTERFACE_FolderFolder(;)
  STDMETHOD(GetSystemIconIndex)(UInt32 index, Int32 *iconIndex);

  CNetFolder(): _netResourcePointer(0) {}
  void Init(const UString &path);
  void Init(const NWindows::NNet::CResourceW *netResource,
      IFolderFolder *parentFolder, const UString &path);
};

#endif
