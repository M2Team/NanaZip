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

    const int Width = 768;
    const int Height = 400;

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



    /*std::wstring HomePageButton = std::wstring(
        ::LangString(IDB_ABOUT_HOMEPAGE));

    const TASKDIALOG_BUTTON Buttons[] =
    {
        { IDB_ABOUT_HOMEPAGE, L"GitHub" }
    };

    auto TaskDialogCallback = [](
        _In_ HWND hwnd,
        _In_ UINT msg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam,
        _In_ LONG_PTR lpRefData) -> HRESULT
    {
        UNREFERENCED_PARAMETER(hwnd);
        UNREFERENCED_PARAMETER(lParam);
        UNREFERENCED_PARAMETER(lpRefData);

        auto OpenWebSite = [](
            _In_ LPCWSTR Url)
        {
            SHELLEXECUTEINFOW ExecInfo = { 0 };
            ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
            ExecInfo.lpVerb = L"open";
            ExecInfo.lpFile = Url;
            ExecInfo.nShow = SW_SHOWNORMAL;
            ::ShellExecuteExW(&ExecInfo);
        };

        if (msg == TDN_BUTTON_CLICKED && wParam == IDB_ABOUT_HOMEPAGE)
        {
            OpenWebSite(L"https://github.com/M2Team/NanaZip");
            return S_FALSE;
        }

        return S_OK;
    };

    

    TASKDIALOGCONFIG TaskDialogConfig = { 0 };
    TaskDialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    TaskDialogConfig.hwndParent = ParentWindowHandle;
    TaskDialogConfig.hInstance = ::GetModuleHandleW(nullptr);
    TaskDialogConfig.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    TaskDialogConfig.dwCommonButtons = TDCBF_OK_BUTTON;
    TaskDialogConfig.pszWindowTitle = WindowTitle.c_str();
    TaskDialogConfig.pszMainIcon = MAKEINTRESOURCEW(IDI_ICON);
    TaskDialogConfig.pszMainInstruction = WindowMainInstruction.c_str();
    TaskDialogConfig.pszContent = WindowContent.c_str();
    TaskDialogConfig.cButtons = ARRAYSIZE(Buttons);
    TaskDialogConfig.pButtons = Buttons;
    TaskDialogConfig.pfCallback = TaskDialogCallback;

    ::TaskDialogIndirect(
        &TaskDialogConfig,
        nullptr,
        nullptr,
        nullptr);*/
}
