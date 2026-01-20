/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Frieren.DarkMode.cpp
 * PURPOSE:    Implementation for NanaZip dark mode support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Mile.Helpers.h>
#include <Mile.Helpers.CppBase.h>

#include <K7Base.h>

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

#include "K7UserPrivate.h"

// TODO: Move some workaround for NanaZip.UI.* to this.

namespace
{
    const COLORREF g_LightModeBackgroundColor = RGB(255, 255, 255);
    const COLORREF g_LightModeForegroundColor = RGB(0, 0, 0);

    const COLORREF g_DarkModeBackgroundColor = RGB(0, 0, 0);
    const COLORREF g_DarkModeForegroundColor = RGB(255, 255, 255);
    const COLORREF g_DarkModeBorderColor = RGB(127, 127, 127);
    const COLORREF g_DarkModeMenuSelectedBackgroundColor = RGB(65, 65, 65);

    static HBRUSH GetDarkModeBackgroundBrush()
    {
        static HBRUSH CachedResult =
            ::CreateSolidBrush(g_DarkModeBackgroundColor);
        return CachedResult;
    }

    static HBRUSH GetDarkModeForegroundBrush()
    {
        static HBRUSH CachedResult =
            ::CreateSolidBrush(g_DarkModeForegroundColor);
        return CachedResult;
    }

    static HBRUSH GetDarkModeBorderBrush()
    {
        static HBRUSH CachedResult =
            ::CreateSolidBrush(g_DarkModeBorderColor);
        return CachedResult;
    }

    static HBRUSH GetDarkModeMenuSelectedBackgroundBrush()
    {
        static HBRUSH CachedResult =
            ::CreateSolidBrush(g_DarkModeMenuSelectedBackgroundColor);
        return CachedResult;
    }

