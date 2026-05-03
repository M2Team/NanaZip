/*
 * PROJECT:   NanaZip
 * FILE:      CNanaZipCopyHook.cpp
 * PURPOSE:   Copy hook implementation
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 */

#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winrt/windows.foundation.h>
#include <ShlObj.h>

struct CNanaZipCopyHook : winrt::implements<CNanaZipCopyHook, ICopyHookW> {
    STDMETHOD_(UINT, CopyCallback)(
        _In_opt_ HWND hwnd,
        UINT wFunc,
        UINT wFlags,
        _In_ PCWSTR pszSrcFile,
        DWORD dwSrcAttribs,
        _In_opt_ PCWSTR pszDestFile,
        DWORD dwDestAttribs);
};
