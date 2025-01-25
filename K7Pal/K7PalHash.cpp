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
    typedef struct _K7_PAL_HASH_ALGORITHM
    {
        LPCWSTR Identifier;
        BCRYPT_ALG_HANDLE Handle;
        BCRYPT_ALG_HANDLE HmacHandle;
    } K7_PAL_HASH_ALGORITHM, *PK7_PAL_HASH_ALGORITHM;

    K7_PAL_HASH_ALGORITHM g_HashAlgorithms[] =
    {
        { BCRYPT_MD2_ALGORITHM, nullptr, nullptr },
        { BCRYPT_MD4_ALGORITHM, nullptr, nullptr },
        { BCRYPT_MD5_ALGORITHM, nullptr, nullptr },
        { BCRYPT_SHA1_ALGORITHM, nullptr, nullptr },
        { BCRYPT_SHA256_ALGORITHM, nullptr, nullptr },
        { BCRYPT_SHA384_ALGORITHM, nullptr, nullptr },
        { BCRYPT_SHA512_ALGORITHM, nullptr, nullptr },
    };

    const std::size_t g_HashAlgorithmsCount =
        sizeof(g_HashAlgorithms) / sizeof(*g_HashAlgorithms);

    typedef struct _K7_PAL_HASH_CONTEXT
    {
        UINT32 ContextSize;
        UINT32 HashSize;
        UINT32 HashObjectSize;
        PVOID HashObject;
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
}

EXTERN_C HRESULT WINAPI K7PalHashCreate(
    _Inout_ PK7_PAL_HASH_HANDLE HashHandle,
    _In_ LPCWSTR AlgorithmIdentifier,
    _In_opt_ PVOID SecretBuffer,
    _In_ UINT32 SecretSize)
{
    PK7_PAL_HASH_ALGORITHM CurrentAlgorithm = nullptr;
    if (AlgorithmIdentifier)
    {
        for (std::size_t i = 0; i < g_HashAlgorithmsCount; ++i)
        {
            if (0 == std::wcscmp(
                AlgorithmIdentifier,
                g_HashAlgorithms[i].Identifier))
            {
                CurrentAlgorithm = &g_HashAlgorithms[i];
                break;
            }
        }
    }
    if (!CurrentAlgorithm)
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

        BCRYPT_ALG_HANDLE AlgorithmHandle = nullptr;
        if (SecretBuffer)
        {
            if (!CurrentAlgorithm->HmacHandle)
            {
                if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(
                    &CurrentAlgorithm->HmacHandle,
                    CurrentAlgorithm->Identifier,
                    nullptr,
                    BCRYPT_ALG_HANDLE_HMAC_FLAG)))
                {
                    break;
                }
            }
            AlgorithmHandle = CurrentAlgorithm->HmacHandle;
        }
        else
        {
            if (!CurrentAlgorithm->Handle)
            {
                if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(
                    &CurrentAlgorithm->Handle,
                    CurrentAlgorithm->Identifier,
                    nullptr,
                    0)))
                {
                    break;
                }
            }
            AlgorithmHandle = CurrentAlgorithm->Handle;
        }

        {
            DWORD ReturnLength = 0;
            if (!BCRYPT_SUCCESS(::BCryptGetProperty(
                AlgorithmHandle,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&Context->HashSize),
                sizeof(Context->HashSize),
                &ReturnLength,
                0)))
            {
                break;
            }
        }

        {
            DWORD ReturnLength = 0;
            if (!BCRYPT_SUCCESS(::BCryptGetProperty(
                AlgorithmHandle,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&Context->HashObjectSize),
                sizeof(Context->HashObjectSize),
                &ReturnLength,
                0)))
            {
                break;
            }
        }

        Context->HashObject = ::MileAllocateMemory(Context->HashObjectSize);
        if (!Context->HashObject)
        {
            break;
        }

        Result = BCRYPT_SUCCESS(::BCryptCreateHash(
            AlgorithmHandle,
            &Context->HashHandle,
            reinterpret_cast<PUCHAR>(Context->HashObject),
            Context->HashObjectSize,
            SecretBuffer ? reinterpret_cast<PUCHAR>(SecretBuffer) : nullptr,
            SecretBuffer ? SecretSize : 0,
            0));

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

    Context->ContextSize = 0;

    Context->HashSize = 0;

    Context->HashObjectSize = 0;

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

    bool Result = false;

    PVOID FinishHashObject = ::MileAllocateMemory(
        Context->HashObjectSize);
    if (FinishHashObject)
    {
        BCRYPT_HASH_HANDLE FinishHashHandle = nullptr;
        if (BCRYPT_SUCCESS(::BCryptDuplicateHash(
            Context->HashHandle,
            &FinishHashHandle,
            reinterpret_cast<PUCHAR>(FinishHashObject),
            Context->HashObjectSize,
            0)))
        {
            Result = BCRYPT_SUCCESS(::BCryptFinishHash(
                FinishHashHandle,
                reinterpret_cast<PUCHAR>(OutputBuffer),
                OutputSize,
                0));

            ::BCryptDestroyHash(FinishHashHandle);
        }

        ::MileFreeMemory(FinishHashObject);
    }

    if (!Result)
    {
        if (OutputBuffer)
        {
            std::memset(OutputBuffer, 0, OutputSize);
        }
    }

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

        Context->HashSize = SourceContext->HashSize;

        Context->HashObjectSize = SourceContext->HashObjectSize;

        Context->HashObject = ::MileAllocateMemory(Context->HashObjectSize);
        if (!Context->HashObject)
        {
            break;
        }

        Result = BCRYPT_SUCCESS(::BCryptDuplicateHash(
            SourceContext->HashHandle,
            &Context->HashHandle,
            reinterpret_cast<PUCHAR>(Context->HashObject),
            Context->HashObjectSize,
            0));

    } while (false);

    if (!Result)
    {
        ::K7PalHashDestroy(*DestinationHashHandle);
        *DestinationHashHandle = nullptr;
    }

    return Result ? S_OK : E_FAIL;
}
