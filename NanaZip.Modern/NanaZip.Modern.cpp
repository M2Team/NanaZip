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

#pragma comment(lib, "comctl32.lib")

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

#include <mutex>
#include <map>

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
            case WM_DESTROY:
            {
                winrt::DesktopWindowXamlSource XamlSource = nullptr;
                winrt::copy_from_abi(
                    XamlSource,
                    ::GetPropW(hWnd, L"XamlWindowSource"));
                // Clear the property first to avoid use-after-free issue.
                ::RemovePropW(hWnd, L"XamlWindowSource");
                if (XamlSource)
                {
                    // Release the reference count from SetPropW.
                    XamlSource.as<IUnknown>()->Release();
                    // Close the XAML Island.
                    XamlSource.Close();
                    // Destroy the content immediately to avoid unintended access.
                    XamlSource = nullptr;
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
        return ::K7ModernCreateXamlWindow(
            ParentWindowHandle,
            WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME,
            WS_CAPTION | WS_SYSMENU);
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
