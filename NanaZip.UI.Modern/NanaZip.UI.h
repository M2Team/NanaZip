/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.UI.h
 * PURPOSE:   Definition for NanaZip Modern UI Shared Infrastructures
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_UI
#define NANAZIP_UI

#include <Windows.h>

namespace NanaZip::UI
{
    HWND CreateXamlDialog(
        _In_opt_ HWND ParentWindowHandle);

    int ShowXamlWindow(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ LPVOID Content,
        _In_ HWND ParentWindowHandle);

    int ShowXamlDialog(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ LPVOID Content,
        _In_ HWND ParentWindowHandle);
}

#endif // !NANAZIP_UI
