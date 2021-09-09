// Windows/Window.h

#ifndef __WINDOWS_WINDOW_H
#define __WINDOWS_WINDOW_H

#include "../Common/MyWindows.h"
#include "../Common/MyString.h"

#include "Defs.h"

#ifndef UNDER_CE

#define MY__WM_CHANGEUISTATE  0x0127
#define MY__WM_UPDATEUISTATE  0x0128
#define MY__WM_QUERYUISTATE   0x0129

// LOWORD(wParam) values in WM_*UISTATE
#define MY__UIS_SET         1
#define MY__UIS_CLEAR       2
#define MY__UIS_INITIALIZE  3

// HIWORD(wParam) values in WM_*UISTATE
#define MY__UISF_HIDEFOCUS  0x1
#define MY__UISF_HIDEACCEL  0x2
#define MY__UISF_ACTIVE     0x4

#endif

namespace NWindows {

inline ATOM MyRegisterClass(CONST WNDCLASS *wndClass)
  { return ::RegisterClass(wndClass); }

#ifndef _UNICODE
ATOM MyRegisterClass(CONST WNDCLASSW *wndClass);
#endif

#ifdef _UNICODE
inline bool MySetWindowText(HWND wnd, LPCWSTR s) { return BOOLToBool(::SetWindowText(wnd, s)); }
#else
bool MySetWindowText(HWND wnd, LPCWSTR s);
#endif


#ifdef UNDER_CE
#define GWLP_USERDATA GWL_USERDATA
#define GWLP_WNDPROC GWL_WNDPROC
#define BTNS_BUTTON TBSTYLE_BUTTON
#define WC_COMBOBOXW L"ComboBox"
#define DWLP_MSGRESULT DWL_MSGRESULT
#endif

class CWindow
{
private:
   // bool ModifyStyleBase(int styleOffset, DWORD remove, DWORD add, UINT flags);
protected:
  HWND _window;
public:
  CWindow(HWND newWindow = NULL): _window(newWindow){};
  CWindow& operator=(HWND newWindow)
  {
    _window = newWindow;
    return *this;
  }
  operator HWND() const { return _window; }
  void Attach(HWND newWindow) { _window = newWindow; }
  HWND Detach()
  {
    HWND window = _window;
    _window = NULL;
    return window;
  }

  bool Foreground() { return BOOLToBool(::SetForegroundWindow(_window)); }
  
  HWND GetParent() const { return ::GetParent(_window); }
  bool GetWindowRect(LPRECT rect) const { return BOOLToBool(::GetWindowRect(_window,rect)); }
  #ifndef UNDER_CE
  bool IsZoomed() const { return BOOLToBool(::IsZoomed(_window)); }
  #endif
  bool ClientToScreen(LPPOINT point) const { return BOOLToBool(::ClientToScreen(_window, point)); }
  bool ScreenToClient(LPPOINT point) const { return BOOLToBool(::ScreenToClient(_window, point)); }

  bool CreateEx(DWORD exStyle, LPCTSTR className,
      LPCTSTR windowName, DWORD style,
      int x, int y, int width, int height,
      HWND parentWindow, HMENU idOrHMenu,
      HINSTANCE instance, LPVOID createParam)
  {
    _window = ::CreateWindowEx(exStyle, className, windowName,
      style, x, y, width, height, parentWindow,
      idOrHMenu, instance, createParam);
    return (_window != NULL);
  }

  bool Create(LPCTSTR className,
      LPCTSTR windowName, DWORD style,
      int x, int y, int width, int height,
      HWND parentWindow, HMENU idOrHMenu,
      HINSTANCE instance, LPVOID createParam)
  {
    _window = ::CreateWindow(className, windowName,
      style, x, y, width, height, parentWindow,
      idOrHMenu, instance, createParam);
    return (_window != NULL);
  }

