/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BasePolicies.h
 * PURPOSE:    Definition for NanaZip Platform Base Policies Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_BASE_POLICIES
#define K7_BASE_POLICIES

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
 * @brief Initializes the NanaZip policy settings.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 * @remarks The function should be called only once during the K7Base library
 *          initialization phase as early as possible.
 */
EXTERN_C MO_RESULT MOAPI K7BaseInitializePolicies();

/**
 * @brief Retrieves the policy setting for allowing dynamic code generation.
 * @return Returns MO_TRUE if dynamic code generation is allowed, or MO_FALSE if
 *         not.
 */
EXTERN_C MO_BOOL MOAPI K7BaseGetAllowDynamicCodeGenerationPolicy();

/**
 * @brief Retrieves the policy setting for allowing child process creation.
 * @return Returns MO_TRUE if child process creation is allowed, or MO_FALSE if
 *         not.
 */
EXTERN_C MO_BOOL MOAPI K7BaseGetAllowChildProcessCreationPolicy();

#endif // !K7_BASE_POLICIES
