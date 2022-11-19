/*
 * PROJECT:   NanaZip
 * FILE:      AboutDialog.cpp
 * PURPOSE:   Implementation for About Dialog
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "AboutDialog.h"

#include <CommCtrl.h>
#pragma comment(lib,"comctl32.lib")

#include "pch.h"
#include "AboutPage.h"

void NanaZip::FileManager::AboutDialog::Show(
    _In_opt_ HWND ParentWindowHandle)
{
    winrt::NanaZip::AboutPage XamlWindowContent =
        winrt::make<winrt::NanaZip::implementation::AboutPage>();

    HWND WindowHandle = ::CreateWindowExW(
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
        ::GetModuleHandleW(nullptr),
        winrt::get_abi(XamlWindowContent));
    if (!WindowHandle)
    {
        return;
    }

    HMENU MenuHandle = ::GetSystemMenu(WindowHandle, FALSE);
    if (MenuHandle)
    {
        ::RemoveMenu(MenuHandle, 0, MF_SEPARATOR);
        ::RemoveMenu(MenuHandle, SC_RESTORE, MF_BYCOMMAND);
        ::RemoveMenu(MenuHandle, SC_SIZE, MF_BYCOMMAND);
        ::RemoveMenu(MenuHandle, SC_MINIMIZE, MF_BYCOMMAND);
        ::RemoveMenu(MenuHandle, SC_MAXIMIZE, MF_BYCOMMAND);
        ::RemoveMenu(MenuHandle, SC_TASKLIST, MF_BYCOMMAND);
    }

    const int Width = 600;
    const int Height = 192 + (32 + 8) * 2;

    UINT DpiValue = ::GetDpiForWindow(WindowHandle);

    int ScaledWidth = ::MulDiv(Width, DpiValue, USER_DEFAULT_SCREEN_DPI);
    int ScaledHeight = ::MulDiv(Height, DpiValue, USER_DEFAULT_SCREEN_DPI);

    RECT ParentWindowRect;
    ::GetWindowRect(ParentWindowHandle, &ParentWindowRect);

    int ParentWidth = ParentWindowRect.right - ParentWindowRect.left;
    int ParentHeight = ParentWindowRect.bottom - ParentWindowRect.top;

    ::SetWindowPos(
        WindowHandle,
        nullptr,
        ParentWindowRect.left + ((ParentWidth - ScaledWidth) / 2),
        ParentWindowRect.top + ((ParentHeight - ScaledHeight) / 2),
        ScaledWidth,
        ScaledHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
    ::ShowWindow(WindowHandle, SW_SHOW);
    ::UpdateWindow(WindowHandle);

    ::EnableWindow(ParentWindowHandle, FALSE);

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

    ::EnableWindow(ParentWindowHandle, TRUE);
    ::SetActiveWindow(ParentWindowHandle);
}
