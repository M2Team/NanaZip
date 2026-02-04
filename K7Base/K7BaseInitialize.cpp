/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseInitialize.cpp
 * PURPOSE:    Implementation for NanaZip Platform Base Initialize Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7BasePrivate.h"

EXTERN_C MO_RESULT MOAPI K7BaseInitialize()
{
    static MO_BOOL g_Initialized = MO_FALSE;
    if (g_Initialized)
    {
        return MO_RESULT_SUCCESS_OK;
    }

    MO_RESULT Result = MO_RESULT_SUCCESS_OK;

    Result = ::K7BaseInitializePolicies();
    if (MO_RESULT_SUCCESS_OK != Result)
    {
        return Result;
    }

    Result = ::K7BaseEnableMandatoryMitigations();
    if (MO_RESULT_SUCCESS_OK != Result)
    {
        return Result;
    }

    Result = ::K7BaseInitializeDynamicLinkLibraryBlocker();
    if (MO_RESULT_SUCCESS_OK != Result)
    {
        return Result;
    }

    g_Initialized = MO_TRUE;

    // Do nothing if already initialized.
    return MO_RESULT_SUCCESS_OK;
}
