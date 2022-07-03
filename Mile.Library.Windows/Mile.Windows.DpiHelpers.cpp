/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.Windows.DpiHelpers.cpp
 * PURPOSE:   Implementation for Windows DPI Helpers
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "Mile.Windows.DpiHelpers.h"

#include <cstring>

namespace
{
    static bool IsPrivatePerMonitorSupportExtensionApplicable()
    {
        static bool CachedResult = ([]() -> bool
        {
            OSVERSIONINFOEXW OSVersionInfoEx;

            // The private Per-Monitor DPI Awareness support extension is
            // Windows 10 only.
            std::memset(&OSVersionInfoEx, 0, sizeof(OSVERSIONINFOEXW));
            OSVersionInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
            OSVersionInfoEx.dwMajorVersion = 10;
            if (!::VerifyVersionInfoW(
                &OSVersionInfoEx,
                VER_MAJORVERSION,
                ::VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL)))
            {
                return false;
            }

            // We don't need the private Per-Monitor DPI Awareness support
            // extension if the Per-Monitor (V2) DPI Awareness exists.
            std::memset(&OSVersionInfoEx, 0, sizeof(OSVERSIONINFOEXW));
            OSVersionInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
            OSVersionInfoEx.dwBuildNumber = 14986;
            if (::VerifyVersionInfoW(
                &OSVersionInfoEx,
                VER_BUILDNUMBER,
                ::VerSetConditionMask(0, VER_BUILDNUMBER, VER_GREATER_EQUAL)))
            {
                return false;
            }

            return true;
        }());

        return CachedResult;
    }
}

EXTERN_C BOOL WINAPI MileEnablePerMonitorDialogScaling()
{
    if (!::IsPrivatePerMonitorSupportExtensionApplicable())
    {
        return FALSE;
    }

    HMODULE ModuleHandle = ::GetModuleHandleW(L"user32.dll");
    if (!ModuleHandle)
    {
        return FALSE;
    }

    // Reference: https://github.com/microsoft/terminal/blob
    //            /9b92986b49bed8cc41fde4d6ef080921c41e6d9e
    //            /src/interactivity/win32/windowdpiapi.cpp#L24
    typedef BOOL(WINAPI* ProcType)();

    ProcType ProcAddress = reinterpret_cast<ProcType>(
        ::GetProcAddress(ModuleHandle, reinterpret_cast<LPCSTR>(2577)));
    if (!ProcAddress)
    {
        return FALSE;
    }

    return ProcAddress();
}
