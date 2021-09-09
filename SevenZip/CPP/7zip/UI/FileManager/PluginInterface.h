// PluginInterface.h

#ifndef __PLUGIN_INTERFACE_H
#define __PLUGIN_INTERFACE_H

/*
#include "../../../Common/Types.h"
#include "../../IDecl.h"

#define PLUGIN_INTERFACE(i, x) DECL_INTERFACE(i, 0x0A, x)

PLUGIN_INTERFACE(IInitContextMenu, 0x00)
{
  STDMETHOD(InitContextMenu)(const wchar_t *folder, const wchar_t * const *names, UInt32 numFiles) PURE;
};

PLUGIN_INTERFACE(IPluginOptionsCallback, 0x01)
{
  STDMETHOD(GetProgramFolderPath)(BSTR *value) PURE;
  STDMETHOD(GetProgramPath)(BSTR *value) PURE;
  STDMETHOD(GetRegistryCUPath)(BSTR *value) PURE;
};

PLUGIN_INTERFACE(IPluginOptions, 0x02)
{
  STDMETHOD(PluginOptions)(HWND hWnd, IPluginOptionsCallback *callback) PURE;
  // STDMETHOD(GetFileExtensions)(BSTR *extensions) PURE;
};
*/

#endif
