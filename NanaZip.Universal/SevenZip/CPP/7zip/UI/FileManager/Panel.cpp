// Panel.cpp

#include "StdAfx.h"

#include <windowsx.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/Thread.h"

#include "../../PropID.h"

#include "resource.h"
#include "../GUI/ExtractRes.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ZipRegistry.h"

#include "../Agent/IFolderArchive.h"

#include "App.h"
#include "ExtractCallback.h"
#include "FSFolder.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "Panel.h"
#include "RootFolder.h"
// **************** NanaZip Modification Start ****************
#include <K7Base.h>
#include <K7User.h>
// **************** NanaZip Modification End ****************

#include "PropertyNameRes.h"

// **************** NanaZip Modification Start ****************
#include <Mile.Xaml.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <vector>
// Note: All HWND should belong to the same main window thread.
extern std::vector<HWND> g_K7ControlList;
// **************** NanaZip Modification End ****************

using namespace NWindows;
using namespace NControl;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

static const UINT_PTR kTimerID = 1;
static const UINT kTimerElapse = 1000;

// **************** NanaZip Modification Start ****************
static const int AddressBarHeight = 36;
static const int StatusBarHeight = 32;
// **************** NanaZip Modification End ****************

static DWORD kStyles[4] = { LVS_ICON, LVS_SMALLICON, LVS_LIST, LVS_REPORT };

// static const int kCreateFolderID = 101;

extern HINSTANCE g_hInstance;

void CPanel::ReleasePanel()
{
  Disable_Processing_Timer_Notify_StatusBar();
  // It's for unloading COM dll's: don't change it.
  CloseOpenFolders();
  _sevenZipContextMenu.Release();
  _systemContextMenu.Release();
}

CPanel::~CPanel()
{
  CloseOpenFolders();
}

HWND CPanel::GetParent() const
{
  const HWND h = CWindow2::GetParent();
  return h ? h : _mainWindow;
}

// **************** NanaZip Modification Start ****************
// #define kClassName L"7-Zip::Panel"
#define kClassName L"NanaZip::Panel"
// **************** NanaZip Modification End ****************


HRESULT CPanel::Create(HWND mainWindow, HWND parentWindow, UINT id,
    const UString &currentFolderPrefix,
    const UString &arcFormat,
    CPanelCallback *panelCallback, CAppState *appState,
    bool needOpenArc,
    COpenResult &openRes)
{
  _mainWindow = mainWindow;
  _processTimer = true;
  _processNotify = true;
  _processStatusBar = true;

  _panelCallback = panelCallback;
  _appState = appState;
  // _index = index;
  _baseID = id;
  _comboBoxID = _baseID + 3;
  _statusBarID = _comboBoxID + 1;

  UString cfp = currentFolderPrefix;

  if (!currentFolderPrefix.IsEmpty())
    if (currentFolderPrefix[0] == L'.')
    {
      FString cfpF;
      if (NFile::NDir::MyGetFullPathName(us2fs(currentFolderPrefix), cfpF))
        cfp = fs2us(cfpF);
    }

  RINOK(BindToPath(cfp, arcFormat, openRes))

  if (needOpenArc && !openRes.ArchiveIsOpened)
    return S_OK;

  if (!CreateEx(0, kClassName, NULL, WS_CHILD | WS_VISIBLE,
      0, 0, _xSize, 260,
      parentWindow, (HMENU)(UINT_PTR)id, g_hInstance))
    return E_FAIL;
  PanelCreated = true;

  return S_OK;
}

// extern UInt32 g_NumMessages;

LRESULT CPanel::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // g_NumMessages++;
  switch (message)
  {
    case kShiftSelectMessage:
      OnShiftSelectMessage();
      return 0;
    case kReLoadMessage:
      RefreshListCtrl(_selectedState);
      return 0;
    case kSetFocusToListView:
      _listView.SetFocus();
      return 0;
    case kOpenItemChanged:
      return OnOpenItemChanged(lParam);
    case kRefresh_StatusBar:
      if (_processStatusBar)
        Refresh_StatusBar();
      return 0;
    #ifdef UNDER_CE
    case kRefresh_HeaderComboBox:
      LoadFullPathAndShow();
      return 0;
    #endif
    case WM_TIMER:
      OnTimer();
      return 0;
    case WM_CONTEXTMENU:
      if (OnContextMenu(HANDLE(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        return 0;
      break;
    /*
    case WM_DROPFILES:
      CompressDropFiles(HDROP(wParam));
      return 0;
    */
  }
  return CWindow2::OnMessage(message, wParam, lParam);
}

LRESULT CMyListView::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_CHAR)
  {
    UINT scanCode = (UINT)((lParam >> 16) & 0xFF);
    bool extended = ((lParam & 0x1000000) != 0);
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_MULTIPLY || virtualKey == VK_ADD ||
        virtualKey == VK_SUBTRACT)
      return 0;
    if ((wParam == '/' && extended)
        || wParam == '\\' || wParam == '/')
    {
      _panel->OpenDrivesFolder();
      return 0;
    }
  }
  else if (message == WM_SYSCHAR)
  {
    // For Alt+Enter Beep disabling
    UINT scanCode = (UINT)(lParam >> 16) & 0xFF;
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_RETURN || virtualKey == VK_MULTIPLY ||
        virtualKey == VK_ADD || virtualKey == VK_SUBTRACT)
      return 0;
  }
  /*
  else if (message == WM_SYSKEYDOWN)
  {
    // return 0;
  }
  */
  else if (message == WM_KEYDOWN)
  {
    bool alt = IsKeyDown(VK_MENU);
    bool ctrl = IsKeyDown(VK_CONTROL);
    bool shift = IsKeyDown(VK_SHIFT);
    switch (wParam)
    {
      /*
      case VK_RETURN:
      {
        if (shift && !alt && !ctrl)
        {
          _panel->OpenSelectedItems(false);
          return 0;
        }
        break;
      }
      */
      case VK_NEXT:
      {
        if (ctrl && !alt && !shift)
        {
          _panel->OpenFocusedItemAsInternal();
          return 0;
        }
        break;
      }
      case VK_PRIOR:
      if (ctrl && !alt && !shift)
      {
        _panel->OpenParentFolder();
        return 0;
      }
    }
  }
  #ifdef UNDER_CE
  else if (message == WM_KEYUP)
  {
    if (wParam == VK_F2) // it's VK_TSOFT2
    {
      // Activate Menu
      ::PostMessage(g_HWND, WM_SYSCOMMAND, SC_KEYMENU, 0);
      return 0;
    }
  }
  #endif
  else if (message == WM_SETFOCUS)
  {
    _panel->_lastFocusedIsList = true;
    _panel->_panelCallback->PanelWasFocused();
  }
  return CListView2::OnMessage(message, wParam, lParam);
}

