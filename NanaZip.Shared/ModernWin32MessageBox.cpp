/*
 * PROJECT:   NanaZip
 * FILE:      ModernWin32MessageBox.cpp
 * PURPOSE:   Implementation for Modern Win32 Message Box
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include <Windows.h>

#include <CommCtrl.h>
#pragma comment(lib,"comctl32.lib")

int WINAPI OriginalMessageBoxW(
    _In_opt_ HWND hWnd,
    _In_opt_ LPCWSTR lpText,
    _In_opt_ LPCWSTR lpCaption,
    _In_ UINT uType)
{
    static decltype(MessageBoxW)* volatile pMessageBoxW =
        reinterpret_cast<decltype(MessageBoxW)*>(::GetProcAddress(
            ::GetModuleHandleW(L"user32.dll"),
            "MessageBoxW"));
    if (pMessageBoxW)
    {
        return pMessageBoxW(hWnd, lpText, lpCaption, uType);
    }
    else
    {
        ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return 0;
    }
}

EXTERN_C int WINAPI ModernMessageBoxW(
    _In_opt_ HWND hWnd,
    _In_opt_ LPCWSTR lpText,
    _In_opt_ LPCWSTR lpCaption,
    _In_ UINT uType)
{
    if (uType != (uType & (MB_ICONMASK | MB_TYPEMASK)))
    {
        return ::OriginalMessageBoxW(hWnd, lpText, lpCaption, uType);
    }

    TASKDIALOGCONFIG TaskDialogConfig = { 0 };

    TaskDialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    TaskDialogConfig.hwndParent = hWnd;
    TaskDialogConfig.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    TaskDialogConfig.pszWindowTitle = lpCaption;
    TaskDialogConfig.pszMainInstruction = lpText;

    switch (uType & MB_TYPEMASK)
    {
    case MB_OK:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_OK_BUTTON;
        break;
    case MB_OKCANCEL:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
        break;
    case MB_YESNOCANCEL:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
        break;
    case MB_YESNO:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
        break;
    case MB_RETRYCANCEL:
        TaskDialogConfig.dwCommonButtons =
            TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON;
        break;
    default:
        return ::OriginalMessageBoxW(hWnd, lpText, lpCaption, uType);
    }

    switch (uType & MB_ICONMASK)
    {
    case MB_ICONHAND:
        TaskDialogConfig.pszMainIcon = TD_ERROR_ICON;
        break;
    case MB_ICONQUESTION:
        TaskDialogConfig.dwFlags |= TDF_USE_HICON_MAIN;
        TaskDialogConfig.hMainIcon = ::LoadIconW(nullptr, IDI_QUESTION);
        break;
    case MB_ICONEXCLAMATION:
        TaskDialogConfig.pszMainIcon = TD_WARNING_ICON;
        break;
    case MB_ICONASTERISK:
        TaskDialogConfig.pszMainIcon = TD_INFORMATION_ICON;
        break;
    default:
        break;
    }

    int ButtonID = 0;

    HRESULT hr = ::TaskDialogIndirect(
        &TaskDialogConfig,
        &ButtonID,
        nullptr,
        nullptr);

    if (ButtonID == 0)
    {
        ::SetLastError(hr);
    }

    return ButtonID;
}

#if defined(_M_IX86)
#pragma warning(suppress:4483)
extern "C" __declspec(selectany) void const* const __identifier("_imp__MessageBoxW@16") = reinterpret_cast<void const*>(::ModernMessageBoxW);
#pragma comment(linker, "/include:__imp__MessageBoxW@16")
#else
extern "C" __declspec(selectany) void const* const __imp_MessageBoxW = reinterpret_cast<void const*>(::ModernMessageBoxW);
#pragma comment(linker, "/include:__imp_MessageBoxW")
#endif
