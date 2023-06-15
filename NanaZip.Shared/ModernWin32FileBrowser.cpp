/*
 * PROJECT:   NanaZip
 * FILE:      ModernWin32FileBrowser.cpp
 * PURPOSE:   Implementation for Modern Win32 File Browser
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: reflectronic (john-tur@outlook.com)
 *            MouriNaruto (KurikoMouri@outlook.jp)
 */

#include <Windows.h>
#include <ShlObj_core.h>
#include <winrt/base.h>

EXTERN_C PIDLIST_ABSOLUTE WINAPI ModernSHBrowseForFolderW(LPBROWSEINFOW uType)
{
    winrt::com_ptr<IFileDialog> fileDialog = winrt::try_create_instance<IFileDialog>(CLSID_FileOpenDialog);

    if (!fileDialog)
    {
        return nullptr;
    }

    DWORD flags;
    if (FAILED(fileDialog->GetOptions(&flags)))
    {
        return nullptr;
    }

    if (FAILED(fileDialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM)))
    {
        return nullptr;
    }

    if (FAILED(fileDialog->SetTitle(uType->lpszTitle)))
    {
        return nullptr;
    }

    // 7-Zip does not use the pidlRoot parameter to configure the default folder.
    // Instead, it sets it by configuring the message loop for the shell dialog,
    // passing the path of the default folder through the lParam, and navigating
    // to that folder inside the message loop.
    //
    // Since we cannot augment the IFileDialog's message loop, we will reach into
    // the lParam given to us to find the initial path and hope that 7-Zip does
    // not change this behavior.

    winrt::com_ptr<IShellItem> defaultFolder;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<PCWSTR>(uType->lParam), nullptr, IID_IShellItem, defaultFolder.put_void())))
    {
        return nullptr;
    }

    if (FAILED(fileDialog->SetDefaultFolder(defaultFolder.get())))
    {
        return nullptr;
    }

    HRESULT hr = fileDialog->Show(uType->hwndOwner);
    if (FAILED(hr))
    {
        return nullptr;
    }

    winrt::com_ptr<IShellItem> result;
    if (FAILED(fileDialog->GetResult(result.put())))
    {
        return nullptr;
    }

    LPITEMIDLIST pidl;
    if (FAILED(SHGetIDListFromObject(result.get(), &pidl)))
    {
        return nullptr;
    }

    return pidl;
}

#if defined(_M_IX86)
#pragma warning(suppress:4483)
extern "C" __declspec(selectany) void const* const __identifier("_imp__SHBrowseForFolderW@4") = reinterpret_cast<void const*>(::ModernSHBrowseForFolderW);
#pragma comment(linker, "/include:__imp__SHBrowseForFolderW@4")
#else
extern "C" __declspec(selectany) void const* const __imp_SHBrowseForFolderW = reinterpret_cast<void const*>(::ModernSHBrowseForFolderW);
#pragma comment(linker, "/include:__imp_SHBrowseForFolderW")
#endif