  #ifndef _UNICODE
  bool Create(LPCWSTR className,
      LPCWSTR windowName, DWORD style,
      int x, int y, int width, int height,
      HWND parentWindow, HMENU idOrHMenu,
      HINSTANCE instance, LPVOID createParam);
  bool CreateEx(DWORD exStyle, LPCWSTR className,
      LPCWSTR windowName, DWORD style,
      int x, int y, int width, int height,
      HWND parentWindow, HMENU idOrHMenu,
      HINSTANCE instance, LPVOID createParam);
  #endif


  bool Destroy()
  {
    if (_window == NULL)
      return true;
    bool result = BOOLToBool(::DestroyWindow(_window));
    if (result)
      _window = NULL;
    return result;
  }
  bool IsWindow() {  return BOOLToBool(::IsWindow(_window)); }
  bool Move(int x, int y, int width, int height, bool repaint = true)
    { return BOOLToBool(::MoveWindow(_window, x, y, width, height, BoolToBOOL(repaint))); }

  bool ChangeSubWindowSizeX(HWND hwnd, int xSize)
  {
    RECT rect;
    ::GetWindowRect(hwnd, &rect);
    POINT p1;
    p1.x = rect.left;
    p1.y = rect.top;
    ScreenToClient(&p1);
    return BOOLToBool(::MoveWindow(hwnd, p1.x, p1.y, xSize, rect.bottom - rect.top, TRUE));
  }

  void ScreenToClient(RECT *rect)
  {
    POINT p1, p2;
    p1.x = rect->left;
    p1.y = rect->top;
    p2.x = rect->right;
    p2.y = rect->bottom;
    ScreenToClient(&p1);
    ScreenToClient(&p2);

    rect->left = p1.x;
    rect->top = p1.y;
    rect->right = p2.x;
    rect->bottom = p2.y;
  }

  bool GetClientRect(LPRECT rect) { return BOOLToBool(::GetClientRect(_window, rect)); }
  bool Show(int cmdShow) { return BOOLToBool(::ShowWindow(_window, cmdShow)); }
  bool Show_Bool(bool show) { return Show(show ? SW_SHOW: SW_HIDE); }

  #ifndef UNDER_CE
  bool SetPlacement(CONST WINDOWPLACEMENT *placement) { return BOOLToBool(::SetWindowPlacement(_window, placement)); }
  bool GetPlacement(WINDOWPLACEMENT *placement) { return BOOLToBool(::GetWindowPlacement(_window, placement)); }
  #endif
  bool Update() { return BOOLToBool(::UpdateWindow(_window)); }
  bool InvalidateRect(LPCRECT rect, bool backgroundErase = true)
    { return BOOLToBool(::InvalidateRect(_window, rect, BoolToBOOL(backgroundErase))); }
  void SetRedraw(bool redraw = true) { SendMsg(WM_SETREDRAW, (WPARAM)BoolToBOOL(redraw), 0); }

  LONG_PTR SetStyle(LONG_PTR style) { return SetLongPtr(GWL_STYLE, style); }
  LONG_PTR GetStyle() const { return GetLongPtr(GWL_STYLE); }
  // bool MyIsMaximized() const { return ((GetStyle() & WS_MAXIMIZE) != 0); }

  LONG_PTR SetLong(int index, LONG newLongPtr) { return ::SetWindowLong(_window, index, newLongPtr); }
  LONG_PTR GetLong(int index) const { return ::GetWindowLong(_window, index); }
  LONG_PTR SetUserDataLong(LONG newLongPtr) { return SetLong(GWLP_USERDATA, newLongPtr); }
  LONG_PTR GetUserDataLong() const { return GetLong(GWLP_USERDATA); }


  #ifdef UNDER_CE

  LONG_PTR SetLongPtr(int index, LONG_PTR newLongPtr) { return SetLong(index, newLongPtr); }
  LONG_PTR GetLongPtr(int index) const { return GetLong(index); }

  LONG_PTR SetUserDataLongPtr(LONG_PTR newLongPtr) { return SetUserDataLong(newLongPtr); }
  LONG_PTR GetUserDataLongPtr() const { return GetUserDataLong(); }
  
  #else
  
