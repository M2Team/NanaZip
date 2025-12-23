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
 * @brief Checks the related user settings in the Windows registry to know
 *        whether the security mitigation policies should be disabled or not.
 * @return Returns MO_TRUE if the security mitigations policy should be
 *         disabled, or MO_FALSE if not.
 */
EXTERN_C MO_BOOL MOAPI K7BaseIsSecurityMitigationPoliciesDisabled();

#endif // !K7_BASE_POLICIES
