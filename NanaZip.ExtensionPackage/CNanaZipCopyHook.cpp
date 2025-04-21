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

#include "CNanaZipCopyHook.hpp"

static constexpr PCWSTR DRAGNOTIFIER_PREFIX = L"{7F4FD2EA-8CC8-43C4-8440-CD76805B4E95}";
static constexpr ULONG_PTR DRAGNOTIFIER_COPY = 0x7F4FD2EA;

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

        std::filesystem::path srcpath(pszSrcFile);
        std::filesystem::path dstpath(pszDestFile);
        if (srcpath.stem() != DRAGNOTIFIER_PREFIX)
            return IDYES;

        auto hwndString = srcpath.extension().wstring();
        if (!hwndString.length())
            return IDYES;

        auto hwndDest = reinterpret_cast<HWND>(std::stoull(hwndString.substr(1)));
        auto dest = dstpath.parent_path();
        DragNotifierCopyData data{};
        if (FAILED(StringCchCopyW(&data.filename[0], ARRAYSIZE(data.filename), dest.c_str())))
            return IDYES;

        COPYDATASTRUCT cds{DRAGNOTIFIER_COPY, sizeof(DragNotifierCopyData), &data};
        DWORD_PTR res;
        // we can't use PostMessage since the copydata message needs to stay alive during the send
        SendMessageTimeoutW(
            hwndDest,
            WM_COPYDATA,
            reinterpret_cast<WPARAM>(hwnd),
            reinterpret_cast<LPARAM>(&cds),
            SMTO_ABORTIFHUNG,
            100,
            &res);
        // but it doesn't matter if the caller returns or not
        return IDNO;
    } catch (const std::exception &e) {
        OutputDebugStringA(e.what());
        return IDYES;
    }
}
