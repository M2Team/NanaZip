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

#include <Detours.h>

#include <atomic>

namespace
{
#ifdef NDEBUG
    static std::atomic<decltype(::SetThreadInformation)*> SetThreadInformationPtr = nullptr;
#endif

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

    static bool IsWindows8OrLater()
    {
        static bool CachedResult = ::MileIsWindowsVersionAtLeast(6, 2, 0);
        return CachedResult;
    }

#ifdef NDEBUG
    static bool IsWindows8Point1OrLater()
    {
        static bool CachedResult = ::MileIsWindowsVersionAtLeast(6, 3, 0);
        return CachedResult;
    }
#endif

    static bool IsWindows10OrLater()
    {
        static bool CachedResult = ::MileIsWindowsVersionAtLeast(10, 0, 0);
        return CachedResult;
    }

    static bool IsWindows10_1709OrLater()
    {
        static bool CachedResult = ::MileIsWindowsVersionAtLeast(10, 0, 16299);
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

        HMODULE ModuleHandle = ::GetKernel32ModuleHandle();
        if (ModuleHandle)
        {
            using ProcType = decltype(::SetThreadInformation)*;
            FARPROC ProcAddress = ::GetProcAddress(
                ModuleHandle,
                "SetThreadInformation");
            SetThreadInformationPtr.store(
                reinterpret_cast<ProcType>(ProcAddress),
                std::memory_order_relaxed);
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

EXTERN_C BOOL WINAPI NanaZipDisableChildProcesses()
{
    if (!::IsWindows10_1709OrLater())
    {
        return TRUE;
    }

    PROCESS_MITIGATION_CHILD_PROCESS_POLICY Policy = { 0 };
    Policy.NoChildProcessCreation = 1;
    if (!::SetProcessMitigationPolicyWrapper(
        ProcessChildProcessPolicy,
        &Policy,
        sizeof(PROCESS_MITIGATION_CHILD_PROCESS_POLICY)))
    {
        return FALSE;
    }

    return TRUE;
}

#ifdef NDEBUG

EXTERN_C BOOL WINAPI NanaZipSetThreadDynamicCodeOptout(BOOL optout)
{
    using ProcType = decltype(::SetThreadInformation)*;
    ProcType SetThreadInformationWrapper = SetThreadInformationPtr.load(std::memory_order_relaxed);
    if (SetThreadInformationWrapper) {
        DWORD ThreadPolicy = optout ? THREAD_DYNAMIC_CODE_ALLOW : 0;
        return SetThreadInformationWrapper(
            ::GetCurrentThread(),
            ThreadDynamicCodePolicy,
            &ThreadPolicy,
            sizeof(DWORD));
    }
    else {
        return TRUE;
    }
}

#else

EXTERN_C BOOL WINAPI NanaZipSetThreadDynamicCodeOptout(BOOL optout)
{
    UNREFERENCED_PARAMETER(optout);
    return TRUE;
}

#endif
