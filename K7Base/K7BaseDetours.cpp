/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseDetours.cpp
 * PURPOSE:    Implementation for NanaZip Platform Detours Library Wrappers
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7BaseDetours.h"

#include <Detours.h>

EXTERN_C LONG MOAPI K7BaseDetourTransactionBegin()
{
    return ::DetourTransactionBegin();
}

EXTERN_C LONG MOAPI K7BaseDetourTransactionCommit()
{
    return ::DetourTransactionCommit();
}

EXTERN_C LONG MOAPI K7BaseDetourUpdateThread(
    _In_ HANDLE ThreadHandle)
{
    return ::DetourUpdateThread(ThreadHandle);
}

EXTERN_C LONG MOAPI K7BaseDetourAttach(
    _Inout_ PMO_POINTER OriginalFunction,
    _In_ MO_POINTER DetouredFunction)
{
    return ::DetourAttach(OriginalFunction, DetouredFunction);
}

EXTERN_C LONG MOAPI K7BaseDetourDetach(
    _Inout_ PMO_POINTER OriginalFunction,
    _In_ MO_POINTER DetouredFunction)
{
    return ::DetourDetach(OriginalFunction, DetouredFunction);
}
