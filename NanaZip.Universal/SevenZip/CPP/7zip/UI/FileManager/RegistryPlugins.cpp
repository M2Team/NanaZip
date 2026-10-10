// RegistryPlugins.cpp

#include "StdAfx.h"

/*
#include "../../../Windows/DLL.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/PropVariant.h"

#include "IFolder.h"
*/
#include "RegistryPlugins.h"

// using namespace NWindows;
// using namespace NFile;

/*
typedef UINT32 (WINAPI * Func_GetPluginProperty)(PROPID propID, PROPVARIANT *value);

static bool ReadPluginInfo(CPluginInfo &plugin, bool needCheckDll)
{
  if (needCheckDll)
  {
    NDLL::CLibrary lib;
    if (!lib.LoadEx(plugin.FilePath, LOAD_LIBRARY_AS_DATAFILE))
      return false;
  }
  NDLL::CLibrary lib;
  if (!lib.Load(plugin.FilePath))
    return false;
  const
  Func_GetPluginProperty
     f_GetPluginProperty = ZIP7_GET_PROC_ADDRESS(
  Func_GetPluginProperty, lib.Get_HMODULE(),
      "GetPluginProperty");
  if (!f_GetPluginProperty)
    return false;

  NCOM::CPropVariant prop;
  if (f_GetPluginProperty(NPlugin::kType, &prop) != S_OK)
    return false;
  if (prop.vt == VT_EMPTY)
    plugin.Type = kPluginTypeFF;
  else if (prop.vt == VT_UI4)
    plugin.Type = (EPluginType)prop.ulVal;
  else
    return false;
  prop.Clear();

  if (f_GetPluginProperty(NPlugin::kName, &prop) != S_OK)
    return false;
  if (prop.vt != VT_BSTR)
    return false;
  plugin.Name = prop.bstrVal;
  prop.Clear();

  if (f_GetPluginProperty(NPlugin::kClassID, &prop) != S_OK)
    return false;
  if (prop.vt == VT_EMPTY)
    plugin.ClassID_Defined = false;
  else if (prop.vt != VT_BSTR)
    return false;
  else
  {
    plugin.ClassID_Defined = true;
    plugin.ClassID = *(const GUID *)(const void *)prop.bstrVal;
  }
  prop.Clear();
  return true;
*/

/*
{
  if (f_GetPluginProperty(NPlugin::kOptionsClassID, &prop) != S_OK)
    return false;
  if (prop.vt == VT_EMPTY)
    plugin.OptionsClassID_Defined = false;
  else if (prop.vt != VT_BSTR)
    return false;
  else
  {
    plugin.OptionsClassID_Defined = true;
    plugin.OptionsClassID = *(const GUID *)(const void *)prop.bstrVal;
  }
}
*/

    /*
    {
      // very old 7-zip used agent plugin in "7-zip.dll"
      // but then agent code was moved to 7zfm.
      // so now we don't need to load "7-zip.dll" here
      CPluginInfo plugin;
      plugin.FilePath = baseFolderPrefix + FTEXT("7-zip.dll");
      if (::ReadPluginInfo(plugin, false))
      if (plugin.Type == kPluginTypeFF)
        plugins.Add(plugin);
    }
    */
    /*
    FString folderPath = NDLL::GetModuleDirPrefix();
    folderPath += "Plugins" STRING_PATH_SEPARATOR;
    NFind::CEnumerator enumerator;
    enumerator.SetDirPrefix(folderPath);
    NFind::CFileInfo fi;
    while (enumerator.Next(fi))
    {
      if (fi.IsDir())
        continue;
      CPluginInfo plugin;
      plugin.FilePath = folderPath + fi.Name;
      if (::ReadPluginInfo(plugin, true))
      if (plugin.Type == kPluginTypeFF)
        plugins.Add(plugin);
    }
    */

  /*
  ReadPluginInfoList(plugins);
  for (unsigned i = 0; i < plugins.Size();)
    if (plugins[i].Type != kPluginTypeFF)
      plugins.Delete(i);
    else
      i++;
  */

/*
void ReadFileFolderPluginInfoList(CObjectVector<CPluginInfo> &plugins)
{
  plugins.Clear();
  {
  }

  {
    CPluginInfo &plugin = plugins.AddNew();
    // p.FilePath.Empty();
    plugin.Type = kPluginTypeFF;
    plugin.Name = "7-Zip";
    // plugin.ClassID = CLSID_CAgentArchiveHandler;
    // plugin.ClassID_Defined = true;
    // plugin.ClassID_Defined = false;
    // plugin.OptionsClassID_Defined = false;
  }
}
*/
