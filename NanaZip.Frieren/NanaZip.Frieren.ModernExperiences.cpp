/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Frieren.ModernExperiences.cpp
 * PURPOSE:    Implementation for NanaZip Modern Experiences
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 *             reflectronic (john-tur@outlook.com)
 */

#include <Windows.h>

#include <CommCtrl.h>
#pragma comment(lib,"comctl32.lib")

#include <ShlObj_core.h>

namespace
{
    int WINAPI OriginalMessageBoxW(
        _In_opt_ HWND hWnd,
        _In_opt_ LPCWSTR lpText,
        _In_opt_ LPCWSTR lpCaption,
        _In_ UINT uType)
    {
        static decltype(::MessageBoxW)* ProcAddress =
            reinterpret_cast<decltype(::MessageBoxW)*>(::GetProcAddress(
                ::GetModuleHandleW(L"user32.dll"),
                "MessageBoxW"));
        if (ProcAddress)
        {
            return ProcAddress(hWnd, lpText, lpCaption, uType);
        }
        else
        {
            ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
            return 0;
        }
    }
}

EXTERN_C int WINAPI NanaZipFrierenModernMessageBoxW(
    _In_opt_ HWND hWnd,
    _In_opt_ LPCWSTR lpText,
    _In_opt_ LPCWSTR lpCaption,
    _In_ UINT uType)
{
    if (uType != (uType & (MB_ICONMASK | MB_TYPEMASK)))
    {
        return ::OriginalMessageBoxW(hWnd, lpText, lpCaption, uType);
    }

    TASKDIALOGCONFIG TaskDialogConfig = {};

    TaskDialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    TaskDialogConfig.hwndParent = hWnd;
    TaskDialogConfig.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    TaskDialogConfig.pszWindowTitle = lpCaption;
    TaskDialogConfig.pszMainInstruction = lpText;

    switch (uType & MB_TYPEMASK)
    {
    case MB_OK:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_OK_BUTTON;
        break;
    case MB_OKCANCEL:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
        break;
    case MB_YESNOCANCEL:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
        break;
    case MB_YESNO:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
        break;
    case MB_RETRYCANCEL:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON;
        break;
    default:
        return ::OriginalMessageBoxW(hWnd, lpText, lpCaption, uType);
    }

    switch (uType & MB_ICONMASK)
    {
    case MB_ICONHAND:
        TaskDialogConfig.pszMainIcon = TD_ERROR_ICON;
        break;
    case MB_ICONQUESTION:
        TaskDialogConfig.dwFlags |= TDF_USE_HICON_MAIN;
        TaskDialogConfig.hMainIcon = ::LoadIconW(nullptr, IDI_QUESTION);
        break;
    case MB_ICONEXCLAMATION:
        TaskDialogConfig.pszMainIcon = TD_WARNING_ICON;
        break;
    case MB_ICONASTERISK:
        TaskDialogConfig.pszMainIcon = TD_INFORMATION_ICON;
        break;
    default:
        break;
    }

    int ButtonID = 0;

    HRESULT hr = ::TaskDialogIndirect(
        &TaskDialogConfig,
        &ButtonID,
        nullptr,
        nullptr);

    if (ButtonID == 0)
    {
        ::SetLastError(hr);
    }

    return ButtonID;
}

EXTERN_C PIDLIST_ABSOLUTE WINAPI NanaZipFrierenModernSHBrowseForFolderW(
    _In_ LPBROWSEINFOW lpbi)
{
    LPITEMIDLIST IDList = nullptr;

    IFileDialog* FileDialog = nullptr;
    if (SUCCEEDED(::CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&FileDialog))))
    {
        DWORD Flags = 0;
        if (SUCCEEDED(FileDialog->GetOptions(
            &Flags)))
        {
            if (SUCCEEDED(FileDialog->SetOptions(
                Flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM)))
            {
                if (SUCCEEDED(FileDialog->SetTitle(
                    lpbi->lpszTitle)))
                {
                    // 7-Zip does not use the pidlRoot parameter to configure
                    // the default folder. Instead, it sets it by configuring
                    // the message loop for the shell dialog, passing the path
                    // of the default folder through the lParam, and navigating
                    // to that folder inside the message loop.
                    //
                    // Since we cannot augment the IFileDialog's message loop,
                    // we will reach into the lParam given to us to find the
                    // initial path and hope that 7-Zip does not change this
                    // behavior.

                    IShellItem* DefaultFolder = nullptr;
                    if (SUCCEEDED(::SHCreateItemFromParsingName(
                        reinterpret_cast<PCWSTR>(lpbi->lParam),
                        nullptr,
                        IID_PPV_ARGS(&DefaultFolder))))
                    {
                        FileDialog->SetDefaultFolder(DefaultFolder);

                        DefaultFolder->Release();
                    }

                    if (SUCCEEDED(FileDialog->Show(
                        lpbi->hwndOwner)))
                    {
                        IShellItem* Result = nullptr;
                        if (SUCCEEDED(FileDialog->GetResult(
                            &Result)))
                        {
                            ::SHGetIDListFromObject(Result, &IDList);

                            Result->Release();
                        }
                    }
                }
            }
        }

        FileDialog->Release();
    }

    return IDList;
}
