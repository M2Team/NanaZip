// Archive/Common/ItemNameUtils.h

#ifndef __ARCHIVE_ITEM_NAME_UTILS_H
#define __ARCHIVE_ITEM_NAME_UTILS_H

#include "../../../Common/MyString.h"

namespace NArchive {
namespace NItemName {

void ReplaceSlashes_OsToUnix(UString &name);
  
UString GetOsPath(const UString &name);
UString GetOsPath_Remove_TailSlash(const UString &name);
  
void ReplaceToOsSlashes_Remove_TailSlash(UString &name, bool useBackslashReplacement = false);
  
bool HasTailSlash(const AString &name, UINT codePage);
  
#ifdef _WIN32
  inline UString WinPathToOsPath(const UString &name)  { return name; }
#else
  UString WinPathToOsPath(const UString &name);
#endif

}}

#endif
