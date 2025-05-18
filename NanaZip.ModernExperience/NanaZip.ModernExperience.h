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

EXTERN_C LPVOID WINAPI K7ModernCreateSponsorPage(
    _In_opt_ HWND ParentWindowHandle);

EXTERN_C LPVOID WINAPI K7ModernCreateAboutPage(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage);

#endif // !NANAZIP_MODERN_EXPERIENCE
