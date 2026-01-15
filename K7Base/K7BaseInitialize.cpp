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

namespace
{
    static MO_UINT8 g_InitializationPhase = 0;
}

EXTERN_C MO_BOOL MOAPI K7BaseGetInitialized()
{
    // The K7Base library is considered initialized only after at least the
    // second initialization phase is completed.
    return g_InitializationPhase > 1;
}

EXTERN_C MO_RESULT MOAPI K7BaseInitialize()
{
    MO_RESULT Result = MO_RESULT_SUCCESS_OK;

    if (0 == g_InitializationPhase)
    {
        // First initialization phase.

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

        ++g_InitializationPhase;
        return MO_RESULT_SUCCESS_OK;
    }
    else if (1 == g_InitializationPhase)
    {
        // Second initialization phase.

        Result = ::K7BaseDisableDynamicCodeGeneration();
        if (MO_RESULT_SUCCESS_OK != Result)
        {
            return Result;
        }

        ++g_InitializationPhase;
        return MO_RESULT_SUCCESS_OK;
    }
    else if (2 == g_InitializationPhase)
    {
        // Third initialization phase.

        Result = ::K7BaseDisableChildProcessCreation();
        if (MO_RESULT_SUCCESS_OK != Result)
        {
            return Result;
        }

        ++g_InitializationPhase;
        return MO_RESULT_SUCCESS_OK;
    }

    // Do nothing if already initialized.
    return MO_RESULT_SUCCESS_OK;
}
