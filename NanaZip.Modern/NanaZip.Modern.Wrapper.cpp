/*
 * PROJECT:    NanaZip.Modern
 * FILE:       NanaZip.Modern.Wrapper.cpp
 * PURPOSE:    Implementation for NanaZip Modern Experience Wrapper
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Modern.h"

#include <string>

// Only used in Shell Extension to check whether NanaZip Modern is loaded.
EXTERN_C HMODULE K7ModernCurrentModule = nullptr;

namespace
{
    std::wstring GetCurrentProcessModulePath()
    {
        // 32767 is the maximum path length without the terminating null character.
        std::wstring Path(32767, L'\0');
        Path.resize(::GetModuleFileNameW(
            ::K7ModernCurrentModule,
            &Path[0],
            static_cast<DWORD>(Path.size())));
        return Path;
    }

    static HMODULE GetNanaZipModernModuleHandle()
    {
        static HMODULE CachedResult = ([]() -> HMODULE
        {
            std::wstring ModulePath = ::GetCurrentProcessModulePath();
            size_t LastBackslashPosition = ModulePath.rfind(L'\\');
            if (std::wstring::npos != LastBackslashPosition)
            {
                ModulePath.resize(LastBackslashPosition + 1);
            }
            ModulePath.append(L"NanaZip.Modern.dll");
            return ::LoadLibraryExW(
                ModulePath.c_str(),
                nullptr,
                LOAD_WITH_ALTERED_SEARCH_PATH);
        }());

        return CachedResult;
    }
}

EXTERN_C LPCWSTR WINAPI K7ModernGetLegacyStringResource(
    _In_ UINT32 ResourceId)
{
    using ProcType = decltype(::K7ModernGetLegacyStringResource)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernGetLegacyStringResource");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress(ResourceId);
    }

    return nullptr;
}

EXTERN_C BOOL WINAPI K7ModernAvailable()
{
    using ProcType = decltype(::K7ModernAvailable)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernAvailable");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress();
    }

    return FALSE;
}

EXTERN_C HRESULT WINAPI K7ModernInitialize()
{
    using ProcType = decltype(::K7ModernInitialize)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernInitialize");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress();
    }

    return E_NOINTERFACE;
}

EXTERN_C HRESULT WINAPI K7ModernUninitialize()
{
    using ProcType = decltype(::K7ModernUninitialize)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernUninitialize");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress();
    }

    return E_NOINTERFACE;
}

EXTERN_C INT WINAPI K7ModernShowSponsorDialog(
    _In_opt_ HWND ParentWindowHandle)
{
    using ProcType = decltype(::K7ModernShowSponsorDialog)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernShowSponsorDialog");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress(ParentWindowHandle);
    }

    return -1;
}

EXTERN_C INT WINAPI K7ModernShowAboutDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage)
{
    using ProcType = decltype(::K7ModernShowAboutDialog)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernShowAboutDialog");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress(ParentWindowHandle, ExtendedMessage);
    }

    return -1;
}

EXTERN_C INT WINAPI K7ModernShowInformationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Content)
{
    using ProcType = decltype(::K7ModernShowInformationDialog)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernShowInformationDialog");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress(ParentWindowHandle, Title, Content);
    }

    return -1;
}

EXTERN_C VOID WINAPI K7ModernUpdateProgressWindowStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_PROGRESS_WINDOW_STATUS Status)
{
    using ProcType = decltype(::K7ModernUpdateProgressWindowStatus)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernUpdateProgressWindowStatus");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        ProcAddress(WindowHandle, Status);
    }
}

EXTERN_C VOID WINAPI K7ModernSetProgressWindowPausedMode(
    _In_ HWND WindowHandle,
    _In_ BOOL Paused)
{
    using ProcType = decltype(::K7ModernSetProgressWindowPausedMode)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernSetProgressWindowPausedMode");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        ProcAddress(WindowHandle, Paused);
    }
}

EXTERN_C INT WINAPI K7ModernShowProgressWindow(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_ BOOL ShowCompressionInformation,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
{
    using ProcType = decltype(::K7ModernShowProgressWindow)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernShowProgressWindow");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress(
            ParentWindowHandle,
            Title,
            ShowCompressionInformation,
            WindowSubclassHandler,
            WindowSubclassContext);
    }
    return -1;
}

EXTERN_C LPVOID WINAPI K7ModernCreateMainWindowToolBarPage(
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MoreMenuHandle)
{
    using ProcType = decltype(::K7ModernCreateMainWindowToolBarPage)*;

    static ProcType ProcAddress = reinterpret_cast<ProcType>([]() -> FARPROC
    {
        HMODULE ModuleHandle = ::GetNanaZipModernModuleHandle();
        if (ModuleHandle)
        {
            return ::GetProcAddress(
                ModuleHandle,
                "K7ModernCreateMainWindowToolBarPage");
        }
        return nullptr;
    }());

    if (ProcAddress)
    {
        return ProcAddress(ParentWindowHandle, MoreMenuHandle);
    }

    return nullptr;
}
