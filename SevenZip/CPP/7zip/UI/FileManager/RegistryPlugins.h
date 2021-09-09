// RegistryPlugins.h

#ifndef __REGISTRY_PLUGINS_H
#define __REGISTRY_PLUGINS_H

#include "../../../Common/MyString.h"

enum EPluginType
{
  kPluginTypeFF = 0
};

struct CPluginInfo
{
  FString FilePath;
  EPluginType Type;
  UString Name;
  CLSID ClassID;
  CLSID OptionsClassID;
  bool ClassIDDefined;
  bool OptionsClassIDDefined;

  // CSysString Extension;
  // CSysString AddExtension;
  // bool UpdateEnabled;
  // bool KeepName;
};

void ReadPluginInfoList(CObjectVector<CPluginInfo> &plugins);
void ReadFileFolderPluginInfoList(CObjectVector<CPluginInfo> &plugins);

#endif
