// CopyDialog.cpp

#include "StdAfx.h"

#undef GetCurrentTime

#include "../../../Windows/FileName.h"

#include "../../../Windows/Control/Static.h"

#include "BrowseDialog.h"
#include "CopyDialog.h"

#ifdef LANG
#include "LangUtils.h"
#endif

#include <Mile.Xaml.h>
#include <dwmapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <Mile.Helpers.h>

using namespace NWindows;

INT_PTR CCopyDialog::Create(HWND parentWindow)
{
  /*
  #ifdef LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  _path.Attach(GetItem(IDC_COPY));
  SetText(Title);

  NControl::CStatic staticContol;
  staticContol.Attach(GetItem(IDT_COPY));
  staticContol.SetText(Static);
  #ifdef UNDER_CE
  // we do it, since WinCE selects Value\something instead of Value !!!!
  _path.AddString(Value);
  #endif
  FOR_VECTOR (i, Strings)
    _path.AddString(Strings[i]);
  _path.SetText(Value);
  SetItemText(IDT_COPY_INFO, Info);
  NormalizeSize(true);
  return CModalDialog::OnInit();
  */

  m_CopyPage = {};
  m_CopyPage.TitleText(
    winrt::hstring(
      Title.Ptr(),
      Title.Len()));
  m_CopyPage.PathLabelText(
    winrt::hstring(
      Static.Ptr(),
      Static.Len()));
  m_CopyPage.PathText(
    winrt::hstring(
      Value.Ptr(),
      Value.Len()));
  m_CopyPage.InfoText(
    winrt::hstring(
      Info.Ptr(),
      Info.Len()));

  m_CopyPage.OkButtonClicked(
    [&](auto&& sender, auto&& args)
    {
      UNREFERENCED_PARAMETER(sender);
      UNREFERENCED_PARAMETER(args);

      Value = UString(m_CopyPage.PathText().c_str());
      Result = IDOK;
      ::SendMessageW(this->m_IslandsHwnd, WM_CLOSE, 0, 0);
    });

  m_CopyPage.CancelButtonClicked(
    [&](auto&& sender, auto&& args)
    {
      UNREFERENCED_PARAMETER(sender);
      UNREFERENCED_PARAMETER(args);

      Result = IDCANCEL;
      ::SendMessageW(this->m_IslandsHwnd, WM_CLOSE, 0, 0);
    });

  m_CopyPage.PickerButtonClicked(
    [&](auto&& sender, auto&& args)
    {
      UNREFERENCED_PARAMETER(sender);
      UNREFERENCED_PARAMETER(args);
      this->OnButtonSetPath();
    });

  m_IslandsHwnd = CreateWindowEx(
      WS_EX_NOREDIRECTIONBITMAP,
      L"Mile.Xaml.ContentWindow",
      Title.Ptr(),
      WS_CAPTION | WS_SYSMENU,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
      parentWindow,
      nullptr,
      nullptr,
      winrt::get_abi(m_CopyPage));

  ::MileAllowNonClientDefaultDrawingForWindow(m_IslandsHwnd, FALSE);

  winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource XamlSource = nullptr;
  winrt::copy_from_abi(
      XamlSource,
      ::GetPropW(m_IslandsHwnd, L"XamlWindowSource"));

  ::SetWindowSubclass(
      m_IslandsHwnd,
      [](
          _In_ HWND hWnd,
          _In_ UINT uMsg,
          _In_ WPARAM wParam,
          _In_ LPARAM lParam,
          _In_ UINT_PTR uIdSubclass,
          _In_ DWORD_PTR dwRefData) -> LRESULT
      {
          UNREFERENCED_PARAMETER(uIdSubclass);
          UNREFERENCED_PARAMETER(dwRefData);

          switch (uMsg)
          {
          case WM_CLOSE:
          {
              HWND parentWindow = ::GetWindow(hWnd, GW_OWNER);
              ::EnableWindow(parentWindow, TRUE);
              ::SetForegroundWindow(parentWindow);
              break;
          }
          case WM_DESTROY:
          {
              winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource XamlSource = nullptr;
              winrt::copy_from_abi(
                  XamlSource,
                  ::GetPropW(hWnd, L"XamlWindowSource"));
              XamlSource.Close();
              break;
          }
          case WM_ERASEBKGND:
              ::RemovePropW(
                  hWnd,
                  L"BackgroundFallbackColor");
              break;
          }

          return ::DefSubclassProc(
              hWnd,
              uMsg,
              wParam,
              lParam);
      },
      0,
      0);

  UINT DpiValue = ::GetDpiForWindow(m_IslandsHwnd);

  int ScaledWidth = ::MulDiv(500, DpiValue, USER_DEFAULT_SCREEN_DPI);
  int ScaledHeight = ::MulDiv(400, DpiValue, USER_DEFAULT_SCREEN_DPI);

  RECT ParentRect = {};
  if (parentWindow)
  {
      ::GetWindowRect(parentWindow, &ParentRect);
  }
  else
  {
      HMONITOR MonitorHandle = ::MonitorFromWindow(
          m_IslandsHwnd,
          MONITOR_DEFAULTTONEAREST);
      if (MonitorHandle)
      {
          MONITORINFO MonitorInfo;
          MonitorInfo.cbSize = sizeof(MONITORINFO);
          if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
          {
              ParentRect = MonitorInfo.rcWork;
          }
      }
  }

  int ParentWidth = ParentRect.right - ParentRect.left;
  int ParentHeight = ParentRect.bottom - ParentRect.top;

  ::SetWindowPos(
      m_IslandsHwnd,
      nullptr,
      ParentRect.left + ((ParentWidth - ScaledWidth) / 2),
      ParentRect.top + ((ParentHeight - ScaledHeight) / 2),
      ScaledWidth,
      ScaledHeight,
      SWP_NOZORDER | SWP_NOACTIVATE);

  ::ShowWindow(m_IslandsHwnd, SW_SHOW);
  ::UpdateWindow(m_IslandsHwnd);

  if (parentWindow)
  {
    ::EnableWindow(parentWindow, FALSE);
  }
  ::MileXamlContentWindowDefaultMessageLoop();

  return Result;
}

void CCopyDialog::OnButtonSetPath()
{
  UString currentPath;
  // _path.GetText(currentPath);
  currentPath = UString(m_CopyPage.PathText().c_str());

  const UString title = LangString(IDS_SET_FOLDER);

  UString resultPath;
  if (!MyBrowseForFolder(m_IslandsHwnd, title, currentPath, resultPath))
    return;
  NFile::NName::NormalizeDirPathPrefix(resultPath);
  // _path.SetCurSel(-1);
  // _path.SetText(resultPath);
  m_CopyPage.PathText(resultPath.Ptr());
}

/*

void CCopyDialog::OnOK()
{
  // _path.GetText(Value);
  Value = UString(m_CopyPage.PathText().c_str());
  CModalDialog::OnOK();
}

*/
