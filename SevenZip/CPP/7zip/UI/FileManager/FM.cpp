// FM.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include <Shlwapi.h>

#include "../../../../C/Alloc.h"
#ifdef _WIN32
#include "../../../../C/DllSecur.h"
#endif

#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/MemoryLock.h"
#include "../../../Windows/NtCheck.h"
#include "../../../Windows/System.h"

#ifndef UNDER_CE
#include "../../../Windows/SecurityUtils.h"
#endif

#include "../GUI/ExtractRes.h"

#include "resource.h"

#include "App.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "MyLoadMenu.h"
#include "Panel.h"
#include "RegistryUtils.h"
#include "StringUtils.h"
#include "ViewSettings.h"

using namespace NWindows;
using namespace NFile;
using namespace NFind;

// #define MAX_LOADSTRING 100

extern
bool g_RAM_Size_Defined;
bool g_RAM_Size_Defined;

static bool g_LargePagesMode = false;
// static bool g_OpenArchive = false;

static bool g_Maximized = false;

extern
UInt64 g_RAM_Size;
UInt64 g_RAM_Size;

#ifdef _WIN32
extern
HINSTANCE g_hInstance;
HINSTANCE g_hInstance;
#endif

HWND g_HWND;

static UString g_MainPath;
static UString g_ArcFormat;

// HRESULT LoadGlobalCodecs();
void FreeGlobalCodecs();

#ifndef UNDER_CE

extern
DWORD g_ComCtl32Version;
DWORD g_ComCtl32Version;

static DWORD GetDllVersion(LPCTSTR dllName)
{
  DWORD dwVersion = 0;
  HINSTANCE hinstDll = LoadLibrary(dllName);
  if (hinstDll)
  {
    DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)(void *)GetProcAddress(hinstDll, "DllGetVersion");
    if (pDllGetVersion)
    {
      DLLVERSIONINFO dvi;
      ZeroMemory(&dvi, sizeof(dvi));
      dvi.cbSize = sizeof(dvi);
      HRESULT hr = (*pDllGetVersion)(&dvi);
      if (SUCCEEDED(hr))
        dwVersion = MAKELONG(dvi.dwMinorVersion, dvi.dwMajorVersion);
    }
    FreeLibrary(hinstDll);
  }
  return dwVersion;
}

#endif

bool g_IsSmallScreen = false;

extern
bool g_LVN_ITEMACTIVATE_Support;
bool g_LVN_ITEMACTIVATE_Support = true;
// LVN_ITEMACTIVATE replaces both NM_DBLCLK & NM_RETURN
// Windows 2000
// NT/98 + IE 3 (g_ComCtl32Version >= 4.70)


static const int kNumDefaultPanels = 1;
static const int kSplitterWidth = 4;
static const int kSplitterRateMax = 1 << 16;
static const int kPanelSizeMin = 120;


class CSplitterPos
{
  int _ratio; // 10000 is max
  int _pos;
  int _fullWidth;
  void SetRatioFromPos(HWND hWnd)
    { _ratio = (_pos + kSplitterWidth / 2) * kSplitterRateMax /
        MyMax(GetWidth(hWnd), 1); }
public:
  int GetPos() const
    { return _pos; }
  int GetWidth(HWND hWnd) const
  {
    RECT rect;
    ::GetClientRect(hWnd, &rect);
    return rect.right;
  }
  void SetRatio(HWND hWnd, int aRatio)
  {
    _ratio = aRatio;
    SetPosFromRatio(hWnd);
  }
  void SetPosPure(HWND hWnd, int pos)
  {
    int posMax = GetWidth(hWnd) - kSplitterWidth;
    if (posMax < kPanelSizeMin * 2)
      pos = posMax / 2;
    else
    {
      if (pos > posMax - kPanelSizeMin)
        pos = posMax - kPanelSizeMin;
      else if (pos < kPanelSizeMin)
        pos = kPanelSizeMin;
    }
    _pos = pos;
  }
  void SetPos(HWND hWnd, int pos)
  {
    _fullWidth = GetWidth(hWnd);
    SetPosPure(hWnd, pos);
    SetRatioFromPos(hWnd);
  }
  void SetPosFromRatio(HWND hWnd)
  {
    int fullWidth = GetWidth(hWnd);
    if (_fullWidth != fullWidth && fullWidth != 0)
    {
      _fullWidth = fullWidth;
      SetPosPure(hWnd, GetWidth(hWnd) * _ratio / kSplitterRateMax - kSplitterWidth / 2);
    }
  }
};

