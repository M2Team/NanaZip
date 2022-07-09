/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.Windows.DwmHelpers.cpp
 * PURPOSE:   Implementation for Windows DWM Helpers
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "Mile.Windows.DwmHelpers.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace
{
    static bool IsWindows10Version20H1OrLater()
    {
        static bool CachedResult = ([]() -> bool
        {
            OSVERSIONINFOEXW OSVersionInfoEx = { 0 };
            OSVersionInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
            OSVersionInfoEx.dwMajorVersion = 10;
            OSVersionInfoEx.dwMinorVersion = 0;
            OSVersionInfoEx.dwBuildNumber = 19041;
            return ::VerifyVersionInfoW(
                &OSVersionInfoEx,
                VER_BUILDNUMBER,
                ::VerSetConditionMask(
                    ::VerSetConditionMask(
                        ::VerSetConditionMask(
                            0,
                            VER_MAJORVERSION,
                            VER_GREATER_EQUAL),
                        VER_MINORVERSION,
                        VER_GREATER_EQUAL),
                    VER_BUILDNUMBER,
                    VER_GREATER_EQUAL));
        }());

        return CachedResult;
    }
}

EXTERN_C HRESULT WINAPI MileSetUseImmersiveDarkModeAttribute(
    _In_ HWND WindowHandle,
    _In_ BOOL Value)
{
    const DWORD DwmWindowUseImmersiveDarkModeBefore20H1Attribute = 19;
    const DWORD DwmWindowUseImmersiveDarkModeAttribute = 20;
    return ::DwmSetWindowAttribute(
        WindowHandle,
        (::IsWindows10Version20H1OrLater()
            ? DwmWindowUseImmersiveDarkModeAttribute
            : DwmWindowUseImmersiveDarkModeBefore20H1Attribute),
        &Value,
        sizeof(BOOL));
}

EXTERN_C HRESULT WINAPI MileSetCaptionColorAttribute(
    _In_ HWND WindowHandle,
    _In_ COLORREF Value)
{
    const DWORD DwmWindowCaptionColorAttribute = 35;
    return ::DwmSetWindowAttribute(
        WindowHandle,
        DwmWindowCaptionColorAttribute,
        &Value,
        sizeof(COLORREF));
}

EXTERN_C HRESULT WINAPI MileDisableSystemBackdrop(
    _In_ HWND WindowHandle)
{
    const DWORD DwmWindowSystemBackdropTypeAttribute = 38;
    const DWORD DwmWindowSystemBackdropTypeNone = 1;
    DWORD Value = DwmWindowSystemBackdropTypeNone;
    return ::DwmSetWindowAttribute(
        WindowHandle,
        DwmWindowSystemBackdropTypeAttribute,
        &Value,
        sizeof(DWORD));
}

EXTERN_C BOOL WINAPI MileShouldAppsUseImmersiveDarkMode()
{
    DWORD Type = REG_DWORD;
    DWORD Value = 0;
    DWORD ValueLength = sizeof(DWORD);
    DWORD Error = ::RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
        &Type,
        &Value,
        &ValueLength);
    if (Error == ERROR_SUCCESS)
    {
        if (Type == REG_DWORD && ValueLength == sizeof(DWORD))
        {
            return (Value == 0) ? TRUE : FALSE;
        }
    }
    return FALSE;
}
