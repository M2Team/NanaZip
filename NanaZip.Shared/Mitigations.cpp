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

#include <K7BaseMitigations.h>
#include <K7BasePolicies.h>

EXTERN_C BOOL WINAPI NanaZipEnableMitigations()
{
    if (MO_RESULT_SUCCESS_OK != ::K7BaseEnableMandatoryMitigations())
    {
        return FALSE;
    }

#ifdef NDEBUG
    if (!::K7BaseGetAllowDynamicCodeGenerationPolicy())
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

    return TRUE;
}

EXTERN_C BOOL WINAPI NanaZipDisableChildProcesses()
{
    if (::K7BaseGetAllowChildProcessCreationPolicy())
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
    if (::K7BaseGetAllowDynamicCodeGenerationPolicy())
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
