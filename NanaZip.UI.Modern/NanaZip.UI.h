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

#include "pch.h"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

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

    winrt::handle ShowAboutDialog(
        _In_ HWND ParentWindowHandle);

    void SpecialCommandHandler();
}

namespace winrt::NanaZip
{
    using namespace ::NanaZip;
}

#endif // !NANAZIP_UI