/*
static LRESULT APIENTRY ComboBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBox *w = (CMyComboBox *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}

LRESULT CMyComboBox::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}
*/

#ifndef UNDER_CE

static LRESULT APIENTRY ComboBoxEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBoxEdit *w = (CMyComboBoxEdit *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}

#endif

LRESULT CMyComboBoxEdit::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // See MSDN / Subclassing a Combo Box / Creating a Combo-box Toolbar
  switch (message)
  {
    case WM_SYSKEYDOWN:
      switch (wParam)
      {
        case VK_F1:
        case VK_F2:
        {
          // check ALT
          if ((lParam & (1<<29)) == 0)
            break;
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (alt && !ctrl && !shift)
          {
            _panel->_panelCallback->SetFocusToPath(wParam == VK_F1 ? 0 : 1);
            return 0;
          }
          break;
        }
      }
      break;
    case WM_KEYDOWN:
      switch (wParam)
      {
        case VK_TAB:
          // SendMessage(hwndMain, WM_ENTER, 0, 0);
          _panel->SetFocusToList();
          return 0;
        case VK_F9:
        {
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (!alt && !ctrl && !shift)
          {
            g_App.SwitchOnOffOnePanel();
            return 0;
          }
          break;
        }
        case 'W':
        {
          bool ctrl = IsKeyDown(VK_CONTROL);
          if (ctrl)
          {
            PostMessage(g_HWND, WM_COMMAND, IDCLOSE, 0);
            return 0;
          }
          break;
        }
      }
      break;
    case WM_CHAR:
      switch (wParam)
      {
        case VK_TAB:
        case VK_ESCAPE:
          return 0;
      }
  }
  #ifndef _UNICODE
  if (g_IsNT)
    return CallWindowProcW(_origWindowProc, *this, message, wParam, lParam);
  else
  #endif
    return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}


/*
  REBARBANDINFO in vista (_WIN32_WINNT >= 0x0600) has additional fields
  we want 2000/xp compatibility.
  so we must use reduced structure, if we compile with (_WIN32_WINNT >= 0x0600)
  Also there are additional fields, if (_WIN32_IE >= 0x0400).
    but (_WIN32_IE >= 0x0400) is expected.
  note:
  in x64 (64-bit):
  {
    (108 == REBARBANDINFO_V6_SIZE)
    (112 == sizeof(REBARBANDINFO) // for (_WIN32_WINNT <  0x0600)
    (128 == sizeof(REBARBANDINFO) // for (_WIN32_WINNT >= 0x0600)
    there is difference in sizes, because REBARBANDINFO size was
    not aligned for 8-bytes in (_WIN32_WINNT < 0x0600).
    We hope that WinVista+ support support both (108 and 112) sizes.
    But does WinXP-x64 support (108 == REBARBANDINFO_V6_SIZE)?
    {
         96   LPARAM  lParam;
        104   UINT    cxHeader;
      #if (_WIN32_WINNT >= 0x0600)
        108   RECT    rcChevronLocation;
        124   UINT    uChevronState;
      #endif
    }
*/

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600) && defined(REBARBANDINFOA_V6_SIZE)
  #define my_compatib_REBARBANDINFO_size  REBARBANDINFO_V6_SIZE
#else
  #define my_compatib_REBARBANDINFO_size  sizeof(REBARBANDINFO)
#endif


bool CPanel::OnCreate(CREATESTRUCT * /* createStruct */)
{
  // _virtualMode = false;
  // _sortIndex = 0;
  _sortID = kpidName;
  _ascending = true;
  _lastFocusedIsList = true;

  DWORD style = WS_CHILD | WS_VISIBLE; //  | WS_BORDER ; // | LVS_SHAREIMAGELISTS; //  | LVS_SHOWSELALWAYS;

  style |= LVS_SHAREIMAGELISTS;
  // style  |= LVS_AUTOARRANGE;
  style |= WS_CLIPCHILDREN;
  style |= WS_CLIPSIBLINGS;

  const UInt32 kNumListModes = Z7_ARRAY_SIZE(kStyles);
  if (_listViewMode >= kNumListModes)
    _listViewMode = kNumListModes - 1;

  style |= kStyles[_listViewMode]
    | WS_TABSTOP
    | LVS_EDITLABELS;
  if (_mySelectMode)
    style |= LVS_SINGLESEL;

  /*
  if (_virtualMode)
    style |= LVS_OWNERDATA;
  */

  DWORD exStyle;
  exStyle = WS_EX_CLIENTEDGE;

  if (!_listView.CreateEx(exStyle, style, 0, 0, 116, 260,
      *this, (HMENU)(UINT_PTR)(_baseID + 1), g_hInstance, NULL))
    return false;

  _listView.SetUnicodeFormat();
  _listView._panel = this;
  _listView.SetWindowProc();

  _listView.SetImageList(Shell_Get_SysImageList_smallIcons(true), LVSIL_SMALL);
  _listView.SetImageList(Shell_Get_SysImageList_smallIcons(false), LVSIL_NORMAL);

  // _exStyle |= LVS_EX_HEADERDRAGDROP;
  // DWORD extendedStyle = _listView.GetExtendedListViewStyle();
  // extendedStyle |= _exStyle;
  //  _listView.SetExtendedListViewStyle(extendedStyle);
  SetExtendedStyle();

  _listView.Show(SW_SHOW);
  _listView.InvalidateRect(NULL, true);
  _listView.Update();

  // Ensure that the common control DLL is loaded.
  INITCOMMONCONTROLSEX icex;

  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC  = ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);

  const TBBUTTON tbb[] =
  {
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    {VIEW_PARENTFOLDER, kParentFolderID, TBSTATE_ENABLED, BTNS_BUTTON, { 0, 0 }, 0, 0 },
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    // {VIEW_NEWFOLDER, kCreateFolderID, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
  };

  // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
