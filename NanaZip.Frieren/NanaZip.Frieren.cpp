/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Frieren.cpp
 * PURPOSE:   Implementation for NanaZip.Frieren
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Frieren.h"

#include <Mile.Helpers.h>
#include <Mile.Helpers.CppBase.h>

#include <Detours.h>

#include <Uxtheme.h>

namespace
{
    namespace FunctionType
    {
        enum
        {
            GetSysColor,
            GetSysColorBrush,
            DrawThemeText,
            DrawThemeBackground,

            MaximumFunction
        };
    }

    struct FunctionItem
    {
        PVOID Original;
        PVOID Detoured;
    };

    thread_local bool volatile g_ThreadInitialized = false;
    thread_local HHOOK volatile g_WindowsHookHandle = nullptr;
    thread_local FunctionItem g_FunctionTable[FunctionType::MaximumFunction];

    LRESULT CALLBACK WindowSubclassCallback(
        _In_ HWND hWnd,
        _In_ UINT uMsg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam,
        _In_ UINT_PTR uIdSubclass,
        _In_ DWORD_PTR dwRefData)
    {
        LRESULT Result = ::DefSubclassProc(
            hWnd,
            uMsg,
            wParam,
            lParam);
        return Result;
    }

    static LRESULT CALLBACK CallWndProcCallback(
        _In_ int nCode,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            PCWPSTRUCT WndProcStruct =
                reinterpret_cast<PCWPSTRUCT>(lParam);

            switch (WndProcStruct->message)
            {
            case WM_CREATE:
            case WM_INITDIALOG:
                wchar_t ClassName[256] = { 0 };
                if (0 != ::GetClassNameW(
                    WndProcStruct->hwnd,
                    ClassName,
                    256))
                {
                    if (!std::wcsstr(ClassName, L"Windows.UI.") &&
                        !std::wcsstr(ClassName, L"Mile.Xaml."))
                    {
                ::SetWindowSubclass(
                    WndProcStruct->hwnd,
                    ::WindowSubclassCallback,
                    0,
                    0);
                    }
                }
                break;
            }
        }