  LONG_PTR SetLongPtr(int index, LONG_PTR newLongPtr)
    { return ::SetWindowLongPtr(_window, index,
          #ifndef _WIN64
          (LONG)
          #endif
          newLongPtr); }
  #ifndef _UNICODE
  LONG_PTR SetLongPtrW(int index, LONG_PTR newLongPtr)
    { return ::SetWindowLongPtrW(_window, index,
          #ifndef _WIN64
          (LONG)
          #endif
          newLongPtr); }
  #endif

  LONG_PTR GetLongPtr(int index) const { return ::GetWindowLongPtr(_window, index); }
  LONG_PTR SetUserDataLongPtr(LONG_PTR newLongPtr) { return SetLongPtr(GWLP_USERDATA, newLongPtr); }
  LONG_PTR GetUserDataLongPtr() const { return GetLongPtr(GWLP_USERDATA); }
  
  #endif
  
  /*
  bool ModifyStyle(HWND hWnd, DWORD remove, DWORD add, UINT flags = 0)
    {  return ModifyStyleBase(GWL_STYLE, remove, add, flags); }
  bool ModifyStyleEx(HWND hWnd, DWORD remove, DWORD add, UINT flags = 0)
    { return ModifyStyleBase(GWL_EXSTYLE, remove, add, flags); }
  */
 
  HWND SetFocus() { return ::SetFocus(_window); }

  LRESULT SendMsg(UINT message, WPARAM wParam = 0, LPARAM lParam = 0)
    { return ::SendMessage(_window, message, wParam, lParam); }
  #ifndef _UNICODE
  LRESULT SendMsgW(UINT message, WPARAM wParam = 0, LPARAM lParam = 0)
    { return ::SendMessageW(_window, message, wParam, lParam); }
  #endif

  bool PostMsg(UINT message, WPARAM wParam = 0, LPARAM lParam = 0)
    { return BOOLToBool(::PostMessage(_window, message, wParam, lParam)); }
  #ifndef _UNICODE
  bool PostMsgW(UINT message, WPARAM wParam = 0, LPARAM lParam = 0)
    { return BOOLToBool(::PostMessageW(_window, message, wParam, lParam)); }
  #endif

  bool SetText(LPCTSTR s) { return BOOLToBool(::SetWindowText(_window, s)); }
  #ifndef _UNICODE
  bool SetText(LPCWSTR s) { return MySetWindowText(_window, s); }
  #endif

  int GetTextLength() const
    { return GetWindowTextLength(_window); }
  int GetText(LPTSTR string, int maxCount) const
    { return GetWindowText(_window, string, maxCount); }
  bool GetText(CSysString &s);
  #ifndef _UNICODE
  /*
  UINT GetText(LPWSTR string, int maxCount) const
    { return GetWindowTextW(_window, string, maxCount); }
  */
  bool GetText(UString &s);
  #endif

  bool Enable(bool enable)
    { return BOOLToBool(::EnableWindow(_window, BoolToBOOL(enable))); }
  
  bool IsEnabled()
    { return BOOLToBool(::IsWindowEnabled(_window)); }
  
  #ifndef UNDER_CE
  HMENU GetSystemMenu(bool revert)
    { return ::GetSystemMenu(_window, BoolToBOOL(revert)); }
  #endif

  UINT_PTR SetTimer(UINT_PTR idEvent, UINT elapse, TIMERPROC timerFunc = 0)
    { return ::SetTimer(_window, idEvent, elapse, timerFunc); }
  bool KillTimer(UINT_PTR idEvent)
    {return BOOLToBool(::KillTimer(_window, idEvent)); }

  HICON SetIcon(WPARAM sizeType, HICON icon) { return (HICON)SendMsg(WM_SETICON, sizeType, (LPARAM)icon); }
};

#define RECT_SIZE_X(r) ((r).right - (r).left)
#define RECT_SIZE_Y(r) ((r).bottom - (r).top)

inline bool IsKeyDown(int virtKey) { return (::GetKeyState(virtKey) & 0x8000) != 0; }

}

#endif
