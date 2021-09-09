// PluginLoader.h

#ifndef __PLUGIN_LOADER_H
#define __PLUGIN_LOADER_H

#include "../../../Windows/DLL.h"

#include "IFolder.h"

class CPluginLibrary: public NWindows::NDLL::CLibrary
{
public:
  HRESULT CreateManager(REFGUID clsID, IFolderManager **manager)
  {
    Func_CreateObject createObject = (Func_CreateObject)GetProc("CreateObject");
    if (!createObject)
      return GetLastError();
    return createObject(&clsID, &IID_IFolderManager, (void **)manager);
  }
  HRESULT LoadAndCreateManager(CFSTR filePath, REFGUID clsID, IFolderManager **manager)
  {
    if (!Load(filePath))
      return GetLastError();
    return CreateManager(clsID, manager);
  }
};

#endif
