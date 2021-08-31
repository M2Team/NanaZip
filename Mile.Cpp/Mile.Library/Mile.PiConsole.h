/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.PiConsole.h
 * PURPOSE:   Definition for Portable Interactive Console (Pi Console)
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#ifndef MILE_PI_CONSOLE
#define MILE_PI_CONSOLE

#include "Mile.Windows.h"

namespace Mile
{
    /**
     * @brief Wraps the Portable Interactive Console (Pi Console).
    */
    class PiConsole
    {
    public:

        /**
         * @brief Creates a Portable Interactive Console (Pi Console) window.
         * @param InstanceHandle A handle to the instance of the module to be
         *                       associated with the Portable Interactive
         *                       Console (Pi Console) window.
         * @param WindowIconHandle A handle to the icon to be associated with
         *                         the Portable Interactive Console (Pi
         *                         Console) window.
         * @param WindowTitle The title of the Portable Interactive Console (Pi
         *                    Console) window.
         * @param ShowWindowMode Controls how the Portable Interactive Console
         *                       (Pi Console) window is to be shown.
         * @return The handle of Portable Interactive Console (Pi Console)
         *         window. If the return value is not nullptr, you should use
         *         DestroyWindow to release.
        */
        static HWND Create(
            _In_ HINSTANCE InstanceHandle,
            _In_ HICON WindowIconHandle,
            _In_ LPCWSTR WindowTitle,
            _In_ DWORD ShowWindowMode);

        /**
         * @brief Prints messages to a Portable Interactive Console (Pi
         *        Console) window.
         * @param WindowHandle The handle of a Portable Interactive Console (Pi
         *                     Console) window.
         * @param Content The content of the message you want to print.
        */
        static void PrintMessage(
            _In_ HWND WindowHandle,
            _In_ LPCWSTR Content);

        /**
         * @brief Gets input from a Portable Interactive Console (Pi Console)
         *        window.
         * @param WindowHandle The handle of a Portable Interactive Console (Pi
         *                     Console) window.
         * @param InputPrompt The prompt you want to notice to the user.
         * @return The next line of characters from the user input. If the
         *         return value is not nullptr, you should use
         *         Mile::HeapMemory::Free to release.
        */
        static LPWSTR GetInput(
            _In_ HWND WindowHandle,
            _In_ LPCWSTR InputPrompt);

    };
}

#endif // !MILE_PI_CONSOLE
