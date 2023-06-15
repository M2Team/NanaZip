/*
 * PROJECT:   NanaZip
 * FILE:      Mitigations.cpp
 * PURPOSE:   Implementation for applied mitigation policy
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 *            MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "Mitigations.h"

#include <Mile.Helpers.h>

namespace
{
    static HMODULE GetKernel32ModuleHandle()
    {
        static HMODULE CachedResult = ::GetModuleHandleW(L"kernel32.dll");
        return CachedResult;
    }

    static BOOL SetProcessMitigationPolicyWrapper(
        _In_ PROCESS_MITIGATION_POLICY MitigationPolicy,
        _In_ PVOID lpBuffer,
        _In_ SIZE_T dwLength)
    {
        static FARPROC CachedProcAddress = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetKernel32ModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "SetProcessMitigationPolicy");
            }
            return nullptr;
        }());

        if (!CachedProcAddress)
        {
            return FALSE;
        }

        using ProcType = decltype(::SetProcessMitigationPolicy)*;

        return reinterpret_cast<ProcType>(CachedProcAddress)(
            MitigationPolicy,
            lpBuffer,
            dwLength);
    }

    static BOOL SetThreadInformationWrapper(
        _In_ HANDLE hThread,
        _In_ THREAD_INFORMATION_CLASS ThreadInformationClass,
        _In_ LPVOID ThreadInformation,
        _In_ DWORD ThreadInformationSize)
    {
        static FARPROC CachedProcAddress = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetKernel32ModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "SetThreadInformation");
            }
            return nullptr;
        }());

        if (!CachedProcAddress)
        {
            return FALSE;
        }

        using ProcType = decltype(::SetThreadInformation)*;

        return reinterpret_cast<ProcType>(CachedProcAddress)(
            hThread,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationSize);
    }

    static bool IsWindows8OrLater()
    {
        static bool CachedResult = ::MileIsWindowsVersionAtLeast(6, 2, 0);
        return CachedResult;
    }

    static bool IsWindows8Point1OrLater()
    {
        static bool CachedResult = ::MileIsWindowsVersionAtLeast(6, 3, 0);
        return CachedResult;
    }

    static bool IsWindows10OrLater()
    {
        static bool CachedResult = ::MileIsWindowsVersionAtLeast(10, 0, 0);
        return CachedResult;
    }
}

EXTERN_C BOOL WINAPI NanaZipEnableMitigations()
{
    if (!::IsWindows8OrLater())
    {
        return TRUE;
    }

    {
        PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY Policy = { 0 };
        Policy.RaiseExceptionOnInvalidHandleReference = 1;
        Policy.HandleExceptionsPermanentlyEnabled = 1;
        if (!::SetProcessMitigationPolicyWrapper(
            ProcessStrictHandleCheckPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY)))
        {
            return FALSE;
        }
    }

#ifdef NDEBUG
    if (IsWindows8Point1OrLater())
    {
        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY Policy = { 0 };
        Policy.ProhibitDynamicCode = 1;
        Policy.AllowThreadOptOut = 1;
        if (!::SetProcessMitigationPolicyWrapper(
            ProcessDynamicCodePolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_DYNAMIC_CODE_POLICY)))
        {
            return FALSE;
        }
    }
#endif // NDEBUG

    if (IsWindows10OrLater())
    {
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY Policy = { 0 };
        Policy.NoRemoteImages = 1;
        Policy.NoLowMandatoryLabelImages = 1;
        if (!::SetProcessMitigationPolicyWrapper(
            ProcessImageLoadPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_IMAGE_LOAD_POLICY)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

EXTERN_C BOOL WINAPI NanaZipThreadDynamicCodeAllow()
{
    if (!::IsWindows8Point1OrLater())
    {
        return TRUE;
    }

    DWORD ThreadPolicy = THREAD_DYNAMIC_CODE_ALLOW;
    return ::SetThreadInformationWrapper(
        ::GetCurrentThread(),
        ThreadDynamicCodePolicy,
        &ThreadPolicy,
        sizeof(DWORD));
}
