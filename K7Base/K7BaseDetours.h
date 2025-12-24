/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseDetours.h
 * PURPOSE:    Definition for NanaZip Platform Detours Library Wrappers
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_BASE_DETOURS
#define K7_BASE_DETOURS

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

#endif // !K7_BASE_DETOURS
