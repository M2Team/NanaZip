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

    if (MO_RESULT_SUCCESS_OK != ::K7BaseDisableDynamicCodeGeneration())
    {
        return FALSE;
    }

    return TRUE;
}
