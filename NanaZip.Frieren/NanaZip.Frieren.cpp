/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Frieren.cpp
 * PURPOSE:   Implementation for NanaZip.Frieren
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Frieren.h"

#include <Mile.Helpers.h>
#include <Mile.Helpers.CppBase.h>

#include <Detours.h>

#include <Uxtheme.h>
#include <vssym32.h>
#include <Richedit.h>

#include <unordered_map>

// Reference: https://github.com/adzm/win32-custom-menubar-aero-theme

// window messages related to menu bar drawing
#define WM_UAHDESTROYWINDOW    0x0090	// handled by DefWindowProc
#define WM_UAHDRAWMENU         0x0091	// lParam is UAHMENU
#define WM_UAHDRAWMENUITEM     0x0092	// lParam is UAHDRAWMENUITEM
#define WM_UAHINITMENU         0x0093	// handled by DefWindowProc
#define WM_UAHMEASUREMENUITEM  0x0094	// lParam is UAHMEASUREMENUITEM
#define WM_UAHNCPAINTMENUPOPUP 0x0095	// handled by DefWindowProc

// describes the sizes of the menu bar or menu item
typedef union tagUAHMENUITEMMETRICS
{
    // cx appears to be 14 / 0xE less than rcItem's width!
    // cy 0x14 seems stable, i wonder if it is 4 less than rcItem's height which
    //    is always 24 atm
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizeBar[2];
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizePopup[4];
} UAHMENUITEMMETRICS;

// not really used in our case but part of the other structures
typedef struct tagUAHMENUPOPUPMETRICS
{
    DWORD rgcx[4];

    // from kernel symbols, padded to full dword
    DWORD fUpdateMaxWidths : 2;
} UAHMENUPOPUPMETRICS;

// hmenu is the main window menu; hdc is the context to draw in
typedef struct tagUAHMENU
{
    HMENU hmenu;
    HDC hdc;
    // no idea what these mean, in my testing it's either 0x00000a00 or
    // sometimes 0x00000a10
    DWORD dwFlags;
} UAHMENU;

