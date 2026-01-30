/*
 * PROJECT:    NanaZip Platform User Library (K7User)
 * FILE:       K7User.h
 * PURPOSE:    Definition for NanaZip Platform User Public Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_USER
#define K7_USER

#include <Mile.Mobility.Portable.Types.h>
#ifndef MILE_MOBILITY_ENABLE_MINIMUM_SAL
#include <sal.h>
#endif // !MILE_MOBILITY_ENABLE_MINIMUM_SAL

 // Workaround for Windows SDK, which defines these types in #ifndef VOID block.
 // That design is terrible, but needs to have a workaround.
#ifdef VOID
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif // VOID

#include <Windows.h>
#include <ShlObj_core.h>

#ifndef K7_USER_DARK_MODE
#define K7_USER_DARK_MODE

/**
 * @brief Initializes the dark mode support.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7UserInitializeDarkModeSupport();

#endif // !K7_USER_DARK_MODE

#ifndef K7_USER_MODERN
#define K7_USER_MODERN

/**
 * @brief Displays a modal dialog box that contains a system icon, a set of
 *        buttons, and a brief application-specific message, such as status or
 *        error information. The message box returns an integer value that
 *        indicates which button the user clicked.
 * @param hWnd A handle to the owner window of the message box to be created. If
 *             this parameter is nullptr, the message box has no owner window.
 * @param lpText The message to be displayed. If the string consists of more
 *               than one line, you can separate the lines using a carriage
 *               return and/or linefeed character between each line.
 * @param lpCaption The dialog box title. If this parameter is nullptr, the
 *                  default title is Error.
 * @param uType The contents and behavior of the dialog box.
 * @return If the function fails, the return value is zero. To get extended
 *         error information, call GetLastError. If the function succeeds, the
 *         return value is one of the button ID values.
 * @remark For more information, see MessageBoxW.
 */
EXTERN_C int WINAPI K7UserModernMessageBoxW(
    _In_opt_ HWND hWnd,
    _In_opt_ LPCWSTR lpText,
    _In_opt_ LPCWSTR lpCaption,
    _In_ UINT uType);

/**
 * @brief Displays a dialog box that enables the user to select a Shell folder.
 * @param lpbi A pointer to a BROWSEINFO structure that contains information
 *             used to display the dialog box.
 * @return Returns a PIDL that specifies the location of the selected folder
 *         relative to the root of the namespace. If the user chooses the Cancel
 *         button in the dialog box, the return value is nullptr. It is possible
 *         that the PIDL returned is that of a folder shortcut rather than a
 *         folder.
 * @remark For more information, see SHBrowseForFolderW.
 */
EXTERN_C PIDLIST_ABSOLUTE WINAPI K7UserModernSHBrowseForFolderW(
    _In_ LPBROWSEINFOW lpbi);

/**
 * @brief Launches the Windows Settings Default Apps settings page for the
 *        current application.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT WINAPI K7UserModernLaunchDefaultAppsSettings();

#endif // !K7_USER_MODERN

#endif // !K7_USER
