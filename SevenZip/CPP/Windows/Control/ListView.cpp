// Windows/Control/ListView.cpp

#include "StdAfx.h"

#include "ListView.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NControl {

bool CListView::CreateEx(DWORD exStyle, DWORD style,
      int x, int y, int width, int height,
      HWND parentWindow, HMENU idOrHMenu,
      HINSTANCE instance, LPVOID createParam)
{
  return CWindow::CreateEx(exStyle, WC_LISTVIEW, TEXT(""), style, x, y, width,
      height, parentWindow, idOrHMenu, instance, createParam);
}

bool CListView::GetItemParam(int index, LPARAM &param) const
{
  LVITEM item;
  item.iItem = index;
  item.iSubItem = 0;
  item.mask = LVIF_PARAM;
  bool aResult = GetItem(&item);
  param = item.lParam;
  return aResult;
}

int CListView::InsertColumn(int columnIndex, LPCTSTR text, int width)
{
  LVCOLUMN ci;
  ci.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
  ci.pszText = (LPTSTR)(void *)text;
  ci.iSubItem = columnIndex;
  ci.cx = width;
  return InsertColumn(columnIndex, &ci);
}

int CListView::InsertItem(int index, LPCTSTR text)
{
  LVITEM item;
  item.mask = LVIF_TEXT | LVIF_PARAM;
  item.iItem = index;
  item.lParam = index;
  item.pszText = (LPTSTR)(void *)text;
  item.iSubItem = 0;
  return InsertItem(&item);
}

int CListView::SetSubItem(int index, int subIndex, LPCTSTR text)
{
  LVITEM item;
  item.mask = LVIF_TEXT;
  item.iItem = index;
  item.pszText = (LPTSTR)(void *)text;
  item.iSubItem = subIndex;
  return SetItem(&item);
}

#ifndef _UNICODE

int CListView::InsertColumn(int columnIndex, LPCWSTR text, int width)
{
  LVCOLUMNW ci;
  ci.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
  ci.pszText = (LPWSTR)(void *)text;
  ci.iSubItem = columnIndex;
  ci.cx = width;
  return InsertColumn(columnIndex, &ci);
}

int CListView::InsertItem(int index, LPCWSTR text)
{
  LVITEMW item;
  item.mask = LVIF_TEXT | LVIF_PARAM;
  item.iItem = index;
  item.lParam = index;
  item.pszText = (LPWSTR)(void *)text;
  item.iSubItem = 0;
  return InsertItem(&item);
}

int CListView::SetSubItem(int index, int subIndex, LPCWSTR text)
{
  LVITEMW item;
  item.mask = LVIF_TEXT;
  item.iItem = index;
  item.pszText = (LPWSTR)(void *)text;
  item.iSubItem = subIndex;
  return SetItem(&item);
}

#endif

static LRESULT APIENTRY ListViewSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow window(hwnd);
  CListView2 *w = (CListView2 *)(window.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}

LRESULT CListView2::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  #ifndef _UNICODE
  if (g_IsNT)
    return CallWindowProcW(_origWindowProc, *this, message, wParam, lParam);
  else
  #endif
    return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}

void CListView2::SetWindowProc()
{
  SetUserDataLongPtr((LONG_PTR)this);
  #ifndef _UNICODE
  if (g_IsNT)
    _origWindowProc = (WNDPROC)SetLongPtrW(GWLP_WNDPROC, (LONG_PTR)ListViewSubclassProc);
  else
  #endif
    _origWindowProc = (WNDPROC)SetLongPtr(GWLP_WNDPROC, (LONG_PTR)ListViewSubclassProc);
}

/*
LRESULT CListView3::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT res = CListView2::OnMessage(message, wParam, lParam);
  if (message == WM_GETDLGCODE)
  {
    // when user presses RETURN, windows sends default (first) button command to parent dialog.
    // we disable this:
    MSG *msg = (MSG *)lParam;
    WPARAM key = wParam;
    bool change = false;
    if (msg)
    {
      if (msg->message == WM_KEYDOWN && msg->wParam == VK_RETURN)
        change = true;
    }
    else if (wParam == VK_RETURN)
      change = true;
    if (change)
      res |= DLGC_WANTALLKEYS;
  }
  return res;
}
*/
  
}}
