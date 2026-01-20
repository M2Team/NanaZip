/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Frieren.cpp
 * PURPOSE:    Implementation for NanaZip.Frieren
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Mile.Mobility.Portable.Types.h>
#ifndef MILE_MOBILITY_ENABLE_MINIMUM_SAL
#include <sal.h>
#endif // !MILE_MOBILITY_ENABLE_MINIMUM_SAL

 // Workaround for Windows SDK, which defines these types in #ifndef VOID block.
 // That design is terrible, but needs to have a workaround.
#ifdef VOID
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif // VOID

#include "NanaZip.Frieren.h"

/**
 * @brief Initializes the dark mode support.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7UserInitializeDarkModeSupport();

EXTERN_C HRESULT WINAPI NanaZipFrierenGlobalInitialize()
{
    return MO_RESULT_SUCCESS_OK == ::K7UserInitializeDarkModeSupport()
        ? S_OK
        : E_FAIL;
}
