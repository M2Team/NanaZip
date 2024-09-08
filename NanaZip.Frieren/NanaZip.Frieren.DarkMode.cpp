/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Frieren.DarkMode.cpp
 * PURPOSE:   Implementation for NanaZip dark mode support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Mile.Helpers.h>
#include <Mile.Helpers.CppBase.h>

#include <Detours.h>

#include <Uxtheme.h>

EXTERN_C HTHEME WINAPI OpenNcThemeData(
    _In_opt_ HWND hwnd,
    _In_ LPCWSTR pszClassList);

#include <vssym32.h>
#include <Richedit.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <ShellScalingApi.h>

#include <unordered_map>

#include "NanaZip.Frieren.WinUserPrivate.h"

// TODO: Move some workaround for NanaZip.UI.* to this.

namespace
{
    static HMODULE GetSHCoreModuleHandle()
    {
        static HMODULE CachedResult = ::GetModuleHandleW(L"SHCore.dll");
        return CachedResult;
    }

    static FARPROC GetGetDpiForMonitorProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetSHCoreModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "GetDpiForMonitor");
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static HMODULE GetUser32ModuleHandle()
    {
        static HMODULE CachedResult = ::GetModuleHandleW(L"user32.dll");
        return CachedResult;
    }

    static FARPROC GetGetDisplayConfigBufferSizesProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetUser32ModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "GetDisplayConfigBufferSizes");
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static FARPROC GetQueryDisplayConfigProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetUser32ModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "QueryDisplayConfig");
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static FARPROC GetDisplayConfigGetDeviceInfoProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetUser32ModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "DisplayConfigGetDeviceInfo");
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static FARPROC GetGetDpiForWindowProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetUser32ModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "GetDpiForWindow");
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static LONG WINAPI GetDisplayConfigBufferSizesWrapper(
        _In_ UINT32 flags,
        _Out_ UINT32* numPathArrayElements,
        _Out_ UINT32* numModeInfoArrayElements)
    {
        using ProcType = decltype(::GetDisplayConfigBufferSizes)*;

        ProcType ProcAddress = reinterpret_cast<ProcType>(
            ::GetGetDisplayConfigBufferSizesProcAddress());

        if (ProcAddress)
        {
            return ProcAddress(
                flags,
                numPathArrayElements,
                numModeInfoArrayElements);
        }

        return ERROR_NOT_SUPPORTED;
    }

    static LONG WINAPI QueryDisplayConfigWrapper(
        _In_ UINT32 flags,
        _Inout_ UINT32* numPathArrayElements,
        _Out_ DISPLAYCONFIG_PATH_INFO* pathArray,
        _Inout_ UINT32* numModeInfoArrayElements,
        _Out_ DISPLAYCONFIG_MODE_INFO* modeInfoArray,
        _Out_opt_ DISPLAYCONFIG_TOPOLOGY_ID* currentTopologyId)
    {
        using ProcType = decltype(::QueryDisplayConfig)*;

        ProcType ProcAddress = reinterpret_cast<ProcType>(
            ::GetQueryDisplayConfigProcAddress());

        if (ProcAddress)
        {
            return ProcAddress(
                flags,
                numPathArrayElements,
                pathArray,
                numModeInfoArrayElements,
                modeInfoArray,
                currentTopologyId);
        }

        return ERROR_NOT_SUPPORTED;
    }

    static LONG WINAPI DisplayConfigGetDeviceInfoWrapper(
        _Inout_ DISPLAYCONFIG_DEVICE_INFO_HEADER* requestPacket)
    {
        using ProcType = decltype(::DisplayConfigGetDeviceInfo)*;

        ProcType ProcAddress = reinterpret_cast<ProcType>(
            ::GetDisplayConfigGetDeviceInfoProcAddress());

        if (ProcAddress)
        {
            return ProcAddress(requestPacket);
        }

        return ERROR_NOT_SUPPORTED;
    }

    static UINT WINAPI GetDpiForWindowWrapper(
        _In_ HWND hWnd)
    {
        {
            using ProcType = decltype(::GetDpiForWindow)*;

            ProcType ProcAddress = reinterpret_cast<ProcType>(
                ::GetGetDpiForWindowProcAddress());

            if (ProcAddress)
            {
                return ProcAddress(hWnd);
            }
        }

        {
            using ProcType = decltype(::GetDpiForMonitor)*;

            ProcType ProcAddress = reinterpret_cast<ProcType>(
                ::GetGetDpiForMonitorProcAddress());

            if (ProcAddress)
            {
                HMONITOR MonitorHandle = ::MonitorFromWindow(
                    hWnd,
                    MONITOR_DEFAULTTONEAREST);
                if (MonitorHandle)
                {
                    UINT DpiX = 0;
                    UINT DpiY = 0;
                    if (SUCCEEDED(ProcAddress(
                        MonitorHandle,
                        MDT_EFFECTIVE_DPI,
                        &DpiX,
                        &DpiY)))
                    {
                        return DpiX;
                    }
                }
            }
        }

        UINT DpiValue = USER_DEFAULT_SCREEN_DPI;
        {
            HDC WindowContextHandle = ::GetDC(hWnd);
            if (WindowContextHandle)
            {
                DpiValue = ::GetDeviceCaps(WindowContextHandle, LOGPIXELSX);
                ::ReleaseDC(hWnd, WindowContextHandle);
            }
        }

        return DpiValue;
    }

    static bool IsStandardDynamicRangeMode()
    {
        static bool CachedResult = ([]() -> bool
        {
            bool Result = true;

            UINT32 NumPathArrayElements = 0;
            UINT32 NumModeInfoArrayElements = 0;
            if (ERROR_SUCCESS == ::GetDisplayConfigBufferSizesWrapper(
                QDC_ONLY_ACTIVE_PATHS,
                &NumPathArrayElements,
                &NumModeInfoArrayElements))
            {
                std::vector<DISPLAYCONFIG_PATH_INFO> PathArray(
                    NumPathArrayElements);
                std::vector<DISPLAYCONFIG_MODE_INFO> ModeInfoArray(
                    NumModeInfoArrayElements);
                if (ERROR_SUCCESS == ::QueryDisplayConfigWrapper(
                    QDC_ONLY_ACTIVE_PATHS,
                    &NumPathArrayElements,
                    &PathArray[0],
                    &NumModeInfoArrayElements,
                    &ModeInfoArray[0],
                    nullptr))
                {
                    for (DISPLAYCONFIG_PATH_INFO const& Path : PathArray)
                    {
                        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO AdvancedColorInfo;
                        std::memset(
                            &AdvancedColorInfo,
                            0,
                            sizeof(DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO));
                        AdvancedColorInfo.header.type =
                            DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
                        AdvancedColorInfo.header.size =
                            sizeof(DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO);
                        AdvancedColorInfo.header.adapterId =
                            Path.targetInfo.adapterId;
                        AdvancedColorInfo.header.id =
                            Path.targetInfo.id;
                        if (ERROR_SUCCESS ==
                            ::DisplayConfigGetDeviceInfoWrapper(
                                &AdvancedColorInfo.header))
                        {
                            if (AdvancedColorInfo.advancedColorEnabled)
                            {
                                Result = false;
                                break;
                            }
                        }
                    }
                }
            }

            return Result;
        }());

        return CachedResult;
    }

    namespace FunctionType
    {
        enum
        {
            GetSysColor,
            GetSysColorBrush,
            DrawThemeText,
            DrawThemeBackground,
            OpenNcThemeData,

            MaximumFunction
        };
    }

    struct FunctionItem
    {
        PVOID Original;
        PVOID Detoured;
    };

    FunctionItem g_FunctionTable[FunctionType::MaximumFunction];

    const COLORREF LightModeBackgroundColor = RGB(255, 255, 255);
    const COLORREF LightModeForegroundColor = RGB(0, 0, 0);

    const COLORREF DarkModeBackgroundColor = RGB(0, 0, 0);
    const COLORREF DarkModeForegroundColor = RGB(192, 192, 192);
    const COLORREF DarkModeBorderColor = RGB(127, 127, 127);

    static const HBRUSH DarkModeBackgroundBrush =
        ::CreateSolidBrush(DarkModeBackgroundColor);
    static const HBRUSH DarkModeForegroundBrush =
        ::CreateSolidBrush(DarkModeForegroundColor);
    static const HBRUSH DarkModeBorderBrush =
        ::CreateSolidBrush(DarkModeBorderColor);

    thread_local bool volatile g_ThreadInitialized = false;
    thread_local bool volatile g_MicaBackdropAvailable = false;
    thread_local HHOOK volatile g_WindowsHookHandle = nullptr;
    thread_local bool volatile g_ShouldAppsUseDarkMode = false;
    thread_local HTHEME g_TabControlThemeHandle = nullptr;
    thread_local HTHEME g_StatusBarThemeHandle = nullptr;

    static void RefreshWindowTheme(
        _In_ HWND WindowHandle)
    {
        wchar_t ClassName[256] = { 0 };
        if (0 != ::GetClassNameW(
            WindowHandle,
            ClassName,
            256))
        {
            if (0 == std::wcscmp(ClassName, WC_BUTTONW))
            {
                ::SetWindowTheme(WindowHandle, L"Explorer", nullptr);
            }
            else if (
                (0 == std::wcscmp(ClassName, WC_COMBOBOXW)) ||
                (0 == std::wcscmp(ClassName, WC_EDITW)))
            {
                ::SetWindowTheme(WindowHandle, L"CFD", nullptr);
                ::MileAllowDarkModeForWindow(WindowHandle, TRUE);
            }
            else if (0 == std::wcscmp(ClassName, WC_HEADERW))
            {
                ::SetWindowTheme(WindowHandle, L"ItemsView", nullptr);
            }
            else if (0 == std::wcscmp(ClassName, WC_LISTVIEWW))
            {
                ::SetWindowTheme(WindowHandle, L"ItemsView", nullptr);

                if (g_ShouldAppsUseDarkMode)
                {
                    ListView_SetTextBkColor(
                        WindowHandle,
                        DarkModeBackgroundColor);
                    ListView_SetBkColor(
                        WindowHandle,
                        DarkModeBackgroundColor);
                    ListView_SetTextColor(
                        WindowHandle,
                        DarkModeForegroundColor);
                }
                else
                {
                    ListView_SetTextBkColor(
                        WindowHandle,
                        LightModeBackgroundColor);
                    ListView_SetBkColor(
                        WindowHandle,
                        LightModeBackgroundColor);
                    ListView_SetTextColor(
                        WindowHandle,
                        LightModeForegroundColor);
                }
            }
            else if (0 == std::wcscmp(ClassName, STATUSCLASSNAMEW))
            {
                ::SetWindowLongW(
                    WindowHandle,
                    GWL_EXSTYLE,
                    ::GetWindowLongW(
                        WindowHandle,
                        GWL_EXSTYLE) | WS_EX_COMPOSITED);
            }
            else if (0 == std::wcscmp(ClassName, WC_TABCONTROLW))
            {
                ::SetWindowLongW(
                    WindowHandle,
                    GWL_EXSTYLE,
                    ::GetWindowLongW(
                        WindowHandle,
                        GWL_EXSTYLE) | WS_EX_COMPOSITED);
            }
            else
            {
                // DO NOT USE ELSE IF INSTEAD
                // FOR HANDLING DYNAMIC DARK AND LIGHT MODE SWITCH PROPERLY

                if (0 == std::wcscmp(ClassName, TOOLBARCLASSNAMEW))
                {
                    // make it double bufferred
                    ::SetWindowLongW(
                        WindowHandle,
                        GWL_EXSTYLE,
                        ::GetWindowLongW(
                            WindowHandle,
                            GWL_EXSTYLE) | WS_EX_COMPOSITED);

                    COLORSCHEME ColorScheme;
                    ColorScheme.dwSize = sizeof(COLORSCHEME);
                    ColorScheme.clrBtnHighlight = CLR_DEFAULT;
                    ColorScheme.clrBtnShadow = CLR_DEFAULT;
                    if (g_ShouldAppsUseDarkMode)
                    {
                        ColorScheme.clrBtnHighlight = DarkModeBackgroundColor;
                        ColorScheme.clrBtnShadow = DarkModeBackgroundColor;
                    }
                    ::SendMessageW(
                        WindowHandle,
                        TB_SETCOLORSCHEME,
                        0,
                        reinterpret_cast<LPARAM>(&ColorScheme));
                }

                ::SendMessageW(WindowHandle, WM_THEMECHANGED, 0, 0);
            }
        }
    }

    static bool IsFileManagerWindow(
        _In_ HWND WindowHandle)
    {
        wchar_t ClassName[256] = { 0 };
        if (0 != ::GetClassNameW(
            WindowHandle,
            ClassName,
            256))
        {
            return (0 == std::wcscmp(
                ClassName,
                L"NanaZip.Modern.FileManager"));
        }

        return false;
    }

    LRESULT CALLBACK WindowSubclassCallback(
        _In_ HWND hWnd,
        _In_ UINT uMsg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam,
        _In_ UINT_PTR uIdSubclass,
        _In_ DWORD_PTR dwRefData)
    {
        UNREFERENCED_PARAMETER(uIdSubclass);
        UNREFERENCED_PARAMETER(dwRefData);

        switch (uMsg)
        {
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        {
            if (g_ShouldAppsUseDarkMode)
            {
                HDC DeviceContextHandle = reinterpret_cast<HDC>(wParam);
                if (DeviceContextHandle)
                {
                    ::SetTextColor(
                        DeviceContextHandle,
                        DarkModeForegroundColor);
                    ::SetBkColor(
                        DeviceContextHandle,
                        DarkModeBackgroundColor);
                }

                return reinterpret_cast<INT_PTR>(DarkModeBackgroundBrush);
            }

            break;
        }
        default:
            break;
        }

        LRESULT Result = ::DefSubclassProc(
            hWnd,
            uMsg,
            wParam,
            lParam);

        switch (uMsg)
        {
        case WM_SETTINGCHANGE:
        {
            LPCTSTR Section = reinterpret_cast<LPCTSTR>(lParam);

            if (Section && 0 == std::wcscmp(Section, L"ImmersiveColorSet"))
            {
                ::MileRefreshImmersiveColorPolicyState();

                g_ShouldAppsUseDarkMode =
                    ::MileShouldAppsUseDarkMode() &&
                    !::MileShouldAppsUseHighContrastMode();

                ::MileEnableImmersiveDarkModeForWindow(
                    hWnd,
                    g_ShouldAppsUseDarkMode);

                bool ShouldExtendFrame = (
                    g_ShouldAppsUseDarkMode &&
                    ::IsStandardDynamicRangeMode() &&
                    g_MicaBackdropAvailable);
                MARGINS Margins = { 0 };
                if (ShouldExtendFrame)
                {
                    Margins = { -1 };
                }
                else if (::IsFileManagerWindow(hWnd))
                {
                    UINT DpiValue = ::GetDpiForWindowWrapper(hWnd);
                    Margins.cyTopHeight =
                        ::MulDiv(48, DpiValue, USER_DEFAULT_SCREEN_DPI);
                }
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);

                ::EnumChildWindows(
                    hWnd,
                    [](
                        _In_ HWND hWnd,
                        _In_ LPARAM lParam) -> BOOL
                    {
                        UNREFERENCED_PARAMETER(lParam);
                        ::RefreshWindowTheme(hWnd);
                        return TRUE;
                    },
                    0);

                ::InvalidateRect(hWnd, nullptr, TRUE);
            }

            break;
        }
        case WM_INITDIALOG:
        case WM_CREATE:
        {
            ::MileAllowDarkModeForWindow(
                hWnd,
                TRUE);

            ::MileSetWindowSystemBackdropTypeAttribute(
                hWnd,
                MILE_WINDOW_SYSTEM_BACKDROP_TYPE_MICA);

            g_MicaBackdropAvailable =
                (S_OK == ::MileEnableImmersiveDarkModeForWindow(
                    hWnd,
                    g_ShouldAppsUseDarkMode));

            bool ShouldExtendFrame = (
                g_ShouldAppsUseDarkMode &&
                ::IsStandardDynamicRangeMode() &&
                g_MicaBackdropAvailable);
            if (ShouldExtendFrame)
            {
                MARGINS Margins = { -1 };
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
            }
            else if (::IsFileManagerWindow(hWnd))
            {
                UINT DpiValue = ::GetDpiForWindowWrapper(hWnd);

                MARGINS Margins = { 0 };
                Margins.cyTopHeight =
                    ::MulDiv(48, DpiValue, USER_DEFAULT_SCREEN_DPI);
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
            }

            ::RefreshWindowTheme(hWnd);

            wchar_t ClassName[256] = { 0 };
            if (0 != ::GetClassNameW(
                hWnd,
                ClassName,
                256))
            {
                if (0 == std::wcscmp(ClassName, WC_TABCONTROLW))
                {
                    ::SetWindowLongPtrW(
                        hWnd,
                        GWL_STYLE,
                        (::GetWindowLongPtrW(hWnd, GWL_STYLE) & ~TCS_BUTTONS)
                        | TCS_TABS);
                    ::SetWindowTheme(hWnd, nullptr, nullptr);
                    g_TabControlThemeHandle = ::GetWindowTheme(hWnd);
                }
                else if (0 == std::wcscmp(ClassName, STATUSCLASSNAMEW))
                {
                    g_StatusBarThemeHandle = ::GetWindowTheme(hWnd);
                }
            }

            break;
        }
        case WM_ERASEBKGND:
        {
            if (g_ShouldAppsUseDarkMode)
            {
                RECT ClientArea = { 0 };
                if (::GetClientRect(hWnd, &ClientArea))
                {
                    ::FillRect(
                        reinterpret_cast<HDC>(wParam),
                        &ClientArea,
                        reinterpret_cast<HBRUSH>(
                            ::GetStockObject(BLACK_BRUSH)));
                }

                return TRUE;
            }

            break;
        }
        case WM_DPICHANGED:
        {
            bool ShouldExtendFrame = (
                g_ShouldAppsUseDarkMode &&
                ::IsStandardDynamicRangeMode() &&
                g_MicaBackdropAvailable);
            if (!ShouldExtendFrame && ::IsFileManagerWindow(hWnd))
            {
                UINT DpiValue = ::GetDpiForWindowWrapper(hWnd);

                MARGINS Margins = { 0 };
                Margins.cyTopHeight =
                    ::MulDiv(48, DpiValue, USER_DEFAULT_SCREEN_DPI);
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
            }

            break;
        }
        default:
            break;
        }

        if (g_ShouldAppsUseDarkMode && ::GetMenu(hWnd))
        {
            if (WM_UAHDRAWMENU == uMsg)
            {
                PUAHMENU UahMenu = reinterpret_cast<PUAHMENU>(lParam);
                if (UahMenu)
                {
                    MENUBARINFO MenuBarInfo;
                    MenuBarInfo.cbSize = sizeof(MENUBARINFO);
                    if (::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &MenuBarInfo))
                    {
                        RECT WindowRect = { 0 };
                        ::GetWindowRect(hWnd, &WindowRect);

                        RECT MenuRect = MenuBarInfo.rcBar;
                        ::OffsetRect(
                            &MenuRect,
                            -WindowRect.left,
                            -WindowRect.top);

                        ::FillRect(
                            UahMenu->hdc,
                            &MenuRect,
                            DarkModeBackgroundBrush);
                    }
                }

                return TRUE;
            }
            else if (WM_UAHDRAWMENUITEM == uMsg)
            {
                PUAHDRAWMENUITEM UahDrawMenuItem =
                    reinterpret_cast<PUAHDRAWMENUITEM>(lParam);
                if (UahDrawMenuItem)
                {
                    PDRAWITEMSTRUCT DrawItemStruct = &UahDrawMenuItem->dis;
                    if (ODT_MENU == DrawItemStruct->CtlType)
                    {
                        wchar_t Buffer[256] = { 0 };
                        MENUITEMINFOW MenuItemInfo;
                        MenuItemInfo.cbSize = sizeof(MENUITEMINFOW);
                        MenuItemInfo.fMask = MIIM_STRING;
                        MenuItemInfo.dwTypeData = Buffer;
                        MenuItemInfo.cch = sizeof(Buffer) - 1;
                        if (::GetMenuItemInfoW(
                            UahDrawMenuItem->um.hmenu,
                            UahDrawMenuItem->umi.iPosition,
                            TRUE,
                            &MenuItemInfo))
                        {
                            int StateId = 0;
                            COLORREF TextColor = DarkModeForegroundColor;
                            HBRUSH BackgroundBrush = DarkModeBackgroundBrush;
                            if (DrawItemStruct->itemState & ODS_INACTIVE)
                            {
                                StateId = MBI_DISABLED;
                                TextColor = RGB(109, 109, 109);
                            }
                            else if ((DrawItemStruct->itemState & ODS_GRAYED) &&
                                (DrawItemStruct->itemState & ODS_HOTLIGHT))
                            {
                                StateId = MBI_DISABLEDHOT;
                            }
                            else if (DrawItemStruct->itemState & ODS_GRAYED)
                            {
                                StateId = MBI_DISABLED;
                                TextColor = RGB(109, 109, 109);
                            }
                            else if (DrawItemStruct->itemState
                                & (ODS_HOTLIGHT | ODS_SELECTED))
                            {
                                StateId = MBI_HOT;
                                static HBRUSH DarkModeMenuSelectedBackgroundBrush =
                                    ::CreateSolidBrush(RGB(65, 65, 65));
                                BackgroundBrush = DarkModeMenuSelectedBackgroundBrush;
                            }
                            else
                            {
                                StateId = MBI_NORMAL;
                            }

                            ::FillRect(
                                DrawItemStruct->hDC,
                                &DrawItemStruct->rcItem,
                                BackgroundBrush);

                            // We have to specify the text colour explicitly as by default
                            // black would be used, making the menu label unreadable on the
                            // (almost) black background.
                            DTTOPTS TextOptions = { 0 };
                            TextOptions.dwSize = sizeof(DTTOPTS);
                            TextOptions.dwFlags = DTT_TEXTCOLOR;
                            TextOptions.crText = TextColor;

                            DWORD TextFlags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
                            if (DrawItemStruct->itemState & ODS_NOACCEL)
                            {
                                TextFlags |= DT_HIDEPREFIX;
                            }

                            HTHEME ThemeHandle = ::OpenThemeData(hWnd, L"Menu");
                            if (ThemeHandle)
                            {
                                ::DrawThemeTextEx(
                                    ThemeHandle,
                                    UahDrawMenuItem->um.hdc,
                                    MENU_BARITEM,
                                    StateId,
                                    Buffer,
                                    static_cast<int>(std::wcslen(Buffer)),
                                    TextFlags,
                                    &DrawItemStruct->rcItem,
                                    &TextOptions);

                                ::CloseThemeData(ThemeHandle);
                            }
                        }
                    }
                }

                return TRUE;
            }
            else if (WM_NCPAINT == uMsg || WM_NCACTIVATE == uMsg)
            {
                MENUBARINFO MenuBarInfo;
                MenuBarInfo.cbSize = sizeof(MENUBARINFO);
                if (::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &MenuBarInfo))
                {
                    RECT ClientRect = { 0 };
                    ::GetClientRect(hWnd, &ClientRect);

                    ::MapWindowPoints(
                        hWnd,
                        nullptr,
                        reinterpret_cast<PPOINT>(&ClientRect),
                        2);

                    RECT WindowRect = { 0 };
                    ::GetWindowRect(hWnd, &WindowRect);

                    ::OffsetRect(
                        &ClientRect,
                        -WindowRect.left,
                        -WindowRect.top);

                    RECT AnnoyingLineRect = ClientRect;
                    AnnoyingLineRect.bottom = AnnoyingLineRect.top;
                    --AnnoyingLineRect.top;

                    HDC DeviceContextHandle = ::GetWindowDC(hWnd);
                    if (DeviceContextHandle)
                    {
                        ::FillRect(
                            DeviceContextHandle,
                            &AnnoyingLineRect,
                            DarkModeBackgroundBrush);

                        ::ReleaseDC(hWnd, DeviceContextHandle);
                    }
                }
            }
        }

        return Result;
    }

    static LRESULT CALLBACK CallWndProcCallback(
        _In_ int nCode,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            PCWPSTRUCT WndProcStruct =
                reinterpret_cast<PCWPSTRUCT>(lParam);

            switch (WndProcStruct->message)
            {
            case WM_CREATE:
            case WM_INITDIALOG:
                wchar_t ClassName[256] = { 0 };
                if (0 != ::GetClassNameW(
                    WndProcStruct->hwnd,
                    ClassName,
                    256))
                {
                    if (!std::wcsstr(ClassName, L"Windows.UI.") &&
                        !std::wcsstr(ClassName, L"Mile.Xaml.") &&
                        !std::wcsstr(ClassName, L"Xaml_WindowedPopupClass"))
                    {
                        ::SetWindowSubclass(
                            WndProcStruct->hwnd,
                            ::WindowSubclassCallback,
                            0,
                            0);
                    }
                }
                break;
            }
        }

        return ::CallNextHookEx(
            nullptr,
            nCode,
            wParam,
            lParam);
    }

    static DWORD WINAPI OriginalGetSysColor(
        _In_ int nIndex)
    {
        return reinterpret_cast<decltype(::GetSysColor)*>(
            g_FunctionTable[FunctionType::GetSysColor].Original)(
                nIndex);
    }

    static DWORD WINAPI DetouredGetSysColor(
        _In_ int nIndex)
    {
        if (!g_ThreadInitialized || !g_ShouldAppsUseDarkMode)
        {
            return ::OriginalGetSysColor(nIndex);
        }

        switch (nIndex)
        {
        case COLOR_WINDOW:
        case COLOR_BTNFACE:
            return DarkModeBackgroundColor;
        case COLOR_WINDOWTEXT:
        case COLOR_BTNTEXT:
            return DarkModeForegroundColor;
        default:
            return ::OriginalGetSysColor(nIndex);
        }
    }

    static HBRUSH WINAPI OriginalGetSysColorBrush(
        _In_ int nIndex)
    {
        return reinterpret_cast<decltype(::GetSysColorBrush)*>(
            g_FunctionTable[FunctionType::GetSysColorBrush].Original)(
                nIndex);
    }

    static HBRUSH WINAPI DetouredGetSysColorBrush(
        _In_ int nIndex)
    {
        if (!g_ThreadInitialized || !g_ShouldAppsUseDarkMode)
        {
            return ::OriginalGetSysColorBrush(nIndex);
        }

        switch (nIndex)
        {
        case COLOR_BTNFACE:
            return DarkModeBackgroundBrush;
        case COLOR_BTNTEXT:
            return DarkModeForegroundBrush;
        default:
            return ::OriginalGetSysColorBrush(nIndex);
        }
    }

    static HRESULT WINAPI OriginalDrawThemeText(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCWSTR pszText,
        _In_ int cchText,
        _In_ DWORD dwTextFlags,
        _In_ DWORD dwTextFlags2,
        _In_ LPCRECT pRect)
    {
        return reinterpret_cast<decltype(::DrawThemeText)*>(
            g_FunctionTable[FunctionType::DrawThemeText].Original)(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pszText,
                cchText,
                dwTextFlags,
                dwTextFlags2,
                pRect);
    }

    static HRESULT WINAPI DetouredDrawThemeText(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCWSTR pszText,
        _In_ int cchText,
        _In_ DWORD dwTextFlags,
        _In_ DWORD dwTextFlags2,
        _In_ LPCRECT pRect)
    {
        if (!g_ThreadInitialized || !g_ShouldAppsUseDarkMode)
        {
            return ::OriginalDrawThemeText(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pszText,
                cchText,
                dwTextFlags,
                dwTextFlags2,
                pRect);
        }

        DTTOPTS TextOptions = { 0 };
        TextOptions.dwSize = sizeof(DTTOPTS);
        TextOptions.dwFlags = DTT_TEXTCOLOR;
        TextOptions.crText = DarkModeForegroundColor;

        return ::DrawThemeTextEx(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pszText,
            cchText,
            dwTextFlags,
            const_cast<LPRECT>(pRect),
            &TextOptions);
    }

    static HRESULT WINAPI OriginalDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        return reinterpret_cast<decltype(::DrawThemeBackground)*>(
            g_FunctionTable[FunctionType::DrawThemeBackground].Original)(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pClipRect);
    }

    static HRESULT WINAPI DetouredDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        if (!g_ThreadInitialized || !g_ShouldAppsUseDarkMode)
        {
            return ::OriginalDrawThemeBackground(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pClipRect);
        }

        if (hTheme == g_TabControlThemeHandle)
        {
            const int HoveredCheckStateId[] =
            {
                -1,
                TIS_HOT,
                TILES_HOT,
                TIRES_HOT,
                TIBES_HOT,
                TTIS_HOT,
                TTILES_HOT,
                TTIRES_HOT,
                TTIBES_HOT,
                -1,
                -1,
                -1
            };

            const int SelectedCheckStateId[] =
            {
                -1,
                TIS_SELECTED,
                TILES_SELECTED,
                TIRES_SELECTED,
                TIBES_SELECTED,
                TTIS_SELECTED,
                TTILES_SELECTED,
                TTIRES_SELECTED,
                TTIBES_SELECTED,
                -1,
                -1,
                -1
            };

            switch (iPartId)
            {
            case TABP_TABITEM:
            case TABP_TABITEMLEFTEDGE:
            case TABP_TABITEMRIGHTEDGE:
            case TABP_TABITEMBOTHEDGE:
            case TABP_TOPTABITEM:
            case TABP_TOPTABITEMLEFTEDGE:
            case TABP_TOPTABITEMRIGHTEDGE:
            case TABP_TOPTABITEMBOTHEDGE:
            {
                RECT paddedRect = *pRect;
                RECT insideRect =
                {
                    pRect->left + 1,
                    pRect->top + 1,
                    pRect->right - 1,
                    pRect->bottom - 1
                };

                if (iStateId == SelectedCheckStateId[iPartId])
                {
                    paddedRect.top += 1;
                    paddedRect.bottom -= 2;

                    // Allow the rect to overlap so the bottom border outline is removed
                    insideRect.top += 1;
                    insideRect.bottom += 1;
                }

                ::FrameRect(
                    hdc,
                    &paddedRect,
                    DarkModeBorderBrush);
                ::FillRect(
                    hdc,
                    &insideRect,
                    iStateId == HoveredCheckStateId[iPartId]
                    ? DarkModeBorderBrush
                    : DarkModeBackgroundBrush);

                return S_OK;
            }
            case TABP_PANE:
                return S_OK;
            default:
                break;
            }
        }
        else if (hTheme == g_StatusBarThemeHandle)
        {
            switch (iPartId)
            {
            case 0:
            {
                // Outside border (top, right)
                ::FillRect(hdc, pRect, DarkModeBorderBrush);
                return S_OK;
            }
            case SP_PANE:
            case SP_GRIPPERPANE:
            case SP_GRIPPER:
            {
                // Everything else
                ::FillRect(hdc, pRect, DarkModeBackgroundBrush);
                return S_OK;
            }
            default:
                break;
            }
        }

        return ::OriginalDrawThemeBackground(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pRect,
            pClipRect);
    }

    static HTHEME WINAPI OriginalOpenNcThemeData(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList)
    {
        return reinterpret_cast<decltype(OpenNcThemeData)*>(
            g_FunctionTable[FunctionType::OpenNcThemeData].Original)(
                hwnd,
                pszClassList);
    }

    static HTHEME WINAPI DetouredOpenNcThemeData(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList)
    {
        // Workaround for dark mode scrollbar
        if (0 == std::wcscmp(pszClassList, L"ScrollBar"))
        {
            return ::OriginalOpenNcThemeData(nullptr, L"Explorer::ScrollBar");
        }

        return ::OriginalOpenNcThemeData(hwnd, pszClassList);
    }
}

