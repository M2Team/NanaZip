/*
 * PROJECT:    NanaZip Platform User Library (K7User)
 * FILE:       K7UserDarkMode.cpp
 * PURPOSE:    Implementation for NanaZip Platform User Dark Mode Support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7UserPrivate.h"

#include <Mile.Helpers.h>
#include <Mile.Helpers.CppBase.h>

#include <K7Base.h>

#include <Uxtheme.h>
#pragma comment(lib, "Uxtheme.lib")

EXTERN_C HTHEME WINAPI OpenNcThemeData(
    _In_opt_ HWND hwnd,
    _In_ LPCWSTR pszClassList);

EXTERN_C HRESULT WINAPI GetThemeClass(
    _In_ HTHEME hTheme,
    _Out_ LPWSTR pszClassName,
    _In_ int cchClassName);

#include <vssym32.h>
#include <Richedit.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <ShellScalingApi.h>

#include <CommCtrl.h>
#pragma comment(lib,"comctl32.lib")

// TODO: Move some workaround for NanaZip.UI.* to this.

namespace
{
    const COLORREF g_LightModeBackgroundColor = RGB(255, 255, 255);
    const COLORREF g_LightModeForegroundColor = RGB(0, 0, 0);

    // The dark background must match the XAML islands' dark theme surface
    // (DarkSolidBackgroundFillColorDefault, #202020). Pure black desynchronizes
    // the classic controls from the XAML surfaces and renders the whole
    // window as an unreadable black void.
    const COLORREF g_DarkModeBackgroundColor = RGB(0x20, 0x20, 0x20);
    const COLORREF g_DarkModeForegroundColor = RGB(255, 255, 255);
    const COLORREF g_DarkModeBorderColor = RGB(127, 127, 127);
    const COLORREF g_DarkModeMenuSelectedBackgroundColor = RGB(65, 65, 65);

    // Whether the user enabled "Invert Theme" for the currently applied
    // policy. This distinguishes an application that is dark because it
    // follows a dark system (native dark; the system draws menus, glyphs
    // and dialogs by itself) from one that is forced dark against a light
    // system (inverted dark; the system hands out light theme data and the
    // workarounds below are required).
    static volatile LONG g_ThemeInverted = 0;

    static bool IsThemeInverted()
    {
        return (0 != g_ThemeInverted);
    }

    static void SetThemeInverted(
        _In_ bool Value)
    {
        ::InterlockedExchange(&g_ThemeInverted, Value ? 1 : 0);
    }

    // >0 while a native system dialog (IFileOpenDialog) owns the theme.
    // During such a scope the process follows the SYSTEM appearance and
    // every inversion-only intervention (class redirects, deferred theme
    // application, ...) is bypassed so system windows render natively.
    static volatile LONG g_NativeThemeSuspendCounter = 0;

    static bool IsNativeThemeSuspended()
    {
        return (0 != g_NativeThemeSuspendCounter);
    }

    static bool K7UserReadThemeInvert()
    {
        // The "Invert Theme" option is exposed by the File Manager settings and
        // is stored as a REG_DWORD under HKCU\Software\NanaZip\FM\InvertTheme.
        DWORD Value = 0;
        DWORD ValueSize = sizeof(Value);

        HKEY KeyHandle = nullptr;
        if (ERROR_SUCCESS == ::RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\NanaZip\\FM",
            0,
            KEY_READ,
            &KeyHandle))
        {
            if (ERROR_SUCCESS != ::RegQueryValueExW(
                KeyHandle,
                L"InvertTheme",
                nullptr,
                nullptr,
                reinterpret_cast<LPBYTE>(&Value),
                &ValueSize))
            {
                Value = 0;
            }
            ::RegCloseKey(KeyHandle);
        }

        return (Value != 0);
    }

    static bool ComputeShouldAppsUseDarkMode()
    {
        const bool SystemShouldUseDarkMode =
            ::MileShouldAppsUseDarkMode() &&
            !::MileShouldAppsUseHighContrastMode();
        const bool ThemeInverted = ::K7UserReadThemeInvert();

        // Publish the inversion state together with the effective theme so
        // the detours and window subclasses can tell native dark apart from
        // inverted dark.
        ::SetThemeInverted(ThemeInverted);

        return ThemeInverted ? !SystemShouldUseDarkMode
                             : SystemShouldUseDarkMode;
    }

    // uxtheme ordinal 135 (SetPreferredAppMode). The Mile headers available
    // in some build environments don't expose MILE_PREFERRED_APP_MODE_FORCE_DARK
    // (enum values can't be guarded with #ifndef), so the undocumented export
    // is called directly. This is the same export MileSetPreferredAppMode
    // wraps; value 2 (ForceDark) is guaranteed by the uxtheme contract.
    enum class K7PreferredAppMode : int
    {
        Default = 0,
        AllowDark = 1,
        ForceDark = 2,
        ForceLight = 3,
        Max = 4,
    };

    using K7SetPreferredAppModeType =
        K7PreferredAppMode(WINAPI*)(K7PreferredAppMode);

    static void K7SetPreferredAppMode(_In_ K7PreferredAppMode Mode)
    {
        static K7SetPreferredAppModeType Cached =
            []() -> K7SetPreferredAppModeType
        {
            HMODULE Uxtheme = ::GetModuleHandleW(L"uxtheme.dll");
            if (!Uxtheme)
            {
                return nullptr;
            }
            return reinterpret_cast<K7SetPreferredAppModeType>(
                ::GetProcAddress(Uxtheme, MAKEINTRESOURCEA(135)));
        }();
        if (Cached)
        {
            Cached(Mode);
        }
    }

    static void ApplyProcessThemePolicy(
        _In_ bool ShouldUseDarkMode)
    {
        if (!::IsThemeInverted())
        {
            // Follow the system appearance exactly (the upstream behavior).
            // Do NOT force an app mode here. Forcing Dark even when the
            // system is already dark diverts the native rendering path for
            // popup menus and system dialogs, which hides the system
            // check/radio glyphs ("View" menu) and pollutes native dialogs.
            // Resetting to Default also undoes a force from a previous
            // in-session inversion toggle.
            ::K7SetPreferredAppMode(K7PreferredAppMode::Default);
        }
        else
        {
            // The application deliberately renders opposite to the system.
            // System components which read the uxtheme ShouldAppsUseDarkMode
            // export directly only honor the forced modes, so the opposite
            // mode has to be forced process-wide.
            // ForceDark: a light system with the inverted theme enabled.
            // ForceLight: a dark system with the inverted theme enabled
            // (otherwise classic controls keep dark data on the light XAML
            // surfaces, producing unreadable black bars such as the header).
            ::K7SetPreferredAppMode(
                ShouldUseDarkMode
                    ? K7PreferredAppMode::ForceDark
                    : K7PreferredAppMode::ForceLight);
        }
        ::MileRefreshImmersiveColorPolicyState();
    }

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

    static void ApplyWindowSystemBackdrop(
        _In_ HWND hWnd,
        _In_ bool ShouldExtendFrame)
    {
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_HOSTBACKDROPBRUSH
#define DWMWA_USE_HOSTBACKDROPBRUSH 17
#endif
#ifndef DWMWA_HOSTBACKDROPBRUSH
#define DWMWA_HOSTBACKDROPBRUSH 18
#endif
        // The XAML islands intentionally leave regions transparent (e.g. the
        // address bar background and the gap between the island bottom and
        // the extended frame margin). Those regions compose the DWM backdrop,
        // which always follows the SYSTEM theme: Mica leaked dark scenery
        // into the light application as an opaque black bar on dark-theme
        // systems, and DWMSBT_NONE composes the same regions as pure black.
        // Replace the system backdrop with a host backdrop brush that always
        // matches the application theme instead.
        INT BackdropType = 2; // DWMSBT_NONE
        ::DwmSetWindowAttribute(
            hWnd,
            DWMWA_SYSTEMBACKDROP_TYPE,
            &BackdropType,
            sizeof(BackdropType));
        BOOL UseHostBackdrop = TRUE;
        ::DwmSetWindowAttribute(
            hWnd,
            DWMWA_USE_HOSTBACKDROPBRUSH,
            &UseHostBackdrop,
            sizeof(UseHostBackdrop));
        static const HBRUSH DarkHostBackdropBrush =
            ::CreateSolidBrush(g_DarkModeBackgroundColor);
        static const HBRUSH LightHostBackdropBrush =
            ::CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
        HBRUSH HostBackdropBrush = (ShouldExtendFrame
            ? DarkHostBackdropBrush
            : LightHostBackdropBrush);
        ::DwmSetWindowAttribute(
            hWnd,
            DWMWA_HOSTBACKDROPBRUSH,
            &HostBackdropBrush,
            sizeof(HostBackdropBrush));
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

    // The uxtheme preferred app mode policy applied by ApplyProcessThemePolicy
    // is process-wide, so the dark mode state consulted by the detours and
    // the window subclasses has to be process-wide too. A per-thread mirror
    // goes stale on threads which never process a theme change notification
    // and then renders with the wrong (light) colors even after the theme
    // was switched to dark.
    static volatile LONG g_ShouldAppsUseDarkMode = 0;

    static bool ShouldAppsUseDarkMode()
    {
        return (0 != g_ShouldAppsUseDarkMode);
    }

    static void SetShouldAppsUseDarkMode(_In_ bool Value)
    {
        ::InterlockedExchange(
            &g_ShouldAppsUseDarkMode,
            Value ? 1 : 0);
    }

    static LRESULT CALLBACK CallWndProcCallback(
        _In_ int nCode,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam);

    struct ThreadContext
    {
    public:

        // Fields for all scenarios.
        // Should always be available if ShouldAppsUseDarkMode is true.

        HHOOK volatile WindowsHookHandle = nullptr;

        // Fields for specific scenarios.
        // May not be available, which need to be checked before use.

        bool volatile MicaBackdropAvailable = false;

    public:

        ThreadContext()
        {
            this->WindowsHookHandle = ::SetWindowsHookExW(
                WH_CALLWNDPROC,
                ::CallWndProcCallback,
                nullptr,
                ::GetCurrentThreadId());
        }

        ~ThreadContext()
        {
            if (this->WindowsHookHandle)
            {
                ::UnhookWindowsHookEx(this->WindowsHookHandle);
                this->WindowsHookHandle = nullptr;
            }
        }
    };
    thread_local ThreadContext g_ThreadContext;

    // Window classes that belong to the NanaZip File Manager. Inversion-only
    // theming must stay inside this ownership boundary; system windows such
    // as the common file dialog (#32770), DirectUI and ShellView surfaces
    // are never redirected.
    static bool IsNanaZipWindowClassName(
        _In_ LPCWSTR ClassName)
    {
        return (
            (0 == std::wcscmp(ClassName, L"NanaZip.Modern.FileManager")) ||
            (0 == std::wcscmp(ClassName, L"NanaZip::Panel")) ||
            (nullptr != std::wcsstr(ClassName, L"Mile.Xaml.")));
    }

    // Decide whether a window belongs to the File Manager by matching its
    // own class or walking the owner/parent chain. NanaZip dialogs (options,
    // message boxes) are owned by the File Manager window and are therefore
    // included; the owner chain is bounded. The common file dialog is also
    // owned by the File Manager, but it is always shown inside a native
    // theme suspend scope (see IsNativeThemeSuspended), which takes
    // precedence and keeps it untouched.
    static bool IsNanaZipOwnedWindow(
        _In_opt_ HWND WindowHandle)
    {
        HWND Current = WindowHandle;
        for (unsigned Depth = 0; Current && (Depth < 16); ++Depth)
        {
            wchar_t ClassName[256] = {};
            if (0 != ::GetClassNameW(
                Current,
                ClassName,
                MO_ARRAY_SIZE(ClassName)))
            {
                if (::IsNanaZipWindowClassName(ClassName))
                {
                    return true;
                }
            }

            HWND Next = ::GetWindow(Current, GW_OWNER);
            if (!Next)
            {
                Next = ::GetParent(Current);
            }
            if ((!Next) || (Next == Current))
            {
                break;
            }
            Current = Next;
        }
        return false;
    }

    // Class redirection is skipped while a native system dialog owns the
    // theme, and it is never applied to windows outside the File Manager
    // ownership boundary.
    static bool ShouldApplyManagedThemeToWindow(
        _In_opt_ HWND WindowHandle)
    {
        return (!::IsNativeThemeSuspended() &&
            ::IsNanaZipOwnedWindow(WindowHandle));
    }

    // SetWindowTheme keeps the theme handle a control opened earlier alive
    // when only the process-wide preferred app mode changed, and assigning
    // the same class name the window already has is a no-op. Detach the
    // current association with an empty class first, so the subsequent
    // assignment makes uxtheme reopen the handle against BOTH the new class
    // and the refreshed light/dark policy. Pass nullptr in ThemeClass to
    // reset the window to its default class.
    static void ForceWindowThemeClass(
        _In_ HWND WindowHandle,
        _In_opt_ LPCWSTR ThemeClass)
    {
        ::SetWindowTheme(WindowHandle, L"", nullptr);
        ::SetWindowTheme(WindowHandle, ThemeClass, nullptr);
    }

    static void RefreshWindowTheme(
        _In_ HWND WindowHandle)
    {
        // Keep class redirection inside the File Manager ownership boundary
        // and out of native system dialog scopes.
        if (!::ShouldApplyManagedThemeToWindow(WindowHandle))
        {
            return;
        }

        wchar_t ClassName[256] = {};
        if (0 != ::GetClassNameW(
            WindowHandle,
            ClassName,
            MO_ARRAY_SIZE(ClassName)))
        {
            if (0 == std::wcscmp(ClassName, WC_BUTTONW))
            {
                if (::IsThemeInverted())
                {
                    // While the theme is inverted the plain "Explorer" class
                    // follows the (opposite) system appearance, so explicit
                    // variants are required: DarkMode_Explorer for effective
                    // dark and a reset for effective light.
                    if (ShouldAppsUseDarkMode())
                    {
                        ::ForceWindowThemeClass(WindowHandle, L"DarkMode_Explorer");
                    }
                    else
                    {
                        ::ForceWindowThemeClass(WindowHandle, nullptr);
                    }
                }
                else
                {
                    // Follow the system appearance (upstream behavior).
                    ::ForceWindowThemeClass(WindowHandle, L"Explorer");
                }
            }
            else if (
                (0 == std::wcscmp(ClassName, WC_COMBOBOXW)) ||
                (0 == std::wcscmp(ClassName, WC_EDITW)))
            {
                if (::IsThemeInverted())
                {
                    // "CFD"/"Explorer" alone follow the system appearance.
                    // Use their explicit dark variants while inverted dark
                    // and reset the class while inverted light, otherwise
                    // freshly created controls keep the system color.
                    if (ShouldAppsUseDarkMode())
                    {
                        ::ForceWindowThemeClass(
                            WindowHandle,
                            (0 == std::wcscmp(ClassName, WC_COMBOBOXW))
                                ? L"DarkMode_CFD"
                                : L"DarkMode_Explorer");
                    }
                    else
                    {
                        ::ForceWindowThemeClass(WindowHandle, nullptr);
                    }
                }
                else
                {
                    // Follow the system appearance (upstream behavior).
                    ::ForceWindowThemeClass(WindowHandle, L"CFD");
                }
                ::MileAllowDarkModeForWindow(WindowHandle, TRUE);
            }
            else if (0 == std::wcscmp(ClassName, WC_HEADERW))
            {
                if (::IsThemeInverted() && !ShouldAppsUseDarkMode())
                {
                    // Inverted light (dark system forced light): reset to the
                    // default header class, otherwise the cached ItemsView
                    // surface keeps rendering as the black "name" column bar
                    // after a restart.
                    ::ForceWindowThemeClass(WindowHandle, nullptr);
                }
                else
                {
                    // Native themes and inverted dark both use ItemsView.
                    ::ForceWindowThemeClass(WindowHandle, L"ItemsView");
                }
            }
            else if (0 == std::wcscmp(ClassName, WC_TREEVIEWW))
            {
                // The namespace tree only needs explicit handling while the
                // theme is inverted (the system dialogs that own such trees
                // render inside a native suspend scope and never get here).
                if (::IsThemeInverted())
                {
                    if (ShouldAppsUseDarkMode())
                    {
                        ::ForceWindowThemeClass(WindowHandle, L"DarkMode_Explorer");
                        TreeView_SetBkColor(WindowHandle, g_DarkModeBackgroundColor);
                        TreeView_SetTextColor(WindowHandle, g_DarkModeForegroundColor);
                    }
                    else
                    {
                        ::ForceWindowThemeClass(WindowHandle, nullptr);
                        TreeView_SetBkColor(WindowHandle, CLR_DEFAULT);
                        TreeView_SetTextColor(WindowHandle, CLR_DEFAULT);
                    }
                }
            }
            else if (0 == std::wcscmp(ClassName, WC_LISTVIEWW))
            {
                if (::IsThemeInverted() && !ShouldAppsUseDarkMode())
                {
                    ::ForceWindowThemeClass(WindowHandle, nullptr);
                }
                else
                {
                    ::ForceWindowThemeClass(WindowHandle, L"ItemsView");
                }

                if (ShouldAppsUseDarkMode())
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
                    if (ShouldAppsUseDarkMode())
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
            }

            // The class (and the explicit list/tree colors) are now in
            // their final state. Tell the control to close and reopen its
            // uxtheme handle against this class and the refreshed preferred
            // app mode, then repaint synchronously. Without this, toggling
            // the inverted theme off on a dark system leaves the surfaces
            // that were opened under the previous forced policy (a white
            // header row and light-gray buttons) until the process is
            // restarted, because SetWindowTheme neither reopens the cached
            // handle by itself nor invalidates the control, and a plain
            // InvalidateRect on the clipped parent never reaches the
            // child windows.
            ::SendMessageW(WindowHandle, WM_THEMECHANGED, 0, 0);
            ::RedrawWindow(
                WindowHandle,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
        }
    }

    static void CALLBACK K7UserWinEventProc(
        _In_opt_ HWINEVENTHOOK WinEventHook,
        _In_ DWORD WinEvent,
        _In_opt_ HWND WindowHandle,
        _In_ LONG ObjectId,
        _In_ LONG ChildId,
        _In_ DWORD EventThreadId,
        _In_ DWORD EventTime)
    {
        UNREFERENCED_PARAMETER(WinEventHook);
        UNREFERENCED_PARAMETER(EventThreadId);
        UNREFERENCED_PARAMETER(EventTime);

        // Apply the class-specific theme settings (Explorer buttons, CFD
        // combo boxes and edit controls, ItemsView headers and list views,
        // composited status bars) to File Manager windows as soon as they
        // are created. The in-session theme switch covers existing windows
        // through K7UserRefreshTheme, but freshly created controls (e.g.
        // after a restart with the inverted theme already enabled, or
        // dialogs opened later on) otherwise keep using their default theme
        // classes and render with the wrong theme data.
        if (EVENT_OBJECT_CREATE != WinEvent ||
            OBJID_WINDOW != ObjectId ||
            0 != ChildId ||
            !WindowHandle ||
            !g_GlobalInitialized)
        {
            return;
        }

        wchar_t ClassName[256] = {};
        if (0 == ::GetClassNameW(
            WindowHandle,
            ClassName,
            MO_ARRAY_SIZE(ClassName)))
        {
            return;
        }

        // Only the classes which RefreshWindowTheme has special handling
        // for need the call; skip everything else to keep the hook cheap.
        if (0 == std::wcscmp(ClassName, WC_BUTTONW) ||
            0 == std::wcscmp(ClassName, WC_COMBOBOXW) ||
            0 == std::wcscmp(ClassName, WC_EDITW) ||
            0 == std::wcscmp(ClassName, WC_HEADERW) ||
            0 == std::wcscmp(ClassName, WC_LISTVIEWW) ||
            0 == std::wcscmp(ClassName, STATUSCLASSNAMEW) ||
            0 == std::wcscmp(ClassName, WC_TABCONTROLW) ||
            0 == std::wcscmp(ClassName, TOOLBARCLASSNAMEW))
        {
            // Never redirect windows of a native system dialog (the common
            // file dialog creates these control classes internally) or
            // windows outside the File Manager ownership boundary.
            if (::ShouldApplyManagedThemeToWindow(WindowHandle))
            {
                ::RefreshWindowTheme(WindowHandle);
            }
        }
    }

    static bool IsFileManagerWindowClassName(
        _In_ LPCWSTR ClassName)
    {
        return (0 == std::wcscmp(ClassName, L"NanaZip.Modern.FileManager"));
    }

    static bool IsFileManagerPanelWindowClassName(
        _In_ LPCWSTR ClassName)
    {
        return (0 == std::wcscmp(ClassName, L"NanaZip::Panel"));
    }

    static bool IsFileManagerWindow(
        _In_ HWND WindowHandle)
    {
        wchar_t ClassName[256] = {};
        if (0 != ::GetClassNameW(
            WindowHandle,
            ClassName,
            MO_ARRAY_SIZE(ClassName)))
        {
            return ::IsFileManagerWindowClassName(ClassName);
        }

        return false;
    }

    static UINT K7GetDeferredThemeApplyMessage()
    {
        static UINT Message = ::RegisterWindowMessageW(
            L"NanaZip.K7User.DeferredThemeApply");
        return Message;
    }

    static void K7ApplyWindowThemeForCreate(HWND hWnd)
    {
        ::MileAllowDarkModeForWindow(
            hWnd,
            TRUE);

        g_ThreadContext.MicaBackdropAvailable =
            (S_OK == ::MileEnableImmersiveDarkModeForWindow(
                hWnd,
                ShouldAppsUseDarkMode()));

        bool ShouldExtendFrame = (
            ShouldAppsUseDarkMode() &&
            ::IsStandardDynamicRangeMode() &&
            g_ThreadContext.MicaBackdropAvailable);

        ::ApplyWindowSystemBackdrop(hWnd, ShouldExtendFrame);

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
                ::MulDiv(84, DpiValue, USER_DEFAULT_SCREEN_DPI);
            Margins.cyBottomHeight =
                ::MulDiv(32, DpiValue, USER_DEFAULT_SCREEN_DPI);
            ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
        }

        ::RefreshWindowTheme(hWnd);

        wchar_t ClassName[256] = {};
        if (0 != ::GetClassNameW(
            hWnd,
            ClassName,
            MO_ARRAY_SIZE(ClassName)))
        {
            if (0 == std::wcscmp(ClassName, WC_TABCONTROLW))
            {
                ::SetWindowLongPtrW(
                    hWnd,
                    GWL_STYLE,
                    (::GetWindowLongPtrW(hWnd, GWL_STYLE) & ~TCS_BUTTONS)
                    | TCS_TABS);
                ::SetWindowTheme(hWnd, nullptr, nullptr);
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
        UNREFERENCED_PARAMETER(uIdSubclass);
        UNREFERENCED_PARAMETER(dwRefData);

        if (uMsg == ::K7GetDeferredThemeApplyMessage())
        {
            // Runs after every synchronous WM_CREATE handler of the window
            // (the framework's own initialization included), so the theme
            // decision applied here wins over any default backdrop the
            // framework set during initialization. Only File Manager owned
            // windows receive this message, and a native dialog suspend
            // scope must not be able to theme a foreign window.
            if (::ShouldApplyManagedThemeToWindow(hWnd))
            {
                ::K7ApplyWindowThemeForCreate(hWnd);

                // Re-evaluate the child controls (header, list view) after
                // the framework finished initializing the top-level window,
                // otherwise a header can keep a stale dark class on a
                // dark-theme system with the inverted (light) theme.
                ::EnumChildWindows(
                    hWnd,
                    [](
                        _In_ HWND ChildWindow,
                        _In_ LPARAM lParam) -> BOOL
                {
                    UNREFERENCED_PARAMETER(lParam);
                    ::RefreshWindowTheme(ChildWindow);
                    return TRUE;
                },
                    0);
            }
            return 0;
        }

        switch (uMsg)
        {
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        {
            // Leave system dialogs and other non File Manager windows with
            // their native control colors.
            if (!::ShouldApplyManagedThemeToWindow(hWnd))
            {
                break;
            }
            HDC DeviceContextHandle = reinterpret_cast<HDC>(wParam);
            if (DeviceContextHandle)
            {
                ::SetTextColor(
                    DeviceContextHandle,
                    ShouldAppsUseDarkMode() ?
                        g_DarkModeForegroundColor :
                        g_LightModeForegroundColor);
                ::SetBkColor(
                    DeviceContextHandle,
                    ShouldAppsUseDarkMode() ?
                        g_DarkModeBackgroundColor :
                        g_LightModeBackgroundColor);
            }

            return reinterpret_cast<INT_PTR>(
                ShouldAppsUseDarkMode() ?
                    ::GetDarkModeBackgroundBrush() :
                    ::GetStockObject(WHITE_BRUSH));
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

                bool ShouldUseDarkMode = ::ComputeShouldAppsUseDarkMode();
                SetShouldAppsUseDarkMode(ShouldUseDarkMode);

                ::ApplyProcessThemePolicy(ShouldUseDarkMode);

                ::MileEnableImmersiveDarkModeForWindow(
                    hWnd,
                    ShouldUseDarkMode);

                bool ShouldExtendFrame = (
                    ShouldUseDarkMode &&
                    ::IsStandardDynamicRangeMode() &&
                    g_ThreadContext.MicaBackdropAvailable);

                ::ApplyWindowSystemBackdrop(hWnd, ShouldExtendFrame);

                MARGINS Margins = {};
                if (ShouldExtendFrame)
                {
                    Margins = { -1 };
                }
                else if (::IsFileManagerWindow(hWnd))
                {
                    UINT DpiValue = ::GetDpiForWindow(hWnd);
                    Margins.cyTopHeight =
                        ::MulDiv(84, DpiValue, USER_DEFAULT_SCREEN_DPI);
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

                ::RedrawWindow(
                    hWnd,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_ERASE | RDW_FRAME |
                    RDW_ALLCHILDREN | RDW_UPDATENOW);
            }

            break;
        }
        case WM_INITDIALOG:
        case WM_CREATE:
        {
            // System dialogs and other non File Manager windows initialize
            // with their native appearance, especially while a native
            // dialog suspend scope is active.
            if (!::ShouldApplyManagedThemeToWindow(hWnd))
            {
                break;
            }

            ::K7ApplyWindowThemeForCreate(hWnd);

            // The framework applies its own default backdrop during window
            // initialization, which can run after this WM_CREATE handler and
            // overwrite the decision above (observed as an opaque black bar
            // when the inverted theme had the application light on a
            // dark-theme system). Re-apply once after all synchronous
            // initialization, but only for the File Manager top-level
            // window and the XAML islands: they are the only surfaces whose
            // framework-set backdrop has to be overridden. Posting this to
            // every created control flooded the queue while a system dialog
            // built its window tree and produced unordered refreshes.
            wchar_t CreateClassName[256] = {};
            bool DeferredTarget = false;
            if (0 != ::GetClassNameW(
                hWnd,
                CreateClassName,
                MO_ARRAY_SIZE(CreateClassName)))
            {
                DeferredTarget =
                    ::IsFileManagerWindowClassName(CreateClassName) ||
                    (nullptr != std::wcsstr(CreateClassName, L"Mile.Xaml."));
            }
            if (DeferredTarget)
            {
                ::PostMessageW(
                    hWnd,
                    ::K7GetDeferredThemeApplyMessage(),
                    0,
                    0);
            }

            break;
        }
        case WM_ERASEBKGND:
        {
            wchar_t ClassName[256] = {};
            if (0 != ::GetClassNameW(
                hWnd,
                ClassName,
                MO_ARRAY_SIZE(ClassName)))
            {
                if (0 == std::wcscmp(ClassName, L"Mile.Xaml.ContentWindow"))
                {
                    // The XAML islands deliberately leave regions of their
                    // content transparent so the backdrop shows through.
                    // Those transparent composition pixels reveal the window
                    // redirection surface beneath, which is never erased and
                    // composes as an opaque black bar when the inverted
                    // theme turned the application light on a dark-theme
                    // system. Erase the surface with the application theme
                    // color so the transparent regions blend into the
                    // application appearance instead.
                    RECT ClientArea = {};
                    if (::GetClientRect(hWnd, &ClientArea))
                    {
                        ::FillRect(
                            reinterpret_cast<HDC>(wParam),
                            &ClientArea,
                            ShouldAppsUseDarkMode()
                                ? ::GetDarkModeBackgroundBrush()
                                : reinterpret_cast<HBRUSH>(
                                    ::GetStockObject(WHITE_BRUSH)));
                        return TRUE;
                    }
                }

                if (ShouldAppsUseDarkMode() &&
                    0 == std::wcscmp(ClassName, STATUSCLASSNAMEW))
                {
                    RECT ClientArea = {};
                    if (::GetClientRect(hWnd, &ClientArea))
                    {
                        ::FillRect(
                            reinterpret_cast<HDC>(wParam),
                            &ClientArea,
                            ::GetDarkModeBackgroundBrush());
                        return TRUE;
                    }
                }

                if (::IsFileManagerWindowClassName(ClassName) ||
                    ::IsFileManagerPanelWindowClassName(ClassName))
                {
                    RECT ClientArea = {};
                    if (::GetClientRect(hWnd, &ClientArea))
                    {
                        ::FillRect(
                            reinterpret_cast<HDC>(wParam),
                            &ClientArea,
                            ShouldAppsUseDarkMode()
                                ? ::GetDarkModeBackgroundBrush()
                                : reinterpret_cast<HBRUSH>(
                                    ::GetStockObject(WHITE_BRUSH)));
                        return TRUE;
                    }
                }
            }

            break;
        }
        case WM_DPICHANGED:
        {
            bool ShouldExtendFrame = (
                ShouldAppsUseDarkMode() &&
                ::IsStandardDynamicRangeMode() &&
                g_ThreadContext.MicaBackdropAvailable);
            if (!ShouldExtendFrame && ::IsFileManagerWindow(hWnd))
            {
                UINT DpiValue = ::GetDpiForWindow(hWnd);

                MARGINS Margins = {};
                Margins.cyTopHeight =
                    ::MulDiv(84, DpiValue, USER_DEFAULT_SCREEN_DPI);
                Margins.cyBottomHeight =
                    ::MulDiv(32, DpiValue, USER_DEFAULT_SCREEN_DPI);
                ::DwmExtendFrameIntoClientArea(hWnd, &Margins);
            }

            break;
        }
        default:
            break;
        }

        if (ShouldAppsUseDarkMode() && ::GetMenu(hWnd))
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
                        MenuItemInfo.cch = MO_ARRAY_SIZE(Buffer) - 1;
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

    static std::wstring GetAssociatedModuleNameFromWindowHandle(
        _In_ HWND WindowHandle)
    {
        // 32767 is the maximum path length without the terminating null
        // character.
        std::wstring Path(32767, L'\0');
        Path.resize(::GetWindowModuleFileNameW(
            WindowHandle, &Path[0], static_cast<UINT>(Path.size())));
        wchar_t* LastBackslash = std::wcsrchr(Path.data(), L'\\');
        return LastBackslash ? std::wstring(LastBackslash + 1) : Path;
    }

    static bool IsModernizedWindow(
        _In_ HWND WindowHandle)
    {
        std::wstring ModuleName =
            ::GetAssociatedModuleNameFromWindowHandle(WindowHandle);
        if (!::_wcsicmp(ModuleName.c_str(), L"combase.dll") ||
            !::_wcsicmp(ModuleName.c_str(), L"CoreMessaging.dll") ||
            !::_wcsicmp(ModuleName.c_str(), L"InputHost.dll") ||
            !::_wcsicmp(ModuleName.c_str(), L"Windows.UI.dll") ||
            !::_wcsicmp(ModuleName.c_str(), L"Windows.UI.Xaml.dll"))
        {
            return true;
        }

        wchar_t ClassName[256] = {};
        if (0 != ::GetClassNameW(
            WindowHandle,
            ClassName,
            MO_ARRAY_SIZE(ClassName)))
        {
            if (std::wcsstr(ClassName, L"Windows.UI.") ||
                std::wcsstr(ClassName, L"Mile.Xaml.") ||
                std::wcsstr(ClassName, L"Xaml_WindowedPopupClass"))
            {
                return true;
            }
        }

        return false;
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
            {
                if (!::IsModernizedWindow(WndProcStruct->hwnd))
                {
                    ::SetWindowSubclass(
                        WndProcStruct->hwnd,
                        ::WindowSubclassCallback,
                        0,
                        0);
                }
                break;
            }
            default:
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
            GetThemeColor,
            DrawThemeText,
            DrawThemeTextEx,
            DrawThemeBackground,
            DrawThemeBackgroundEx,
            OpenNcThemeData,
            OpenThemeData,
            OpenThemeDataEx,
            OpenThemeDataForDpi,
            GetThemeClass,
            GetThemeSysColor,

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
        using FunctionType = decltype(::GetSysColor)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::GetSysColor].Original);
        if (!FunctionAddress)
        {
            return 0;
        }
        return FunctionAddress(nIndex);
    }

    static HBRUSH WINAPI OriginalGetSysColorBrush(
        _In_ int nIndex)
    {
        using FunctionType = decltype(::GetSysColorBrush)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::GetSysColorBrush].Original);
        if (!FunctionAddress)
        {
            return nullptr;
        }
        return FunctionAddress(nIndex);
    }

    static HRESULT WINAPI OriginalGetThemeColor(
        _In_ HTHEME hTheme,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ int iPropId,
        _Out_ COLORREF* pColor)
    {
        using FunctionType = decltype(::GetThemeColor)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::GetThemeColor].Original);
        if (!FunctionAddress)
        {
            return E_NOINTERFACE;
        }
        return FunctionAddress(
            hTheme,
            iPartId,
            iStateId,
            iPropId,
            pColor);
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
        using FunctionType = decltype(::DrawThemeText)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::DrawThemeText].Original);
        if (!FunctionAddress)
        {
            return E_NOINTERFACE;
        }
        return FunctionAddress(
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

    static HRESULT WINAPI OriginalDrawThemeTextEx(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCWSTR pszText,
        _In_ int cchText,
        _In_ DWORD dwTextFlags,
        _In_ LPRECT lprc,
        _In_opt_ const DTTOPTS* pOptions)
    {
        using FunctionType = decltype(::DrawThemeTextEx)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::DrawThemeTextEx].Original);
        if (!FunctionAddress)
        {
            return E_NOINTERFACE;
        }
        return FunctionAddress(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pszText,
            cchText,
            dwTextFlags,
            lprc,
            pOptions);
    }

    static HRESULT WINAPI OriginalDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        using FunctionType = decltype(::DrawThemeBackground)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::DrawThemeBackground].Original);
        if (!FunctionAddress)
        {
            return E_NOINTERFACE;
        }
        return FunctionAddress(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pRect,
            pClipRect);
    }

    static HRESULT WINAPI OriginalDrawThemeBackgroundEx(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ const DTBGOPTS* pOptions)
    {
        using FunctionType = decltype(::DrawThemeBackgroundEx)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::DrawThemeBackgroundEx].Original);
        if (!FunctionAddress)
        {
            return E_NOINTERFACE;
        }
        return FunctionAddress(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pRect,
            pOptions);
    }

    static HTHEME WINAPI OriginalOpenNcThemeData(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList)
    {
        using FunctionType = decltype(::OpenNcThemeData)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::OpenNcThemeData].Original);
        if (!FunctionAddress)
        {
            return nullptr;
        }
        return FunctionAddress(hwnd, pszClassList);
    }

    static HTHEME WINAPI OriginalOpenThemeData(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList)
    {
        using FunctionType = decltype(::OpenThemeData)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::OpenThemeData].Original);
        if (!FunctionAddress)
        {
            return nullptr;
        }
        return FunctionAddress(hwnd, pszClassList);
    }

    static HTHEME WINAPI OriginalOpenThemeDataEx(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList,
        _In_ DWORD dwFlags)
    {
        using FunctionType = decltype(::OpenThemeDataEx)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::OpenThemeDataEx].Original);
        if (!FunctionAddress)
        {
            return nullptr;
        }
        return FunctionAddress(hwnd, pszClassList, dwFlags);
    }

    static HTHEME WINAPI OriginalOpenThemeDataForDpi(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList,
        _In_ UINT dpi)
    {
        using FunctionType = decltype(::OpenThemeDataForDpi)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::OpenThemeDataForDpi].Original);
        if (!FunctionAddress)
        {
            return nullptr;
        }
        return FunctionAddress(hwnd, pszClassList, dpi);
    }

    static HRESULT WINAPI OriginalGetThemeClass(
        _In_ HTHEME hTheme,
        _Out_ LPWSTR pszClassName,
        _In_ int cchClassName)
    {
        using FunctionType = decltype(::GetThemeClass)*;
        FunctionType FunctionAddress = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::GetThemeClass].Original);
        if (!FunctionAddress)
        {
            return E_NOINTERFACE;
        }
        return FunctionAddress(
            hTheme,
            pszClassName,
            cchClassName);
    }

    // Resolves the text color which has to be used for theme text on a
    // system forced into the inverted (dark) mode. The light theme data
    // handed out by uxtheme would otherwise provide a dark text color which
    // is unreadable on the dark backgrounds we draw ourselves.
    static COLORREF GetDarkModeThemeTextColor(
        _In_ bool HasClassName,
        _In_reads_(256) LPCWSTR ClassName,
        _In_ int iPartId,
        _In_ int iStateId)
    {
        if (HasClassName && 0 == ::_wcsicmp(ClassName, L"Explorer"))
        {
            if ((BP_PUSHBUTTON == iPartId && PBS_DISABLED == iStateId) ||
                (BP_CHECKBOX == iPartId &&
                    (CBS_UNCHECKEDDISABLED == iStateId ||
                        CBS_CHECKEDDISABLED == iStateId)) ||
                (BP_RADIOBUTTON == iPartId &&
                    (RBS_UNCHECKEDDISABLED == iStateId ||
                        RBS_CHECKEDDISABLED == iStateId)))
            {
                return RGB(109, 109, 109);
            }
        }

        // Disabled popup menu items (MPI_DISABLED = 3, MPI_DISABLEDHOT = 4)
        // keep the same muted gray as disabled buttons so they stay visually
        // distinct from the enabled entries.
        if (HasClassName && 0 == ::_wcsicmp(ClassName, L"Menu"))
        {
            if (14 == iPartId && (3 == iStateId || 4 == iStateId))
            {
                return RGB(109, 109, 109);
            }
        }

        return g_DarkModeForegroundColor;
    }

    static DWORD WINAPI DetouredGetSysColor(
        _In_ int nIndex)
    {
        if (!g_GlobalInitialized || !ShouldAppsUseDarkMode())
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
        if (!g_GlobalInitialized || !ShouldAppsUseDarkMode())
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

    static COLORREF WINAPI OriginalGetThemeSysColor(
        _In_ HTHEME hTheme,
        _In_ int iColorId)
    {
        using FunctionType = decltype(::GetThemeSysColor)*;
        auto Original = reinterpret_cast<FunctionType>(
            g_FunctionTable[FunctionTypes::GetThemeSysColor].Original);
        return Original(hTheme, iColorId);
    }

    // The DirectUI content draws list backgrounds with GetThemeSysColor.
    // This detour is inversion-only: when following a dark system the native
    // theme data already provides the correct dark colors (upstream does not
    // detour GetThemeSysColor at all), so we pass straight through there.
    static COLORREF WINAPI DetouredGetThemeSysColor(
        _In_ HTHEME hTheme,
        _In_ int iColorId)
    {
        if (!g_GlobalInitialized ||
            !ShouldAppsUseDarkMode() ||
            !::IsThemeInverted())
        {
            return ::OriginalGetThemeSysColor(hTheme, iColorId);
        }

        switch (iColorId)
        {
        case COLOR_WINDOW:
        case COLOR_BTNFACE:
            return g_DarkModeBackgroundColor;
        case COLOR_WINDOWTEXT:
        case COLOR_BTNTEXT:
            return g_DarkModeForegroundColor;
        default:
            return ::OriginalGetThemeSysColor(hTheme, iColorId);
        }
    }

    // Forward declaration: DetouredGetThemeColor resolves theme class names
    // through this helper, whose definition appears further down next to
    // the other Original* wrappers.
    static bool IsThemeClass(
        _In_ HTHEME hTheme,
        _In_z_ LPCWSTR ExpectedClassName);

    static bool IsDarkBackgroundThemeClass(
        _In_z_ LPCWSTR ClassName);

    static bool IsDarkTextThemeClass(
        _In_z_ LPCWSTR ClassName);

    static HRESULT WINAPI DetouredGetThemeColor(
        _In_ HTHEME hTheme,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ int iPropId,
        _Out_ COLORREF* pColor)
    {
        if (!g_GlobalInitialized || !ShouldAppsUseDarkMode())
        {
            return ::OriginalGetThemeColor(
                hTheme,
                iPartId,
                iStateId,
                iPropId,
                pColor);
        }

        HRESULT hr = ::OriginalGetThemeColor(
            hTheme,
            iPartId,
            iStateId,
            iPropId,
            pColor);
        if (S_OK != hr)
        {
            return hr;
        }

        if (TMT_TEXTCOLOR == iPropId)
        {
            wchar_t ClassName[256] = {};
            if (SUCCEEDED(::OriginalGetThemeClass(
                hTheme,
                ClassName,
                MO_ARRAY_SIZE(ClassName))) &&
                0 == ::_wcsicmp(ClassName, VSCLASS_TASKDIALOGSTYLE))
            {
                // Upstream behavior: task dialog text is always forced to the
                // light foreground on an effective dark theme.
                *pColor = g_DarkModeForegroundColor;
            }
            else if (::IsThemeInverted())
            {
                // Inversion-only: a light system forced dark hands out light
                // theme data with dark text colors, so force white for the
                // classes whose backgrounds we draw dark.
                if (FAILED(::OriginalGetThemeClass(
                    hTheme,
                    ClassName,
                    MO_ARRAY_SIZE(ClassName))))
                {
                    *pColor = g_DarkModeForegroundColor;
                }
                else if (0 != std::wcsncmp(ClassName, L"DarkMode_", 9) &&
                    IsDarkTextThemeClass(ClassName))
                {
                    // Native dark data keeps its own color; compound classes
                    // with light backgrounds and non-whitelisted classes keep
                    // their readable dark text (see IsDarkTextThemeClass).
                    *pColor = g_DarkModeForegroundColor;
                }
            }
        }
        else if (TMT_FILLCOLOR == iPropId && ::IsThemeInverted())
        {
            // Inversion-only. DirectUI surfaces resolve their background via
            // GetThemeColor TMT_FILLCOLOR and never call DrawThemeBackground
            // for it, so provide the dark fill. Native dark theme data opened
            // through the DarkMode_ redirect keeps its own (already dark)
            // color. The suspended native file dialogs never reach here.
            wchar_t FillClassName[256] = {};
            bool FillExempt =
                (SUCCEEDED(::OriginalGetThemeClass(
                    hTheme,
                    FillClassName,
                    MO_ARRAY_SIZE(FillClassName))) &&
                    (0 == std::wcsncmp(FillClassName, L"DarkMode_", 9)));
            if (!FillExempt)
            {
                *pColor = g_DarkModeBackgroundColor;
            }
        }

        return S_OK;
    }

    // Classes whose backgrounds are drawn dark by our DrawThemeBackground
    // handler (or which get a dark fill via GetThemeColor TMT_FILLCOLOR).
    // Their theme text must be forced to white on a light system forced
    // into dark mode, because uxtheme would otherwise hand out light theme
    // data with dark text colors.
    static bool IsDarkBackgroundThemeClass(
        _In_z_ LPCWSTR ClassName)
    {
        static const LPCWSTR DarkClasses[] =
        {
            L"ItemsView",
            L"Header",
            L"Explorer",
            L"Button",
            L"TaskDialog",
            L"Tab",
            L"StatusBar",
            L"Tooltip",
            L"Toolbar",
            L"Edit",
            L"Combobox",
            L"REBAR",
            L"SearchBox",
            L"SearchEditBox",
            L"BreadcrumbBar",
            L"TextStyle",
            L"Link",
            L"Progress",
            // Additional classes whose backgrounds get darkened by the
            // GetThemeColor TMT_FILLCOLOR rule below (DirectUI surfaces in
            // the common file dialogs), so their self-drawn text turns white
            // as well.
            L"ExplorerNavPane",
            L"TreeView",
            L"ReadingPane",
            L"ProperTree",
        };
        for (LPCWSTR DarkClass : DarkClasses)
        {
            if (0 == ::_wcsicmp(ClassName, DarkClass))
            {
                return true;
            }
        }
        return false;
    }

    // Compound class lists ("A::B") belong to the system common file
    // dialogs. Their backgrounds are drawn by DirectUI behind our detours
    // (delay-load bound uxtheme calls never reach us), so we cannot control
    // their background color - forcing white text there would only produce
    // unreadable white-on-white headers. Never force text on them.
    static bool IsDarkTextThemeClass(
        _In_z_ LPCWSTR ClassName)
    {
        // Menus belong to the dark text classes now: their backgrounds are
        // drawn dark by the DrawThemeBackground Menu handler above, so the
        // text has to be forced light as well.
        if (0 == std::wcsncmp(ClassName, L"DarkMode_", 9))
        {
            return false;
        }

        if (nullptr != std::wcsstr(ClassName, L"::"))
        {
            return false;
        }

        return IsDarkBackgroundThemeClass(ClassName);
    }

    static bool IsToolbarThemeText(
        _In_ HTHEME hTheme)
    {
        wchar_t ClassName[256] = {};
        if (FAILED(::OriginalGetThemeClass(
            hTheme,
            ClassName,
            MO_ARRAY_SIZE(ClassName))))
        {
            return false;
        }

        // Exempt (= true, keep the native text color) exactly when the
        // unified dark-text rule says no forcing is needed: native dark
        // theme data, compound classes with light backgrounds, and single
        // classes whose backgrounds stay light. Everything else (dark
        // background classes, incl. the whitelisted compounds and menus)
        // gets its text forced to white.
        return !IsDarkTextThemeClass(ClassName);
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
        if (!g_GlobalInitialized || !ShouldAppsUseDarkMode())
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

        if (!::IsThemeInverted())
        {
            // Upstream behavior when following a dark system: every themed
            // text run is forced to the light foreground. Use the unhooked
            // DrawThemeTextEx pointer to avoid re-entering our own detour.
            DTTOPTS TextOptions = {};
            TextOptions.dwSize = sizeof(DTTOPTS);
            TextOptions.dwFlags = DTT_TEXTCOLOR;
            TextOptions.crText = g_DarkModeForegroundColor;

            RECT Rect = *pRect;
            return ::OriginalDrawThemeTextEx(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pszText,
                cchText,
                dwTextFlags,
                &Rect,
                &TextOptions);
        }

        if (IsToolbarThemeText(hTheme))
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

        wchar_t ClassName[256] = {};
        bool HasClassName = SUCCEEDED(::OriginalGetThemeClass(
            hTheme,
            ClassName,
            MO_ARRAY_SIZE(ClassName)));

        DTTOPTS TextOptions = {};
        TextOptions.dwSize = sizeof(DTTOPTS);
        TextOptions.dwFlags = DTT_TEXTCOLOR;
        TextOptions.crText = GetDarkModeThemeTextColor(
            HasClassName,
            ClassName,
            iPartId,
            iStateId);

        RECT Rect = *pRect;

        return ::OriginalDrawThemeTextEx(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pszText,
            cchText,
            dwTextFlags,
            &Rect,
            &TextOptions);
    }

    static HRESULT WINAPI DetouredDrawThemeTextEx(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCWSTR pszText,
        _In_ int cchText,
        _In_ DWORD dwTextFlags,
        _In_ LPRECT lprc,
        _In_opt_ const DTTOPTS* pOptions)
    {
        // Upstream does not detour DrawThemeTextEx: when following a dark
        // system the native implementation is used unchanged. Only inverted
        // dark needs the text color adjustment.
        if (!g_GlobalInitialized ||
            !ShouldAppsUseDarkMode() ||
            !::IsThemeInverted())
        {
            return ::OriginalDrawThemeTextEx(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pszText,
                cchText,
                dwTextFlags,
                lprc,
                pOptions);
        }

        if (IsToolbarThemeText(hTheme))
        {
            return ::OriginalDrawThemeTextEx(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pszText,
                cchText,
                dwTextFlags,
                lprc,
                pOptions);
        }

        const DTTOPTS* OptionPointer = pOptions;
        DTTOPTS AdjustedOptions = {};

        // Keep an explicitly specified color (e.g. the menu bar drawing uses
        // one) but force the color for everything else, because the light
        // theme data would otherwise provide an unreadable dark text color.
        if (!pOptions ||
            (0 == (pOptions->dwFlags & DTT_TEXTCOLOR)) ||
            (CLR_INVALID == pOptions->crText))
        {
            if (pOptions)
            {
                AdjustedOptions = *pOptions;
            }
            else
            {
                AdjustedOptions.dwSize = sizeof(DTTOPTS);
            }

            wchar_t ClassName[256] = {};
            bool HasClassName = SUCCEEDED(::OriginalGetThemeClass(
                hTheme,
                ClassName,
                MO_ARRAY_SIZE(ClassName)));

            AdjustedOptions.dwFlags |= DTT_TEXTCOLOR;
            AdjustedOptions.crText = GetDarkModeThemeTextColor(
                HasClassName,
                ClassName,
                iPartId,
                iStateId);
            OptionPointer = &AdjustedOptions;
        }

        return ::OriginalDrawThemeTextEx(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pszText,
            cchText,
            dwTextFlags,
            lprc,
            OptionPointer);
    }

    static bool IsThemeClass(
        _In_ HTHEME hTheme,
        _In_z_ LPCWSTR ExpectedClassName)
    {
        wchar_t ClassName[256] = {};
        if (FAILED(::OriginalGetThemeClass(
            hTheme,
            ClassName,
            MO_ARRAY_SIZE(ClassName))))
        {
            return false;
        }

        return (0 == ::_wcsicmp(ClassName, ExpectedClassName));
    }

    static HRESULT WINAPI DetouredDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        if (!g_GlobalInitialized || !ShouldAppsUseDarkMode())
        {
            return ::OriginalDrawThemeBackground(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pClipRect);
        }

        // Resolve the class name once for the compound-class matches below.
        wchar_t BgClassName[256] = {};
        if (FAILED(::OriginalGetThemeClass(
            hTheme,
            BgClassName,
            MO_ARRAY_SIZE(BgClassName))))
        {
            BgClassName[0] = L'\0';
        }

        // The class names are resolved through GetThemeClass instead of
        // comparing cached theme handles, because the handles are per-window
        // and can be reopened (and thus invalidated) at any time, while the
        // class name of an opened theme data is stable.
        //
        // Popup menus are painted explicitly whenever the effective theme
        // is dark, so the radio bullet/check glyphs keep a high-contrast
        // light ink and the popup border stays visible both when following
        // a dark system and when the dark appearance is forced for the
        // inverted theme (in the latter case uxtheme would otherwise hand
        // out the system light menu data).
        if (::IsThemeClass(hTheme, L"Menu"))
        {
            // The Menu class part ids from vsstyle.h are easy to get wrong
            // because the check parts sit BEFORE the gutter/item parts:
            //
            //   9  MENU_POPUPBACKGROUND
            //   10 MENU_POPUPBORDERS
            //   11 MENU_POPUPCHECK         (the check mark / radio bullet)
            //   12 MENU_POPUPCHECKBACKGROUND
            //   13 MENU_POPUPGUTTER
            //   14 MENU_POPUPITEM
            //   15 MENU_POPUPSEPARATOR
            //   16 MENU_POPUPSUBMENU
            //
            // The check background and gutter surfaces are left
            // unpainted on purpose: the part 14 item band (or the popup
            // background) already provides the surface, and filling an own
            // color here paints a dark notch over the hovered row. The
            // colors mirror the menu bar painting in the WM_UAHDRAWMENU
            // handler above.
            switch (iPartId)
            {
            case 9: // MENU_POPUPBACKGROUND
            {
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
                return S_OK;
            }
            case 10: // MENU_POPUPBORDERS
            {
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
                ::FrameRect(hdc, pRect, ::GetDarkModeBorderBrush());
                return S_OK;
            }
            case 12: // MENU_POPUPCHECKBACKGROUND
            case 13: // MENU_POPUPGUTTER
            {
                // Keep the item band/popup background painted by parts 9
                // and 14 visible through the check column.
                return S_OK;
            }
            case 14: // MENU_POPUPITEM
            {
                ::FillRect(
                    hdc,
                    pRect,
                    (2 == iStateId || 4 == iStateId)
                        ? ::GetDarkModeMenuSelectedBackgroundBrush()
                        : ::GetDarkModeBackgroundBrush());
                return S_OK;
            }
            case 15: // MENU_POPUPSEPARATOR
            {
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());

                RECT LineRect = *pRect;
                LONG LineHeight = LineRect.bottom - LineRect.top;
                LineRect.top += (LineHeight > 0) ? ((LineHeight - 1) / 2) : 0;
                LineRect.bottom = LineRect.top + 1;
                ::FillRect(hdc, &LineRect, ::GetDarkModeBorderBrush());
                return S_OK;
            }
            case 11: // MENU_POPUPCHECK
            {
                // MENU_POPUPCHECK states: 1 = check normal,
                // 2 = check disabled, 3 = bullet normal, 4 = bullet
                // disabled. The light theme data hands out dark ink for
                // these glyphs; draw the glyph manually in light ink.
                COLORREF GlyphColor =
                    (2 == iStateId || 4 == iStateId)
                        ? RGB(109, 109, 109)
                        : g_DarkModeForegroundColor;
                LONG Width = pRect->right - pRect->left;
                LONG Height = pRect->bottom - pRect->top;

                if (3 == iStateId || 4 == iStateId)
                {
                    // Radio bullet: a filled circle centered in the cell.
                    LONG Radius = ((Width < Height) ? Width : Height) / 4;
                    if (Radius < 2)
                    {
                        Radius = 2;
                    }
                    LONG CenterX = (pRect->left + pRect->right) / 2;
                    LONG CenterY = (pRect->top + pRect->bottom) / 2;
                    HBRUSH BulletBrush = ::CreateSolidBrush(GlyphColor);
                    if (BulletBrush)
                    {
                        HGDIOBJ OldBrush = ::SelectObject(hdc, BulletBrush);
                        HGDIOBJ OldPen =
                            ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
                        ::Ellipse(
                            hdc,
                            CenterX - Radius,
                            CenterY - Radius,
                            CenterX + Radius + 1,
                            CenterY + Radius + 1);
                        ::SelectObject(hdc, OldPen);
                        ::SelectObject(hdc, OldBrush);
                        ::DeleteObject(BulletBrush);
                    }
                }
                else
                {
                    // Check mark: a two-segment polyline.
                    LONG Cell = ((Width < Height) ? Width : Height);
                    if (Cell < 6)
                    {
                        Cell = 6;
                    }
                    LONG Left = pRect->left + (Width - Cell) / 2;
                    LONG Top = pRect->top + (Height - Cell) / 2;
                    POINT Points[3] =
                    {
                        { Left + Cell * 25 / 100, Top + Cell * 55 / 100 },
                        { Left + Cell * 40 / 100, Top + Cell * 70 / 100 },
                        { Left + Cell * 75 / 100, Top + Cell * 30 / 100 }
                    };
                    HPEN GlyphPen = ::CreatePen(PS_SOLID, 2, GlyphColor);
                    if (GlyphPen)
                    {
                        HGDIOBJ OldPen = ::SelectObject(hdc, GlyphPen);
                        HGDIOBJ OldBrush =
                            ::SelectObject(
                                hdc,
                                ::GetStockObject(NULL_BRUSH));
                        ::Polyline(hdc, Points, 3);
                        ::SelectObject(hdc, OldBrush);
                        ::SelectObject(hdc, OldPen);
                        ::DeleteObject(GlyphPen);
                    }
                }
                return S_OK;
            }
            default:
            {
                // The remaining parts (16 = MENU_POPUPSUBMENU,
                // 7/8 = menu bar surfaces) carry glyphs or already-dark
                // surfaces. Paint the dark surface first and let the
                // system glyph render on top of it.
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
                return ::OriginalDrawThemeBackground(
                    hTheme,
                    hdc,
                    iPartId,
                    iStateId,
                    pRect,
                    pClipRect);
            }
            }
        }
        else if (IsThemeClass(hTheme, L"Tab"))
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
        else if (::IsThemeClass(hTheme, L"StatusBar"))
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
        else if (
            ::IsThemeInverted() &&
            (IsThemeClass(hTheme, L"ItemsView") ||
            IsThemeClass(hTheme, L"Header") ||
            0 == ::_wcsicmp(BgClassName, L"ItemsView::Header") ||
            0 == ::_wcsicmp(BgClassName, L"ItemsView::ListView")))
        {
            // Header items and list view item backgrounds. Part 1 covers
            // HP_HEADERITEM as well as LVP_LISTITEM; parts 2-4 cover the
            // sorted/detail variations. The Header class also matches,
            // because header controls only get ItemsView applied via
            // SetWindowTheme during an in-session theme switch, while
            // freshly created controls (e.g. after a restart with inverted
            // theme already enabled) still use the default Header class.
            switch (iPartId)
            {
            case 1:
            {
                ::FillRect(
                    hdc,
                    pRect,
                    (2 == iStateId || 3 == iStateId)
                        ? ::GetDarkModeMenuSelectedBackgroundBrush()
                        : ::GetDarkModeBackgroundBrush());
                return S_OK;
            }
            default:
            {
                // Every remaining part (0 = list background, 2-4 = detail
                // variations, 5 = empty text area, ...) is a plain
                // background surface. The DirectUI list inside the common
                // file dialogs uses parts outside 1-4, which previously
                // fell through and kept rendering with light theme colors.
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
                return S_OK;
            }
            }
        }
        else if (
            ::IsThemeInverted() &&
            (::IsThemeClass(hTheme, L"Explorer") ||
            ::IsThemeClass(hTheme, L"Button")))
        {
            // Inversion-only hand drawing of buttons, check boxes and radio
            // buttons (the native dark system renders these itself). Buttons
            // use the Explorer class via SetWindowTheme; its part ids collide
            // with the tree view glyph parts, but the File Manager windows
            // don't contain tree views, so drawing the button parts dark is
            // safe here. The Button class also matches because buttons created
            // after startup (e.g. reopening the options dialog in a session
            // that started with the inverted theme) still use the default
            // class.
            switch (iPartId)
            {
            case BP_PUSHBUTTON:
            {
                ::FillRect(
                    hdc,
                    pRect,
                    (PBS_HOT == iStateId || PBS_PRESSED == iStateId)
                        ? ::GetDarkModeMenuSelectedBackgroundBrush()
                        : ::GetDarkModeBackgroundBrush());
                if (PBS_DISABLED != iStateId)
                {
                    ::FrameRect(hdc, pRect, ::GetDarkModeBorderBrush());
                }
                return S_OK;
            }
            case BP_CHECKBOX:
            case BP_RADIOBUTTON:
            {
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());

                RECT BoxRect = *pRect;
                LONG Side = BoxRect.bottom - BoxRect.top;
                if (Side > BoxRect.right - BoxRect.left)
                {
                    Side = BoxRect.right - BoxRect.left;
                }
                BoxRect.right = BoxRect.left + Side;

                // The checkbox and radio button states share the same
                // layout: 1-4 unchecked (normal/hot/pressed/disabled) and
                // 5-8 checked (normal/hot/pressed/disabled).
                DWORD Flags = (
                    BP_CHECKBOX == iPartId
                        ? DFCS_BUTTONCHECK
                        : DFCS_BUTTONRADIO);
                if (5 <= iStateId && 8 >= iStateId)
                {
                    Flags |= DFCS_CHECKED;
                }
                if (2 == iStateId || 6 == iStateId)
                {
                    Flags |= DFCS_HOT;
                }
                if (3 == iStateId || 7 == iStateId)
                {
                    Flags |= DFCS_PUSHED;
                }
                if (4 == iStateId || 8 == iStateId)
                {
                    Flags |= DFCS_INACTIVE;
                }
                ::DrawFrameControl(hdc, &BoxRect, DFC_BUTTON, Flags);
                return S_OK;
            }
            case BP_GROUPBOX:
            {
                ::FrameRect(hdc, pRect, ::GetDarkModeBorderBrush());
                return S_OK;
            }
            default:
                break;
            }
        }
        else if (::IsThemeInverted() && ::IsThemeClass(hTheme, L"Tooltip"))
        {
            switch (iPartId)
            {
            case TTP_STANDARD:
            case TTP_STANDARDTITLE:
            {
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
                ::FrameRect(hdc, pRect, ::GetDarkModeBorderBrush());
                return S_OK;
            }
            default:
                break;
            }
        }
        else if (::IsThemeInverted() && ::IsThemeClass(hTheme, L"Toolbar"))
        {
            // The command toolbars (the "Organize / New folder" row inside
            // the common file dialogs and the File Manager main toolbar)
            // keep their light plates otherwise, which clashes with the
            // dark surfaces around them. All toolbar parts are plain
            // background surfaces for the dark look.
            ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
            return S_OK;
        }
        else if (::IsThemeInverted() && ::IsThemeClass(hTheme, L"Edit"))
        {
            // Edit borders (1 = EP_EDITTEXT, 2-5 = the no-scroll / h-scroll
            // / v-scroll / hv-scroll border variants used e.g. by the search
            // box) fall back to light gradient bitmaps otherwise.
            ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
            ::FrameRect(
                hdc,
                pRect,
                (4 == iStateId) // ETS_DISABLED
                    ? ::GetDarkModeMenuSelectedBackgroundBrush()
                    : ::GetDarkModeBorderBrush());
            return S_OK;
        }
        else if (::IsThemeInverted() && ::IsThemeClass(hTheme, L"SearchBox"))
        {
            ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
            ::FrameRect(hdc, pRect, ::GetDarkModeBorderBrush());
            return S_OK;
        }

        return ::OriginalDrawThemeBackground(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pRect,
            pClipRect);
    }

    static HRESULT WINAPI DetouredDrawThemeBackgroundEx(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ const DTBGOPTS* pOptions)
    {
        if (!g_GlobalInitialized || !ShouldAppsUseDarkMode())
        {
            return ::OriginalDrawThemeBackgroundEx(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pOptions);
        }

        bool NeedTaskDialogWorkaround = (
            TDLG_PRIMARYPANEL == iPartId ||
            TDLG_SECONDARYPANEL == iPartId ||
            TDLG_EXPANDOBUTTON == iPartId ||
            TDLG_FOOTNOTEPANE == iPartId ||
            TDLG_FOOTNOTESEPARATOR == iPartId);
        if (NeedTaskDialogWorkaround)
        {
            NeedTaskDialogWorkaround = false;
            wchar_t ClassName[256] = {};
            if (S_OK == ::OriginalGetThemeClass(
                hTheme,
                ClassName,
                MO_ARRAY_SIZE(ClassName)))
            {
                NeedTaskDialogWorkaround =
                    (0 == ::_wcsicmp(ClassName, VSCLASS_TASKDIALOG));
            }
        }

        if (NeedTaskDialogWorkaround)
        {
            if (TDLG_PRIMARYPANEL == iPartId)
            {
                ::FillRect(hdc, pRect, ::GetDarkModeBackgroundBrush());
                return S_OK;
            }
            else if (
                TDLG_SECONDARYPANEL == iPartId ||
                TDLG_FOOTNOTEPANE == iPartId ||
                TDLG_FOOTNOTESEPARATOR == iPartId)
            {
                ::FillRect(hdc, pRect, ::GetDarkModeBorderBrush());
                RECT ContentRect = *pRect;
                ContentRect.top += 1;
                ::FillRect(hdc, &ContentRect, ::GetDarkModeBackgroundBrush());
                return S_OK;
            }
            else if (iPartId == TDLG_EXPANDOBUTTON)
            {
                // It seems our current implementation doesn't have the issue
                // that the button becomes invisible on dark mode in Windows 11,
                // so we don't need to do anything here for now.
            }
        }

        // While the theme is inverted route the rest through
        // DetouredDrawThemeBackground so the Button/Header/Explorer/ItemsView
        // hand-drawn fallbacks also cover callers of DrawThemeBackgroundEx
        // (e.g. freshly created controls after a restart with the inverted
        // theme already enabled). When following the system, keep the native
        // implementation (upstream behavior).
        if (pOptions && (pOptions->dwFlags & DTBG_CLIPRECT))
        {
            return ::OriginalDrawThemeBackgroundEx(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pOptions);
        }

        if (::IsThemeInverted())
        {
            return ::DetouredDrawThemeBackground(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                nullptr);
        }

        return ::OriginalDrawThemeBackgroundEx(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pRect,
            pOptions);
    }

    // Redirect selected theme classes to their built-in explicit dark
    // variants. This is inversion-only: when the application follows a dark
    // system the native theme data already renders dark (upstream does not
    // redirect OpenThemeData at all, and only maps the scrollbar through
    // Explorer::ScrollBar), so nullptr is returned there and the caller falls
    // back to its native open path.
    //
    // Mapping is restricted to a whitelist of classes known to open
    // successfully on Windows 10, and compound class lists ("A::B") are never
    // touched, so system dialogs keep their own theme data. Everything else
    // (Button, Toolbar, ItemsView, ...) falls through to the hand-drawn
    // overrides in DetouredDrawThemeBackground.
    // Returns nullptr when no redirect applies.
    static HTHEME TryOpenDarkModeThemeData(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList)
    {
        if (!g_GlobalInitialized ||
            !ShouldAppsUseDarkMode() ||
            !::IsThemeInverted() ||
            !pszClassList ||
            L'\0' == pszClassList[0] ||
            nullptr != std::wcschr(pszClassList, L';'))
        {
            return nullptr;
        }

        // Compound dark variants confirmed to exist on Windows 10:
        // DarkMode_Explorer::ScrollBar, DarkMode_ItemsView::Header,
        // DarkMode_ItemsView::ListView, DarkMode_CFD::ComboBox,
        // DarkMode_EditComposited::Edit,
        // DarkMode_SearchBoxComposited::SearchBox,
        // DarkMode_Communications::Rebar.
        LPCWSTR DarkClassList = nullptr;
        if (0 == std::wcscmp(pszClassList, L"ScrollBar"))
        {
            // (already narrowed to Explorer::ScrollBar below)
            DarkClassList = L"DarkMode_Explorer::ScrollBar";
        }
        else if (0 == std::wcscmp(pszClassList, L"Header"))
        {
            DarkClassList = L"DarkMode_ItemsView::Header";
        }
        else if (0 == std::wcscmp(pszClassList, L"ListView"))
        {
            DarkClassList = L"DarkMode_ItemsView::ListView";
        }
        else if (0 == std::wcscmp(pszClassList, L"Combobox"))
        {
            DarkClassList = L"DarkMode_CFD::ComboBox";
        }
        else if (0 == std::wcscmp(pszClassList, L"Edit"))
        {
            DarkClassList = L"DarkMode_EditComposited::Edit";
        }
        else if (0 == std::wcscmp(pszClassList, L"SearchBox") ||
                 0 == std::wcscmp(pszClassList, L"SearchEditBox"))
        {
            DarkClassList = L"DarkMode_SearchBoxComposited::SearchBox";
        }
        else if (0 == std::wcscmp(pszClassList, L"REBAR"))
        {
            DarkClassList = L"DarkMode_Communications::Rebar";
        }

        if (DarkClassList)
        {
            return ::OriginalOpenThemeData(hwnd, DarkClassList);
        }
        return nullptr;
    }

    static HTHEME WINAPI DetouredOpenNcThemeData(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList)
    {
        // Workaround for dark mode scrollbar: redirect the scrollbar theme
        // data through the whitelist in TryOpenDarkModeThemeData, falling
        // back to the Explorer-styled scrollbar (light) on failure.
        if (0 == std::wcscmp(pszClassList, L"ScrollBar"))
        {
            HTHEME DarkTheme = ::TryOpenDarkModeThemeData(nullptr, pszClassList);
            if (DarkTheme)
            {
                return DarkTheme;
            }
            return ::OriginalOpenNcThemeData(nullptr, L"Explorer::ScrollBar");
        }

        HTHEME DarkTheme = ::TryOpenDarkModeThemeData(hwnd, pszClassList);
        if (DarkTheme)
        {
            return DarkTheme;
        }

        return ::OriginalOpenNcThemeData(hwnd, pszClassList);
    }

    static HTHEME WINAPI DetouredOpenThemeData(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList)
    {
        HTHEME DarkTheme = ::TryOpenDarkModeThemeData(hwnd, pszClassList);
        if (DarkTheme)
        {
            return DarkTheme;
        }
        return ::OriginalOpenThemeData(hwnd, pszClassList);
    }

    static HTHEME WINAPI DetouredOpenThemeDataEx(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList,
        _In_ DWORD dwFlags)
    {
        HTHEME DarkTheme = ::TryOpenDarkModeThemeData(hwnd, pszClassList);
        if (DarkTheme)
        {
            return DarkTheme;
        }
        return ::OriginalOpenThemeDataEx(hwnd, pszClassList, dwFlags);
    }

    static HTHEME WINAPI DetouredOpenThemeDataForDpi(
        _In_opt_ HWND hwnd,
        _In_ LPCWSTR pszClassList,
        _In_ UINT dpi)
    {
        HTHEME DarkTheme = ::TryOpenDarkModeThemeData(hwnd, pszClassList);
        if (DarkTheme)
        {
            return DarkTheme;
        }
        return ::OriginalOpenThemeDataForDpi(hwnd, pszClassList, dpi);
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

        g_FunctionTable[FunctionTypes::GetThemeColor].Original =
            ::GetThemeColor;
        g_FunctionTable[FunctionTypes::GetThemeColor].Detoured =
            ::DetouredGetThemeColor;

        g_FunctionTable[FunctionTypes::DrawThemeText].Original =
            ::DrawThemeText;
        g_FunctionTable[FunctionTypes::DrawThemeText].Detoured =
            ::DetouredDrawThemeText;

        g_FunctionTable[FunctionTypes::DrawThemeTextEx].Original =
            ::DrawThemeTextEx;
        g_FunctionTable[FunctionTypes::DrawThemeTextEx].Detoured =
            ::DetouredDrawThemeTextEx;

        g_FunctionTable[FunctionTypes::DrawThemeBackground].Original =
            ::DrawThemeBackground;
        g_FunctionTable[FunctionTypes::DrawThemeBackground].Detoured =
            ::DetouredDrawThemeBackground;

        g_FunctionTable[FunctionTypes::DrawThemeBackgroundEx].Original =
            ::DrawThemeBackgroundEx;
        g_FunctionTable[FunctionTypes::DrawThemeBackgroundEx].Detoured =
            ::DetouredDrawThemeBackgroundEx;

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
            g_FunctionTable[FunctionTypes::OpenThemeData].Original =
                reinterpret_cast<PVOID>(::OpenThemeData);
            g_FunctionTable[FunctionTypes::OpenThemeData].Detoured =
                ::DetouredOpenThemeData;

            g_FunctionTable[FunctionTypes::OpenThemeDataEx].Original =
                reinterpret_cast<PVOID>(::OpenThemeDataEx);
            g_FunctionTable[FunctionTypes::OpenThemeDataEx].Detoured =
                ::DetouredOpenThemeDataEx;

            g_FunctionTable[FunctionTypes::OpenThemeDataForDpi].Original =
                reinterpret_cast<PVOID>(::OpenThemeDataForDpi);
            g_FunctionTable[FunctionTypes::OpenThemeDataForDpi].Detoured =
                ::DetouredOpenThemeDataForDpi;

            if (ModuleHandle)
            {
                PVOID ProcAddress = ::GetProcAddress(
                    ModuleHandle,
                    MAKEINTRESOURCEA(74));
                if (ProcAddress)
                {
                    g_FunctionTable[FunctionTypes::GetThemeClass].Original =
                        ProcAddress;
                    g_FunctionTable[FunctionTypes::GetThemeClass].Detoured =
                        nullptr;
                }
            }
        }

        g_FunctionTable[FunctionTypes::GetThemeSysColor].Original =
            ::GetThemeSysColor;
        g_FunctionTable[FunctionTypes::GetThemeSysColor].Detoured =
            ::DetouredGetThemeSysColor;

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

// The system common file dialogs (IFileOpenDialog) render their content
// through DirectUI which caches the process appearance as soon as the
// dialog object is created. The inverted theme can therefore never style
// those dialogs consistently. The clean solution is to show them with
// their native system appearance: the caller enters this suspend scope
// BEFORE creating the IFileOpenDialog and leaves it after the dialog object
// is released. While suspended the process follows the system appearance
// (g_NativeThemeSuspendCounter) and every inversion-only window hook is
// bypassed, so the whole dialog renders natively.
EXTERN_C MO_RESULT MOAPI K7UserSuspendDarkMode()
{
    if (!g_GlobalInitialized)
    {
        return MO_RESULT_SUCCESS_OK;
    }

    if (1 == ::InterlockedIncrement(&g_NativeThemeSuspendCounter))
    {
        // Clear the inversion state so the inversion-only detours (e.g. the
        // hand-drawn popup menus) pass straight through while the native
        // dialog is shown. It is restored by K7UserRefreshTheme ->
        // ComputeShouldAppsUseDarkMode on resume, which re-reads the setting.
        ::SetThemeInverted(false);

        // Follow the system appearance: the common file dialog renders
        // natively, i.e. dark on a dark system and light on a light system.
        // Forcing one specific mode would split the dialog, because the
        // DirectUI content and the classic controls would resolve different
        // states.
        const bool SystemDarkMode = ::MileShouldAppsUseDarkMode() &&
            !::MileShouldAppsUseHighContrastMode();
        SetShouldAppsUseDarkMode(SystemDarkMode);
        ::K7SetPreferredAppMode(K7PreferredAppMode::Default);
        ::MileRefreshImmersiveColorPolicyState();
    }
    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7UserResumeDarkMode()
{
    if (!g_GlobalInitialized)
    {
        return MO_RESULT_SUCCESS_OK;
    }

    LONG Counter = ::InterlockedDecrement(&g_NativeThemeSuspendCounter);
    if (Counter < 0)
    {
        // Unbalanced resume call; clamp back to the neutral state.
        ::InterlockedExchange(&g_NativeThemeSuspendCounter, 0);
        return MO_RESULT_SUCCESS_OK;
    }
    if (0 == Counter)
    {
        // Recompute the (possibly inverted) application policy and repaint
        // the File Manager windows in case anything drew while suspended.
        ::K7UserRefreshTheme();
    }
    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7UserRefreshTheme()
{
    if (!g_GlobalInitialized)
    {
        return MO_RESULT_SUCCESS_OK;
    }

    bool ShouldUseDarkMode = ::ComputeShouldAppsUseDarkMode();
    SetShouldAppsUseDarkMode(ShouldUseDarkMode);

    ::ApplyProcessThemePolicy(ShouldUseDarkMode);

    ::EnumThreadWindows(
        ::GetCurrentThreadId(),
        [](
            _In_ HWND hWnd,
            _In_ LPARAM lParam) -> BOOL
    {
        UNREFERENCED_PARAMETER(lParam);

        ::MileEnableImmersiveDarkModeForWindow(
            hWnd,
            ShouldAppsUseDarkMode());

        bool ShouldExtendFrame = (
            ShouldAppsUseDarkMode() &&
            ::IsStandardDynamicRangeMode() &&
            g_ThreadContext.MicaBackdropAvailable);

        ::ApplyWindowSystemBackdrop(hWnd, ShouldExtendFrame);

        MARGINS Margins = {};
        if (ShouldExtendFrame)
        {
            Margins = { -1 };
        }
        else if (::IsFileManagerWindow(hWnd))
        {
            UINT DpiValue = ::GetDpiForWindow(hWnd);
            Margins.cyTopHeight =
                ::MulDiv(84, DpiValue, USER_DEFAULT_SCREEN_DPI);
            Margins.cyBottomHeight =
                ::MulDiv(32, DpiValue, USER_DEFAULT_SCREEN_DPI);
        }
        ::DwmExtendFrameIntoClientArea(hWnd, &Margins);

        // The top-level window itself also needs the class-specific theme
        // refresh, not only its children, otherwise the main window keeps
        // its stale theme state after the theme was switched from the
        // settings page.
        ::RefreshWindowTheme(hWnd);

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

        // Repaint the whole window tree synchronously, including nested
        // controls such as the list view's header child window: the XAML
        // island hosts clip their children, so invalidating only the
        // top-level window would leave the refreshed controls on their
        // stale light/dark surface until they are recreated.
        ::RedrawWindow(
            hWnd,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME |
            RDW_ALLCHILDREN | RDW_UPDATENOW);
        return TRUE;
    },
        0);

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7UserInitializeDarkModeSupport()
{
    if (g_GlobalInitialized)
    {
        return MO_RESULT_SUCCESS_OK;
    }

    if (!::MileIsWindowsVersionAtLeast(10, 0, 0))
    {
        // Dark mode is only supported on Windows 10 and above, so we can just
        // return success without doing anything on older versions of Windows.
        return MO_RESULT_SUCCESS_OK;
    }

    if (!::InitializeFunctionTable())
    {
        return MO_RESULT_ERROR_FAIL;
    }

    bool ShouldUseDarkMode = ::ComputeShouldAppsUseDarkMode();
    SetShouldAppsUseDarkMode(ShouldUseDarkMode);
    ::ApplyProcessThemePolicy(ShouldUseDarkMode);

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

    // Watch for window creation so every themed control gets its
    // class-specific dark mode settings applied immediately, independent of
    // when it is created (startup, dialogs opened later, restarts with the
    // inverted theme already enabled, ...).
    ::SetWinEventHook(
        EVENT_OBJECT_CREATE,
        EVENT_OBJECT_CREATE,
        nullptr,
        ::K7UserWinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT);

    return MO_RESULT_SUCCESS_OK;
}

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