static bool g_CanChangeSplitter = false;
static UInt32 g_SplitterPos = 0;
static CSplitterPos g_Splitter;
static bool g_PanelsInfoDefined = false;
static bool g_WindowWasCreated = false;

static int g_StartCaptureMousePos;
static int g_StartCaptureSplitterPos;

CApp g_App;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static const wchar_t * const kWindowClass = L"FM";

#ifdef UNDER_CE
#define WS_OVERLAPPEDWINDOW ( \
  WS_OVERLAPPED   | \
  WS_CAPTION      | \
  WS_SYSMENU      | \
  WS_THICKFRAME   | \
  WS_MINIMIZEBOX  | \
  WS_MAXIMIZEBOX)
#endif

//  FUNCTION: InitInstance(HANDLE, int)
static BOOL InitInstance(int nCmdShow)
{
  CWindow wnd;

  // LoadString(hInstance, IDS_CLASS, windowClass, MAX_LOADSTRING);

  UString title ("7-Zip"); // LangString(IDS_APP_TITLE, 0x03000000);

  /*
  //If it is already running, then focus on the window
  hWnd = FindWindow(windowClass, title);
  if (hWnd)
  {
    SetForegroundWindow ((HWND) (((DWORD)hWnd) | 0x01));
    return 0;
  }
  */

  WNDCLASSW wc;

  // wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.style = 0;
  wc.lpfnWndProc = (WNDPROC) WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = g_hInstance;
  wc.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON));

  // wc.hCursor = LoadCursor (NULL, IDC_ARROW);
  wc.hCursor = ::LoadCursor(0, IDC_SIZEWE);
  // wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
  wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);

  wc.lpszMenuName =
    #ifdef UNDER_CE
    0
    #else
    MAKEINTRESOURCEW(IDM_MENU)
    #endif
    ;

  wc.lpszClassName = kWindowClass;

  MyRegisterClass(&wc);

  // RECT rect;
  // GetClientRect(hWnd, &rect);

  DWORD style = WS_OVERLAPPEDWINDOW;
  // DWORD style = 0;
  
  CWindowInfo info;
  info.maximized = false;
  int x, y, xSize, ySize;
  x = y = xSize = ySize = CW_USEDEFAULT;
  bool windowPosIsRead;
  info.Read(windowPosIsRead, g_PanelsInfoDefined);

  if (windowPosIsRead)
  {
    x = info.rect.left;
    y = info.rect.top;
    
    xSize = RECT_SIZE_X(info.rect);
    ySize = RECT_SIZE_Y(info.rect);
  }


  if (g_PanelsInfoDefined)
  {
    g_SplitterPos = info.splitterPos;
    if (info.numPanels < 1 || info.numPanels > 2)
      info.numPanels = kNumDefaultPanels;
    if (info.currentPanel >= 2)
      info.currentPanel = 0;
  }
  else
  {
    info.numPanels = kNumDefaultPanels;
    info.currentPanel = 0;
  }

  g_App.NumPanels = info.numPanels;
  g_App.LastFocusedPanel = info.currentPanel;

  if (!wnd.Create(kWindowClass, title, style,
    x, y, xSize, ySize, NULL, NULL, g_hInstance, NULL))
    return FALSE;

  if (nCmdShow == SW_SHOWNORMAL ||
      nCmdShow == SW_SHOW
      #ifndef UNDER_CE
      || nCmdShow == SW_SHOWDEFAULT
      #endif
      )
  {
    if (info.maximized)
      nCmdShow = SW_SHOWMAXIMIZED;
    else
      nCmdShow = SW_SHOWNORMAL;
  }

  if (nCmdShow == SW_SHOWMAXIMIZED)
    g_Maximized = true;

  #ifndef UNDER_CE
  WINDOWPLACEMENT placement;
  placement.length = sizeof(placement);
  if (wnd.GetPlacement(&placement))
  {
    if (windowPosIsRead)
      placement.rcNormalPosition = info.rect;
    placement.showCmd = nCmdShow;
    wnd.SetPlacement(&placement);
  }
  else
  #endif
    wnd.Show(nCmdShow);

  return TRUE;
}

