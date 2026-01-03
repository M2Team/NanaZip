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
