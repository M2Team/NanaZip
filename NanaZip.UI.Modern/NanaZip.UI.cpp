/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.UI.cpp
 * PURPOSE:   Implementation for NanaZip Modern UI Shared Infrastructures
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.UI.h"

#include <Mile.Helpers.h>
#include <Mile.Xaml.h>

#include "AboutPage.h"

HWND NanaZip::UI::CreateXamlDialog(
    _In_opt_ HWND ParentWindowHandle)
{
    return ::CreateWindowExW(
        WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME,
        L"Mile.Xaml.ContentWindow",
        nullptr,
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        0,
        CW_USEDEFAULT,
        0,
        ParentWindowHandle,
        nullptr,
        nullptr,
        nullptr);
}

int NanaZip::UI::ShowXamlWindow(
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

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        Content)))
    {
        return -1;
    }

    UINT DpiValue = ::GetDpiForWindow(WindowHandle);

    int ScaledWidth = ::MulDiv(Width, DpiValue, USER_DEFAULT_SCREEN_DPI);
    int ScaledHeight = ::MulDiv(Height, DpiValue, USER_DEFAULT_SCREEN_DPI);

    RECT ParentRect = { 0 };
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

    MSG Message;
    while (::GetMessageW(&Message, nullptr, 0, 0))
    {
        // Workaround for capturing Alt+F4 in applications with XAML Islands.
        // Reference: https://github.com/microsoft/microsoft-ui-xaml/issues/2408
        if (Message.message == WM_SYSKEYDOWN && Message.wParam == VK_F4)
        {
            ::SendMessageW(
                ::GetAncestor(Message.hwnd, GA_ROOT),
                Message.message,
                Message.wParam,
                Message.lParam);

            continue;
        }

        ::TranslateMessage(&Message);
        ::DispatchMessageW(&Message);
    }

    return static_cast<int>(Message.wParam);
}

int NanaZip::UI::ShowXamlDialog(
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

    if (FAILED(::MileAllowNonClientDefaultDrawingForWindow(
        WindowHandle,
        FALSE)))
    {
        return -1;
    }

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

    int Result = NanaZip::UI::ShowXamlWindow(
        WindowHandle,
        Width,
        Height,
        Content,
        ParentWindowHandle);

    if (ParentWindowHandle)
    {
        ::EnableWindow(ParentWindowHandle, TRUE);
        ::SetForegroundWindow(ParentWindowHandle);
        ::SetActiveWindow(ParentWindowHandle);
    }

    return Result;
}

winrt::handle NanaZip::UI::ShowAboutDialog(
    _In_ HWND ParentWindowHandle)
{
    return winrt::handle(Mile::CreateThread([=]()
    {
        winrt::check_hresult(::MileXamlThreadInitialize());

        HWND WindowHandle = NanaZip::UI::CreateXamlDialog(ParentWindowHandle);
        if (!WindowHandle)
        {
            return;
        }

        winrt::NanaZip::Modern::AboutPage Window =
            winrt::make<winrt::NanaZip::Modern::implementation::AboutPage>(
                WindowHandle);
        NanaZip::UI::ShowXamlDialog(
            WindowHandle,
            600, // 480,
            192 + (32 + 8) * 2, // 320,
            winrt::get_abi(Window),
            ParentWindowHandle);

        winrt::check_hresult(::MileXamlThreadUninitialize());
    }));
}
