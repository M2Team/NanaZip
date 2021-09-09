// Windows/Menu.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif
#include "Menu.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {

/*
structures
  MENUITEMINFOA
  MENUITEMINFOW
contain additional member:
  #if (WINVER >= 0x0500)
    HBITMAP hbmpItem;
  #endif
If we compile the source code with (WINVER >= 0x0500), some functions
will not work at NT 4.0, if cbSize is set as sizeof(MENUITEMINFO*).
So we use size of old version of structure. */

#if defined(UNDER_CE) || defined(_WIN64) || (WINVER < 0x0500)
  #ifndef _UNICODE
  #define my_compatib_MENUITEMINFOA_size  sizeof(MENUITEMINFOA)
  #endif
  #define my_compatib_MENUITEMINFOW_size  sizeof(MENUITEMINFOW)
#else
  #define MY_STRUCT_SIZE_BEFORE(structname, member) ((UINT)(UINT_PTR)((LPBYTE)(&((structname*)0)->member) - (LPBYTE)(structname*)0))
  #ifndef _UNICODE
  #define my_compatib_MENUITEMINFOA_size MY_STRUCT_SIZE_BEFORE(MENUITEMINFOA, hbmpItem)
  #endif
  #define my_compatib_MENUITEMINFOW_size MY_STRUCT_SIZE_BEFORE(MENUITEMINFOW, hbmpItem)
#endif

static void ConvertItemToSysForm(const CMenuItem &item, MENUITEMINFOW &si)
{
  ZeroMemory(&si, sizeof(si));
  si.cbSize = my_compatib_MENUITEMINFOW_size; // sizeof(si);
  si.fMask = item.fMask;
  si.fType = item.fType;
  si.fState = item.fState;
  si.wID = item.wID;
  si.hSubMenu = item.hSubMenu;
  si.hbmpChecked = item.hbmpChecked;
  si.hbmpUnchecked = item.hbmpUnchecked;
  si.dwItemData = item.dwItemData;
}

#ifndef _UNICODE
static void ConvertItemToSysForm(const CMenuItem &item, MENUITEMINFOA &si)
{
  ZeroMemory(&si, sizeof(si));
  si.cbSize = my_compatib_MENUITEMINFOA_size; // sizeof(si);
  si.fMask = item.fMask;
  si.fType = item.fType;
  si.fState = item.fState;
  si.wID = item.wID;
  si.hSubMenu = item.hSubMenu;
  si.hbmpChecked = item.hbmpChecked;
  si.hbmpUnchecked = item.hbmpUnchecked;
  si.dwItemData = item.dwItemData;
}
#endif

static void ConvertItemToMyForm(const MENUITEMINFOW &si, CMenuItem &item)
{
  item.fMask = si.fMask;
  item.fType = si.fType;
  item.fState = si.fState;
  item.wID = si.wID;
  item.hSubMenu = si.hSubMenu;
  item.hbmpChecked = si.hbmpChecked;
  item.hbmpUnchecked = si.hbmpUnchecked;
  item.dwItemData = si.dwItemData;
}

#ifndef _UNICODE
static void ConvertItemToMyForm(const MENUITEMINFOA &si, CMenuItem &item)
{
  item.fMask = si.fMask;
  item.fType = si.fType;
  item.fState = si.fState;
  item.wID = si.wID;
  item.hSubMenu = si.hSubMenu;
  item.hbmpChecked = si.hbmpChecked;
  item.hbmpUnchecked = si.hbmpUnchecked;
  item.dwItemData = si.dwItemData;
}
#endif

bool CMenu::GetItem(UINT itemIndex, bool byPosition, CMenuItem &item)
{
  const UINT kMaxSize = 512;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    CHAR s[kMaxSize + 1];
    MENUITEMINFOA si;
    ConvertItemToSysForm(item, si);
    if (item.IsString())
    {
      si.cch = kMaxSize;
      si.dwTypeData = s;
    }
    if (GetItemInfo(itemIndex, byPosition, &si))
    {
      ConvertItemToMyForm(si, item);
      if (item.IsString())
        item.StringValue = GetUnicodeString(s);
      return true;
    }
  }
  else
  #endif
  {
    wchar_t s[kMaxSize + 1];
    MENUITEMINFOW si;
    ConvertItemToSysForm(item, si);
    if (item.IsString())
    {
      si.cch = kMaxSize;
      si.dwTypeData = s;
    }
    if (GetItemInfo(itemIndex, byPosition, &si))
    {
      ConvertItemToMyForm(si, item);
      if (item.IsString())
        item.StringValue = s;
      return true;
    }
  }
  return false;
}

bool CMenu::SetItem(UINT itemIndex, bool byPosition, const CMenuItem &item)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    MENUITEMINFOA si;
    ConvertItemToSysForm(item, si);
    AString s;
    if (item.IsString())
    {
      s = GetSystemString(item.StringValue);
      si.dwTypeData = s.Ptr_non_const();
    }
    return SetItemInfo(itemIndex, byPosition, &si);
  }
  else
  #endif
  {
    MENUITEMINFOW si;
    ConvertItemToSysForm(item, si);
    if (item.IsString())
      si.dwTypeData = item.StringValue.Ptr_non_const();
    return SetItemInfo(itemIndex, byPosition, &si);
  }
}

bool CMenu::InsertItem(UINT itemIndex, bool byPosition, const CMenuItem &item)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    MENUITEMINFOA si;
    ConvertItemToSysForm(item, si);
    AString s;
    if (item.IsString())
    {
      s = GetSystemString(item.StringValue);
      si.dwTypeData = s.Ptr_non_const();
    }
    return InsertItem(itemIndex, byPosition, &si);
  }
  else
  #endif
  {
    MENUITEMINFOW si;
    ConvertItemToSysForm(item, si);
    if (item.IsString())
      si.dwTypeData = item.StringValue.Ptr_non_const();
    #ifdef UNDER_CE
    UINT flags = (item.fType & MFT_SEPARATOR) ? MF_SEPARATOR : MF_STRING;
    UINT id = item.wID;
    if ((item.fMask & MIIM_SUBMENU) != 0)
    {
      flags |= MF_POPUP;
      id = (UINT)item.hSubMenu;
    }
    if (!Insert(itemIndex, flags | (byPosition ? MF_BYPOSITION : MF_BYCOMMAND), id, item.StringValue))
      return false;
    return SetItemInfo(itemIndex, byPosition, &si);
    #else
    return InsertItem(itemIndex, byPosition, &si);
    #endif
  }
}

#ifndef _UNICODE
bool CMenu::AppendItem(UINT flags, UINT_PTR newItemID, LPCWSTR newItem)
{
  if (g_IsNT)
    return BOOLToBool(::AppendMenuW(_menu, flags, newItemID, newItem));
  else
    return AppendItem(flags, newItemID, GetSystemString(newItem));
}
#endif

}