#ifdef Z7_USE_DYN_ComCtl32Version
  if (g_ComCtl32Version >= MAKELONG(71, 4))
#endif
  {
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC  = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // if there is no CCS_NOPARENTALIGN, there is space of some pixels after rebar (Incorrect GetWindowRect ?)

    _headerReBar.Attach(::CreateWindowEx(WS_EX_TOOLWINDOW,
      REBARCLASSNAME,
      NULL, WS_VISIBLE | WS_BORDER | WS_CHILD |
      WS_CLIPCHILDREN | WS_CLIPSIBLINGS
      | CCS_NODIVIDER
      | CCS_NOPARENTALIGN
      | CCS_TOP
      | RBS_VARHEIGHT
      | RBS_BANDBORDERS
      ,0,0,0,0, *this, NULL, g_hInstance, NULL));
  }

  DWORD toolbarStyle =  WS_CHILD | WS_VISIBLE ;
  if (_headerReBar)
  {
    toolbarStyle |= 0
      // | WS_CLIPCHILDREN
      // | WS_CLIPSIBLINGS

      | TBSTYLE_TOOLTIPS
      | CCS_NODIVIDER
      | CCS_NORESIZE
      | TBSTYLE_FLAT
      ;
  }

  _headerToolBar.Attach(::CreateToolbarEx ((*this), toolbarStyle,
      _baseID + 2, 11,
      (HINSTANCE)HINST_COMMCTRL,
      IDB_VIEW_SMALL_COLOR,
      (LPCTBBUTTON)&tbb, Z7_ARRAY_SIZE(tbb),
      0, 0, 0, 0, sizeof (TBBUTTON)));

  #ifndef UNDER_CE
  // Load ComboBoxEx class
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_USEREX_CLASSES;
  InitCommonControlsEx(&icex);
  #endif

  _headerComboBox.CreateEx(0,
      #ifdef UNDER_CE
      WC_COMBOBOXW
      #else
      WC_COMBOBOXEXW
      #endif
      , NULL,
    WS_BORDER | WS_VISIBLE |WS_CHILD | CBS_DROPDOWN | CBS_AUTOHSCROLL,
      0, 0, 100, 620,
      (_headerReBar ? _headerToolBar : (HWND)*this),
      (HMENU)(UINT_PTR)(_comboBoxID),
      g_hInstance, NULL);

#ifndef UNDER_CE
  _headerComboBox.SetUnicodeFormat(true);
  _headerComboBox.SetImageList(Shell_Get_SysImageList_smallIcons(true));
  _headerComboBox.SetExtendedStyle(CBES_EX_PATHWORDBREAKPROC, CBES_EX_PATHWORDBREAKPROC);
  /*
  _headerComboBox.SetUserDataLongPtr(LONG_PTR(&_headerComboBox));
  _headerComboBox._panel = this;
  _headerComboBox._origWindowProc =
      (WNDPROC)_headerComboBox.SetLongPtr(GWLP_WNDPROC,
      LONG_PTR(ComboBoxSubclassProc));
  */
  _comboBoxEdit.Attach(_headerComboBox.GetEditControl());
  // _comboBoxEdit.SendMessage(CCM_SETUNICODEFORMAT, (WPARAM)(BOOL)TRUE, 0);
  _comboBoxEdit.SetUserDataLongPtr(LONG_PTR(&_comboBoxEdit));
  _comboBoxEdit._panel = this;
   #ifndef _UNICODE
   if (g_IsNT)
     _comboBoxEdit._origWindowProc =
      (WNDPROC)_comboBoxEdit.SetLongPtrW(GWLP_WNDPROC, LONG_PTR(ComboBoxEditSubclassProc));
   else
   #endif
     _comboBoxEdit._origWindowProc =
      (WNDPROC)_comboBoxEdit.SetLongPtr(GWLP_WNDPROC, LONG_PTR(ComboBoxEditSubclassProc));