/*
static void GetCommands(const UString &aCommandLine, UString &aCommands)
{
  UString aProgramName;
  aCommands.Empty();
  bool aQuoteMode = false;
  for (int i = 0; i < aCommandLine.Length(); i++)
  {
    wchar_t aChar = aCommandLine[i];
    if (aChar == L'\"')
      aQuoteMode = !aQuoteMode;
    else if (aChar == L' ' && !aQuoteMode)
    {
      if (!aQuoteMode)
      {
        i++;
        break;
      }
    }
    else
      aProgramName += aChar;
  }
  aCommands = aCommandLine.Ptr(i);
}
*/

#if defined(_WIN32) && !defined(_WIN64) && !defined(UNDER_CE)

bool g_Is_Wow64;

typedef BOOL (WINAPI *Func_IsWow64Process)(HANDLE, PBOOL);

static void Set_Wow64()
{
  g_Is_Wow64 = false;
  Func_IsWow64Process fnIsWow64Process = (Func_IsWow64Process)(void *)GetProcAddress(
      GetModuleHandleA("kernel32.dll"), "IsWow64Process");
  if (fnIsWow64Process)
  {
    BOOL isWow;
    if (fnIsWow64Process(GetCurrentProcess(), &isWow))
      g_Is_Wow64 = (isWow != FALSE);
  }
}

#endif


bool IsLargePageSupported();
bool IsLargePageSupported()
{
  #ifdef _WIN64
  return true;
  #else
  OSVERSIONINFO vi;
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (!::GetVersionEx(&vi))
    return false;
  if (vi.dwPlatformId != VER_PLATFORM_WIN32_NT)
    return false;
  if (vi.dwMajorVersion < 5) return false;
  if (vi.dwMajorVersion > 5) return true;
  if (vi.dwMinorVersion < 1) return false;
  if (vi.dwMinorVersion > 1) return true;
  // return g_Is_Wow64;
  return false;
  #endif
}

#ifndef UNDER_CE

static void SetMemoryLock()
{
  if (!IsLargePageSupported())
    return;
  // if (ReadLockMemoryAdd())
  NSecurity::AddLockMemoryPrivilege();

  if (ReadLockMemoryEnable())
  if (NSecurity::Get_LargePages_RiskLevel() == 0)
  {
    // note: child processes can inherit that Privilege
    g_LargePagesMode = NSecurity::EnablePrivilege_LockMemory();
  }
}

extern
bool g_SymLink_Supported;
bool g_SymLink_Supported = false;

static void Set_SymLink_Supported()
{
  g_SymLink_Supported = false;
  OSVERSIONINFO vi;
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (!::GetVersionEx(&vi))
    return;
  if (vi.dwPlatformId != VER_PLATFORM_WIN32_NT || vi.dwMajorVersion < 6)
    return;
  g_SymLink_Supported = true;
  // if (g_SymLink_Supported)
  {
    NSecurity::EnablePrivilege_SymLink();
  }
}

#endif

/*
static const int kNumSwitches = 1;

namespace NKey {
enum Enum
{
  kOpenArachive = 0
};

}

static const CSwitchForm kSwitchForms[kNumSwitches] =
  {
    { L"SOA", NSwitchType::kSimple, false },
  };
*/

// int APIENTRY WinMain2(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */, LPSTR /* lpCmdLine */, int /* nCmdShow */);

static void ErrorMessage(const wchar_t *s)
{
  MessageBoxW(0, s, L"7-Zip", MB_ICONERROR);
}

static void ErrorMessage(const char *s)
{
  ErrorMessage(GetUnicodeString(s));
}


#if defined(_UNICODE) && !defined(_WIN64) && !defined(UNDER_CE)
#define NT_CHECK_FAIL_ACTION ErrorMessage("Unsupported Windows version"); return 1;
#endif

