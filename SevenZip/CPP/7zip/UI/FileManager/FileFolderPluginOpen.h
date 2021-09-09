// FileFolderPluginOpen.h

#ifndef __FILE_FOLDER_PLUGIN_OPEN_H
#define __FILE_FOLDER_PLUGIN_OPEN_H

#include "../../../Windows/DLL.h"

struct CFfpOpen
{
  CLASS_NO_COPY(CFfpOpen)
public:
  // out:
  bool Encrypted;
  UString Password;

  NWindows::NDLL::CLibrary Library;
  CMyComPtr<IFolderFolder> Folder;
  UString ErrorMessage;

  CFfpOpen(): Encrypted (false) {}

  HRESULT OpenFileFolderPlugin(IInStream *inStream,
      const FString &path, const UString &arcFormat, HWND parentWindow);
};


#endif