#endif

  if (_headerReBar)
  {
    REBARINFO     rbi;
    rbi.cbSize = sizeof(REBARINFO);  // Required when using this struct.
    rbi.fMask  = 0;
    rbi.himl   = (HIMAGELIST)NULL;
    _headerReBar.SetBarInfo(&rbi);

    // Send the TB_BUTTONSTRUCTSIZE message, which is required for
    // backward compatibility.
    // _headerToolBar.SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SIZE size;
    _headerToolBar.GetMaxSize(&size);

    REBARBANDINFO rbBand;
    memset(&rbBand, 0, sizeof(rbBand));
    // rbBand.cbSize = sizeof(rbBand);  // for debug
    // rbBand.cbSize = REBARBANDINFO_V3_SIZE; // for debug
    rbBand.cbSize = my_compatib_REBARBANDINFO_size;
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
    rbBand.fStyle = RBBS_NOGRIPPER;
    rbBand.cxMinChild = (UINT)size.cx;
    rbBand.cyMinChild = (UINT)size.cy;
    rbBand.cyChild = (UINT)size.cy;
    rbBand.cx = (UINT)size.cx;
    rbBand.hwndChild  = _headerToolBar;
    _headerReBar.InsertBand(-1, &rbBand);

    RECT rc;
    ::GetWindowRect(_headerComboBox, &rc);
    rbBand.cxMinChild = 30;
    rbBand.cyMinChild = (UINT)(rc.bottom - rc.top);
    rbBand.cx = 1000;
    rbBand.hwndChild  = _headerComboBox;
    _headerReBar.InsertBand(-1, &rbBand);
    // _headerReBar.MaximizeBand(1, false);
  }

  _statusBar.Create(WS_CHILD | WS_VISIBLE, L"Status", (*this), _statusBarID);
  // _statusBar2.Create(WS_CHILD | WS_VISIBLE, L"Status", (*this), _statusBarID + 1);

  const int sizes[] = {220, 320, 420, -1};
  _statusBar.SetParts(4, sizes);
  // _statusBar2.SetParts(5, sizes);
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  _addressBarControl = winrt::NanaZip::Modern::AddressBar{};
  _addressBarWindow = ::CreateWindowEx(
      WS_EX_NOREDIRECTIONBITMAP,
      L"Mile.Xaml.ContentWindow",
      nullptr,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
      0,
      0,
      0,
      0,
      *this,
      nullptr,
      nullptr,
      winrt::get_abi(_addressBarControl)
  );

  {
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs;
      using winrt::Windows::UI::Xaml::Hosting::XamlSourceFocusNavigationReason;
      using winrt::Windows::UI::Xaml::Input::KeyboardNavigationMode;

      DesktopWindowXamlSource XamlSource = nullptr;
      winrt::copy_from_abi(
          XamlSource,
          ::GetPropW(_addressBarWindow, L"XamlWindowSource"));

      XamlSource.Content().TabFocusNavigation(KeyboardNavigationMode::Local);

      XamlSource.TakeFocusRequested(
          [this](
              DesktopWindowXamlSource const& sender,
              DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& e)
      {
          UNREFERENCED_PARAMETER(sender);
          XamlSourceFocusNavigationReason Reason = e.Request().Reason();
          switch (Reason)
          {
          case XamlSourceFocusNavigationReason::First:
          case XamlSourceFocusNavigationReason::Last:
          {
              std::size_t Count = g_K7ControlList.size();
              std::size_t Index = static_cast<std::size_t>(-1);
              for (size_t i = 0; i < Count; ++i)
              {
                  if (this->_addressBarWindow == g_K7ControlList[i])
                  {
                      Index = i;
                      break;
                  }
              }
              if (static_cast<std::size_t>(-1) != Index)
              {
                  std::size_t NextIndex = (XamlSourceFocusNavigationReason::Last == Reason)
                      ? ((Index == 0) ? (Count - 1) : (Index - 1))
                      : ((Index + 1) % Count);
                  ::SetFocus(g_K7ControlList[NextIndex]);
              }
              break;
          }
          default:
              break;
          }
      });
  }

  _sysImageList = Shell_Get_SysImageList_smallIcons(true);

  _addressBarControl.UpButtonClicked(
      [this](auto&&, auto&&)
      {
          this->OpenParentFolder();
          this->SetFocusToList();
      });

  _addressBarControl.QuerySubmitted({ this, &CPanel::OnAddressBarQuerySubmitted });

  _addressBarControl.GotFocus(
      [&](auto&&, auto&&)
      {
          _panelCallback->PanelWasFocused();
      }
  );

  _items =
      winrt::single_threaded_observable_vector<
      winrt::NanaZip::Modern::AddressBarItem>();
  _addressBarControl.ItemsSource(_items);
  _addressBarControl.DropDownOpened({ this, &CPanel::OnDropDownOpened });

  _addressBarControl.KeyUp(
      [&]
      (
          auto&&,
          winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs
          const& args
      )
      {
          if (args.Key() == winrt::Windows::System::VirtualKey::Escape)
          {
              _addressBarControl.Text(_currentFolderPrefix.Ptr());
              PostMsg(kSetFocusToListView);
          }
      }
  );

  _statusBarControl = winrt::NanaZip::Modern::StatusBar{};
  _statusBarWindow = ::CreateWindowEx(
      WS_EX_NOREDIRECTIONBITMAP,
      L"Mile.Xaml.ContentWindow",
      nullptr,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
      0,
      0,
      0,
      0,
      *this,
      nullptr,
      nullptr,
      winrt::get_abi(_statusBarControl)
  );

  {
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs;
      using winrt::Windows::UI::Xaml::Hosting::XamlSourceFocusNavigationReason;
      using winrt::Windows::UI::Xaml::Input::KeyboardNavigationMode;

      DesktopWindowXamlSource XamlSource = nullptr;
      winrt::copy_from_abi(
          XamlSource,
          ::GetPropW(_statusBarWindow, L"XamlWindowSource"));

      XamlSource.Content().TabFocusNavigation(KeyboardNavigationMode::Local);

      XamlSource.TakeFocusRequested(
          [this](
              DesktopWindowXamlSource const& sender,
              DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& e)
      {
          UNREFERENCED_PARAMETER(sender);
          XamlSourceFocusNavigationReason Reason = e.Request().Reason();
          switch (Reason)
          {
          case XamlSourceFocusNavigationReason::First:
          case XamlSourceFocusNavigationReason::Last:
          {
              std::size_t Count = g_K7ControlList.size();
              std::size_t Index = static_cast<std::size_t>(-1);
              for (size_t i = 0; i < Count; ++i)
              {
                  if (this->_statusBarWindow == g_K7ControlList[i])
                  {
                      Index = i;
                      break;
                  }
              }
              if (static_cast<std::size_t>(-1) != Index)
              {
                  std::size_t NextIndex = (XamlSourceFocusNavigationReason::Last == Reason)
                      ? ((Index == 0) ? (Count - 1) : (Index - 1))
                      : ((Index + 1) % Count);
                  ::SetFocus(g_K7ControlList[NextIndex]);
              }
              break;
          }
          default:
              break;
          }
      });
  }

  g_K7ControlList.push_back(_addressBarWindow);
  g_K7ControlList.push_back(_listView);
  g_K7ControlList.push_back(_statusBarWindow);
  // **************** NanaZip Modification End ****************

  /*
  RECT rect;
  GetClientRect(&rect);
  OnSize(0, RECT_SIZE_X(rect), RECT_SIZE_Y(rect));
  */

  SetTimer(kTimerID, kTimerElapse);

  // InitListCtrl();
  RefreshListCtrl();

  return true;
}

