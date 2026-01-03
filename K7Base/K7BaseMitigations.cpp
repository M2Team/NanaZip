/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseMitigations.cpp
 * PURPOSE:    Implementation for NanaZip Platform Base Mitigations Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7BaseMitigations.h"

#include "K7BasePolicies.h"

#include <Windows.h>

EXTERN_C MO_RESULT MOAPI K7BaseEnableMandatoryMitigations()
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
            return MO_RESULT_ERROR_FAIL;
        }
    }

    {
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY Policy = {};
        Policy.NoRemoteImages = 1;
        Policy.NoLowMandatoryLabelImages = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessImageLoadPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_IMAGE_LOAD_POLICY)))
        {
            return MO_RESULT_ERROR_FAIL;
        }
    }

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseDisableDynamicCodeGeneration()
{
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
            return MO_RESULT_ERROR_FAIL;
        }
    }
#endif // NDEBUG

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseDisableChildProcessCreation()
{
    if (!::K7BaseGetAllowChildProcessCreationPolicy())
    {
        PROCESS_MITIGATION_CHILD_PROCESS_POLICY Policy = {};
        Policy.NoChildProcessCreation = 1;
        if (!::SetProcessMitigationPolicy(
            ProcessChildProcessPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_CHILD_PROCESS_POLICY)))
        {
            return MO_RESULT_ERROR_FAIL;
        }
    }
    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseSetCurrentThreadDynamicCodePolicyOptOut(
    _In_ MO_BOOL AllowDynamicCodeGeneration)
{
#ifdef NDEBUG
    if (::K7BaseGetAllowDynamicCodeGenerationPolicy())
    {
        return TRUE;
    }
    DWORD PolicyValue = AllowDynamicCodeGeneration
        ? THREAD_DYNAMIC_CODE_ALLOW
        : 0;
    if (!::SetThreadInformation(
        ::GetCurrentThread(),
        ThreadDynamicCodePolicy,
        &PolicyValue,
        sizeof(DWORD)))
    {
        return MO_RESULT_ERROR_FAIL;
    }
#else
    UNREFERENCED_PARAMETER(AllowDynamicCodeGeneration);
#endif
    return MO_RESULT_SUCCESS_OK;
}
