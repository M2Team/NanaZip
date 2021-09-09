// PanelSort.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"
#include "../../../Windows/PropVariant.h"

#include "../../PropID.h"

#include "Panel.h"

using namespace NWindows;

int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2)
{
  for (;;)
  {
    wchar_t c1 = *s1;
    wchar_t c2 = *s2;
    if ((c1 >= '0' && c1 <= '9') &&
        (c2 >= '0' && c2 <= '9'))
    {
      for (; *s1 == '0'; s1++);
      for (; *s2 == '0'; s2++);
      size_t len1 = 0;
      size_t len2 = 0;
      for (; (s1[len1] >= '0' && s1[len1] <= '9'); len1++);
      for (; (s2[len2] >= '0' && s2[len2] <= '9'); len2++);
      if (len1 < len2) return -1;
      if (len1 > len2) return 1;
      for (; len1 > 0; s1++, s2++, len1--)
      {
        if (*s1 == *s2) continue;
        return (*s1 < *s2) ? -1 : 1;
      }
      c1 = *s1;
      c2 = *s2;
    }
    s1++;
    s2++;
    if (c1 != c2)
    {
      // Probably we need to change the order for special characters like in Explorer.
      wchar_t u1 = MyCharUpper(c1);
      wchar_t u2 = MyCharUpper(c2);
      if (u1 < u2) return -1;
      if (u1 > u2) return 1;
    }
    if (c1 == 0) return 0;
  }
}

static int CompareFileNames_Le16(const Byte *s1, unsigned size1, const Byte *s2, unsigned size2)
{
  size1 &= ~1;
  size2 &= ~1;
  for (unsigned i = 0;; i += 2)
  {
    if (i >= size1)
      return (i >= size2) ? 0 : -1;
    if (i >= size2)
      return 1;
    UInt16 c1 = GetUi16(s1 + i);
    UInt16 c2 = GetUi16(s2 + i);
    if (c1 == c2)
    {
      if (c1 == 0)
        return 0;
      continue;
    }
    if (c1 < c2)
      return -1;
    return 1;
  }
}

static inline const wchar_t *GetExtensionPtr(const UString &name)
{
  int dotPos = name.ReverseFind_Dot();
  return name.Ptr((dotPos < 0) ? name.Len() : dotPos);
}

void CPanel::SetSortRawStatus()
{
  _isRawSortProp = false;
  FOR_VECTOR (i, _columns)
  {
    const CPropColumn &prop = _columns[i];
    if (prop.ID == _sortID)
    {
      _isRawSortProp = prop.IsRawProp ? 1 : 0;
      return;
    }
  }
}


