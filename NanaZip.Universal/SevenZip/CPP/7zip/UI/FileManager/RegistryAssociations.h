// RegistryAssociations.h

#ifndef ZIP7_INC_REGISTRY_ASSOCIATIONS_H
#define ZIP7_INC_REGISTRY_ASSOCIATIONS_H

#include "../../../Common/MyString.h"

namespace NRegistryAssoc {

  struct CShellExtInfo
  {
    CSysString ProgramKey;
    UString IconPath;
    int IconIndex;

    bool ReadFromRegistry(HKEY hkey, const CSysString &ext);
    bool IsIt7Zip() const;
  };

  LONG DeleteShellExtensionInfo(HKEY hkey, const CSysString &ext);

  LONG AddShellExtensionInfo(HKEY hkey,
      const CSysString &ext,
      const UString &programTitle,
      const UString &programOpenCommand,
      const UString &iconPath, int iconIndex
      // , const void *shellNewData, int shellNewDataSize
      );
}

#endif
