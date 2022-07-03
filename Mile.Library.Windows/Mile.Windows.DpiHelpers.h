/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.Windows.DpiHelpers.h
 * PURPOSE:   Definition for Windows DPI Helpers
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#pragma once

#ifndef MILE_WINDOWS_DPIHELPERS
#define MILE_WINDOWS_DPIHELPERS

#include <Windows.h>

/**
 * @brief Enables automatic display scaling of the dialogs in high-DPI
 *        displays. Must be called before the creation of dialogs.
 * @return If the function succeeds, the return value is nonzero. If the
 *         function fails, the return value is zero. To get extended error
 *         information, call GetLastError.
 * @remarks Applications running at a DPI_AWARENESS_CONTEXT of
 *          DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 automatically scale
 *          their dialogs by default. They do not need to call this function.
 *          Starting from Windows 10 Build 14986, Microsoft introduces the
 *          Per-Monitor (V2) DPI Awareness.
 *          Reference: https://blogs.windows.com/windows-insider/2016/12/07/
 *                     announcing-windows-10-insider-preview-build-14986-pc
 *          The Windows Notepad in Windows 10 Build 14986 started to use
 *          Per-Monitor (V2) DPI Awareness.
*/
EXTERN_C BOOL WINAPI MileEnablePerMonitorDialogScaling();

#endif // !MILE_WINDOWS_DPIHELPERS