void CPanel::OnDestroy()
{
  SaveListViewInfo();
  CWindow2::OnDestroy();
}

void CPanel::ChangeWindowSize(int xSize, int ySize)
{
  if (!(HWND)*this)
    return;
  // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  int kHeaderSize;
  int kStatusBarSize;
  // int kStatusBar2Size;
  RECT rect;
  if (_headerReBar)
    _headerReBar.GetWindowRect(&rect);
  else
    _headerToolBar.GetWindowRect(&rect);

  kHeaderSize = RECT_SIZE_Y(rect);

  _statusBar.GetWindowRect(&rect);
  kStatusBarSize = RECT_SIZE_Y(rect);
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  const UINT dpi = ::GetDpiForWindow(*this);
  const int kHeaderSize = ::MulDiv(AddressBarHeight, dpi, USER_DEFAULT_SCREEN_DPI);
  const int kStatusBarSize = ::MulDiv(StatusBarHeight, dpi, USER_DEFAULT_SCREEN_DPI);
  // **************** NanaZip Modification End ****************

  // _statusBar2.GetWindowRect(&rect);
  // kStatusBar2Size = RECT_SIZE_Y(rect);

  int yListViewSize = MyMax(ySize - kHeaderSize - kStatusBarSize, 0);
  // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  const int kStartXPos = 32;
  if (_headerReBar)
  {
  }
  else
  {
    _headerToolBar.Move(0, 0, xSize, 0);
    _headerComboBox.Move(kStartXPos, 2,
        MyMax(xSize - kStartXPos - 10, kStartXPos), 0);
  }
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  // **************** NanaZip Modification End ****************
  _listView.Move(0, kHeaderSize, xSize, yListViewSize);
  // **************** NanaZip Modification Start ****************
  // _statusBar.Move(0, kHeaderSize + yListViewSize, xSize, kStatusBarSize);
  if (_statusBarWindow)
  {
    ::SetWindowPos(
        _statusBarWindow,
        nullptr,
        0,
        kHeaderSize + yListViewSize,
        xSize,
        kStatusBarSize,
        SWP_SHOWWINDOW);
  }
  // **************** NanaZip Modification End ****************
  // _statusBar2.MoveWindow(0, kHeaderSize + yListViewSize + kStatusBarSize, xSize, kStatusBar2Size);
  // _statusBar.MoveWindow(0, 100, xSize, kStatusBarSize);
  // _statusBar2.MoveWindow(0, 200, xSize, kStatusBar2Size);
}

bool CPanel::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  if (!(HWND)*this)
    return true;
  // **************** NanaZip Modification Start ****************
  // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  if (_headerReBar)
    _headerReBar.Move(0, 0, xSize, 0);
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  if (_addressBarWindow)
  {
      int ControlDpi = ::GetDpiForWindow(_addressBarWindow);
      ::SetWindowPos(
          _addressBarWindow,
          nullptr,
          0,
          0,
          xSize,
          ::MulDiv(AddressBarHeight, ControlDpi, USER_DEFAULT_SCREEN_DPI),
          SWP_SHOWWINDOW
      );
  }
  // **************** NanaZip Modification End ****************
  ChangeWindowSize(xSize, ySize);
  return true;
}

bool CPanel::OnNotifyReBar(LPNMHDR header, LRESULT & /* result */)
{
  switch (header->code)
  {
    case RBN_HEIGHTCHANGE:
    {
      RECT rect;
      GetWindowRect(&rect);
      ChangeWindowSize(RECT_SIZE_X(rect), RECT_SIZE_Y(rect));
      return false;
    }
  }
  return false;
}

/*
UInt32 g_OnNotify = 0;
UInt32 g_LVIF_TEXT = 0;
UInt32 g_Time = 0;

void Print_OnNotify(const char *name)
{
  char s[256];
  DWORD tim = GetTickCount();
  sprintf(s,
      "Time = %7u ms, Notify = %9u, TEXT = %9u, %s",
      tim - g_Time,
      g_OnNotify,
      g_LVIF_TEXT,
      name);
  g_Time = tim;
  OutputDebugStringA(s);
  g_OnNotify = 0;
  g_LVIF_TEXT = 0;
}
*/

bool CPanel::OnNotify(UINT /* controlID */, LPNMHDR header, LRESULT &result)
{
  /*
  g_OnNotify++;

  if (header->hwndFrom == _listView)
  {
    if (header->code == LVN_GETDISPINFOW)
    {
      LV_DISPINFOW *dispInfo = (LV_DISPINFOW *)header;
        if ((dispInfo->item.mask & LVIF_TEXT))
          g_LVIF_TEXT++;
    }
  }
  */

  if (!_processNotify)
    return false;

  // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  if (header->hwndFrom == _headerComboBox)
    return OnNotifyComboBox(header, result);
  else if (header->hwndFrom == _headerReBar)
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  if (header->hwndFrom == _headerReBar)
  // **************** NanaZip Modification End ****************
    return OnNotifyReBar(header, result);
  else if (header->hwndFrom == _listView)
    return OnNotifyList(header, result);
  else if (::GetParent(header->hwndFrom) == _listView)
  {
    // NMHDR:code is UINT
    // NM_RCLICK is unsigned in windows sdk
    // NM_RCLICK is int      in MinGW
    if (header->code == (UINT)NM_RCLICK)
      return OnRightClick((MY_NMLISTVIEW_NMITEMACTIVATE *)header, result);
  }
  return false;
}

bool CPanel::OnCommand(unsigned code, unsigned itemID, LPARAM lParam, LRESULT &result)
{
  if (itemID == kParentFolderID)
  {
    OpenParentFolder();
    result = 0;
    return true;
  }
  /*
  if (itemID == kCreateFolderID)
  {
    CreateFolder();
    result = 0;
    return true;
  }
  */
  // **************** NanaZip Modification Start ****************
  // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  if (itemID == _comboBoxID)
  {
    if (OnComboBoxCommand(code, lParam, result))
      return true;
  }
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  // **************** NanaZip Modification End ****************
  return CWindow2::OnCommand(code, itemID, lParam, result);
}



