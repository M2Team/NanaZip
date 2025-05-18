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

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace NanaZip::UI
{
    winrt::handle ShowAboutDialog(
        _In_ HWND ParentWindowHandle);

    void SpecialCommandHandler();
}

#endif // !NANAZIP_UI
