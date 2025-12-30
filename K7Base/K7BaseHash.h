/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseHash.h
 * PURPOSE:    Definition for NanaZip Platform Hash Algorithms Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_BASE_HASH
#define K7_BASE_HASH

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

MO_DECLARE_HANDLE(K7_BASE_HASH_HANDLE);
typedef K7_BASE_HASH_HANDLE *PK7_BASE_HASH_HANDLE;

EXTERN_C MO_RESULT MOAPI K7BaseHashCreate(
    _Inout_ PK7_BASE_HASH_HANDLE HashHandle,
    _In_ MO_CONSTANT_WIDE_STRING AlgorithmIdentifier,
    _In_opt_ MO_POINTER SecretBuffer,
    _In_ MO_UINT32 SecretSize);

EXTERN_C MO_RESULT MOAPI K7BaseHashDestroy(
    _Inout_opt_ K7_BASE_HASH_HANDLE HashHandle);

EXTERN_C MO_RESULT MOAPI K7BaseHashUpdate(
    _Inout_ K7_BASE_HASH_HANDLE HashHandle,
    _In_ MO_POINTER InputBuffer,
    _In_ MO_UINT32 InputSize);

EXTERN_C MO_RESULT MOAPI K7BaseHashFinal(
    _Inout_ K7_BASE_HASH_HANDLE HashHandle,
    _Out_ MO_POINTER OutputBuffer,
    _In_ MO_UINT32 OutputSize);

EXTERN_C MO_RESULT MOAPI K7BaseHashGetSize(
    _In_ K7_BASE_HASH_HANDLE HashHandle,
    _Out_ PMO_UINT32 HashSize);

EXTERN_C MO_RESULT MOAPI K7BaseHashDuplicate(
    _In_ K7_BASE_HASH_HANDLE SourceHashHandle,
    _Out_ PK7_BASE_HASH_HANDLE DestinationHashHandle);

#endif // !K7_BASE_HASH
