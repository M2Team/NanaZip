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

#ifdef NDEBUG

    static FARPROC GetSetThreadInformationProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
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

        return CachedResult;
    }

    static BOOL SetThreadInformationWrapper(
        _In_ HANDLE hThread,
        _In_ THREAD_INFORMATION_CLASS ThreadInformationClass,
        _In_ LPVOID ThreadInformation,
        _In_ DWORD ThreadInformationSize)
    {
        using ProcType = decltype(::SetThreadInformation)*;

        ProcType ProcAddress = reinterpret_cast<ProcType>(
            ::GetSetThreadInformationProcAddress());

        if (!ProcAddress)
        {
            return FALSE;
        }

        return ProcAddress(
            hThread,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationSize);
    }
#endif

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

EXTERN_C DWORD WINAPI NanaZipGetMitigationDisable()
{
    static DWORD CachedResult = ([]() -> DWORD
    {
        DWORD Result = 0;
        DWORD Length = sizeof(DWORD);
        // Use RRF_ZEROONFAILURE to ensure the result is initialized.
        ::RegGetValueW(
            HKEY_LOCAL_MACHINE,
            L"Software\\NanaZip\\Policies",
            L"DisableMitigations",
            RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY | RRF_ZEROONFAILURE,
            nullptr,
            &CachedResult,
            &Length);
        return Result;
    }());

    return CachedResult;
}

EXTERN_C BOOL WINAPI NanaZipEnableMitigations()
{
    if (NanaZipGetMitigationDisable() & 1)
    {
        return TRUE;
    }

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

EXTERN_C BOOL WINAPI NanaZipDisableChildProcesses()
{
    if (NanaZipGetMitigationDisable() & 1)
    {
        return TRUE;
    }

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

EXTERN_C BOOL WINAPI NanaZipSetThreadDynamicCodeOptout(
    _In_ BOOL OptOut)
{
    if (NanaZipGetMitigationDisable() & 1)
    {
        return TRUE;
    }

    if (::GetSetThreadInformationProcAddress())
    {
        DWORD ThreadPolicy = OptOut ? THREAD_DYNAMIC_CODE_ALLOW : 0;
        return ::SetThreadInformationWrapper(
            ::GetCurrentThread(),
            ThreadDynamicCodePolicy,
            &ThreadPolicy,
            sizeof(DWORD));
    }
    else
    {
        return TRUE;
    }
}

#else

EXTERN_C BOOL WINAPI NanaZipSetThreadDynamicCodeOptout(
    _In_ BOOL OptOut)
{
    UNREFERENCED_PARAMETER(OptOut);
    return TRUE;
}

#endif