/*
void CPanel::MessageBox_Info(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK); }
void CPanel::MessageBox_Warning(LPCWSTR message) const
  { ::MessageBoxW((HWND)*this, message, L"7-Zip", MB_OK | MB_ICONWARNING); }
*/

void CPanel::MessageBox_Error_Caption(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK | MB_ICONSTOP); }

void CPanel::MessageBox_Error(LPCWSTR message) const
  // **************** NanaZip Modification Start ****************
  // { MessageBox_Error_Caption(message, L"7-Zip"); }
  { MessageBox_Error_Caption(message, L"NanaZip"); }
  // **************** NanaZip Modification End ****************

static UString ErrorHResult_To_Message(HRESULT errorCode)
{
  if (errorCode == 0)
    errorCode = E_FAIL;
  return HResultToMessage(errorCode);
}

void CPanel::MessageBox_Error_HRESULT_Caption(HRESULT errorCode, LPCWSTR caption) const
{
  MessageBox_Error_Caption(ErrorHResult_To_Message(errorCode), caption);
}

void CPanel::MessageBox_Error_HRESULT(HRESULT errorCode) const
  // **************** NanaZip Modification Start ****************
  // { MessageBox_Error_HRESULT_Caption(errorCode, L"7-Zip"); }
  { MessageBox_Error_HRESULT_Caption(errorCode, L"NanaZip"); }
  // **************** NanaZip Modification End ****************

void CPanel::MessageBox_Error_2Lines_Message_HRESULT(LPCWSTR message, HRESULT errorCode) const
{
  UString m = message;
  m.Add_LF();
  m += ErrorHResult_To_Message(errorCode);
  MessageBox_Error(m);
}

void CPanel::MessageBox_LastError(LPCWSTR caption) const
  { MessageBox_Error_HRESULT_Caption(GetLastError_noZero_HRESULT(), caption); }

void CPanel::MessageBox_LastError() const
  // **************** NanaZip Modification Start ****************
  // { MessageBox_LastError(L"7-Zip"); }
  { MessageBox_LastError(L"NanaZip"); }
  // **************** NanaZip Modification End ****************

void CPanel::MessageBox_Error_LangID(UINT resourceID) const
  { MessageBox_Error(LangString(resourceID)); }

void CPanel::MessageBox_Error_UnsupportOperation() const
  { MessageBox_Error_LangID(IDS_OPERATION_IS_NOT_SUPPORTED); }




void CPanel::SetFocusToList()
{
  _listView.SetFocus();
  // SetCurrentPathText();
}

void CPanel::SetFocusToLastRememberedItem()
{
  if (_lastFocusedIsList)
    SetFocusToList();
  else
    // **************** NanaZip Modification Start ****************
    // _headerComboBox.SetFocus();
    ;
    // **************** NanaZip Modification End ****************
}

UString CPanel::GetFolderTypeID() const
{
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidType, &prop) == S_OK)
      if (prop.vt == VT_BSTR)
        return (const wchar_t *)prop.bstrVal;
  }
  return UString();
}

bool CPanel::IsFolderTypeEqTo(const char *s) const
{
  return StringsAreEqual_Ascii(GetFolderTypeID(), s);
}

bool CPanel::IsRootFolder() const { return IsFolderTypeEqTo("RootFolder"); }
bool CPanel::IsFSFolder() const { return IsFolderTypeEqTo("FSFolder"); }
bool CPanel::IsFSDrivesFolder() const { return IsFolderTypeEqTo("FSDrives"); }
bool CPanel::IsAltStreamsFolder() const { return IsFolderTypeEqTo("AltStreamsFolder"); }
bool CPanel::IsArcFolder() const
{
  // **************** NanaZip Modification Start ****************
  // return GetFolderTypeID().IsPrefixedBy_Ascii_NoCase("7-Zip");
  return GetFolderTypeID().IsPrefixedBy_Ascii_NoCase("NanaZip");
  // **************** NanaZip Modification End ****************
}

bool CPanel::IsHashFolder() const
{
  if (_folder)
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidIsHash, &prop) == S_OK)
      if (prop.vt == VT_BOOL)
        return VARIANT_BOOLToBool(prop.boolVal);
  }
  return false;
}

UString CPanel::GetFsPath() const
{
  if (IsFSDrivesFolder() && !IsDeviceDrivesPrefix() && !IsSuperDrivesPrefix())
    return UString();
  return _currentFolderPrefix;
}

UString CPanel::GetDriveOrNetworkPrefix() const
{
  if (!IsFSFolder())
    return UString();
  UString drive = GetFsPath();
  drive.DeleteFrom(NFile::NName::GetRootPrefixSize(drive));
  return drive;
}

void CPanel::SetListViewMode(UInt32 index)
{
  if (index >= 4)
    return;
  _listViewMode = index;
  const LONG_PTR oldStyle = _listView.GetStyle();
  const DWORD newStyle = kStyles[index];

  // DWORD tickCount1 = GetTickCount();
  if ((oldStyle & LVS_TYPEMASK) != (LONG_PTR)newStyle)
    _listView.SetStyle((oldStyle & ~(LONG_PTR)(DWORD)LVS_TYPEMASK) | (LONG_PTR)newStyle);
  // RefreshListCtrlSaveFocused();
  /*
  DWORD tickCount2 = GetTickCount();
  char s[256];
  sprintf(s, "SetStyle = %5d",
      tickCount2 - tickCount1
      );
  OutputDebugStringA(s);
  */

}

void CPanel::ChangeFlatMode()
{
  _flatMode = !_flatMode;
  if (!_parentFolders.IsEmpty())
    _flatModeForArc = _flatMode;
  else
    _flatModeForDisk = _flatMode;
  RefreshListCtrl_SaveFocused();
}

