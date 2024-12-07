/*
 * PROJECT:   NanaZip
 * FILE:      ModernWin32FileBrowser.cpp
 * PURPOSE:   Implementation for Modern Win32 File Browser
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 *             reflectronic (john-tur@outlook.com)
 */

#include <Windows.h>
#include <ShlObj_core.h>

EXTERN_C PIDLIST_ABSOLUTE WINAPI ModernSHBrowseForFolderW(
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

#if defined(_M_IX86)
#pragma warning(suppress:4483)
extern "C" __declspec(selectany) void const* const __identifier("_imp__SHBrowseForFolderW@4") = reinterpret_cast<void const*>(::ModernSHBrowseForFolderW);
#pragma comment(linker, "/include:__imp__SHBrowseForFolderW@4")
#else
extern "C" __declspec(selectany) void const* const __imp_SHBrowseForFolderW = reinterpret_cast<void const*>(::ModernSHBrowseForFolderW);
#pragma comment(linker, "/include:__imp_SHBrowseForFolderW")
#endif
