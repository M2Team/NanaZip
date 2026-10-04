/*
 * PROJECT:    NanaZip.Modern
 * FILE:       NanaZip.Modern.cpp
 * PURPOSE:    Implementation for NanaZip Modern Experience
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "pch.h"

#include "NanaZip.Modern.h"

#include <Mile.Helpers.h>
#include <Mile.Xaml.h>

#include "App.h"
#include "SponsorPage.h"
#include "AboutPage.h"
#include "InformationPage.h"
#include "ProgressPage.h"
#include "CopyLocationPage.h"

#pragma comment(lib, "comctl32.lib")

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <cwchar>
#include <iterator>
#include <mutex>
#include <map>
#include <utility>
#include <vector>

namespace winrt
{
    using Windows::ApplicationModel::Resources::Core::ResourceManager;
    using Windows::ApplicationModel::Resources::Core::ResourceMap;
}

namespace
{
    static winrt::ResourceMap GetMainResourceMap()
    {
        static winrt::ResourceMap CachedResult = ([]() -> winrt::ResourceMap
        {
            try
            {
                return winrt::ResourceManager::Current().MainResourceMap();
            }
            catch (...)
            {
                // Do nothing.
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static std::mutex g_CachedLanguageStringResourcesMutex;
    static std::map<UINT32, winrt::hstring> g_CachedLanguageStringResources;
}

EXTERN_C LPCWSTR WINAPI K7ModernGetLegacyStringResource(
    _In_ UINT32 ResourceId)
{
    {
        std::lock_guard Lock(g_CachedLanguageStringResourcesMutex);
        auto Iterator = g_CachedLanguageStringResources.find(ResourceId);
        if (g_CachedLanguageStringResources.end() != Iterator)
        {
            return Iterator->second.c_str();
        }
    }

    static winrt::ResourceMap LegacyResourceMap = ([]() -> winrt::ResourceMap
    {
        winrt::ResourceMap MainResourceMap = ::GetMainResourceMap();
        if (MainResourceMap)
        {
            return MainResourceMap.GetSubtree(L"Legacy");
        }
        return nullptr;
    }());
    if (!LegacyResourceMap)
    {
        return nullptr;
    }

    winrt::hstring ResourceName = L"Resource" + winrt::to_hstring(ResourceId);
    if (!LegacyResourceMap.HasKey(ResourceName))
    {
        return nullptr;
    }

    winrt::hstring Content = LegacyResourceMap.Lookup(
        ResourceName).Candidates().GetAt(0).ValueAsString();
    std::lock_guard Lock(g_CachedLanguageStringResourcesMutex);
    auto Iterator = g_CachedLanguageStringResources.emplace(
        ResourceId,
        std::move(Content));
    return Iterator.first->second.c_str();
}

namespace
{
    static winrt::NanaZip::Modern::App g_AppInstance = nullptr;
}

namespace
{
    static bool K7ModernReadThemeInvert()
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
            if (ERROR_SUCCESS == ::RegQueryValueExW(
                KeyHandle,
                L"InvertTheme",
                nullptr,
                nullptr,
                reinterpret_cast<LPBYTE>(&Value),
                &ValueSize))
            {
                // The value exists, use it as is.
            }
            else
            {
                Value = 0;
            }
            ::RegCloseKey(KeyHandle);
        }

        return (Value != 0);
    }

    static winrt::Windows::UI::Xaml::ApplicationTheme K7ModernComputeTheme()
    {
        const bool BaseShouldUseDarkMode =
            ::MileShouldAppsUseDarkMode() &&
            !::MileShouldAppsUseHighContrastMode();
        const bool InvertTheme = ::K7ModernReadThemeInvert();
        const bool UseDark = InvertTheme ? !BaseShouldUseDarkMode
                                         : BaseShouldUseDarkMode;
        return UseDark ?
            winrt::Windows::UI::Xaml::ApplicationTheme::Dark :
            winrt::Windows::UI::Xaml::ApplicationTheme::Light;
    }

    static void K7ModernApplyXamlWindowTheme(
        _In_ HWND WindowHandle,
        _In_ winrt::Windows::UI::Xaml::ApplicationTheme Theme)
    {
        winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource XamlSource =
            nullptr;
        winrt::copy_from_abi(
            XamlSource,
            ::GetPropW(WindowHandle, L"XamlWindowSource"));
        if (!XamlSource)
        {
            return;
        }

        try
        {
            auto Content = XamlSource.Content();
            if (Content)
            {
                winrt::Windows::UI::Xaml::FrameworkElement RootElement =
                    Content.try_as<
                        winrt::Windows::UI::Xaml::FrameworkElement>();
                if (RootElement)
                {
                    // FrameworkElement.RequestedTheme takes ElementTheme
                    // which cannot be implicitly converted from
                    // ApplicationTheme.
                    RootElement.RequestedTheme(
                        (winrt::Windows::UI::Xaml::ApplicationTheme::Dark
                            == Theme)
                            ? winrt::Windows::UI::Xaml::ElementTheme::Dark
                            : winrt::Windows::UI::Xaml::ElementTheme::Light);
                    // The islands deliberately leave regions of their XAML
                    // content transparent (e.g. the address bar bottom), so
                    // the DWM backdrop shows through. The backdrop follows
                    // the SYSTEM theme and turned those regions into an
                    // opaque black bar while the inverted theme had the
                    // application light on a dark-theme system. Give every
                    // island root an opaque background that follows the
                    // application theme instead. Only the islands created
                    // with the Mile.Xaml.ContentWindow class are touched;
                    // popup sources keep their own self-painted flyout
                    // presenters.
                    WCHAR WindowClassName[64] = {};
                    if (::GetClassNameW(
                        WindowHandle,
                        WindowClassName,
                        (int)(std::size(WindowClassName))) &&
                        0 == std::wcscmp(
                            WindowClassName,
                            L"Mile.Xaml.ContentWindow"))
                    {
                        const winrt::Windows::UI::Xaml::Media::SolidColorBrush
                            IslandBackgroundBrush =
                                winrt::Windows::UI::Xaml::Media::SolidColorBrush(
                                    (winrt::Windows::UI::Xaml::ApplicationTheme::Dark == Theme)
                                        ? winrt::Windows::UI::Color{
                                              0xFF, 0x20, 0x20, 0x20 }
                                        : winrt::Windows::UI::Color{
                                              0xFF, 0xFF, 0xFF, 0xFF });
                        auto TryPaintBackground = [&](
                            winrt::Windows::UI::Xaml::FrameworkElement const&
                                Element) -> bool
                        {
                            if (auto PanelRoot = Element.try_as<
                                winrt::Windows::UI::Xaml::Controls::Panel>())
                            {
                                PanelRoot.Background(IslandBackgroundBrush);
                                return true;
                            }
                            if (auto ControlRoot = Element.try_as<
                                winrt::Windows::UI::Xaml::Controls::Control>())
                            {
                                ControlRoot.Background(IslandBackgroundBrush);
                                return true;
                            }
                            if (auto BorderRoot = Element.try_as<
                                winrt::Windows::UI::Xaml::Controls::Border>())
                            {
                                BorderRoot.Background(IslandBackgroundBrush);
                                return true;
                            }
                            if (auto PresenterRoot = Element.try_as<
                                winrt::Windows::UI::Xaml::Controls::ContentPresenter>())
                            {
                                PresenterRoot.Background(IslandBackgroundBrush);
                                return true;
                            }
                            return false;
                        };
                        // The content root type varies per island. Try the
                        // root first and walk the first two layers of the
                        // visual tree otherwise.
                        if (!TryPaintBackground(RootElement))
                        {
                            bool Painted = false;
                            try
                            {
                                std::vector<
                                    winrt::Windows::UI::Xaml::DependencyObject>
                                    NodesToVisit{ RootElement };
                                for (int Layer = 0;
                                    Layer < 2 && !Painted && !NodesToVisit.empty();
                                    ++Layer)
                                {
                                    std::vector<
                                        winrt::Windows::UI::Xaml::DependencyObject>
                                        NextLayer;
                                    for (const auto& Node : NodesToVisit)
                                    {
                                        const int Count =
                                            winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(
                                                Node);
                                        for (int Index = 0;
                                            Index < Count && !Painted;
                                            ++Index)
                                        {
                                            auto Child =
                                                winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(
                                                    Node,
                                                    Index);
                                            if (auto ChildElement = Child.try_as<
                                                winrt::Windows::UI::Xaml::FrameworkElement>())
                                            {
                                                Painted = TryPaintBackground(
                                                    ChildElement);
                                            }
                                            if (!Painted)
                                            {
                                                NextLayer.push_back(Child);
                                            }
                                        }
                                    }
                                    NodesToVisit = std::move(NextLayer);
                                }
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                }
            }
        }
        catch (...)
        {
            // Ignore windows whose XAML content is not ready yet.
        }
    }

    static void K7ModernApplyTheme()
    {
        if (!g_AppInstance)
        {
            return;
        }
        const winrt::Windows::UI::Xaml::ApplicationTheme Theme =
            ::K7ModernComputeTheme();
        // Application.RequestedTheme throws once XAML content exists (and
        // before the XAML framework is initialized in this process), which
        // aborted this whole function and silently skipped the per-island
        // refresh below. The setter is best-effort only; the per-island
        // RootElement.RequestedTheme + WM_SETTINGCHANGE path is what
        // actually switches live XAML Islands.
        try
        {
            winrt::Windows::UI::Xaml::Application::Current().RequestedTheme(
                Theme);
        }
        catch (...)
        {
            // RequestedTheme is best-effort; ignore if it is unavailable.
        }

        // XAML Islands do not refresh already loaded DesktopWindowXamlSource
        // contents when Application.RequestedTheme changes at runtime, so
        // the root element theme of every hosted XAML window has to be
        // updated directly.
        std::vector<HWND> XamlWindows;
        ::EnumThreadWindows(
            ::GetCurrentThreadId(),
            [](
                _In_ HWND hWnd,
                _In_ LPARAM lParam) -> BOOL
        {
            auto Windows = reinterpret_cast<std::vector<HWND>*>(lParam);

            Windows->push_back(hWnd);

            ::EnumChildWindows(
                hWnd,
                [](
                    _In_ HWND hWnd,
                    _In_ LPARAM lParam) -> BOOL
            {
                auto Windows = reinterpret_cast<std::vector<HWND>*>(lParam);

                Windows->push_back(hWnd);

                return TRUE;
            },
                lParam);

            return TRUE;
        },
            reinterpret_cast<LPARAM>(&XamlWindows));

        for (HWND WindowHandle : XamlWindows)
        {
            ::K7ModernApplyXamlWindowTheme(WindowHandle, Theme);
        }

        // XAML Islands re-evaluate their theme resources when they receive
        // WM_SETTINGCHANGE with the ImmersiveColorSet section, which is also
        // the message the file manager forwards to its toolbar window when
        // the system color changes. RequestedTheme changes alone do not
        // refresh already loaded island contents, leaving the CommandBar
        // with stale foreground brushes (invisible icons).
        // Re-entrancy guard: the file manager WM_SETTINGCHANGE handler calls
        // K7ModernRefreshTheme, so a XAML window forwarding this message
        // back to its parent must not start an infinite recursion.
        static bool SendingSettingChange = false;
        if (!SendingSettingChange)
        {
            SendingSettingChange = true;
            for (HWND WindowHandle : XamlWindows)
            {
                if (::GetPropW(WindowHandle, L"XamlWindowSource"))
                {
                    ::SendMessageW(
                        WindowHandle,
                        WM_SETTINGCHANGE,
                        0,
                        reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
                }
            }
            SendingSettingChange = false;
        }
    }
}

EXTERN_C BOOL WINAPI K7ModernAvailable()
{
    return nullptr != g_AppInstance;
}

EXTERN_C HRESULT WINAPI K7ModernInitialize()
{
    if (g_AppInstance)
    {
        return S_OK;
    }
    if (!::GetMainResourceMap())
    {
        // NanaZip.Modern requires resources.pri to get XAML resources.
        return E_NOINTERFACE;
    }
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        using Implementation = winrt::NanaZip::Modern::implementation::App;
        g_AppInstance = winrt::make<Implementation>();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
    // Refresh the XAML theme with best effort. It must not fail the
    // initialization because the XAML island may not be ready for
    // Application::RequestedTheme at this point.
    ::K7ModernRefreshTheme();
    return S_OK;
}

EXTERN_C HRESULT WINAPI K7ModernUninitialize()
{
    if (!g_AppInstance)
    {
        return S_OK;
    }
    try
    {
        g_AppInstance.Close();
        g_AppInstance = nullptr;
        winrt::uninit_apartment();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
    return S_OK;
}

EXTERN_C VOID WINAPI K7ModernRefreshTheme()
{
    // Best effort: the XAML framework or an island may not be ready when
    // this is called during initialization or teardown.
    try
    {
        ::K7ModernApplyTheme();
    }
    catch (...)
    {
    }
}

namespace winrt
{
    using Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
}

namespace
{
    HWND K7ModernCreateXamlWindow(
        _In_opt_ HWND ParentWindowHandle,
        _In_ DWORD ExtendedWindowStyle,
        _In_ DWORD WindowStyle)
    {
        HWND WindowHandle = ::CreateWindowExW(
            ExtendedWindowStyle,
            L"Mile.Xaml.ContentWindow",
            nullptr,
            WindowStyle,
            CW_USEDEFAULT,
            0,
            CW_USEDEFAULT,
            0,
            ParentWindowHandle,
            nullptr,
            nullptr,
            nullptr);
        if (!WindowHandle)
        {
            return nullptr;
        }
        if (!::SetWindowSubclass(
            WindowHandle,
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
                HWND ParentWindow = ::GetWindow(hWnd, GW_OWNER);
                if (ParentWindow)
                {
                    ::EnableWindow(ParentWindow, TRUE);
                }
                break;
            }
            default:
                break;
            }

            return ::DefSubclassProc(
                hWnd,
                uMsg,
                wParam,
                lParam);
        },
            0,
            0))
        {
            ::DestroyWindow(WindowHandle);
            return nullptr;
        }
        return WindowHandle;
    }

    HWND K7ModernCreateXamlDialog(
        _In_opt_ HWND ParentWindowHandle)
    {
        HWND WindowHandle = ::K7ModernCreateXamlWindow(
            ParentWindowHandle,
            WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME,
            WS_CAPTION | WS_SYSMENU);

        MILE_WINDOW_SYSTEM_BACKDROP_TYPE SystemBackdropType =
            MILE_WINDOW_SYSTEM_BACKDROP_TYPE_AUTO;
        if (S_OK == ::MileGetWindowSystemBackdropTypeAttribute(
            WindowHandle,
            &SystemBackdropType))
        {
            if (MILE_WINDOW_SYSTEM_BACKDROP_TYPE_AUTO != SystemBackdropType &&
                MILE_WINDOW_SYSTEM_BACKDROP_TYPE_NONE != SystemBackdropType)
            {
                const COLORREF IgnoreAccentColor = static_cast<COLORREF>(-2);
                ::MileSetWindowCaptionColorAttribute(
                    WindowHandle,
                    IgnoreAccentColor);
            }
        }

        return WindowHandle;
    }

    int K7ModernShowXamlWindow(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ HWND ParentWindowHandle)
    {
        if (!WindowHandle)
        {
            return -1;
        }

        UINT DpiValue = ::GetDpiForWindow(WindowHandle);

        int ScaledWidth = ::MulDiv(Width, DpiValue, USER_DEFAULT_SCREEN_DPI);
        int ScaledHeight = ::MulDiv(Height, DpiValue, USER_DEFAULT_SCREEN_DPI);

        RECT ParentRect = {};
        if (ParentWindowHandle)
        {
            ::GetWindowRect(ParentWindowHandle, &ParentRect);
        }
        else
        {
            HMONITOR MonitorHandle = ::MonitorFromWindow(
                WindowHandle,
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
            WindowHandle,
            nullptr,
            ParentRect.left + ((ParentWidth - ScaledWidth) / 2),
            ParentRect.top + ((ParentHeight - ScaledHeight) / 2),
            ScaledWidth,
            ScaledHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);

        ::ShowWindow(WindowHandle, SW_SHOW);
        ::UpdateWindow(WindowHandle);

        return ::MileXamlContentWindowDefaultMessageLoop();
    }

    int K7ModernShowXamlDialog(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ LPVOID Content,
        _In_ HWND ParentWindowHandle)
    {
        if (!WindowHandle)
        {
            return -1;
        }

        ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

        HMENU MenuHandle = ::GetSystemMenu(WindowHandle, FALSE);
        if (MenuHandle)
        {
            ::RemoveMenu(MenuHandle, 0, MF_SEPARATOR);
            ::RemoveMenu(MenuHandle, SC_RESTORE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_SIZE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_MINIMIZE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_MAXIMIZE, MF_BYCOMMAND);
        }

        if (ParentWindowHandle)
        {
            ::EnableWindow(ParentWindowHandle, FALSE);
        }

        if (FAILED(::MileXamlSetXamlContentForContentWindow(
            WindowHandle,
            Content)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }

        int Result = ::K7ModernShowXamlWindow(
            WindowHandle,
            Width,
            Height,
            ParentWindowHandle);

        return Result;
    }

    winrt::DesktopWindowXamlSource K7ModernGetDesktopWindowXamlSource(
        _In_ HWND WindowHandle)
    {
        winrt::DesktopWindowXamlSource XamlSource = nullptr;
        winrt::copy_from_abi(
            XamlSource,
            ::GetPropW(WindowHandle, L"XamlWindowSource"));
        return XamlSource;
    }
}

EXTERN_C INT WINAPI K7ModernShowSponsorDialog(
    _In_opt_ HWND ParentWindowHandle)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::SponsorPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::SponsorPage;

    Interface Window = winrt::make<Implementation>(WindowHandle);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        460,
        320,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowAboutDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::AboutPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::AboutPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        ExtendedMessage);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        480,
        320,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowInformationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Content)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::InformationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::InformationPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        Content);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        560,
        560,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C VOID WINAPI K7ModernUpdateProgressWindowStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_PROGRESS_WINDOW_STATUS Status)
{
    if (!WindowHandle || !Status)
    {
        return;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return;
    }

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return;
    }
    winrt::get_self<Implementation>(InstanceObject)->UpdateStatus(Status);
}

EXTERN_C VOID WINAPI K7ModernSetProgressWindowPausedMode(
    _In_ HWND WindowHandle,
    _In_ BOOL Paused)
{
    if (!WindowHandle)
    {
        return;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return;
    }

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return;
    }
    winrt::get_self<Implementation>(InstanceObject)->SetPausedMode(Paused);
}

EXTERN_C INT WINAPI K7ModernShowProgressWindow(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_ BOOL ShowCompressionInformation,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
{
    HWND WindowHandle = ::K7ModernCreateXamlWindow(
        ParentWindowHandle,
        WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME,
        WS_OVERLAPPEDWINDOW);
    if (!WindowHandle)
    {
        return -1;
    }

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, FALSE);

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        ShowCompressionInformation);

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    if (WindowSubclassHandler)
    {
        if (!::SetWindowSubclass(
            WindowHandle,
            WindowSubclassHandler,
            1,
            reinterpret_cast<DWORD_PTR>(WindowSubclassContext)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        600,
        360,
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowCopyLocationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Subtitle,
    _In_opt_ LPCWSTR AdditionalInformation,
    _In_opt_ LPCWSTR InitialPath,
    _In_ BOOL ShowExtractAll,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::CopyLocationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CopyLocationPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        Subtitle,
        AdditionalInformation,
        InitialPath,
        ShowExtractAll);

    if (WindowSubclassHandler)
    {
        if (!::SetWindowSubclass(
            WindowHandle,
            WindowSubclassHandler,
            1,
            reinterpret_cast<DWORD_PTR>(WindowSubclassContext)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }
    }

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        600,
        400,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C LPCWSTR WINAPI K7ModernGetCopyLocationDialogPath(
    _In_ HWND WindowHandle)
{
    if (!WindowHandle)
    {
        return nullptr;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return nullptr;
    }

    using Interface =
        winrt::NanaZip::Modern::CopyLocationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CopyLocationPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return nullptr;
    }
    return winrt::get_self<Implementation>(InstanceObject)->GetPath();
}
