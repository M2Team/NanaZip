/*
 * PROJECT:   NanaZip.ModernExperience
 * FILE:      NanaZip.ModernExperience.h
 * PURPOSE:   Definition for NanaZip Modern Experience
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_MODERN_EXPERIENCE
#define NANAZIP_MODERN_EXPERIENCE

#include <Windows.h>

EXTERN_C INT WINAPI K7ModernShowSponsorDialog(
    _In_opt_ HWND ParentWindowHandle);

EXTERN_C INT WINAPI K7ModernShowAboutDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage);

#endif // !NANAZIP_MODERN_EXPERIENCE