// menu items are always referred to by iPosition here
typedef struct tagUAHMENUITEM
{
    int iPosition; // 0-based position of menu item in menubar
    UAHMENUITEMMETRICS umim;
    UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM;

// the DRAWITEMSTRUCT contains the states of the menu items, as well as
// the position index of the item in the menu, which is duplicated in
// the UAHMENUITEM's iPosition as well
typedef struct UAHDRAWMENUITEM
{
    DRAWITEMSTRUCT dis; // itemID looks uninitialized
    UAHMENU um;
    UAHMENUITEM umi;
} UAHDRAWMENUITEM;

// the MEASUREITEMSTRUCT is intended to be filled with the size of the item
// height appears to be ignored, but width can be modified
typedef struct tagUAHMEASUREMENUITEM
{
    MEASUREITEMSTRUCT mis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHMEASUREMENUITEM;

namespace
{
    namespace FunctionType
    {
        enum
        {
            GetSysColor,
            GetSysColorBrush,
            DrawThemeText,
            DrawThemeBackground,

            MaximumFunction
        };
    }

    struct FunctionItem
    {
        PVOID Original;
        PVOID Detoured;
    };

    FunctionItem g_FunctionTable[FunctionType::MaximumFunction];

    const COLORREF DarkModeBackgroundColor = RGB(56, 56, 56);
    const COLORREF DarkModeForegroundColor = RGB(255, 255, 255);

    thread_local bool volatile g_ThreadInitialized = false;
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
            else if (0 == std::wcscmp(ClassName, WC_COMBOBOXW))
            {
                ::SetWindowTheme(WindowHandle, L"CFD", nullptr);
            }
            else if (0 == std::wcscmp(ClassName, WC_HEADERW))
            {
                ::SetWindowTheme(WindowHandle, L"ItemsView", nullptr);
            }
            else if (0 == std::wcscmp(ClassName, WC_LISTVIEWW))
            {
                ::SetWindowTheme(WindowHandle, L"Explorer", nullptr);

                HTHEME ThemeHandle = ::OpenThemeData(nullptr, L"ItemsView");
                if (ThemeHandle)
                {
                    COLORREF BackgroundColor = 0;
                    if (SUCCEEDED(::GetThemeColor(
                        ThemeHandle,
                        0,
                        0,
                        TMT_FILLCOLOR,
                        &BackgroundColor)))
                    {
                        ListView_SetTextBkColor(
                            WindowHandle,
                            BackgroundColor);
                        ListView_SetBkColor(
                            WindowHandle,
                            BackgroundColor);
                    }
                    else
                    {
                        ListView_SetTextBkColor(
                            WindowHandle,
                            DarkModeBackgroundColor);
                        ListView_SetBkColor(
                            WindowHandle,
                            DarkModeBackgroundColor);
                    }

                    COLORREF ForegroundColor = 0;
                    if (SUCCEEDED(::GetThemeColor(
                        ThemeHandle,
                        0,
                        0,
                        TMT_TEXTCOLOR,
                        &ForegroundColor)))
                    {
                        ListView_SetTextColor(
                            WindowHandle,
                            ForegroundColor);
                    }
                    else {
                        ListView_SetTextColor(
                            WindowHandle,
                            DarkModeForegroundColor);
                    }

                    ::CloseThemeData(ThemeHandle);
                }
            }
            else if (0 == std::wcscmp(ClassName, STATUSCLASSNAMEW))
            {
                // Do nothing.
            }
            else if (0 == std::wcscmp(ClassName, WC_TABCONTROLW))
            {
                // Do nothing.
            }
            else
            {
                if (0 == std::wcscmp(ClassName, TOOLBARCLASSNAMEW))
                {
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

    LRESULT CALLBACK WindowSubclassCallback(
        _In_ HWND hWnd,
        _In_ UINT uMsg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam,
        _In_ UINT_PTR uIdSubclass,
        _In_ DWORD_PTR dwRefData)
    {
        uIdSubclass;
        dwRefData;

        static HBRUSH DarkModeBackgroundBrush =
            ::CreateSolidBrush(DarkModeBackgroundColor);

        switch (uMsg)
        {
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
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
            ::MileEnableImmersiveDarkModeForWindow(
                hWnd,
                g_ShouldAppsUseDarkMode);
            ::MileAllowDarkModeForWindow(
                hWnd,
                TRUE);

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
        default:
            break;
        }

        if (g_ShouldAppsUseDarkMode && ::GetMenu(hWnd))
        {
            if (WM_UAHDRAWMENU == uMsg)
            {
                UAHMENU* UahMenu = reinterpret_cast<UAHMENU*>(lParam);
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
                UAHDRAWMENUITEM* UahDrawMenuItem =
                    reinterpret_cast<UAHDRAWMENUITEM*>(lParam);
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
                            DTTOPTS TextOptions;
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
                        !std::wcsstr(ClassName, L"Mile.Xaml."))
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

        static HBRUSH BackgroundBrush = ::CreateSolidBrush(
            ::GetSysColor(COLOR_BTNFACE));
        static HBRUSH ForegroundBrush = ::CreateSolidBrush(
            ::GetSysColor(COLOR_BTNTEXT));

        switch (nIndex)
        {
        case COLOR_BTNFACE:
            return BackgroundBrush;
        case COLOR_BTNTEXT:
            return ForegroundBrush;
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

        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, DarkModeForegroundColor);
        ::DrawTextW(
            hdc,
            pszText,
            cchText,
            const_cast<LPRECT>(pRect),
            dwTextFlags);
        return S_OK;
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
            static HBRUSH TabControlButtonBorderBrush =
                ::CreateSolidBrush(RGB(127, 127, 127));
            static HBRUSH TabControlButtonFillBrush =
                ::CreateSolidBrush(RGB(56, 56, 56));

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
                    TabControlButtonBorderBrush);
                ::FillRect(
                    hdc,
                    &insideRect,
                    iStateId == HoveredCheckStateId[iPartId]
                    ? TabControlButtonBorderBrush
                    : TabControlButtonFillBrush);

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
            static HBRUSH StatusBarBorderBrush =
                ::CreateSolidBrush(RGB(127, 127, 127));
            static HBRUSH StatusBarFillBrush =
                ::CreateSolidBrush(RGB(56, 56, 56));

            switch (iPartId)
            {
            case 0:
            {
                // Outside border (top, right)
                ::FillRect(hdc, pRect, StatusBarBorderBrush);
                return S_OK;
            }
            case SP_PANE:
            case SP_GRIPPERPANE:
            case SP_GRIPPER:
            {
                // Everything else
                ::FillRect(hdc, pRect, StatusBarFillBrush);
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
}

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadInitialize()
{
    if (g_ThreadInitialized)
    {
        return S_OK;
    }

    g_WindowsHookHandle = ::SetWindowsHookExW(
        WH_CALLWNDPROC,
        ::CallWndProcCallback,
        nullptr,
        ::GetCurrentThreadId());
    if (!g_WindowsHookHandle)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    g_ShouldAppsUseDarkMode =
        ::MileShouldAppsUseDarkMode() &&
        !::MileShouldAppsUseHighContrastMode();

    g_ThreadInitialized = true;

    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadUninitialize()
{
    if (!g_ThreadInitialized)
    {
        return S_OK;
    }

    g_ThreadInitialized = false;

    ::UnhookWindowsHookEx(g_WindowsHookHandle);
    g_WindowsHookHandle = nullptr;
    g_ShouldAppsUseDarkMode = false;

    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenGlobalInitialize()
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

    ::NanaZipFrierenThreadInitialize();

    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenGlobalUninitialize()
{
    ::NanaZipFrierenThreadUninitialize();

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

    return S_OK;
}