    static bool IsStandardDynamicRangeMode()
    {
        static bool CachedResult = ([]() -> bool
        {
            bool Result = true;

            UINT32 NumPathArrayElements = 0;
            UINT32 NumModeInfoArrayElements = 0;
            if (ERROR_SUCCESS == ::GetDisplayConfigBufferSizes(
                QDC_ONLY_ACTIVE_PATHS,
                &NumPathArrayElements,
                &NumModeInfoArrayElements))
            {
                std::vector<DISPLAYCONFIG_PATH_INFO> PathArray(
                    NumPathArrayElements);
                std::vector<DISPLAYCONFIG_MODE_INFO> ModeInfoArray(
                    NumModeInfoArrayElements);
                if (ERROR_SUCCESS == ::QueryDisplayConfig(
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
                        if (ERROR_SUCCESS == ::DisplayConfigGetDeviceInfo(
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

    static volatile bool g_GlobalInitialized = false;

    static LRESULT CALLBACK CallWndProcCallback(
        _In_ int nCode,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam);

    struct ThreadContext
    {
    public:

        bool volatile MicaBackdropAvailable = false;
        HHOOK volatile WindowsHookHandle = nullptr;
        bool volatile ShouldAppsUseDarkMode = false;
        HTHEME TabControlThemeHandle = nullptr;
        HTHEME StatusBarThemeHandle = nullptr;

    public:

        ThreadContext()
        {
            this->WindowsHookHandle = ::SetWindowsHookExW(
                WH_CALLWNDPROC,
                ::CallWndProcCallback,
                nullptr,
                ::GetCurrentThreadId());
            if (!this->WindowsHookHandle)
            {
                return;
            }

            this->ShouldAppsUseDarkMode =
                ::MileShouldAppsUseDarkMode() &&
                !::MileShouldAppsUseHighContrastMode();
        }

        ~ThreadContext()
        {
            ::UnhookWindowsHookEx(this->WindowsHookHandle);
            this->WindowsHookHandle = nullptr;

            this->ShouldAppsUseDarkMode = false;
        }
    };
    thread_local ThreadContext g_ThreadContext;

    static void RefreshWindowTheme(
        _In_ HWND WindowHandle)
    {
        wchar_t ClassName[256] = {};
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

                if (g_ThreadContext.ShouldAppsUseDarkMode)
                {
                    ListView_SetTextBkColor(
                        WindowHandle,
                        g_DarkModeBackgroundColor);
                    ListView_SetBkColor(
                        WindowHandle,
                        g_DarkModeBackgroundColor);
                    ListView_SetTextColor(
                        WindowHandle,
                        g_DarkModeForegroundColor);
                }
                else
                {
                    ListView_SetTextBkColor(
                        WindowHandle,
                        g_LightModeBackgroundColor);
                    ListView_SetBkColor(
                        WindowHandle,
                        g_LightModeBackgroundColor);
                    ListView_SetTextColor(
                        WindowHandle,
                        g_LightModeForegroundColor);
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
                    if (g_ThreadContext.ShouldAppsUseDarkMode)
                    {
                        ColorScheme.clrBtnHighlight = g_DarkModeBackgroundColor;
                        ColorScheme.clrBtnShadow = g_DarkModeBackgroundColor;
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
        wchar_t ClassName[256] = {};
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
            if (g_ThreadContext.ShouldAppsUseDarkMode)
            {
                HDC DeviceContextHandle = reinterpret_cast<HDC>(wParam);
                if (DeviceContextHandle)
                {
                    ::SetTextColor(
                        DeviceContextHandle,
                        g_DarkModeForegroundColor);
                    ::SetBkColor(
                        DeviceContextHandle,
                        g_DarkModeBackgroundColor);
                }

                return reinterpret_cast<INT_PTR>(
                    ::GetDarkModeBackgroundBrush());
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

                g_ThreadContext.ShouldAppsUseDarkMode =
                    ::MileShouldAppsUseDarkMode() &&
                    !::MileShouldAppsUseHighContrastMode();

                ::MileEnableImmersiveDarkModeForWindow(
                    hWnd,
                    g_ThreadContext.ShouldAppsUseDarkMode);

                bool ShouldExtendFrame = (
                    g_ThreadContext.ShouldAppsUseDarkMode &&
                    ::IsStandardDynamicRangeMode() &&
                    g_ThreadContext.MicaBackdropAvailable);
                MARGINS Margins = {};
                if (ShouldExtendFrame)
                {
                    Margins = { -1 };
                }
                else if (::IsFileManagerWindow(hWnd))
                {
                    UINT DpiValue = ::GetDpiForWindow(hWnd);
                    Margins.cyTopHeight =
                        ::MulDiv(80, DpiValue, USER_DEFAULT_SCREEN_DPI);
                    Margins.cyBottomHeight =
                        ::MulDiv(32, DpiValue, USER_DEFAULT_SCREEN_DPI);
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

            g_ThreadContext.MicaBackdropAvailable =
                (S_OK == ::MileEnableImmersiveDarkModeForWindow(
                    hWnd,
                    g_ThreadContext.ShouldAppsUseDarkMode));

            bool ShouldExtendFrame = (
                g_ThreadContext.ShouldAppsUseDarkMode &&
                ::IsStandardDynamicRangeMode() &&
                g_ThreadContext.MicaBackdropAvailable);
            if (ShouldExtendFrame)
            {
                MARGINS Margins = { -1 };
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
            }
            else if (::IsFileManagerWindow(hWnd))
            {
                UINT DpiValue = ::GetDpiForWindow(hWnd);

                MARGINS Margins = {};
                Margins.cyTopHeight =
                    ::MulDiv(80, DpiValue, USER_DEFAULT_SCREEN_DPI);
                Margins.cyBottomHeight =
                    ::MulDiv(32, DpiValue, USER_DEFAULT_SCREEN_DPI);
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
            }

            ::RefreshWindowTheme(hWnd);

            wchar_t ClassName[256] = {};
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
                    g_ThreadContext.TabControlThemeHandle =
                        ::GetWindowTheme(hWnd);
                }
                else if (0 == std::wcscmp(ClassName, STATUSCLASSNAMEW))
                {
                    g_ThreadContext.StatusBarThemeHandle =
                        ::GetWindowTheme(hWnd);
                }
            }

            break;
        }
        case WM_DPICHANGED:
        {
            bool ShouldExtendFrame = (
                g_ThreadContext.ShouldAppsUseDarkMode &&
                ::IsStandardDynamicRangeMode() &&
                g_ThreadContext.MicaBackdropAvailable);
            if (!ShouldExtendFrame && ::IsFileManagerWindow(hWnd))
            {
                UINT DpiValue = ::GetDpiForWindow(hWnd);

                MARGINS Margins = {};
                Margins.cyTopHeight =
                    ::MulDiv(80, DpiValue, USER_DEFAULT_SCREEN_DPI);
                Margins.cyBottomHeight =
                    ::MulDiv(32, DpiValue, USER_DEFAULT_SCREEN_DPI);
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
            }

            break;
        }
        default:
            break;
        }

        if (g_ThreadContext.ShouldAppsUseDarkMode && ::GetMenu(hWnd))
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
                        RECT WindowRect = {};
                        ::GetWindowRect(hWnd, &WindowRect);

                        RECT MenuRect = MenuBarInfo.rcBar;
                        ::OffsetRect(
                            &MenuRect,
                            -WindowRect.left,
                            -WindowRect.top);

                        ::FillRect(
                            UahMenu->hdc,
                            &MenuRect,
                            ::GetDarkModeBackgroundBrush());
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
                        wchar_t Buffer[256] = {};
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
                            COLORREF TextColor = g_DarkModeForegroundColor;
                            HBRUSH BackgroundBrush =
                                ::GetDarkModeBackgroundBrush();
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
                                BackgroundBrush =
                                    ::GetDarkModeMenuSelectedBackgroundBrush();
                            }
                            else
                            {
                                StateId = MBI_NORMAL;
                            }

                            ::FillRect(
                                DrawItemStruct->hDC,
                                &DrawItemStruct->rcItem,
                                BackgroundBrush);

                            // We have to specify the text colour explicitly as
                            // by default black would be used, making the menu
                            // label unreadable on the (almost) black
                            // background.
                            DTTOPTS TextOptions = {};
                            TextOptions.dwSize = sizeof(DTTOPTS);
                            TextOptions.dwFlags = DTT_TEXTCOLOR;
                            TextOptions.crText = TextColor;

                            DWORD TextFlags =
                                DT_CENTER | DT_VCENTER | DT_SINGLELINE;
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
                    RECT ClientRect = {};
                    ::GetClientRect(hWnd, &ClientRect);

                    ::MapWindowPoints(
                        hWnd,
                        nullptr,
                        reinterpret_cast<PPOINT>(&ClientRect),
                        2);

                    RECT WindowRect = {};
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
                            ::GetDarkModeBackgroundBrush());

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
        if (g_GlobalInitialized && nCode == HC_ACTION)
        {
            PCWPSTRUCT WndProcStruct =
                reinterpret_cast<PCWPSTRUCT>(lParam);

            switch (WndProcStruct->message)
            {
            case WM_CREATE:
            case WM_INITDIALOG:
                wchar_t ClassName[256] = {};
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

    namespace FunctionTypes
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

    FunctionItem g_FunctionTable[FunctionTypes::MaximumFunction];

    static DWORD WINAPI OriginalGetSysColor(
        _In_ int nIndex)
    {
        return reinterpret_cast<decltype(::GetSysColor)*>(
            g_FunctionTable[FunctionTypes::GetSysColor].Original)(
                nIndex);
    }

    static HBRUSH WINAPI OriginalGetSysColorBrush(
        _In_ int nIndex)
    {
        return reinterpret_cast<decltype(::GetSysColorBrush)*>(
            g_FunctionTable[FunctionTypes::GetSysColorBrush].Original)(
                nIndex);
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
            g_FunctionTable[FunctionTypes::DrawThemeText].Original)(
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

    static HRESULT WINAPI OriginalDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        return reinterpret_cast<decltype(::DrawThemeBackground)*>(
            g_FunctionTable[FunctionTypes::DrawThemeBackground].Original)(
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
            g_FunctionTable[FunctionTypes::OpenNcThemeData].Original)(
                hwnd,
                pszClassList);
    }

    static DWORD WINAPI DetouredGetSysColor(
        _In_ int nIndex)
    {
        if (!g_GlobalInitialized || !g_ThreadContext.ShouldAppsUseDarkMode)
        {
            return ::OriginalGetSysColor(nIndex);
        }

        switch (nIndex)
        {
        case COLOR_WINDOW:
        case COLOR_BTNFACE:
            return g_DarkModeBackgroundColor;
        case COLOR_WINDOWTEXT:
        case COLOR_BTNTEXT:
            return g_DarkModeForegroundColor;
        default:
            return ::OriginalGetSysColor(nIndex);
        }
    }

    static HBRUSH WINAPI DetouredGetSysColorBrush(
        _In_ int nIndex)
    {
        if (!g_GlobalInitialized || !g_ThreadContext.ShouldAppsUseDarkMode)
        {
            return ::OriginalGetSysColorBrush(nIndex);
        }

        switch (nIndex)
        {
        case COLOR_BTNFACE:
            return ::GetDarkModeBackgroundBrush();
        case COLOR_BTNTEXT:
            return ::GetDarkModeForegroundBrush();
        default:
            return ::OriginalGetSysColorBrush(nIndex);
        }
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
        if (!g_GlobalInitialized || !g_ThreadContext.ShouldAppsUseDarkMode)
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

        DTTOPTS TextOptions = {};
        TextOptions.dwSize = sizeof(DTTOPTS);
        TextOptions.dwFlags = DTT_TEXTCOLOR;
        TextOptions.crText = g_DarkModeForegroundColor;

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

    static HRESULT WINAPI DetouredDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        if (!g_GlobalInitialized || !g_ThreadContext.ShouldAppsUseDarkMode)
        {
            return ::OriginalDrawThemeBackground(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pClipRect);
        }

        if (hTheme == g_ThreadContext.TabControlThemeHandle)
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
                    ::GetDarkModeBorderBrush());
                ::FillRect(
                    hdc,
                    &insideRect,
                    iStateId == HoveredCheckStateId[iPartId]
                    ? ::GetDarkModeBorderBrush()
                    : ::GetDarkModeBackgroundBrush());

                return S_OK;
            }
            case TABP_PANE:
                return S_OK;
            default:
                break;
            }
        }
        else if (hTheme == g_ThreadContext.StatusBarThemeHandle)
        {
            switch (iPartId)
            {
            case 0:
            {
                // Outside border (top, right)
                ::FillRect(hdc, pRect, ::GetDarkModeBorderBrush());
                return S_OK;
            }
            case SP_PANE:
            case SP_GRIPPERPANE:
            case SP_GRIPPER:
            {
                // Everything else
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
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

    static bool InitializeFunctionTable()
    {
        g_FunctionTable[FunctionTypes::GetSysColor].Original =
            ::GetSysColor;
        g_FunctionTable[FunctionTypes::GetSysColor].Detoured =
            ::DetouredGetSysColor;

        g_FunctionTable[FunctionTypes::GetSysColorBrush].Original =
            ::GetSysColorBrush;
        g_FunctionTable[FunctionTypes::GetSysColorBrush].Detoured =
            ::DetouredGetSysColorBrush;

        g_FunctionTable[FunctionTypes::DrawThemeText].Original =
            ::DrawThemeText;
        g_FunctionTable[FunctionTypes::DrawThemeText].Detoured =
            ::DetouredDrawThemeText;

        g_FunctionTable[FunctionTypes::DrawThemeBackground].Original =
            ::DrawThemeBackground;
        g_FunctionTable[FunctionTypes::DrawThemeBackground].Detoured =
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
                    g_FunctionTable[FunctionTypes::OpenNcThemeData].Original =
                        ProcAddress;
                    g_FunctionTable[FunctionTypes::OpenNcThemeData].Detoured =
                        ::DetouredOpenNcThemeData;
                }
            }
        }

        return true;
    }

    static void UninitializeFunctionTable()
    {
        for (size_t i = 0; i < FunctionTypes::MaximumFunction; ++i)
        {
            g_FunctionTable[i].Original = nullptr;
            g_FunctionTable[i].Detoured = nullptr;
        }
    }
}

/**
 * @brief Initializes the dark mode support.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7UserInitializeDarkModeSupport()
{
    if (g_GlobalInitialized)
    {
        return MO_RESULT_SUCCESS_OK;
    }

    if (!::InitializeFunctionTable())
    {
        return MO_RESULT_ERROR_FAIL;
    }

    ::MileAllowDarkModeForApp(TRUE);
    ::MileRefreshImmersiveColorPolicyState();

    ::K7BaseDetourTransactionBegin();
    ::K7BaseDetourUpdateThread(::GetCurrentThread());
    for (size_t i = 0; i < FunctionTypes::MaximumFunction; ++i)
    {
        if (g_FunctionTable[i].Original &&
            g_FunctionTable[i].Detoured)
        {
            if (NO_ERROR != ::K7BaseDetourAttach(
                &g_FunctionTable[i].Original,
                g_FunctionTable[i].Detoured))
            {
                ::K7BaseDetourTransactionAbort();
                ::UninitializeFunctionTable();
                return MO_RESULT_ERROR_FAIL;
            }
        }
    }
    ::K7BaseDetourTransactionCommit();

    g_GlobalInitialized = true;

    return MO_RESULT_SUCCESS_OK;
}

/**
 * @brief Uninitializes the dark mode support.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7UserUninitializeDarkModeSupport()
{
    if (!g_GlobalInitialized)
    {
        return MO_RESULT_SUCCESS_OK;
    }
    g_GlobalInitialized = false;

    ::MileAllowDarkModeForApp(FALSE);
    ::MileRefreshImmersiveColorPolicyState();

    ::K7BaseDetourTransactionBegin();
    ::K7BaseDetourUpdateThread(::GetCurrentThread());
    for (size_t i = 0; i < FunctionTypes::MaximumFunction; ++i)
    {
        if (g_FunctionTable[i].Original &&
            g_FunctionTable[i].Detoured)
        {
            if (NO_ERROR != ::K7BaseDetourDetach(
                &g_FunctionTable[i].Original,
                g_FunctionTable[i].Detoured))
            {
                ::K7BaseDetourTransactionAbort();
                return MO_RESULT_ERROR_FAIL;
            }
        }
    }
    ::K7BaseDetourTransactionCommit();

    ::UninitializeFunctionTable();

    return MO_RESULT_SUCCESS_OK;
}
