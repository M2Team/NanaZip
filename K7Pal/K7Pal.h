/*
 * PROJECT:   NanaZip Platform Abstraction Layer (K7Pal)
 * FILE:      K7Pal.h
 * PURPOSE:   Definition for NanaZip Platform Abstraction Layer (K7Pal)
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_PAL
#define K7_PAL

#include <Windows.h>

DECLARE_HANDLE(K7_PAL_HASH_HANDLE);
typedef K7_PAL_HASH_HANDLE *PK7_PAL_HASH_HANDLE;

EXTERN_C HRESULT WINAPI K7PalHashCreate(
    _Inout_ PK7_PAL_HASH_HANDLE HashHandle,
    _In_ LPCWSTR AlgorithmIdentifier,
    _In_opt_ PVOID SecretBuffer,
    _In_ UINT32 SecretSize);

EXTERN_C HRESULT WINAPI K7PalHashDestroy(
    _Inout_opt_ K7_PAL_HASH_HANDLE HashHandle);

EXTERN_C HRESULT WINAPI K7PalHashUpdate(
    _Inout_ K7_PAL_HASH_HANDLE HashHandle,
    _In_ PVOID InputBuffer,
    _In_ UINT32 InputSize);

EXTERN_C HRESULT WINAPI K7PalHashFinal(
    _Inout_ K7_PAL_HASH_HANDLE HashHandle,
    _Out_ PVOID OutputBuffer,
    _In_ ULONG OutputSize);

EXTERN_C HRESULT WINAPI K7PalHashGetSize(
    _In_ K7_PAL_HASH_HANDLE HashHandle,
    _Out_ PUINT32 HashSize);

EXTERN_C HRESULT WINAPI K7PalHashDuplicate(
    _In_ K7_PAL_HASH_HANDLE SourceHashHandle,
    _Out_ PK7_PAL_HASH_HANDLE DestinationHashHandle);

#endif // !K7_PAL
