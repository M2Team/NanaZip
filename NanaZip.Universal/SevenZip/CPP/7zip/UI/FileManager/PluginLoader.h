// PluginLoader.h

#ifndef ZIP7_INC_PLUGIN_LOADER_H
#define ZIP7_INC_PLUGIN_LOADER_H

#include "../../../Windows/DLL.h"

#include "IFolder.h"

Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION

class CPluginLibrary: public NWindows::NDLL::CLibrary
{
public:
  HRESULT CreateManager(REFGUID clsID, IFolderManager **manager)
  {
    const
    Func_CreateObject createObject = Z7_GET_PROC_ADDRESS(
    Func_CreateObject, Get_HMODULE(),
        "CreateObject");
    if (!createObject)
      return GetLastError_noZero_HRESULT();
    return createObject(&clsID, &IID_IFolderManager, (void **)manager);
  }
  HRESULT LoadAndCreateManager(CFSTR filePath, REFGUID clsID, IFolderManager **manager)
  {
    if (!Load(filePath))
      return GetLastError_noZero_HRESULT();
    return CreateManager(clsID, manager);
  }
};

#endif