static int WINAPI WinMain2(int nCmdShow)
{
  g_RAM_Size_Defined = NSystem::GetRamSize(g_RAM_Size);

  #ifdef _WIN32

  /*
  #ifndef _WIN64
  #ifndef UNDER_CE
  {
    HMODULE hMod = GetModuleHandle("Kernel32.dll");
    if (hMod)
    {
      typedef BOOL (WINAPI *PSETDEP)(DWORD);
      #define MY_PROCESS_DEP_ENABLE 1
      PSETDEP procSet = (PSETDEP)GetProcAddress(hMod,"SetProcessDEPPolicy");
      if (procSet)
        procSet(MY_PROCESS_DEP_ENABLE);

      typedef BOOL (WINAPI *HSI)(HANDLE, HEAP_INFORMATION_CLASS ,PVOID, SIZE_T);
      HSI hsi = (HSI)GetProcAddress(hMod, "HeapSetInformation");
      #define MY_HeapEnableTerminationOnCorruption ((HEAP_INFORMATION_CLASS)1)
      if (hsi)
        hsi(NULL, MY_HeapEnableTerminationOnCorruption, NULL, 0);
    }
  }
  #endif
  #endif
  */

  NT_CHECK
  SetLargePageSize();

  #endif

  LoadLangOneTime();

  InitCommonControls();

  #ifndef UNDER_CE
  g_ComCtl32Version = ::GetDllVersion(TEXT("comctl32.dll"));
  g_LVN_ITEMACTIVATE_Support = (g_ComCtl32Version >= MAKELONG(71, 4));
  #endif

  #if defined(_WIN32) && !defined(_WIN64) && !defined(UNDER_CE)
  Set_Wow64();
  #endif


  g_IsSmallScreen = !NWindows::NControl::IsDialogSizeOK(200, 200);

  // OleInitialize is required for drag and drop.
  #ifndef UNDER_CE
  OleInitialize(NULL);
  #endif
  // Maybe needs CoInitializeEx also ?
  // NCOM::CComInitializer comInitializer;

  UString commandsString;
  // MessageBoxW(0, GetCommandLineW(), L"", 0);

  #ifdef UNDER_CE
  commandsString = GetCommandLineW();
  #else
  UString programString;
  SplitStringToTwoStrings(GetCommandLineW(), programString, commandsString);
  #endif

  commandsString.Trim();
  UString paramString, tailString;
  SplitStringToTwoStrings(commandsString, paramString, tailString);
  paramString.Trim();
  tailString.Trim();
  if (tailString.IsPrefixedBy(L"-t"))
    g_ArcFormat = tailString.Ptr(2);

  /*
  UStringVector switches;
  for (;;)
  {
    if (tailString.IsEmpty())
      break;
    UString s1, s2;
    SplitStringToTwoStrings(tailString, s1, s2);
    if (s2.IsEmpty())
    {
      tailString.Trim();
      switches.Add(tailString);
      break;
    }
    s1.Trim();
    switches.Add(s1);
    tailString = s2;
  }

  FOR_VECTOR(i, switches)
  {
    const UString &sw = switches[i];
    if (sw.IsPrefixedBy(L"-t"))
      g_ArcFormat = sw.Ptr(2);
    //
    else if (sw.IsPrefixedBy(L"-stp"))
    {
      const wchar_t *end;
      UInt32 val = ConvertStringToUInt32(sw.Ptr(4), &end);
      if (*end != 0)
        throw 111;
      g_TypeParseLevel = val;
    }
    else
    //
      throw 112;
  }
  */

  if (!paramString.IsEmpty())
  {
    g_MainPath = paramString;
    // return WinMain2(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    // MessageBoxW(0, paramString, L"", 0);
  }
  /*
  UStringVector commandStrings;
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), commandStrings);
  NCommandLineParser::CParser parser(kNumSwitches);
  try
  {
    parser.ParseStrings(kSwitchForms, commandStrings);
    const UStringVector &nonSwitchStrings = parser.NonSwitchStrings;
    if (nonSwitchStrings.Size() > 1)
    {
      g_MainPath = nonSwitchStrings[1];
      // g_OpenArchive = parser[NKey::kOpenArachive].ThereIs;
      CFileInfoW fileInfo;
      if (FindFile(g_MainPath, fileInfo))
      {
        if (!fileInfo.IsDir())
          g_OpenArchive = true;
      }
    }
  }
  catch(...) { }
  */


  #if defined(_WIN32) && !defined(UNDER_CE)
  SetMemoryLock();
  Set_SymLink_Supported();
  #endif

  g_App.ReloadLang();

  MSG msg;
  if (!InitInstance (nCmdShow))
    return FALSE;

  // we will load Global_Codecs at first use instead.
  /*
  OutputDebugStringW(L"Before LoadGlobalCodecs");
  LoadGlobalCodecs();
  OutputDebugStringW(L"After LoadGlobalCodecs");
  */

  #ifndef _UNICODE
  if (g_IsNT)
  {
    HACCEL hAccels = LoadAcceleratorsW(g_hInstance, MAKEINTRESOURCEW(IDR_ACCELERATOR1));
    while (GetMessageW(&msg, NULL, 0, 0))
    {
      if (TranslateAcceleratorW(g_HWND, hAccels, &msg) == 0)
      {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
  }
  else
  #endif
  {
    HACCEL hAccels = LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
    while (GetMessage(&msg, NULL, 0, 0))
    {
      if (TranslateAccelerator(g_HWND, hAccels, &msg) == 0)
      {
        // if (g_Hwnd != NULL || !IsDialogMessage(g_Hwnd, &msg))
        // if (!IsDialogMessage(g_Hwnd, &msg))
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }

  // Destructor of g_CodecsReleaser can release DLLs.
  // But we suppose that it's better to release DLLs here (before destructor).
  FreeGlobalCodecs();

  g_HWND = 0;
  #ifndef UNDER_CE
  OleUninitialize();
  #endif
  return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
    #ifdef UNDER_CE
    LPWSTR
    #else
    LPSTR
    #endif
    /* lpCmdLine */, int nCmdShow)
{
  g_hInstance = hInstance;

  try
  {
    try
    {
      #ifdef _WIN32
      My_SetDefaultDllDirectories();
      #endif
      return WinMain2(nCmdShow);
    }
    catch (...)
    {
      g_ExitEventLauncher.Exit(true);
      throw;
    }
  }
  catch(const CNewException &)
  {
    ErrorMessage(LangString(IDS_MEM_ERROR));
    return 1;
  }
  catch(const UString &s)
  {
    ErrorMessage(s);
    return 1;
  }
  catch(const AString &s)
  {
    ErrorMessage(s.Ptr());
    return 1;
  }
  catch(const wchar_t *s)
  {
    ErrorMessage(s);
    return 1;
  }
  catch(const char *s)
  {
    ErrorMessage(s);
    return 1;
  }
  catch(int v)
  {
    AString e ("Error: ");
    e.Add_UInt32(v);
    ErrorMessage(e);
    return 1;
  }
  catch(...)
  {
    ErrorMessage("Unknown error");
    return 1;
  }
}

static void SaveWindowInfo(HWND aWnd)
{
  CWindowInfo info;

  #ifdef UNDER_CE
  
  if (!::GetWindowRect(aWnd, &info.rect))
    return;
  info.maximized = g_Maximized;
  
  #else
  
  WINDOWPLACEMENT placement;
  placement.length = sizeof(placement);
  if (!::GetWindowPlacement(aWnd, &placement))
    return;
  info.rect = placement.rcNormalPosition;
  info.maximized = BOOLToBool(::IsZoomed(aWnd));
  
  #endif
  
  info.numPanels = g_App.NumPanels;
  info.currentPanel = g_App.LastFocusedPanel;
  info.splitterPos = g_Splitter.GetPos();

  info.Save();
}

static void ExecuteCommand(UINT commandID)
{
  CPanel::CDisableTimerProcessing disableTimerProcessing1(g_App.Panels[0]);
  CPanel::CDisableTimerProcessing disableTimerProcessing2(g_App.Panels[1]);

  switch (commandID)
  {
    case kMenuCmdID_Toolbar_Add: g_App.AddToArchive(); break;
    case kMenuCmdID_Toolbar_Extract: g_App.ExtractArchives(); break;
    case kMenuCmdID_Toolbar_Test: g_App.TestArchives(); break;
  }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_COMMAND:
    {
      unsigned wmId    = LOWORD(wParam);
      unsigned wmEvent = HIWORD(wParam);
      if ((HWND) lParam != NULL && wmEvent != 0)
        break;
      if (wmId >= kMenuCmdID_Toolbar_Start && wmId < kMenuCmdID_Toolbar_End)
      {
        ExecuteCommand(wmId);
        return 0;
      }
      if (OnMenuCommand(hWnd, wmId))
        return 0;
      break;
    }
    case WM_INITMENUPOPUP:
      OnMenuActivating(hWnd, HMENU(wParam), LOWORD(lParam));
      break;

    /*
    It doesn't help
    case WM_EXITMENULOOP:
      {
        OnMenuUnActivating(hWnd);
        break;
      }
    case WM_UNINITMENUPOPUP:
      OnMenuUnActivating(hWnd, HMENU(wParam), lParam);
      break;
    */

    case WM_CREATE:
    {
      g_HWND = hWnd;
      /*
      INITCOMMONCONTROLSEX icex;
      icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
      icex.dwICC  = ICC_BAR_CLASSES;
      InitCommonControlsEx(&icex);
      
      // Toolbar buttons used to create the first 4 buttons.
      TBBUTTON tbb [ ] =
      {
        // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
        // {VIEW_PARENTFOLDER, kParentFolderID, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
          // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
        {VIEW_NEWFOLDER, ID_FILE_CREATEFOLDER, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
      };
      
      int baseID = 100;
      NWindows::NControl::CToolBar aToolBar;
      aToolBar.Attach(::CreateToolbarEx (hWnd,
        WS_CHILD | WS_BORDER | WS_VISIBLE | TBSTYLE_TOOLTIPS, //  | TBSTYLE_FLAT
        baseID + 2, 11,
        (HINSTANCE)HINST_COMMCTRL, IDB_VIEW_SMALL_COLOR,
        (LPCTBBUTTON)&tbb, ARRAY_SIZE(tbb),
        0, 0, 100, 30, sizeof (TBBUTTON)));
      */
      // HCURSOR cursor = ::LoadCursor(0, IDC_SIZEWE);
      // ::SetCursor(cursor);

      if (g_PanelsInfoDefined)
        g_Splitter.SetPos(hWnd, g_SplitterPos);
      else
      {
        g_Splitter.SetRatio(hWnd, kSplitterRateMax / 2);
        g_SplitterPos = g_Splitter.GetPos();
      }

      RECT rect;
      ::GetClientRect(hWnd, &rect);
      int xSize = rect.right;
      int xSizes[2];
      xSizes[0] = g_Splitter.GetPos();
      xSizes[1] = xSize - kSplitterWidth - xSizes[0];
      if (xSizes[1] < 0)
        xSizes[1] = 0;

      g_App.CreateDragTarget();
      
      COpenResult openRes;
      bool needOpenArc = false;

      UString fullPath = g_MainPath;
      if (!fullPath.IsEmpty() /* && g_OpenArchive */)
      {
        if (!NFile::NName::IsAbsolutePath(fullPath))
        {
          FString fullPathF;
          if (NFile::NName::GetFullPath(us2fs(fullPath), fullPathF))
            fullPath = fs2us(fullPathF);
        }
        if (NFile::NFind::DoesFileExist_FollowLink(us2fs(fullPath)))
          needOpenArc = true;
      }
      
      HRESULT res = g_App.Create(hWnd, fullPath, g_ArcFormat, xSizes,
          needOpenArc,
          openRes);

      if (res == E_ABORT)
        return -1;
      
      if ((needOpenArc && !openRes.ArchiveIsOpened) || res != S_OK)
      {
        UString m ("Error");
        if (res == S_FALSE || res == S_OK)
        {
          m = MyFormatNew(openRes.Encrypted ?
                IDS_CANT_OPEN_ENCRYPTED_ARCHIVE :
                IDS_CANT_OPEN_ARCHIVE,
              fullPath);
        }
        else if (res != S_OK)
          m = HResultToMessage(res);
        if (!openRes.ErrorMessage.IsEmpty())
        {
          m.Add_LF();
          m += openRes.ErrorMessage;
        }
        ErrorMessage(m);
        return -1;
      }

      g_WindowWasCreated = true;
      
      // g_SplitterPos = 0;

      // ::DragAcceptFiles(hWnd, TRUE);
      RegisterDragDrop(hWnd, g_App._dropTarget);

      break;
    }

    case WM_DESTROY:
    {
      // ::DragAcceptFiles(hWnd, FALSE);
      RevokeDragDrop(hWnd);
      g_App._dropTarget.Release();

      if (g_WindowWasCreated)
        g_App.Save();
    
      g_App.Release();
      
      if (g_WindowWasCreated)
        SaveWindowInfo(hWnd);

      g_ExitEventLauncher.Exit(true);
      PostQuitMessage(0);
      break;
    }
    
    // case WM_MOVE: break;
    
    case WM_LBUTTONDOWN:
      g_StartCaptureMousePos = LOWORD(lParam);
      g_StartCaptureSplitterPos = g_Splitter.GetPos();
      ::SetCapture(hWnd);
      break;
    
    case WM_LBUTTONUP:
    {
      ::ReleaseCapture();
      break;
    }
    
    case WM_MOUSEMOVE:
    {
      if ((wParam & MK_LBUTTON) != 0 && ::GetCapture() == hWnd)
      {
        g_Splitter.SetPos(hWnd, g_StartCaptureSplitterPos +
            (short)LOWORD(lParam) - g_StartCaptureMousePos);
        g_App.MoveSubWindows();
      }
      break;
    }

    case WM_SIZE:
    {
      if (g_CanChangeSplitter)
        g_Splitter.SetPosFromRatio(hWnd);
      else
      {
        g_Splitter.SetPos(hWnd, g_SplitterPos );
        g_CanChangeSplitter = true;
      }
      
      g_Maximized = (wParam == SIZE_MAXIMIZED) || (wParam == SIZE_MAXSHOW);

      g_App.MoveSubWindows();
      /*
      int xSize = LOWORD(lParam);
      int ySize = HIWORD(lParam);
      // int xSplitter = 2;
      int xWidth = g_SplitPos;
      // int xSplitPos = xWidth;
      g_Panel[0]._listView.MoveWindow(0, 0, xWidth, ySize);
      g_Panel[1]._listView.MoveWindow(xSize - xWidth, 0, xWidth, ySize);
      */
      return 0;
      // break;
    }
    
    case WM_SETFOCUS:
      // g_App.SetFocus(g_App.LastFocusedPanel);
      g_App.SetFocusToLastItem();
      break;
    
    /*
    case WM_ACTIVATE:
    {
      int fActive = LOWORD(wParam);
      switch (fActive)
      {
        case WA_INACTIVE:
        {
          // g_FocusIndex = g_App.LastFocusedPanel;
          // g_App.LastFocusedPanel = g_App.GetFocusedPanelIndex();
          // return 0;
        }
      }
      break;
    }
    */
    
    /*
    case kLangWasChangedMessage:
      MyLoadMenu();
      return 0;
    */
      
    /*
    case WM_SETTINGCHANGE:
      break;
    */
    
    case WM_NOTIFY:
    {
      g_App.OnNotify((int)wParam, (LPNMHDR)lParam);
      break;
    }
    
    /*
    case WM_DROPFILES:
    {
      g_App.GetFocusedPanel().CompressDropFiles((HDROP)wParam);
      return 0 ;
    }
    */
  }
  #ifndef _UNICODE
  if (g_IsNT)
    return DefWindowProcW(hWnd, message, wParam, lParam);
  else
  #endif
    return DefWindowProc(hWnd, message, wParam, lParam);

}

static int Window_GetRealHeight(NWindows::CWindow &w)
{
  RECT rect;
  w.GetWindowRect(&rect);
  int res = RECT_SIZE_Y(rect);
  #ifndef UNDER_CE
  WINDOWPLACEMENT placement;
  if (w.GetPlacement(&placement))
    res += placement.rcNormalPosition.top;
  #endif
  return res;
}

void CApp::MoveSubWindows()
{
  HWND hWnd = _window;
  RECT rect;
  if (hWnd == 0)
    return;
  ::GetClientRect(hWnd, &rect);
  int xSize = rect.right;
  if (xSize == 0)
    return;
  int headerSize = 0;

  #ifdef UNDER_CE
  _commandBar.AutoSize();
  {
    _commandBar.Show(true); // maybe we need it for
    headerSize += _commandBar.Height();
  }
  #endif

  if (_toolBar)
  {
    _toolBar.AutoSize();
    #ifdef UNDER_CE
    int h2 = Window_GetRealHeight(_toolBar);
    _toolBar.Move(0, headerSize, xSize, h2);
    #endif
    headerSize += Window_GetRealHeight(_toolBar);
  }
  
  int ySize = MyMax((int)(rect.bottom - headerSize), 0);
  
  if (NumPanels > 1)
  {
    Panels[0].Move(0, headerSize, g_Splitter.GetPos(), ySize);
    int xWidth1 = g_Splitter.GetPos() + kSplitterWidth;
    Panels[1].Move(xWidth1, headerSize, xSize - xWidth1, ySize);
  }
  else
  {
    /*
    int otherPanel = 1 - LastFocusedPanel;
    if (PanelsCreated[otherPanel])
      Panels[otherPanel].Move(0, headerSize, 0, ySize);
    */
    Panels[LastFocusedPanel].Move(0, headerSize, xSize, ySize);
  }
}
