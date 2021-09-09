// Windows/Control/Dialog.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../../Common/StringConvert.h"
#endif

#include "Dialog.h"

extern HINSTANCE g_hInstance;
#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NControl {

static INT_PTR APIENTRY DialogProcedure(HWND dialogHWND, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(dialogHWND);
  if (message == WM_INITDIALOG)
    tempDialog.SetUserDataLongPtr(lParam);
  CDialog *dialog = (CDialog *)(tempDialog.GetUserDataLongPtr());
  if (dialog == NULL)
    return FALSE;
  if (message == WM_INITDIALOG)
    dialog->Attach(dialogHWND);

  /* MSDN: The dialog box procedure should return
       TRUE  - if it processed the message
       FALSE - if it did not process the message
     If the dialog box procedure returns FALSE,
     the dialog manager performs the default dialog operation in response to the message.
  */

  try { return BoolToBOOL(dialog->OnMessage(message, wParam, lParam)); }
  catch(...) { return TRUE; }
}

bool CDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_INITDIALOG: return OnInit();
    case WM_COMMAND: return OnCommand(wParam, lParam);
    case WM_NOTIFY: return OnNotify((UINT)wParam, (LPNMHDR) lParam);
    case WM_TIMER: return OnTimer(wParam, lParam);
    case WM_SIZE: return OnSize(wParam, LOWORD(lParam), HIWORD(lParam));
    case WM_DESTROY: return OnDestroy();
    case WM_HELP: OnHelp(); return true;
    /*
        OnHelp(
          #ifdef UNDER_CE
          (void *)
          #else
          (LPHELPINFO)
          #endif
          lParam);
        return true;
    */
    default: return false;
  }
}

bool CDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
  return OnCommand(HIWORD(wParam), LOWORD(wParam), lParam);
}

bool CDialog::OnCommand(int code, int itemID, LPARAM lParam)
{
  if (code == BN_CLICKED)
    return OnButtonClicked(itemID, (HWND)lParam);
  return false;
}

bool CDialog::OnButtonClicked(int buttonID, HWND /* buttonHWND */)
{
  switch (buttonID)
  {
    case IDOK: OnOK(); break;
    case IDCANCEL: OnCancel(); break;
    case IDCLOSE: OnClose(); break;
    case IDHELP: OnHelp(); break;
    default: return false;
  }
  return true;
}

static bool GetWorkAreaRect(RECT *rect)
{
  // use another function for multi-monitor.
  return BOOLToBool(::SystemParametersInfo(SPI_GETWORKAREA, 0, rect, 0));
}

bool IsDialogSizeOK(int xSize, int ySize)
{
  // it returns for system font. Real font uses another values
  LONG v = GetDialogBaseUnits();
  int x = LOWORD(v);
  int y = HIWORD(v);

  RECT rect;
  GetWorkAreaRect(&rect);
  int wx = RECT_SIZE_X(rect);
  int wy = RECT_SIZE_Y(rect);
  return
    xSize / 4 * x <= wx &&
    ySize / 8 * y <= wy;
}

bool CDialog::GetMargins(int margin, int &x, int &y)
{
  x = margin;
  y = margin;
  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = margin;
  rect.bottom = margin;
  if (!MapRect(&rect))
    return false;
  x = rect.right - rect.left;
  y = rect.bottom - rect.top;
  return true;
}

int CDialog::Units_To_Pixels_X(int units)
{
  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = units;
  rect.bottom = units;
  if (!MapRect(&rect))
    return units * 3 / 2;
  return rect.right - rect.left;
}

bool CDialog::GetItemSizes(int id, int &x, int &y)
{
  RECT rect;
  if (!::GetWindowRect(GetItem(id), &rect))
    return false;
  x = RECT_SIZE_X(rect);
  y = RECT_SIZE_Y(rect);
  return true;
}

void CDialog::GetClientRectOfItem(int id, RECT &rect)
{
  ::GetWindowRect(GetItem(id), &rect);
  ScreenToClient(&rect);
}

bool CDialog::MoveItem(int id, int x, int y, int width, int height, bool repaint)
{
  return BOOLToBool(::MoveWindow(GetItem(id), x, y, width, height, BoolToBOOL(repaint)));
}

void CDialog::NormalizeSize(bool fullNormalize)
{
  RECT workRect;
  GetWorkAreaRect(&workRect);
  int xSize = RECT_SIZE_X(workRect);
  int ySize = RECT_SIZE_Y(workRect);
  RECT rect;
  GetWindowRect(&rect);
  int xSize2 = RECT_SIZE_X(rect);
  int ySize2 = RECT_SIZE_Y(rect);
  bool needMove = (xSize2 > xSize || ySize2 > ySize);
  if (xSize2 > xSize || (needMove && fullNormalize))
  {
    rect.left = workRect.left;
    rect.right = workRect.right;
    xSize2 = xSize;
  }
  if (ySize2 > ySize || (needMove && fullNormalize))
  {
    rect.top = workRect.top;
    rect.bottom = workRect.bottom;
    ySize2 = ySize;
  }
  if (needMove)
  {
    if (fullNormalize)
      Show(SW_SHOWMAXIMIZED);
    else
      Move(rect.left, rect.top, xSize2, ySize2, true);
  }
}

void CDialog::NormalizePosition()
{
  RECT workRect, rect;
  GetWorkAreaRect(&workRect);
  GetWindowRect(&rect);
  if (rect.bottom > workRect.bottom && rect.top > workRect.top)
    Move(rect.left, workRect.top, RECT_SIZE_X(rect), RECT_SIZE_Y(rect), true);
}

bool CModelessDialog::Create(LPCTSTR templateName, HWND parentWindow)
{
  HWND aHWND = CreateDialogParam(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
  if (aHWND == 0)
    return false;
  Attach(aHWND);
  return true;
}

INT_PTR CModalDialog::Create(LPCTSTR templateName, HWND parentWindow)
{
  return DialogBoxParam(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
}

#ifndef _UNICODE

bool CModelessDialog::Create(LPCWSTR templateName, HWND parentWindow)
{
  HWND aHWND;
  if (g_IsNT)
    aHWND = CreateDialogParamW(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
  else
  {
    AString name;
    LPCSTR templateNameA;
    if (IS_INTRESOURCE(templateName))
      templateNameA = (LPCSTR)templateName;
    else
    {
      name = GetSystemString(templateName);
      templateNameA = name;
    }
    aHWND = CreateDialogParamA(g_hInstance, templateNameA, parentWindow, DialogProcedure, (LPARAM)this);
  }
  if (aHWND == 0)
    return false;
  Attach(aHWND);
  return true;
}

INT_PTR CModalDialog::Create(LPCWSTR templateName, HWND parentWindow)
{
  if (g_IsNT)
    return DialogBoxParamW(g_hInstance, templateName, parentWindow, DialogProcedure, (LPARAM)this);
  AString name;
  LPCSTR templateNameA;
  if (IS_INTRESOURCE(templateName))
    templateNameA = (LPCSTR)templateName;
  else
  {
    name = GetSystemString(templateName);
    templateNameA = name;
  }
  return DialogBoxParamA(g_hInstance, templateNameA, parentWindow, DialogProcedure, (LPARAM)this);
}
#endif

}}
