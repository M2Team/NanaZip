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

#include <Windows.h>

/**
 * @brief Begin a new transaction for attaching or detaching detours.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourTransactionBegin.
 */
EXTERN_C LONG MOAPI K7BaseDetourTransactionBegin();

/**
 * @brief Abort the current transaction for attaching or detaching detours.
 * @return Returns NO_ERROR if the pending transaction was completely aborted;
 *         otherwise, returns an error code.
 * @remark For more information, see DetourTransactionAbort.
 */
EXTERN_C LONG MOAPI K7BaseDetourTransactionAbort();

/**
 * @brief Commit the current transaction for attaching or detaching detours.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourTransactionCommit.
 */
EXTERN_C LONG MOAPI K7BaseDetourTransactionCommit();

/**
 * @brief Enlist a thread for update in the current transaction.
 * @param ThreadHandle The handle of the thread to be updated with the pending
 *                     transaction. If hThread is equal to the current threads
 *                     pseudo handle (as returned by GetCurrentThread()) no
 *                     action is performed and NO_ERROR is returned.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourUpdateThread.
 */
EXTERN_C LONG MOAPI K7BaseDetourUpdateThread(
    _In_ HANDLE ThreadHandle);

/**
 * @brief Attach a detour to a target function.
 * @param OriginalFunction Pointer to the target pointer to which the detour
 *                         will be attached.
 * @param DetouredFunction Pointer to the detour function.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourAttach.
 */
EXTERN_C LONG MOAPI K7BaseDetourAttach(
    _Inout_ PMO_POINTER OriginalFunction,
    _In_ MO_POINTER DetouredFunction);

/**
 * @brief Detach a detour from a target function.
 * @param OriginalFunction Pointer to the target pointer from which the detour
 *                         will be detached.
 * @param DetouredFunction Pointer to the detour function.
 * @return Returns NO_ERROR if successful; otherwise, returns an error code.
 * @remark For more information, see DetourDetach.
 */
EXTERN_C LONG MOAPI K7BaseDetourDetach(
    _Inout_ PMO_POINTER OriginalFunction,
    _In_ MO_POINTER DetouredFunction);

#endif // !K7_BASE_DETOURS
