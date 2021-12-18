/*
 * PROJECT:   NanaZip
 * FILE:      AboutDialog.h
 * PURPOSE:   Definition for About Dialog
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#ifndef NANAZIP_FILEMANAGER_ABOUTDIALOG
#define NANAZIP_FILEMANAGER_ABOUTDIALOG

#if (defined(__cplusplus) && __cplusplus >= 201703L)
#elif (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#else
#error "[Mile] You should use a C++ compiler with the C++17 standard."
#endif

#include <Windows.h>

namespace NanaZip::FileManager::AboutDialog
{
    void Show(
        _In_opt_ HWND ParentWindowHandle);
}

#endif // NANAZIP_FILEMANAGER_ABOUTDIALOG