/*
void CPanel::Change_ShowNtfsStrems_Mode()
{
  _showNtfsStrems_Mode = !_showNtfsStrems_Mode;
  if (!_parentFolders.IsEmpty())
    _showNtfsStrems_ModeForArc = _showNtfsStrems_Mode;
  else
    _showNtfsStrems_ModeForDisk = _showNtfsStrems_Mode;
  RefreshListCtrlSaveFocused();
}
*/

void CPanel::Post_Refresh_StatusBar()
{
  if (_processStatusBar)
    PostMsg(kRefresh_StatusBar);
}

// **************** NanaZip Modification Start ****************
void CPanel::AddToExistingArchive()
{
    if (!DoesItSupportOperations() || _parentFolders.Size() != 1)
    {
        MessageBox_Error_UnsupportOperation();
        return;
    }

    CFolderLink &Link = _parentFolders.Back();
    const UString &ArchivePath = Link.VirtualPath;

    // MyBrowseForFile doesn't have multiselect, so use IFileOpenDialog directly
    CMyComPtr<IFileOpenDialog> FileDialog;
    if (FAILED(::CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&FileDialog))))
    {
        return;
    }

    FILEOPENDIALOGOPTIONS DialogOptions;
    if (FAILED(FileDialog->GetOptions(&DialogOptions)))
    {
        return;
    }
    DialogOptions |= FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM;
    if (FAILED(FileDialog->SetOptions(DialogOptions)))
    {
        return;
    }
    {
        CMyComPtr<IShellItem> DefaultFolder;
        if (SUCCEEDED(::SHCreateItemFromParsingName(
            ArchivePath,
            nullptr,
            IID_PPV_ARGS(&DefaultFolder))))
        {
            FileDialog->SetDefaultFolder(DefaultFolder);
        }
    }

    CMyComPtr<IShellItemArray> Items;
    if (FAILED(FileDialog->Show(GetParent())) ||
        FAILED(FileDialog->GetResults(&Items)))
    {
        return;
    }

    UStringVector SelectedPaths;
    DWORD Count;
    if (FAILED(Items->GetCount(&Count)))
    {
        return;
    }
    for (DWORD i = 0; i < Count; ++i)
    {
        CMyComPtr<IShellItem> Item;
        if (SUCCEEDED(Items->GetItemAt(
            i,
            &Item)))
        {
            LPWSTR DisplayName = nullptr;
            if (SUCCEEDED(Item->GetDisplayName(
                SIGDN_FILESYSPATH,
                &DisplayName)))
            {
                SelectedPaths.Add(UString(DisplayName));
                ::CoTaskMemFree(DisplayName);
            }
        }
    }

    if (SelectedPaths.Size() == 0)
    {
        return;
    }
    // we can't use CompressFiles here, since that uses NanaZipG which doesn't
    // support adding files to an archive subdirectory, and there's no way to
    // just use the dialog from within FM without actually committing files
    CopyFromAsk(SelectedPaths);
}
// **************** NanaZip Modification End ****************

void CPanel::AddToArchive()
{
  if (!Is_IO_FS_Folder())
  {
    // **************** NanaZip Modification Start ****************
    // MessageBox_Error_UnsupportOperation();
    if (IsArcFolder())
    {
        AddToExistingArchive();
    }
    else
    {
        MessageBox_Error_UnsupportOperation();
    }
    // **************** NanaZip Modification End ****************
    return;
  }
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.Size() == 0)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UString destCurDirPrefix = GetFsPath();
  if (IsFSDrivesFolder())
    destCurDirPrefix = ROOT_FS_FOLDER;
  UStringVector names;
  GetFilePaths(indices, names);
  UString baseName;
  const UString arcName = CreateArchiveName(names,
      false, // isHash
      NULL,  // CFileInfo *fi
      baseName);
  const HRESULT res = CompressFiles(destCurDirPrefix, arcName, L"",
      true,   // addExtension
      names,
      false,  // email
      true,   // showDialog
      false); // waitFinish
  if (res != S_OK)
  {
    if (destCurDirPrefix.Len() >= MAX_PATH)
      MessageBox_Error_LangID(IDS_MESSAGE_UNSUPPORTED_OPERATION_FOR_LONG_PATH_FOLDER);
  }
  // KillSelection();
}

// function from ContextMenu.cpp
UString GetSubFolderNameForExtract(const UString &arcPath);

static UString GetSubFolderNameForExtract2(const UString &arcPath)
{
  int slashPos = arcPath.ReverseFind_PathSepar();
  UString s;
  UString name = arcPath;
  if (slashPos >= 0)
  {
    s = arcPath.Left((unsigned)(slashPos + 1));
    name = arcPath.Ptr((unsigned)(slashPos + 1));
  }
  s += GetSubFolderNameForExtract(name);
  return s;
}


int CPanel::FindDir_InOperatedList(const CRecordVector<UInt32> &operatedIndices) const
{
  const bool *isDirVector = _isDirVector.ConstData();
  const UInt32 *indices = operatedIndices.ConstData();
  const unsigned numItems = operatedIndices.Size();
  for (unsigned i = 0; i < numItems; i++)
    if (isDirVector[indices[i]])
      return (int)i;
  return -1;
}


void CPanel::GetFilePaths(const CRecordVector<UInt32> &operatedIndices, UStringVector &paths) const
{
  paths.ClearAndReserve(operatedIndices.Size());
  UString path = GetFsPath();
  const unsigned prefixLen = path.Len();
  const UInt32 *indices = operatedIndices.ConstData();
  const unsigned numItems = operatedIndices.Size();
  // for (unsigned y = 0; y < 10000; y++, paths.Clear())
  for (unsigned i = 0; i < numItems; i++)
  {
    path.DeleteFrom(prefixLen);
    Add_ItemRelPath2_To_String(indices[i], path);
    // ODS_U(path)
    paths.AddInReserved(path);
  }
}


