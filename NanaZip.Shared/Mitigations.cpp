/*
 * PROJECT:    NanaZip
 * FILE:       Mitigations.cpp
 * PURPOSE:    Implementation for applied mitigation policy
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "Mitigations.h"

#include <K7BasePolicies.h>

EXTERN_C BOOL WINAPI NanaZipEnableMitigations()
{
    {
        PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY Policy = {};
        Policy.RaiseExceptionOnInvalidHandleReference = 1;
        Policy.HandleExceptionsPermanentlyEnabled = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessStrictHandleCheckPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY)))
        {
            return FALSE;
        }
    }

#ifdef NDEBUG
    if (!::K7BasePoliciesGetAllowDynamicCodeGeneration())
    {
        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY Policy = {};
        Policy.ProhibitDynamicCode = 1;
        Policy.AllowThreadOptOut = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessDynamicCodePolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_DYNAMIC_CODE_POLICY)))
        {
            return FALSE;
        }
    }
#endif // NDEBUG

    {
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY Policy = {};
        Policy.NoRemoteImages = 1;
        Policy.NoLowMandatoryLabelImages = 1;
        if (!::SetProcessMitigationPolicy(
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
    if (::K7BasePoliciesGetAllowChildProcessCreation())
    {
        return TRUE;
    }

    PROCESS_MITIGATION_CHILD_PROCESS_POLICY Policy = {};
    Policy.NoChildProcessCreation = 1;
    if (!::SetProcessMitigationPolicy(
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
    if (::K7BasePoliciesGetAllowDynamicCodeGeneration())
    {
        return TRUE;
    }

    DWORD ThreadPolicy = OptOut ? THREAD_DYNAMIC_CODE_ALLOW : 0;
    return ::SetThreadInformation(
        ::GetCurrentThread(),
        ThreadDynamicCodePolicy,
        &ThreadPolicy,
        sizeof(DWORD));
}

#else

EXTERN_C BOOL WINAPI NanaZipSetThreadDynamicCodeOptout(
    _In_ BOOL OptOut)
{
    UNREFERENCED_PARAMETER(OptOut);
    return TRUE;
}

#endif
