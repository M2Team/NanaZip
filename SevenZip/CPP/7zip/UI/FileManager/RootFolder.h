// RootFolder.h

#ifndef __ROOT_FOLDER_H
#define __ROOT_FOLDER_H

#include "../../../Common/MyString.h"

#include "IFolder.h"

const unsigned kNumRootFolderItems_Max = 4;

class CRootFolder:
  public IFolderFolder,
  public IFolderGetSystemIconIndex,
  public CMyUnknownImp
{
  UString _names[kNumRootFolderItems_Max];
  int _iconIndices[kNumRootFolderItems_Max];

public:
  MY_UNKNOWN_IMP1(IFolderGetSystemIconIndex)
  INTERFACE_FolderFolder(;)
  STDMETHOD(GetSystemIconIndex)(UInt32 index, Int32 *iconIndex);
  void Init();
};

#endif
