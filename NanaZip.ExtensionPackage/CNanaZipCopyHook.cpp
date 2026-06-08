/*
 * PROJECT:   NanaZip
 * FILE:      CNanaZipCopyHook.cpp
 * PURPOSE:   Copy hook implementation
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <exception>
#include <filesystem>
#include <Unknwn.h>
#include <olectl.h>
#include <shellapi.h>
#include <strsafe.h>

#include "CopyHook.h"
#include "CNanaZipCopyHook.hpp"

IFACEMETHODIMP_(UINT)
CNanaZipCopyHook::CopyCallback(
    _In_opt_ HWND hwnd,
    UINT wFunc,
    UINT wFlags,
    _In_ PCWSTR pszSrcFile,
    DWORD dwSrcAttribs,
    _In_opt_ PCWSTR pszDestFile,
    DWORD dwDestAttribs) {

    UNREFERENCED_PARAMETER(wFlags);
    UNREFERENCED_PARAMETER(dwSrcAttribs);
    UNREFERENCED_PARAMETER(dwDestAttribs);

    try {
        if (wFunc != FO_COPY && wFunc != FO_MOVE)
            return IDYES;

        std::filesystem::path SourcePath(pszSrcFile);
        std::filesystem::path DestinationPath(pszDestFile);
        if (SourcePath.stem() != COPY_HOOK_PREFIX)
            return IDYES;

        auto HwndString = SourcePath.extension().wstring();
        if (!HwndString.length())
            return IDYES;

        auto DestinationHwnd = reinterpret_cast<HWND>(std::stoull(
            HwndString.substr(1)));
        auto DestinationDir = DestinationPath.parent_path();
        COPY_HOOK_DATA CopyHookData = {};
        if (FAILED(::StringCchCopyW(
            &CopyHookData.FileName[0],
            ARRAYSIZE(CopyHookData.FileName),
            DestinationDir.c_str())))
            return IDYES;

        COPYDATASTRUCT CopyMessageData;
        DWORD_PTR MessageResult;

        CopyMessageData.dwData = COPY_HOOK_COMMAND_COPY;
        CopyMessageData.cbData = sizeof(COPY_HOOK_DATA);
        CopyMessageData.lpData = &CopyHookData;

        // we can't use PostMessage since the copydata message needs to stay
        // alive during the send
        ::SendMessageTimeoutW(
            DestinationHwnd,
            WM_COPYDATA,
            reinterpret_cast<WPARAM>(hwnd),
            reinterpret_cast<LPARAM>(&CopyMessageData),
            SMTO_ABORTIFHUNG,
            100,
            &MessageResult);
        // but it doesn't matter if the caller returns or not
        return IDNO;
    } catch (std::exception const& ex) {
        ::OutputDebugStringA(ex.what());
        return IDYES;
    }
}
