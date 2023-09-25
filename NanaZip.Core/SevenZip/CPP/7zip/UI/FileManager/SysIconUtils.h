// SysIconUtils.h

#ifndef ZIP7_INC_SYS_ICON_UTILS_H
#define ZIP7_INC_SYS_ICON_UTILS_H

#include "../../../Common/MyWindows.h"

#include <CommCtrl.h>

#include "../../../Common/MyString.h"

struct CExtIconPair
{
  UString Ext;
  int IconIndex;
  // UString TypeName;

  // int Compare(const CExtIconPair &a) const { return MyStringCompareNoCase(Ext, a.Ext); }
};

struct CAttribIconPair
{
  DWORD Attrib;
  int IconIndex;
  // UString TypeName;

  // int Compare(const CAttribIconPair &a) const { return Ext.Compare(a.Ext); }
};

class CExtToIconMap
{
public:
  CRecordVector<CAttribIconPair> _attribMap;
  CObjectVector<CExtIconPair> _extMap;
  int SplitIconIndex;
  int SplitIconIndex_Defined;
  
  CExtToIconMap(): SplitIconIndex_Defined(false) {}

  void Clear()
  {
    SplitIconIndex_Defined = false;
    _extMap.Clear();
    _attribMap.Clear();
  }
  int GetIconIndex(DWORD attrib, const wchar_t *fileName /* , UString *typeName */);
  // int GetIconIndex(DWORD attrib, const UString &fileName);
};

DWORD_PTR GetRealIconIndex(CFSTR path, DWORD attrib, int &iconIndex);
int GetIconIndexForCSIDL(int csidl);

HIMAGELIST GetSysImageList(bool smallIcons);

#endif
