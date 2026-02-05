/*
 * PROJECT:    NanaZip.Modern
 * FILE:       NanaZip.Modern.h
 * PURPOSE:    Definition for NanaZip Modern Experience
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_MODERN_EXPERIENCE
#define NANAZIP_MODERN_EXPERIENCE

#include <Windows.h>
#include <CommCtrl.h>

/**
 * @brief Get legacy string resource from NanaZip Modern Experience resources.
 * @param ResourceId The legacy string resource ID.
 * @return If the legacy string resource is found, it returns the pointer to the
 *         string. Otherwise, it returns nullptr.
 * @remark This function can be used without calling K7ModernInitialize.
 */
EXTERN_C LPCWSTR WINAPI K7ModernGetLegacyStringResource(
    _In_ UINT32 ResourceId);

/**
 * @brief Check whether NanaZip Modern Experience is available.
 * @return If NanaZip Modern Experience is available, it returns TRUE.
 *         Otherwise, it returns FALSE.
 */
EXTERN_C BOOL WINAPI K7ModernAvailable();

/**
 * @brief Initialize NanaZip Modern Experience.
 * @return If the function succeeds, it returns S_OK. Otherwise, it returns an
 *         HRESULT error code.
 */
EXTERN_C HRESULT WINAPI K7ModernInitialize();

/**
 * @brief Uninitialize NanaZip Modern Experience.
 * @return If the function succeeds, it returns S_OK. Otherwise, it returns an
 *         HRESULT error code.
 */
EXTERN_C HRESULT WINAPI K7ModernUninitialize();

/**
 * @brief Show the "Sponsor NanaZip" dialog.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowSponsorDialog(
    _In_opt_ HWND ParentWindowHandle);

/**
 * @brief Show the "About NanaZip" dialog.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param ExtendedMessage The extended message to be displayed. If this
 *                        parameter is nullptr, the extended message is empty.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowAboutDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage);

/**
 * @brief Show an information dialog with the specified title and content.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Title The title of the information dialog. If this parameter is
 *              nullptr, the title is empty.
 * @param Content The content of the information dialog. If this parameter is
 *                nullptr, the content is empty.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowInformationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Content);

/**
 * @brief The progress window status structure.
 */
typedef struct _K7_PROGRESS_WINDOW_STATUS
{
    /**
     * @brief If true, the progress is shown in bytes. If false, the progress is
     *        shown in files.
     */
    BOOLEAN BytesProgressMode;

    /**
     * @brief If true, the operation is compression. If false, the operation is
     *        extraction.
     */
    BOOLEAN CompressionMode;

    /**
     * @brief If true, there is an error. If false, there is no error.
     */
    BOOLEAN HaveError;

    /**
     * @brief The title of the progress window.
     */
    LPCWSTR Title;

    /**
     * @brief The current file path being processed.
     */
    LPCWSTR FilePath;

    /**
     * @brief The total size of the operation in bytes. If unknown, it is set to
     *        static_cast<UINT64>(-1).
     */
    UINT64 TotalSize;

    /**
     * @brief The processed size of the operation in bytes.
     */
    UINT64 ProcessedSize;

    /**
     * @brief The number of total files. If unknown, it is set to
     *        static_cast<UINT64>(-1).
     */
    UINT64 TotalFiles;

    /**
     * @brief The number of processed files.
     */
    UINT64 ProcessedFiles;

    /**
     * @brief The input size in bytes. For compression, it is the original size.
     *        For extraction, it is the compressed size. If unknown, it is set
     *        to static_cast<UINT64>(-1).
     */
    UINT64 InputSize;

    /**
     * @brief The output size in bytes. For compression, it is the compressed
     *        size. For extraction, it is the original size. If unknown, it is
     *        set to static_cast<UINT64>(-1).
     */
    UINT64 OutputSize;

    /**
     * @brief The status message.
     */
    LPCWSTR Status;
} K7_PROGRESS_WINDOW_STATUS, *PK7_PROGRESS_WINDOW_STATUS;

/**
 * @brief Update the progress window status.
 * @param WindowHandle A handle to the progress window which acquired from the
 *                     window subclass procedure.
 * @param Status The progress window status to be updated.
 * @remark You must call this function only in the window subclass procedure of
 *         the progress window.
 */
EXTERN_C VOID WINAPI K7ModernUpdateProgressWindowStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_PROGRESS_WINDOW_STATUS Status);

/**
 * @brief The WM_COMMAND BN_CLICKED ID for the "Pause" button will be sent to
 *        the progress window when the "Pause" button is clicked.
 */
#define K7_PROGRESS_WINDOW_COMMAND_PAUSE 446

/**
 * @brief Set the paused mode of the progress window.
 * @param WindowHandle A handle to the progress window which acquired from the
 *                     window subclass procedure.
 * @param Paused If true, the UI of the progress window will be updated to the
 *               paused mode. If false, the UI of the progress window will be
 *               updated to the normal mode.
 * @remark You must call this function only in the window subclass procedure of
 *         the progress window.
 */
EXTERN_C VOID WINAPI K7ModernSetProgressWindowPausedMode(
    _In_ HWND WindowHandle,
    _In_ BOOL Paused);

/**
 * @brief Show the progress window.
 * @param ParentWindowHandle A handle to the owner window of the dialog to be
 *                           created. If this parameter is nullptr, the dialog
 *                           has no owner window.
 * @param Title The title of the progress window. If this parameter is
 *              nullptr, the title is empty.
 * @param ShowCompressionInformation If true, the progress window will show
 *                                   the packed size and compression ratio.
 *                                   Otherwise, the packed size and compression
 *                                   ratio will be hidden.
 * @param WindowSubclassHandler The window subclass procedure for the progress
 *                              window.
 * @param WindowSubclassContext The context pointer for the window subclass
 *                              procedure.
 * @return The message loop exit code of the dialog.
 */
EXTERN_C INT WINAPI K7ModernShowProgressWindow(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_ BOOL ShowCompressionInformation,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext);

/**
 * @brief Create the toolbar control for the main window.
 * @param ParentWindowHandle A handle to the owner window of the control to be
 *                           created. This parameter must be valid.
 * @param MoreMenuHandle A menu handle for the "More" menu. This parameter must
 *                       be valid.
 * @return The toolbar control instance pointer.
 */
EXTERN_C LPVOID WINAPI K7ModernCreateMainWindowToolBarPage(
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MoreMenuHandle);

#endif // !NANAZIP_MODERN_EXPERIENCE