void CPanel::ExtractArchives()
{
  if (!_parentFolders.IsEmpty())
  {
    // **************** NanaZip Modification Start ****************
    // _panelCallback->OnCopy(false, false);
    CopyFromArchive();
    // **************** NanaZip Modification End ****************
    return;
  }
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.IsEmpty() || FindDir_InOperatedList(indices) != -1)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths);
  UString outFolder = GetFsPath();
  if (indices.Size() == 1)
    outFolder += GetSubFolderNameForExtract2(GetItemRelPath(indices[0]));
  else
    outFolder.Add_Char('*');
  outFolder.Add_PathSepar();

  CContextMenuInfo ci;
  ci.Load();

  ::ExtractArchives(paths, outFolder
      , true   // showDialog
      , false  // elimDup
      , ci.WriteZone
      );
}

// **************** NanaZip Modification Start ****************
void CPanel::CopyFromArchive()
{
    UString archivePath = this->_parentFolders[0].VirtualPath;
    bool wantsExtractAll = true;

    if (this->_parentFolders.Size() > 1)
    {
        wantsExtractAll = false;
    }

    CRecordVector<UInt32> indices;
    this->Get_ItemIndices_Operated(indices);
    if (indices.Size() > 0)
    {
        wantsExtractAll = false;
    }

    UString path = this->GetFsPath();
    if (::IsPathSepar(path.Back()))
    {
        path.DeleteBack();
    }
    if (path != archivePath)
    {
        wantsExtractAll = false;
    }

    if (wantsExtractAll || !this->_panelCallback->OnCopyOrExtract())
    {
        UStringVector paths;
        paths.Add(archivePath);
        UString outFolder = ::GetSubFolderNameForExtract2(archivePath);

        CContextMenuInfo ci;
        ci.Load();

        ::ExtractArchives(
            paths,
            outFolder,
            true, // showDialog
            false, // elimDup
            ci.WriteZone);
    }
}
// **************** NanaZip Modification End ****************

/*
static void AddValuePair(UINT resourceID, UInt64 value, UString &s)
{
  AddLangString(s, resourceID);
  char sz[32];
  s += ": ";
  ConvertUInt64ToString(value, sz);
  s += sz;
  s.Add_LF();
}

// now we don't need CThreadTest, since now we call CopyTo for "test command

class CThreadTest: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  CRecordVector<UInt32> Indices;
  CExtractCallbackImp *ExtractCallbackSpec;
  CMyComPtr<IFolderArchiveExtractCallback> ExtractCallback;
  CMyComPtr<IArchiveFolder> ArchiveFolder;
};

HRESULT CThreadTest::ProcessVirt()
{
  RINOK(ArchiveFolder->Extract(&Indices[0], Indices.Size(),
      true, // includeAltStreams
      false, // replaceAltStreamColon
      NExtract::NPathMode::kFullPathnames,
      NExtract::NOverwriteMode::kAskBefore,
      NULL, // path
      BoolToInt(true), // testMode
      ExtractCallback));
  if (ExtractCallbackSpec->IsOK())
  {
    UString s;
    AddValuePair(IDS_PROP_FOLDERS, ExtractCallbackSpec->NumFolders, s);
    AddValuePair(IDS_PROP_FILES, ExtractCallbackSpec->NumFiles, s);
    // AddValuePair(IDS_PROP_SIZE, ExtractCallbackSpec->UnpackSize, s);
    // AddSizePair(IDS_COMPRESSED_COLON, Stat.PackSize, s);
    s.Add_LF();
    AddLangString(s, IDS_MESSAGE_NO_ERRORS);
    FinalMessage.OkMessage.Message = s;
  }
  return S_OK;
}

static void AddSizePair(UInt32 langID, UInt64 value, UString &s)
{
  char sz[32];
  AddLangString(s, langID);
  s += L' ';
  ConvertUInt64ToString(value, sz);
  s += sz;
  ConvertUInt64ToString(value >> 20, sz);
  s += " (";
  s += sz;
  s += " MB)";
  s.Add_LF();
}
*/

void CPanel::TestArchives()
{
  CRecordVector<UInt32> indices;
  Get_ItemIndices_OperSmart(indices);
  CMyComPtr<IArchiveFolder> archiveFolder;
  _folder.QueryInterface(IID_IArchiveFolder, &archiveFolder);
  if (archiveFolder)
  {
    CCopyToOptions options;
    options.streamMode = true;
    options.showErrorMessages = true;
    options.testMode = true;

    UStringVector messages;
    HRESULT res = CopyTo(options, indices, &messages);
    if (res != S_OK)
    {
      if (res != E_ABORT)
        MessageBox_Error_HRESULT(res);
    }
    return;

    /*
    {
    CThreadTest extracter;

    extracter.ArchiveFolder = archiveFolder;
    extracter.ExtractCallbackSpec = new CExtractCallbackImp;
    extracter.ExtractCallback = extracter.ExtractCallbackSpec;
    extracter.ExtractCallbackSpec->ProgressDialog = &extracter.ProgressDialog;
    if (!_parentFolders.IsEmpty())
    {
      const CFolderLink &fl = _parentFolders.Back();
      extracter.ExtractCallbackSpec->PasswordIsDefined = fl.UsePassword;
      extracter.ExtractCallbackSpec->Password = fl.Password;
    }

    if (indices.IsEmpty())
      return;

    extracter.Indices = indices;

    const UString title = LangString(IDS_PROGRESS_TESTING);

    extracter.ProgressDialog.CompressingMode = false;
    extracter.ProgressDialog.MainWindow = GetParent();
    extracter.ProgressDialog.MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
    extracter.ProgressDialog.MainAddTitle = title + L' ';

    extracter.ExtractCallbackSpec->OverwriteMode = NExtract::NOverwriteMode::kAskBefore;
    extracter.ExtractCallbackSpec->Init();

    if (extracter.Create(title, GetParent()) != S_OK)
      return;

    }
    RefreshTitleAlways();
    return;
    */
  }

  if (!IsFSFolder())
  {
    MessageBox_Error_UnsupportOperation();
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths);
  if (paths.IsEmpty())
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  ::TestArchives(paths);
}
