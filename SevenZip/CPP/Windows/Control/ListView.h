// Windows/Control/ListView.h

#ifndef __WINDOWS_CONTROL_LISTVIEW_H
#define __WINDOWS_CONTROL_LISTVIEW_H

#include "../../Common/MyWindows.h"

#include <CommCtrl.h>

#include "../Window.h"

namespace NWindows {
namespace NControl {

class CListView: public NWindows::CWindow
{
public:
  bool CreateEx(DWORD exStyle, DWORD style,
      int x, int y, int width, int height,
      HWND parentWindow, HMENU idOrHMenu,
      HINSTANCE instance, LPVOID createParam);

  void SetUnicodeFormat()
  {
    #ifndef UNDER_CE
    ListView_SetUnicodeFormat(_window, TRUE);
    #endif
  }
 
  bool DeleteAllItems() { return BOOLToBool(ListView_DeleteAllItems(_window)); }
  bool DeleteColumn(int columnIndex) { return BOOLToBool(ListView_DeleteColumn(_window, columnIndex)); }

  int InsertColumn(int columnIndex, const LVCOLUMN *columnInfo) { return ListView_InsertColumn(_window, columnIndex, columnInfo); }
  int InsertColumn(int columnIndex, LPCTSTR text, int width);
  bool SetColumnOrderArray(int count, const int *columns)
    { return BOOLToBool(ListView_SetColumnOrderArray(_window, count, (int *)(void *)columns)); }

  /*
  int GetNumColumns()
  {
    HWND header = ListView_GetHeader(_window);
    if (!header)
      return -1;
    return Header_GetItemCount(header);
  }
  */

  int InsertItem(const LVITEM* item) { return ListView_InsertItem(_window, item); }
  int InsertItem(int index, LPCTSTR text);
  bool SetItem(const LVITEM* item) { return BOOLToBool(ListView_SetItem(_window, item)); }
  int SetSubItem(int index, int subIndex, LPCTSTR text);

  #ifndef _UNICODE

  int InsertColumn(int columnIndex, const LVCOLUMNW *columnInfo) { return (int)SendMsg(LVM_INSERTCOLUMNW, (WPARAM)columnIndex, (LPARAM)columnInfo); }
  int InsertColumn(int columnIndex, LPCWSTR text, int width);
  int InsertItem(const LV_ITEMW* item) { return (int)SendMsg(LVM_INSERTITEMW, 0, (LPARAM)item); }
  int InsertItem(int index, LPCWSTR text);
  bool SetItem(const LV_ITEMW* item) { return BOOLToBool((BOOL)SendMsg(LVM_SETITEMW, 0, (LPARAM)item)); }
  int SetSubItem(int index, int subIndex, LPCWSTR text);

  #endif

  bool DeleteItem(int itemIndex) { return BOOLToBool(ListView_DeleteItem(_window, itemIndex)); }

  UINT GetSelectedCount() const { return ListView_GetSelectedCount(_window); }
  int GetItemCount() const { return ListView_GetItemCount(_window); }

  INT GetSelectionMark() const { return ListView_GetSelectionMark(_window); }

  void SetItemCount(int numItems) { ListView_SetItemCount(_window, numItems); }
  void SetItemCountEx(int numItems, DWORD flags) {  ListView_SetItemCountEx(_window, numItems, flags); }

  int GetNextItem(int startIndex, UINT flags) const { return ListView_GetNextItem(_window, startIndex, flags); }
  int GetNextSelectedItem(int startIndex) const { return GetNextItem(startIndex, LVNI_SELECTED); }
  int GetFocusedItem() const { return GetNextItem(-1, LVNI_FOCUSED); }
  
  bool GetItem(LVITEM* item) const { return BOOLToBool(ListView_GetItem(_window, item)); }
  bool GetItemParam(int itemIndex, LPARAM &param) const;
  void GetItemText(int itemIndex, int subItemIndex, LPTSTR text, int textSizeMax) const
    { ListView_GetItemText(_window, itemIndex, subItemIndex, text, textSizeMax); }
  bool SortItems(PFNLVCOMPARE compareFunction, LPARAM dataParam)
    { return BOOLToBool(ListView_SortItems(_window, compareFunction, dataParam)); }

  void SetItemState(int index, UINT state, UINT mask) { ListView_SetItemState(_window, index, state, mask); }
  void SetItemState_Selected(int index, bool select) { SetItemState(index, select ? LVIS_SELECTED : 0, LVIS_SELECTED); }
  void SetItemState_Selected(int index) { SetItemState(index, LVIS_SELECTED, LVIS_SELECTED); }
  void SelectAll() { SetItemState_Selected(-1); }
  void SetItemState_FocusedSelected(int index) { SetItemState(index, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED); }
  UINT GetItemState(int index, UINT mask) const { return ListView_GetItemState(_window, index, mask); }
  bool IsItemSelected(int index) const { return GetItemState(index, LVIS_SELECTED) == LVIS_SELECTED; }

  bool GetColumn(int columnIndex, LVCOLUMN* columnInfo) const
    { return BOOLToBool(ListView_GetColumn(_window, columnIndex, columnInfo)); }

  HIMAGELIST SetImageList(HIMAGELIST imageList, int imageListType)
    { return ListView_SetImageList(_window, imageList, imageListType); }

  // version 4.70: NT5 | (NT4 + ie3) | w98 | (w95 + ie3)
  DWORD GetExtendedListViewStyle() { return ListView_GetExtendedListViewStyle(_window); }
  void SetExtendedListViewStyle(DWORD exStyle) { ListView_SetExtendedListViewStyle(_window, exStyle); }
  void SetExtendedListViewStyle(DWORD exMask, DWORD exStyle) { ListView_SetExtendedListViewStyleEx(_window, exMask, exStyle); }

  void SetCheckState(UINT index, bool checkState) { ListView_SetCheckState(_window, index, BoolToBOOL(checkState)); }
  bool GetCheckState(UINT index) { return BOOLToBool(ListView_GetCheckState(_window, index)); }

  bool EnsureVisible(int index, bool partialOK) { return BOOLToBool(ListView_EnsureVisible(_window, index, BoolToBOOL(partialOK))); }

  bool GetItemRect(int index, RECT *rect, int code) { return BOOLToBool(ListView_GetItemRect(_window, index, rect, code)); }

  HWND GetEditControl() { return ListView_GetEditControl(_window) ; }
  HWND EditLabel(int itemIndex) { return ListView_EditLabel(_window, itemIndex) ; }

  bool RedrawItems(int firstIndex, int lastIndex) { return BOOLToBool(ListView_RedrawItems(_window, firstIndex, lastIndex)); }
  bool RedrawAllItems()
  {
    if (GetItemCount() > 0)
      return RedrawItems(0, GetItemCount() - 1);
    return true;
  }
  bool RedrawItem(int index) { return RedrawItems(index, index); }
 
  int HitTest(LPLVHITTESTINFO info) { return ListView_HitTest(_window, info); }
  COLORREF GetBkColor() { return ListView_GetBkColor(_window); }
  bool SetColumnWidth(int iCol, int cx) { return BOOLToBool(ListView_SetColumnWidth(_window, iCol, cx)); }
  bool SetColumnWidthAuto(int iCol) { return SetColumnWidth(iCol, LVSCW_AUTOSIZE); }
};

class CListView2: public CListView
{
  WNDPROC _origWindowProc;
public:
  void SetWindowProc();
  virtual LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
};

/*
class CListView3: public CListView2
{
public:
  virtual LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
};
*/

}}

#endif
