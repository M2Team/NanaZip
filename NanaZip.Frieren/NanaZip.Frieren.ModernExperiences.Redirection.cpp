/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Frieren.ModernExperiences.Redirection.cpp
 * PURPOSE:    Implementation for NanaZip Modern Experiences Redirection
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Windows.h>

#include <ShlObj_core.h>

EXTERN_C int WINAPI NanaZipFrierenModernMessageBoxW(
    _In_opt_ HWND hWnd,
    _In_opt_ LPCWSTR lpText,
    _In_opt_ LPCWSTR lpCaption,
    _In_ UINT uType);

EXTERN_C PIDLIST_ABSOLUTE WINAPI NanaZipFrierenModernSHBrowseForFolderW(
    _In_ LPBROWSEINFOW lpbi);

// Here are the linker-time redirections to replace the original APIs.
// Implementations only for x64 and ARM64, if you want to learn how to achieve
// that in x86, please refer to NanaZip.Shared.ModernExperienceShims project in
// the historical NanaZip versions source code.

extern "C" __declspec(selectany) void const* const __imp_MessageBoxW =
    reinterpret_cast<void const*>(::NanaZipFrierenModernMessageBoxW);
#pragma comment(linker, "/include:__imp_MessageBoxW")

extern "C" __declspec(selectany) void const* const __imp_SHBrowseForFolderW =
    reinterpret_cast<void const*>(::NanaZipFrierenModernSHBrowseForFolderW);
#pragma comment(linker, "/include:__imp_SHBrowseForFolderW")
