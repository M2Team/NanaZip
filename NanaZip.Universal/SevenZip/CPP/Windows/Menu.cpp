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
will not work at NT4, if cbSize is set as sizeof(MENUITEMINFO).
So we use size of old version of structure in some conditions.
Win98 probably supports full structure including hbmpItem.

We have 2 ways to get/set string in menu item:
win95/NT4: we must use MIIM_TYPE only.
  MIIM_TYPE    : Retrieves or sets the fType and dwTypeData members.
win98/win2000: there are new flags that can be used instead of MIIM_TYPE:
  MIIM_FTYPE   : Retrieves or sets the fType member.
  MIIM_STRING  : Retrieves or sets the dwTypeData member.

Windows versions probably support MIIM_TYPE flag, if we set MENUITEMINFO::cbSize
as sizeof of old (small) MENUITEMINFO that doesn't include (hbmpItem) field.
But do all Windows versions support old MIIM_TYPE flag, if we use
MENUITEMINFO::cbSize as sizeof of new (big) MENUITEMINFO including (hbmpItem) field ?
win10 probably supports any combination of small/big (cbSize) and old/new MIIM_TYPE/MIIM_STRING.
*/

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
#if defined(__clang__) && __clang_major__ >= 13
// error : performing pointer subtraction with a null pointer may have undefined behavior
#pragma GCC diagnostic ignored "-Wnull-pointer-subtraction"
#endif
#endif


#define COPY_MENUITEM_field(d, s, name) \
  d.name = s.name;

#define COPY_MENUITEM_fields(d, s)  \
  COPY_MENUITEM_field(d, s, fMask)  \
  COPY_MENUITEM_field(d, s, fType)  \
  COPY_MENUITEM_field(d, s, fState)  \
  COPY_MENUITEM_field(d, s, wID)  \
  COPY_MENUITEM_field(d, s, hSubMenu)  \
  COPY_MENUITEM_field(d, s, hbmpChecked)  \
  COPY_MENUITEM_field(d, s, hbmpUnchecked)  \
  COPY_MENUITEM_field(d, s, dwItemData)  \

static void ConvertItemToSysForm(const CMenuItem &item, MENUITEMINFOW &si)
{
  ZeroMemory(&si, sizeof(si));
  si.cbSize = my_compatib_MENUITEMINFOW_size; // sizeof(si);
  COPY_MENUITEM_fields(si, item)
}

#ifndef _UNICODE
static void ConvertItemToSysForm(const CMenuItem &item, MENUITEMINFOA &si)
{
  ZeroMemory(&si, sizeof(si));
  si.cbSize = my_compatib_MENUITEMINFOA_size; // sizeof(si);
  COPY_MENUITEM_fields(si, item)
}
#endif

static void ConvertItemToMyForm(const MENUITEMINFOW &si, CMenuItem &item)
{
  COPY_MENUITEM_fields(item, si)
}

#ifndef _UNICODE
static void ConvertItemToMyForm(const MENUITEMINFOA &si, CMenuItem &item)
{
  COPY_MENUITEM_fields(item, si)
}
#endif


bool CMenu::GetItem(UINT itemIndex, bool byPosition, CMenuItem &item) const
{
  item.StringValue.Empty();
  const unsigned kMaxSize = 512;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    MENUITEMINFOA si;
    ConvertItemToSysForm(item, si);
    const bool isString = item.IsString();
    unsigned bufSize = kMaxSize;
    AString a;
    if (isString)
    {
      si.cch = bufSize;
      si.dwTypeData = a.GetBuf(bufSize);
    }
    bool res = GetItemInfo(itemIndex, byPosition, &si);
    if (isString)
      a.ReleaseBuf_CalcLen(bufSize);
    if (!res)
      return false;
    {
      if (isString && si.cch >= bufSize - 1)
      {
        si.dwTypeData = NULL;
        res = GetItemInfo(itemIndex, byPosition, &si);
        if (!res)
          return false;
        si.cch++;
        bufSize = si.cch;
        si.dwTypeData = a.GetBuf(bufSize);
        res = GetItemInfo(itemIndex, byPosition, &si);
        a.ReleaseBuf_CalcLen(bufSize);
        if (!res)
          return false;
      }
      ConvertItemToMyForm(si, item);
      if (isString)
        item.StringValue = GetUnicodeString(a);
      return true;
    }
  }
  else
  #endif
  {
    wchar_t s[kMaxSize + 1];
    s[0] = 0;
    MENUITEMINFOW si;
    ConvertItemToSysForm(item, si);
    const bool isString = item.IsString();
    unsigned bufSize = kMaxSize;
    if (isString)
    {
      si.cch = bufSize;
      si.dwTypeData = s;
    }
    bool res = GetItemInfo(itemIndex, byPosition, &si);
    if (!res)
      return false;
    if (isString)
    {
      s[Z7_ARRAY_SIZE(s) - 1] = 0;
      item.StringValue = s;
      if (si.cch >= bufSize - 1)
      {
        si.dwTypeData = NULL;
        res = GetItemInfo(itemIndex, byPosition, &si);
        if (!res)
          return false;
        si.cch++;
        bufSize = si.cch;
        si.dwTypeData = item.StringValue.GetBuf(bufSize);
        res = GetItemInfo(itemIndex, byPosition, &si);
        item.StringValue.ReleaseBuf_CalcLen(bufSize);
        if (!res)
          return false;
      }
      // if (item.StringValue.Len() != si.cch) throw 123; // for debug
    }
    ConvertItemToMyForm(si, item);
    return true;
  }
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
    UINT_PTR id = item.wID;
    if ((item.fMask & MIIM_SUBMENU) != 0)
    {
      flags |= MF_POPUP;
      id = (UINT_PTR)item.hSubMenu;
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
