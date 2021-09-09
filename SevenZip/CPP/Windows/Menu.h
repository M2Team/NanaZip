// Windows/Menu.h

#ifndef __WINDOWS_MENU_H
#define __WINDOWS_MENU_H

#include "../Common/MyString.h"

#include "Defs.h"

namespace NWindows {

struct CMenuItem
{
  UString StringValue;
  UINT fMask;
  UINT fType;
  UINT fState;
  UINT wID;
  HMENU hSubMenu;
  HBITMAP hbmpChecked;
  HBITMAP hbmpUnchecked;
  ULONG_PTR dwItemData;
  // LPTSTR dwTypeData;
  // UINT cch;
  // HBITMAP hbmpItem;
  bool IsString() const // change it MIIM_STRING
    { return ((fMask & MIIM_TYPE) != 0 && (fType == MFT_STRING)); }
  bool IsSeparator() const { return (fType == MFT_SEPARATOR); }
  CMenuItem(): fMask(0), fType(0), fState(0), wID(0), hSubMenu(0), hbmpChecked(0),
    hbmpUnchecked(0), dwItemData(0) {}
};

class CMenu
{
  HMENU _menu;
public:
  CMenu(): _menu(NULL) {};
  operator HMENU() const { return _menu; }
  void Attach(HMENU menu) { _menu = menu; }
  
  HMENU Detach()
  {
    HMENU menu = _menu;
    _menu = NULL;
    return menu;
  }
  
  bool Create()
  {
    _menu = ::CreateMenu();
    return (_menu != NULL);
  }

  bool CreatePopup()
  {
    _menu = ::CreatePopupMenu();
    return (_menu != NULL);
  }
  
  bool Destroy()
  {
    if (_menu == NULL)
      return false;
    return BOOLToBool(::DestroyMenu(Detach()));
  }
  
  int GetItemCount()
  {
    #ifdef UNDER_CE
    for (int i = 0;; i++)
    {
      CMenuItem item;
      item.fMask = MIIM_STATE;
      if (!GetItem(i, true, item))
        return i;
    }
    #else
    return GetMenuItemCount(_menu);
    #endif
  }

  HMENU GetSubMenu(int pos) { return ::GetSubMenu(_menu, pos); }
  #ifndef UNDER_CE
  /*
  bool GetItemString(UINT idItem, UINT flag, CSysString &result)
  {
    result.Empty();
    int len = ::GetMenuString(_menu, idItem, 0, 0, flag);
    int len2 = ::GetMenuString(_menu, idItem, result.GetBuf(len + 2), len + 1, flag);
    if (len > len2)
      len = len2;
    result.ReleaseBuf_CalcLen(len + 2);
    return (len != 0);
  }
  */
  UINT GetItemID(int pos) { return ::GetMenuItemID(_menu, pos);   }
  UINT GetItemState(UINT id, UINT flags) { return ::GetMenuState(_menu, id, flags);   }
  #endif
  
  bool GetItemInfo(UINT itemIndex, bool byPosition, LPMENUITEMINFO itemInfo)
    { return BOOLToBool(::GetMenuItemInfo(_menu, itemIndex, BoolToBOOL(byPosition), itemInfo)); }
  bool SetItemInfo(UINT itemIndex, bool byPosition, LPMENUITEMINFO itemInfo)
    { return BOOLToBool(::SetMenuItemInfo(_menu, itemIndex, BoolToBOOL(byPosition), itemInfo)); }

  bool AppendItem(UINT flags, UINT_PTR newItemID, LPCTSTR newItem)
    { return BOOLToBool(::AppendMenu(_menu, flags, newItemID, newItem)); }

  bool Insert(UINT position, UINT flags, UINT_PTR idNewItem, LPCTSTR newItem)
    { return BOOLToBool(::InsertMenu(_menu, position, flags, idNewItem, newItem)); }

  #ifndef UNDER_CE
  bool InsertItem(UINT itemIndex, bool byPosition, LPCMENUITEMINFO itemInfo)
    { return BOOLToBool(::InsertMenuItem(_menu, itemIndex, BoolToBOOL(byPosition), itemInfo)); }
  #endif

  bool RemoveItem(UINT item, UINT flags) { return BOOLToBool(::RemoveMenu(_menu, item, flags)); }
  void RemoveAllItemsFrom(UINT index) { while (RemoveItem(index, MF_BYPOSITION)); }
  void RemoveAllItems() { RemoveAllItemsFrom(0); }

  #ifndef _UNICODE
  bool GetItemInfo(UINT itemIndex, bool byPosition, LPMENUITEMINFOW itemInfo)
    { return BOOLToBool(::GetMenuItemInfoW(_menu, itemIndex, BoolToBOOL(byPosition), itemInfo)); }
  bool InsertItem(UINT itemIndex, bool byPosition, LPMENUITEMINFOW itemInfo)
    { return BOOLToBool(::InsertMenuItemW(_menu, itemIndex, BoolToBOOL(byPosition), itemInfo)); }
  bool SetItemInfo(UINT itemIndex, bool byPosition, LPMENUITEMINFOW itemInfo)
    { return BOOLToBool(::SetMenuItemInfoW(_menu, itemIndex, BoolToBOOL(byPosition), itemInfo)); }
  bool AppendItem(UINT flags, UINT_PTR newItemID, LPCWSTR newItem);
  #endif

  bool GetItem(UINT itemIndex, bool byPosition, CMenuItem &item);
  bool SetItem(UINT itemIndex, bool byPosition, const CMenuItem &item);
  bool InsertItem(UINT itemIndex, bool byPosition, const CMenuItem &item);

  int Track(UINT flags, int x, int y, HWND hWnd) { return ::TrackPopupMenuEx(_menu, flags, x, y, hWnd, NULL); }

  bool CheckRadioItem(UINT idFirst, UINT idLast, UINT idCheck, UINT flags)
    { return BOOLToBool(::CheckMenuRadioItem(_menu, idFirst, idLast, idCheck, flags)); }

  DWORD CheckItem(UINT id, UINT uCheck) { return ::CheckMenuItem(_menu, id, uCheck); }
  DWORD CheckItemByID(UINT id, bool check) { return CheckItem(id, MF_BYCOMMAND | (check ? MF_CHECKED : MF_UNCHECKED)); }

  BOOL EnableItem(UINT uIDEnableItem, UINT uEnable) { return EnableMenuItem(_menu, uIDEnableItem, uEnable); }
};

class CMenuDestroyer
{
  CMenu *_menu;
public:
  CMenuDestroyer(CMenu &menu): _menu(&menu) {}
  CMenuDestroyer(): _menu(0) {}
  ~CMenuDestroyer() { if (_menu) _menu->Destroy(); }
  void Attach(CMenu &menu) { _menu = &menu; }
  void Disable() { _menu = 0; }
};

}

#endif