static int CALLBACK CompareItems2(LPARAM lParam1, LPARAM lParam2, LPARAM lpData)
{
  if (lpData == 0)
    return 0;
  CPanel *panel = (CPanel*)lpData;
  

  PROPID propID = panel->_sortID;

  if (propID == kpidNoProperty)
    return MyCompare(lParam1, lParam2);

  if (panel->_isRawSortProp)
  {
    // Sha1, NtSecurity, NtReparse
    const void *data1;
    const void *data2;
    UInt32 dataSize1;
    UInt32 dataSize2;
    UInt32 propType1;
    UInt32 propType2;
    if (panel->_folderRawProps->GetRawProp((UInt32)lParam1, propID, &data1, &dataSize1, &propType1) != 0) return 0;
    if (panel->_folderRawProps->GetRawProp((UInt32)lParam2, propID, &data2, &dataSize2, &propType2) != 0) return 0;
    if (dataSize1 == 0)
      return (dataSize2 == 0) ? 0 : -1;
    if (dataSize2 == 0)
      return 1;
    if (propType1 != NPropDataType::kRaw) return 0;
    if (propType2 != NPropDataType::kRaw) return 0;
    if (propID == kpidNtReparse)
    {
      NFile::CReparseShortInfo r1; r1.Parse((const Byte *)data1, dataSize1);
      NFile::CReparseShortInfo r2; r2.Parse((const Byte *)data2, dataSize2);
      return CompareFileNames_Le16(
          (const Byte *)data1 + r1.Offset, r1.Size,
          (const Byte *)data2 + r2.Offset, r2.Size);
    }
  }

  if (panel->_folderCompare)
    return panel->_folderCompare->CompareItems((UInt32)lParam1, (UInt32)lParam2, propID, panel->_isRawSortProp);
  
  switch (propID)
  {
    // if (panel->_sortIndex == 0)
    case kpidName:
    {
      const UString name1 = panel->GetItemName((int)lParam1);
      const UString name2 = panel->GetItemName((int)lParam2);
      int res = CompareFileNames_ForFolderList(name1, name2);
      /*
      if (res != 0 || !panel->_flatMode)
        return res;
      const UString prefix1 = panel->GetItemPrefix(lParam1);
      const UString prefix2 = panel->GetItemPrefix(lParam2);
      return res = CompareFileNames_ForFolderList(prefix1, prefix2);
      */
      return res;
    }
    case kpidExtension:
    {
      const UString name1 = panel->GetItemName((int)lParam1);
      const UString name2 = panel->GetItemName((int)lParam2);
      return CompareFileNames_ForFolderList(
          GetExtensionPtr(name1),
          GetExtensionPtr(name2));
    }
  }
  /*
  if (panel->_sortIndex == 1)
    return MyCompare(file1.Size, file2.Size);
  return ::CompareFileTime(&file1.MTime, &file2.MTime);
  */

  // PROPID propID = panel->_columns[panel->_sortIndex].ID;

  NCOM::CPropVariant prop1, prop2;
  // Name must be first property
  panel->_folder->GetProperty((UInt32)lParam1, propID, &prop1);
  panel->_folder->GetProperty((UInt32)lParam2, propID, &prop2);
  if (prop1.vt != prop2.vt)
    return MyCompare(prop1.vt, prop2.vt);
  if (prop1.vt == VT_BSTR)
    return MyStringCompareNoCase(prop1.bstrVal, prop2.bstrVal);
  return prop1.Compare(prop2);
}

int CALLBACK CompareItems(LPARAM lParam1, LPARAM lParam2, LPARAM lpData);
int CALLBACK CompareItems(LPARAM lParam1, LPARAM lParam2, LPARAM lpData)
{
  if (lpData == 0) return 0;
  if (lParam1 == kParentIndex) return -1;
  if (lParam2 == kParentIndex) return 1;

  CPanel *panel = (CPanel*)lpData;

  bool isDir1 = panel->IsItem_Folder((int)lParam1);
  bool isDir2 = panel->IsItem_Folder((int)lParam2);
  
  if (isDir1 && !isDir2) return -1;
  if (isDir2 && !isDir1) return 1;

  int result = CompareItems2(lParam1, lParam2, lpData);
  return panel->_ascending ? result: (-result);
}


/*
void CPanel::SortItems(int index)
{
  if (index == _sortIndex)
    _ascending = !_ascending;
  else
  {
    _sortIndex = index;
    _ascending = true;
    switch (_columns[_sortIndex].ID)
    {
      case kpidSize:
      case kpidPackedSize:
      case kpidCTime:
      case kpidATime:
      case kpidMTime:
      _ascending = false;
      break;
    }
  }
  _listView.SortItems(CompareItems, (LPARAM)this);
  _listView.EnsureVisible(_listView.GetFocusedItem(), false);
}

void CPanel::SortItemsWithPropID(PROPID propID)
{
  int index = _columns.FindItem_for_PropID(propID);
  if (index >= 0)
    SortItems(index);
}
*/

void CPanel::SortItemsWithPropID(PROPID propID)
{
  if (propID == _sortID)
    _ascending = !_ascending;
  else
  {
    _sortID = propID;
    _ascending = true;
    switch (propID)
    {
      case kpidSize:
      case kpidPackSize:
      case kpidCTime:
      case kpidATime:
      case kpidMTime:
        _ascending = false;
      break;
    }
  }
  SetSortRawStatus();
  _listView.SortItems(CompareItems, (LPARAM)this);
  _listView.EnsureVisible(_listView.GetFocusedItem(), false);
}


void CPanel::OnColumnClick(LPNMLISTVIEW info)
{
  /*
  int index = _columns.FindItem_for_PropID(_visibleColumns[info->iSubItem].ID);
  SortItems(index);
  */
  SortItemsWithPropID(_visibleColumns[info->iSubItem].ID);
}
