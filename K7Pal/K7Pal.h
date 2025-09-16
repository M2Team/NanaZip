/*
 * PROJECT:    NanaZip Platform Abstraction Layer (K7Pal)
 * FILE:       K7Pal.h
 * PURPOSE:    Definition for NanaZip Platform Abstraction Layer (K7Pal)
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_PAL
#define K7_PAL

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

MO_DECLARE_HANDLE(K7_PAL_HASH_HANDLE);
typedef K7_PAL_HASH_HANDLE *PK7_PAL_HASH_HANDLE;

EXTERN_C MO_RESULT MOAPI K7PalHashCreate(
    _Inout_ PK7_PAL_HASH_HANDLE HashHandle,
    _In_ MO_CONSTANT_WIDE_STRING AlgorithmIdentifier,
    _In_opt_ MO_POINTER SecretBuffer,
    _In_ MO_UINT32 SecretSize);

EXTERN_C MO_RESULT MOAPI K7PalHashDestroy(
    _Inout_opt_ K7_PAL_HASH_HANDLE HashHandle);

EXTERN_C MO_RESULT MOAPI K7PalHashUpdate(
    _Inout_ K7_PAL_HASH_HANDLE HashHandle,
    _In_ MO_POINTER InputBuffer,
    _In_ MO_UINT32 InputSize);

EXTERN_C MO_RESULT MOAPI K7PalHashFinal(
    _Inout_ K7_PAL_HASH_HANDLE HashHandle,
    _Out_ MO_POINTER OutputBuffer,
    _In_ MO_UINT32 OutputSize);

EXTERN_C MO_RESULT MOAPI K7PalHashGetSize(
    _In_ K7_PAL_HASH_HANDLE HashHandle,
    _Out_ PMO_UINT32 HashSize);

EXTERN_C MO_RESULT MOAPI K7PalHashDuplicate(
    _In_ K7_PAL_HASH_HANDLE SourceHashHandle,
    _Out_ PK7_PAL_HASH_HANDLE DestinationHashHandle);

#endif // !K7_PAL
