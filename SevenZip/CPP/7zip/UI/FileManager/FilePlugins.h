// FilePlugins.h

#ifndef __FILE_PLUGINS_H
#define __FILE_PLUGINS_H

#include "RegistryPlugins.h"

struct CPluginToIcon
{
  int PluginIndex;
  UString IconPath;
  int IconIndex;
  
  CPluginToIcon(): IconIndex(-1) {}
};

struct CExtPlugins
{
  UString Ext;
  CObjectVector<CPluginToIcon> Plugins;
};

class CExtDatabase
{
  int FindExt(const UString &ext);
public:
  CObjectVector<CExtPlugins> Exts;
  CObjectVector<CPluginInfo> Plugins;
  
  void Read();
};

#endif
