/*
 * PROJECT:    NanaZip
 * FILE:       K7User.h
 * PURPOSE:    Definition for NanaZip Platform User Public Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_USER
#define K7_USER

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

#ifndef K7_USER_DARK_MODE
#define K7_USER_DARK_MODE

/**
 * @brief Initializes the dark mode support.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7UserInitializeDarkModeSupport();

#endif // !K7_USER_DARK_MODE

#endif // !K7_USER
