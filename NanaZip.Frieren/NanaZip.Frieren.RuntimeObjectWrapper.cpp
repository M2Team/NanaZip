/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Frieren.RuntimeObjectWrapper.cpp
 * PURPOSE:   Implementation for Windows Runtime C API Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Windows.h>

#define _ROAPI_
#include <roapi.h>

namespace
{
    static HMODULE GetComBaseModuleHandle()
    {
        static HMODULE CachedResult = ::LoadLibraryExW(
            L"combase.dll",
            nullptr,
            LOAD_LIBRARY_SEARCH_SYSTEM32);
        return CachedResult;
    }

    static FARPROC GetRoGetActivationFactoryProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetComBaseModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "RoGetActivationFactory");
            }
            return nullptr;
        }());

        return CachedResult;
    }
}

EXTERN_C HRESULT WINAPI RoGetActivationFactory(
    _In_ HSTRING activatableClassId,
    _In_ REFIID iid,
    _Out_ LPVOID* factory)
{
    using ProcType = decltype(::RoGetActivationFactory)*;

    ProcType ProcAddress = reinterpret_cast<ProcType>(
        ::GetRoGetActivationFactoryProcAddress());

    if (ProcAddress)
    {
        return ProcAddress(activatableClassId, iid, factory);
    }

    *factory = nullptr;
    return CLASS_E_CLASSNOTAVAILABLE;
}
