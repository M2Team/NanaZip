/*
 * PROJECT:   NanaZip Platform Abstraction Layer (K7Pal)
 * FILE:      K7PalHash.cpp
 * PURPOSE:   Implementation for K7Pal Hash Algorithms Interfaces
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7Pal.h"

#include <Mile.Helpers.Base.h>

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include <cstring>
#include <cwchar>

namespace
{
    typedef struct _K7_PAL_HASH_CONTEXT
    {
        UINT32 ContextSize;
        UINT32 SecretSize;
        PVOID SecretBuffer;
        LPWSTR AlgorithmIdentifier;
        BCRYPT_ALG_HANDLE AlgorithmHandle;
        PVOID HashObject;
        UINT32 HashSize;
        UINT32 Reserved;
        BCRYPT_HASH_HANDLE HashHandle;
    } K7_PAL_HASH_CONTEXT, *PK7_PAL_HASH_CONTEXT;

    static PK7_PAL_HASH_CONTEXT K7PalHashInternalGetContextFromHandle(
        _In_opt_ K7_PAL_HASH_HANDLE HashHandle)
    {
        if (HashHandle)
        {
            __try
            {
                PK7_PAL_HASH_CONTEXT Context =
                    reinterpret_cast<PK7_PAL_HASH_CONTEXT>(HashHandle);
                if (sizeof(K7_PAL_HASH_CONTEXT) == Context->ContextSize)
                {
                    return Context;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                // Fall through.
            }
        }

        return nullptr;
    }

    static bool K7PalHashInternalInitializeVolatileContext(
        _Inout_ PK7_PAL_HASH_CONTEXT Context)
    {
        if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(
            &Context->AlgorithmHandle,
            Context->AlgorithmIdentifier,
            nullptr,
            Context->SecretBuffer ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0)))
        {
            return false;
        }

        DWORD HashObjectSize = 0;
        {
            DWORD ReturnLength = 0;
            if (!BCRYPT_SUCCESS(::BCryptGetProperty(
                Context->AlgorithmHandle,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&HashObjectSize),
                sizeof(HashObjectSize),
                &ReturnLength,
                0)))
            {
                return false;
            }
        }

        DWORD HashSize = 0;
        {
            DWORD ReturnLength = 0;
            if (!BCRYPT_SUCCESS(::BCryptGetProperty(
                Context->AlgorithmHandle,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&HashSize),
                sizeof(HashSize),
                &ReturnLength,
                0)))
            {
                return false;
            }
        }
        Context->HashSize = HashSize;

        Context->HashObject = ::MileAllocateMemory(HashObjectSize);
        if (!Context->HashObject)
        {
            return false;
        }

        return BCRYPT_SUCCESS(::BCryptCreateHash(
            Context->AlgorithmHandle,
            &Context->HashHandle,
            reinterpret_cast<PUCHAR>(Context->HashObject),
            HashObjectSize,
            Context->SecretBuffer
                ? reinterpret_cast<PUCHAR>(Context->SecretBuffer)
                : nullptr,
            Context->SecretBuffer
                ? Context->SecretSize
                : 0,
            0));
    }

    static void K7PalHashInternalUninitializeVolatileContext(
        _Inout_ PK7_PAL_HASH_CONTEXT Context)
    {
        Context->HashSize = 0;

        if (Context->HashHandle)
        {
            ::BCryptDestroyHash(Context->HashHandle);
            Context->HashHandle = nullptr;
        }

        if (Context->HashObject)
        {
            ::MileFreeMemory(Context->HashObject);
            Context->HashObject = nullptr;
        }

        if (Context->AlgorithmHandle)
        {
            ::BCryptCloseAlgorithmProvider(Context->AlgorithmHandle, 0);
            Context->AlgorithmHandle = nullptr;
        }
    }

    static void K7PalHashInternalUninitializeContext(
        _Inout_ PK7_PAL_HASH_CONTEXT Context)
    {
        Context->ContextSize = 0;

        ::K7PalHashInternalUninitializeVolatileContext(Context);

        Context->SecretSize = 0;

        if (Context->SecretBuffer)
        {
            ::MileFreeMemory(Context->SecretBuffer);
            Context->SecretBuffer = nullptr;
        }

        if (Context->AlgorithmIdentifier)
        {
            ::MileFreeMemory(Context->AlgorithmIdentifier);
            Context->AlgorithmIdentifier = nullptr;
        }
    }

    static bool K7PalHashInternalResetVolatileContext(
        _Inout_ PK7_PAL_HASH_CONTEXT Context)
    {
        ::K7PalHashInternalUninitializeVolatileContext(Context);
        return ::K7PalHashInternalInitializeVolatileContext(Context);
    }
}

EXTERN_C HRESULT WINAPI K7PalHashCreate(
    _Inout_ PK7_PAL_HASH_HANDLE HashHandle,
    _In_ LPCWSTR AlgorithmIdentifier,
    _In_opt_ PVOID SecretBuffer,
    _In_ UINT32 SecretSize)
{
    if (!AlgorithmIdentifier)
    {
        return E_INVALIDARG;
    }

    if (!HashHandle)
    {
        return E_INVALIDARG;
    }
    *HashHandle = nullptr;

    bool Result = false;

    do
    {
        PK7_PAL_HASH_CONTEXT Context = reinterpret_cast<PK7_PAL_HASH_CONTEXT>(
            ::MileAllocateMemory(sizeof(K7_PAL_HASH_CONTEXT)));
        if (!Context)
        {
            break;
        }
        *HashHandle = reinterpret_cast<K7_PAL_HASH_HANDLE>(Context);
        Context->ContextSize = sizeof(K7_PAL_HASH_CONTEXT);

        SIZE_T AlgorithmIdentifierSize =
            sizeof(wchar_t) * (std::wcslen(AlgorithmIdentifier) + 1);
        Context->AlgorithmIdentifier = reinterpret_cast<LPWSTR>(
            ::MileAllocateMemory(AlgorithmIdentifierSize));
        if (!Context->AlgorithmIdentifier)
        {
            break;
        }
        std::memcpy(
            Context->AlgorithmIdentifier,
            AlgorithmIdentifier,
            AlgorithmIdentifierSize);

        if (SecretBuffer)
        {
            Context->SecretBuffer = ::MileAllocateMemory(SecretSize);
            if (!Context->SecretBuffer)
            {
                break;
            }
            std::memcpy(
                Context->SecretBuffer,
                SecretBuffer,
                SecretSize);
            Context->SecretSize = SecretSize;
        }

        Result = ::K7PalHashInternalInitializeVolatileContext(Context);

    } while (false);

    if (!Result)
    {
        ::K7PalHashDestroy(*HashHandle);
        *HashHandle = nullptr;
    }

    return Result ? S_OK : E_FAIL;
}

EXTERN_C HRESULT WINAPI K7PalHashDestroy(
    _Inout_opt_ K7_PAL_HASH_HANDLE HashHandle)
{
    PK7_PAL_HASH_CONTEXT Context =
        ::K7PalHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return E_INVALIDARG;
    }

    ::K7PalHashInternalUninitializeContext(Context);
    ::MileFreeMemory(Context);
    return S_OK;
}

EXTERN_C HRESULT WINAPI K7PalHashUpdate(
    _Inout_ K7_PAL_HASH_HANDLE HashHandle,
    _In_ PVOID InputBuffer,
    _In_ UINT32 InputSize)
{
    PK7_PAL_HASH_CONTEXT Context =
        ::K7PalHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return E_INVALIDARG;
    }

    bool Result = BCRYPT_SUCCESS(::BCryptHashData(
        Context->HashHandle,
        reinterpret_cast<PUCHAR>(InputBuffer),
        InputSize,
        0));

    return Result ? S_OK : E_FAIL;
}

EXTERN_C HRESULT WINAPI K7PalHashFinal(
    _Inout_ K7_PAL_HASH_HANDLE HashHandle,
    _Out_ PVOID OutputBuffer,
    _In_ ULONG OutputSize)
{
    PK7_PAL_HASH_CONTEXT Context =
        ::K7PalHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return E_INVALIDARG;
    }

    bool Result = BCRYPT_SUCCESS(::BCryptFinishHash(
        Context->HashHandle,
        reinterpret_cast<PUCHAR>(OutputBuffer),
        OutputSize,
        0));
    if (!Result)
    {
        if (OutputBuffer)
        {
            std::memset(OutputBuffer, 0, OutputSize);
        }
    }

    ::K7PalHashInternalResetVolatileContext(Context);

    return Result ? S_OK : E_FAIL;
}

EXTERN_C HRESULT WINAPI K7PalHashGetSize(
    _In_ K7_PAL_HASH_HANDLE HashHandle,
    _Out_ PUINT32 HashSize)
{
    if (!HashSize)
    {
        return E_INVALIDARG;
    }

    PK7_PAL_HASH_CONTEXT Context =
        ::K7PalHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return E_INVALIDARG;
    }

    *HashSize = Context->HashSize;
    return S_OK;
}

EXTERN_C HRESULT WINAPI K7PalHashDuplicate(
    _In_ K7_PAL_HASH_HANDLE SourceHashHandle,
    _Out_ PK7_PAL_HASH_HANDLE DestinationHashHandle)
{
    if (!DestinationHashHandle)
    {
        return E_INVALIDARG;
    }
    *DestinationHashHandle = nullptr;

    PK7_PAL_HASH_CONTEXT SourceContext =
        ::K7PalHashInternalGetContextFromHandle(SourceHashHandle);
    if (!SourceContext)
    {
        return E_INVALIDARG;
    }

    bool Result = false;

    do
    {
        PK7_PAL_HASH_CONTEXT Context = reinterpret_cast<PK7_PAL_HASH_CONTEXT>(
            ::MileAllocateMemory(sizeof(K7_PAL_HASH_CONTEXT)));
        if (!Context)
        {
            break;
        }
        *DestinationHashHandle = reinterpret_cast<K7_PAL_HASH_HANDLE>(Context);
        Context->ContextSize = sizeof(K7_PAL_HASH_CONTEXT);

        SIZE_T AlgorithmIdentifierSize =
            sizeof(wchar_t) * (std::wcslen(
                SourceContext->AlgorithmIdentifier) + 1);
        Context->AlgorithmIdentifier = reinterpret_cast<LPWSTR>(
            ::MileAllocateMemory(AlgorithmIdentifierSize));
        if (!Context->AlgorithmIdentifier)
        {
            break;
        }
        std::memcpy(
            Context->AlgorithmIdentifier,
            SourceContext->AlgorithmIdentifier,
            AlgorithmIdentifierSize);

        if (SourceContext->SecretBuffer)
        {
            Context->SecretBuffer = ::MileAllocateMemory(
                SourceContext->SecretSize);
            if (!Context->SecretBuffer)
            {
                break;
            }
            std::memcpy(
                Context->SecretBuffer,
                SourceContext->SecretBuffer,
                SourceContext->SecretSize);
            Context->SecretSize = SourceContext->SecretSize;
        }

        if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(
            &Context->AlgorithmHandle,
            Context->AlgorithmIdentifier,
            nullptr,
            Context->SecretBuffer ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0)))
        {
            break;
        }

        DWORD HashObjectSize = 0;
        {
            DWORD ReturnLength = 0;
            if (!BCRYPT_SUCCESS(::BCryptGetProperty(
                Context->AlgorithmHandle,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&HashObjectSize),
                sizeof(HashObjectSize),
                &ReturnLength,
                0)))
            {
                break;
            }
        }

        DWORD HashSize = 0;
        {
            DWORD ReturnLength = 0;
            if (!BCRYPT_SUCCESS(::BCryptGetProperty(
                Context->AlgorithmHandle,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&HashSize),
                sizeof(HashSize),
                &ReturnLength,
                0)))
            {
                break;
            }
        }
        Context->HashSize = HashSize;

        Context->HashObject = ::MileAllocateMemory(HashObjectSize);
        if (!Context->HashObject)
        {
            break;
        }

        Result = BCRYPT_SUCCESS(::BCryptDuplicateHash(
            SourceContext->HashHandle,
            &Context->HashHandle,
            reinterpret_cast<PUCHAR>(Context->HashObject),
            HashObjectSize,
            0));

    } while (false);

    if (!Result)
    {
        ::K7PalHashDestroy(*DestinationHashHandle);
        *DestinationHashHandle = nullptr;
    }

    return Result ? S_OK : E_FAIL;
}