EXTERN_C VOID WINAPI NanaZipFrierenDarkModeThreadInitialize()
{
    if (g_ThreadInitialized)
    {
        return;
    }

    g_WindowsHookHandle = ::SetWindowsHookExW(
        WH_CALLWNDPROC,
        ::CallWndProcCallback,
        nullptr,
        ::GetCurrentThreadId());
    if (!g_WindowsHookHandle)
    {
        return;
    }

    g_ShouldAppsUseDarkMode =
        ::MileShouldAppsUseDarkMode() &&
        !::MileShouldAppsUseHighContrastMode();

    g_ThreadInitialized = true;
}

EXTERN_C VOID WINAPI NanaZipFrierenDarkModeThreadUninitialize()
{
    if (!g_ThreadInitialized)
    {
        return;
    }

    g_ThreadInitialized = false;

    ::UnhookWindowsHookEx(g_WindowsHookHandle);
    g_WindowsHookHandle = nullptr;
    g_ShouldAppsUseDarkMode = false;
}

EXTERN_C VOID WINAPI NanaZipFrierenDarkModeGlobalInitialize()
{
    ::MileAllowDarkModeForApp(true);
    ::MileRefreshImmersiveColorPolicyState();

    g_FunctionTable[FunctionType::GetSysColor].Original =
        ::GetSysColor;
    g_FunctionTable[FunctionType::GetSysColor].Detoured =
        ::DetouredGetSysColor;

    g_FunctionTable[FunctionType::GetSysColorBrush].Original =
        ::GetSysColorBrush;
    g_FunctionTable[FunctionType::GetSysColorBrush].Detoured =
        ::DetouredGetSysColorBrush;

    g_FunctionTable[FunctionType::DrawThemeText].Original =
        ::DrawThemeText;
    g_FunctionTable[FunctionType::DrawThemeText].Detoured =
        ::DetouredDrawThemeText;

    g_FunctionTable[FunctionType::DrawThemeBackground].Original =
        ::DrawThemeBackground;
    g_FunctionTable[FunctionType::DrawThemeBackground].Detoured =
        ::DetouredDrawThemeBackground;

    {
        HMODULE ModuleHandle = ::GetModuleHandleW(
            L"uxtheme.dll");
        if (ModuleHandle)
        {
            PVOID ProcAddress = ::GetProcAddress(
                ModuleHandle,
                MAKEINTRESOURCEA(49));
            if (ProcAddress)
            {
                g_FunctionTable[FunctionType::OpenNcThemeData].Original =
                    ProcAddress;
                g_FunctionTable[FunctionType::OpenNcThemeData].Detoured =
                    ::DetouredOpenNcThemeData;
            }
        }
    }  

    ::DetourTransactionBegin();
    ::DetourUpdateThread(::GetCurrentThread());
    for (size_t i = 0; i < FunctionType::MaximumFunction; ++i)
    {
        if (g_FunctionTable[i].Original)
        {
            ::DetourAttach(
                &g_FunctionTable[i].Original,
                g_FunctionTable[i].Detoured);
        }
    }
    ::DetourTransactionCommit();

    ::NanaZipFrierenDarkModeThreadInitialize();
}

EXTERN_C VOID WINAPI NanaZipFrierenDarkModeGlobalUninitialize()
{
    ::NanaZipFrierenDarkModeThreadUninitialize();

    ::DetourTransactionBegin();
    ::DetourUpdateThread(::GetCurrentThread());
    for (size_t i = 0; i < FunctionType::MaximumFunction; ++i)
    {
        if (g_FunctionTable[i].Original)
        {
            ::DetourDetach(
                &g_FunctionTable[i].Original,
                g_FunctionTable[i].Detoured);
            g_FunctionTable[i].Original = nullptr;
            g_FunctionTable[i].Detoured = nullptr;
        }
    }
    ::DetourTransactionCommit();
}
