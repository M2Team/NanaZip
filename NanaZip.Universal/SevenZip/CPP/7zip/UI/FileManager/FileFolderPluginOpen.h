// FileFolderPluginOpen.h

#ifndef ZIP7_INC_FILE_FOLDER_PLUGIN_OPEN_H
#define ZIP7_INC_FILE_FOLDER_PLUGIN_OPEN_H

#include "../../../Windows/DLL.h"

struct CFfpOpen
{
  Z7_CLASS_NO_COPY(CFfpOpen)
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
