/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseMitigations.h
 * PURPOSE:    Definition for NanaZip Platform Base Mitigations Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_BASE_MITIGATIONS
#define K7_BASE_MITIGATIONS

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

/**
 * @brief Enables the mandatory mitigations for the current process.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseEnableMandatoryMitigations();

#endif // !K7_BASE_MITIGATIONS
