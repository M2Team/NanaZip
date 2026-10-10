// NetFolder.h

#ifndef ZIP7_INC_NET_FOLDER_H
#define ZIP7_INC_NET_FOLDER_H

#include "../../../Common/MyCom.h"

#include "../../../Windows/Net.h"

#include "IFolder.h"

struct CResourceEx: public NWindows::NNet::CResourceW
{
  UString Name;
};

Z7_CLASS_IMP_NOQIB_2(
  CNetFolder
  , IFolderFolder
  , IFolderGetSystemIconIndex
)
  NWindows::NNet::CResourceW _netResource;
  NWindows::NNet::CResourceW *_netResourcePointer;

  CObjectVector<CResourceEx> _items;

  CMyComPtr<IFolderFolder> _parentFolder;
  UString _path;
public:
  CNetFolder(): _netResourcePointer(NULL) {}
  void Init(const UString &path);
  void Init(const NWindows::NNet::CResourceW *netResource,
      IFolderFolder *parentFolder, const UString &path);
};

#endif