        return ::CallNextHookEx(
            nullptr,
            nCode,
            wParam,
            lParam);
    }

    static DWORD WINAPI OriginalGetSysColor(
        _In_ int nIndex)
    {
        return reinterpret_cast<decltype(::GetSysColor)*>(
            g_FunctionTable[FunctionType::GetSysColor].Original)(
                nIndex);
    }

    static DWORD WINAPI DetouredGetSysColor(
        _In_ int nIndex)
    {
        if (!g_ThreadInitialized)
        {
            return ::OriginalGetSysColor(nIndex);
        }

        return ::OriginalGetSysColor(nIndex);
    }

    static HBRUSH WINAPI OriginalGetSysColorBrush(
        _In_ int nIndex)
    {
        return reinterpret_cast<decltype(::GetSysColorBrush)*>(
            g_FunctionTable[FunctionType::GetSysColorBrush].Original)(
                nIndex);
    }

    static HBRUSH WINAPI DetouredGetSysColorBrush(
        _In_ int nIndex)
    {
        if (!g_ThreadInitialized)
        {
            return ::OriginalGetSysColorBrush(nIndex);
        }

        return ::OriginalGetSysColorBrush(nIndex);
    }

    static HRESULT WINAPI OriginalDrawThemeText(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCWSTR pszText,
        _In_ int cchText,
        _In_ DWORD dwTextFlags,
        _In_ DWORD dwTextFlags2,
        _In_ LPCRECT pRect)
    {
        return reinterpret_cast<decltype(::DrawThemeText)*>(
            g_FunctionTable[FunctionType::DrawThemeText].Original)(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pszText,
                cchText,
                dwTextFlags,
                dwTextFlags2,
                pRect);
    }

    static HRESULT WINAPI DetouredDrawThemeText(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCWSTR pszText,
        _In_ int cchText,
        _In_ DWORD dwTextFlags,
        _In_ DWORD dwTextFlags2,
        _In_ LPCRECT pRect)
    {
        if (!g_ThreadInitialized)
        {
            return ::OriginalDrawThemeText(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pszText,
                cchText,
                dwTextFlags,
                dwTextFlags2,
                pRect);
        }

        return ::OriginalDrawThemeText(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pszText,
            cchText,
            dwTextFlags,
            dwTextFlags2,
            pRect);
    }

    static HRESULT WINAPI OriginalDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        return reinterpret_cast<decltype(::DrawThemeBackground)*>(
            g_FunctionTable[FunctionType::DrawThemeBackground].Original)(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pClipRect);
    }

    static HRESULT WINAPI DetouredDrawThemeBackground(
        _In_ HTHEME hTheme,
        _In_ HDC hdc,
        _In_ int iPartId,
        _In_ int iStateId,
        _In_ LPCRECT pRect,
        _In_opt_ LPCRECT pClipRect)
    {
        if (!g_ThreadInitialized)
        {
            return ::OriginalDrawThemeBackground(
                hTheme,
                hdc,
                iPartId,
                iStateId,
                pRect,
                pClipRect);
        }

        return ::OriginalDrawThemeBackground(
            hTheme,
            hdc,
            iPartId,
            iStateId,
            pRect,
            pClipRect);
    }
}

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadInitialize()
{
    if (g_ThreadInitialized)
    {
        return S_OK;
    }

    g_WindowsHookHandle = ::SetWindowsHookExW(
        WH_CALLWNDPROC,
        ::CallWndProcCallback,
        nullptr,
        ::GetCurrentThreadId());
    if (!g_WindowsHookHandle)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    g_FunctionTable[FunctionType::GetSysColor].Original =
        ::GetSysColor;
    g_FunctionTable[FunctionType::GetSysColor].Detoured =
        ::DetouredGetSysColor;

    g_FunctionTable[FunctionType::GetSysColorBrush].Original =
        ::GetSysColorBrush;
    g_FunctionTable[FunctionType::GetSysColorBrush].Detoured =
        ::DetouredGetSysColorBrush;

    g_FunctionTable[FunctionType::DrawThemeText].Original =
        ::DrawThemeText;
    g_FunctionTable[FunctionType::DrawThemeText].Detoured =
        ::DetouredDrawThemeText;

    g_FunctionTable[FunctionType::DrawThemeBackground].Original =
        ::DrawThemeBackground;
    g_FunctionTable[FunctionType::DrawThemeBackground].Detoured =
        ::DetouredDrawThemeBackground;

    ::DetourTransactionBegin();
    ::DetourUpdateThread(::GetCurrentThread());
    for (size_t i = 0; i < FunctionType::MaximumFunction; ++i)
    {
        if (g_FunctionTable[i].Original)
        {
            ::DetourAttach(
                &g_FunctionTable[i].Original,
                g_FunctionTable[i].Detoured);
        }
    }
    ::DetourTransactionCommit();

    g_ThreadInitialized = true;

    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadUninitialize()
{
    if (!g_ThreadInitialized)
    {
        return S_OK;
    }

    g_ThreadInitialized = false;

    ::DetourTransactionBegin();
    ::DetourUpdateThread(::GetCurrentThread());
    for (size_t i = 0; i < FunctionType::MaximumFunction; ++i)
    {
        if (g_FunctionTable[i].Original)
        {
            ::DetourDetach(
                &g_FunctionTable[i].Original,
                g_FunctionTable[i].Detoured);
            g_FunctionTable[i].Original = nullptr;
            g_FunctionTable[i].Detoured = nullptr;
        }
    }
    ::DetourTransactionCommit();

    ::UnhookWindowsHookEx(g_WindowsHookHandle);
    g_WindowsHookHandle = nullptr;

    return S_OK;
}
