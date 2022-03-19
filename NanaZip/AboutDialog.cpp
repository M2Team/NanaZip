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

#include <string>

#include "../SevenZip/CPP/Common/Common.h"
#include "Mile.Project.Properties.h"
#include "../SevenZip/C/CpuArch.h"
#include "../SevenZip/CPP/7zip/UI/Common/LoadCodecs.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/LangUtils.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/resourceGui.h"

extern CCodecs* g_CodecsObj;

#define IDD_ABOUT  2900
#define IDT_ABOUT_INFO  2901
#define IDB_ABOUT_HOMEPAGE   110

void NanaZip::FileManager::AboutDialog::Show(
    _In_opt_ HWND ParentWindowHandle)
{
    std::wstring HomePageButton = std::wstring(
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

    std::wstring WindowTitle = std::wstring(
        ::LangString(IDD_ABOUT));
    if (WindowTitle.empty())
    {
        WindowTitle = L"About NanaZip";
    }

    std::wstring WindowMainInstruction = std::wstring(
        "NanaZip " MILE_PROJECT_VERSION_STRING);
    WindowMainInstruction.append(
        L" (" MILE_PROJECT_DOT_VERSION_STRING L")");
    WindowMainInstruction.append(
        UString(" (" MY_CPU_NAME ")"));

    std::wstring WindowContent = std::wstring(
        ::LangString(IDT_ABOUT_INFO));
    if (WindowContent.empty())
    {
        WindowContent = L"NanaZip is free software";
    }
#ifdef EXTERNAL_CODECS
    if (g_CodecsObj)
    {
        UString s;
        g_CodecsObj->GetCodecsErrorMessage(s);
        if (!s.IsEmpty())
        {
            WindowContent.append(L"\r\n\r\n");
            WindowContent.append(s);
        }
    }
#endif

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
        nullptr);
}
