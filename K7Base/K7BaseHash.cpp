/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseHash.cpp
 * PURPOSE:    Implementation for NanaZip Platform Hash Algorithms Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7BaseHash.h"

#include <Mile.Helpers.Base.h>

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include <cstring>
#include <cwchar>

namespace
{
    typedef struct _K7_BASE_HASH_ALGORITHM
    {
        MO_CONSTANT_WIDE_STRING Identifier;
        BCRYPT_ALG_HANDLE Handle;
        BCRYPT_ALG_HANDLE HmacHandle;
    } K7_BASE_HASH_ALGORITHM, *PK7_BASE_HASH_ALGORITHM;

    K7_BASE_HASH_ALGORITHM g_HashAlgorithms[] =
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

    typedef struct _K7_BASE_HASH_CONTEXT
    {
        MO_UINT32 ContextSize;
        MO_UINT32 HashSize;
        MO_UINT32 HashObjectSize;
        MO_POINTER HashObject;
        BCRYPT_HASH_HANDLE HashHandle;
    } K7_BASE_HASH_CONTEXT, *PK7_BASE_HASH_CONTEXT;

    static PK7_BASE_HASH_CONTEXT K7BaseHashInternalGetContextFromHandle(
        _In_opt_ K7_BASE_HASH_HANDLE HashHandle)
    {
        if (HashHandle)
        {
            __try
            {
                PK7_BASE_HASH_CONTEXT Context =
                    reinterpret_cast<PK7_BASE_HASH_CONTEXT>(HashHandle);
                if (sizeof(K7_BASE_HASH_CONTEXT) == Context->ContextSize)
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

EXTERN_C MO_RESULT MOAPI K7BaseHashCreate(
    _Inout_ PK7_BASE_HASH_HANDLE HashHandle,
    _In_ MO_CONSTANT_WIDE_STRING AlgorithmIdentifier,
    _In_opt_ MO_POINTER SecretBuffer,
    _In_ MO_UINT32 SecretSize)
{
    PK7_BASE_HASH_ALGORITHM CurrentAlgorithm = nullptr;
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
        return MO_RESULT_ERROR_INVALID_PARAMETER;
    }

    if (!HashHandle)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
    }
    *HashHandle = nullptr;

    bool Result = false;

    do
    {
        PK7_BASE_HASH_CONTEXT Context = reinterpret_cast<PK7_BASE_HASH_CONTEXT>(
            ::MileAllocateMemory(sizeof(K7_BASE_HASH_CONTEXT)));
        if (!Context)
        {
            break;
        }
        *HashHandle = reinterpret_cast<K7_BASE_HASH_HANDLE>(Context);
        Context->ContextSize = sizeof(K7_BASE_HASH_CONTEXT);

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
        ::K7BaseHashDestroy(*HashHandle);
        *HashHandle = nullptr;
    }

    return Result ? MO_RESULT_SUCCESS_OK : MO_RESULT_ERROR_FAIL;
}

EXTERN_C MO_RESULT MOAPI K7BaseHashDestroy(
    _Inout_opt_ K7_BASE_HASH_HANDLE HashHandle)
{
    PK7_BASE_HASH_CONTEXT Context =
        ::K7BaseHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
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

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseHashUpdate(
    _Inout_ K7_BASE_HASH_HANDLE HashHandle,
    _In_ MO_POINTER InputBuffer,
    _In_ MO_UINT32 InputSize)
{
    PK7_BASE_HASH_CONTEXT Context =
        ::K7BaseHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
    }

    bool Result = BCRYPT_SUCCESS(::BCryptHashData(
        Context->HashHandle,
        reinterpret_cast<PUCHAR>(InputBuffer),
        InputSize,
        0));

    return Result ? MO_RESULT_SUCCESS_OK : MO_RESULT_ERROR_FAIL;
}

EXTERN_C MO_RESULT MOAPI K7BaseHashFinal(
    _Inout_ K7_BASE_HASH_HANDLE HashHandle,
    _Out_ MO_POINTER OutputBuffer,
    _In_ MO_UINT32 OutputSize)
{
    PK7_BASE_HASH_CONTEXT Context =
        ::K7BaseHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
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

    return Result ? MO_RESULT_SUCCESS_OK : MO_RESULT_ERROR_FAIL;
}

EXTERN_C MO_RESULT MOAPI K7BaseHashGetSize(
    _In_ K7_BASE_HASH_HANDLE HashHandle,
    _Out_ PMO_UINT32 HashSize)
{
    if (!HashSize)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
    }

    PK7_BASE_HASH_CONTEXT Context =
        ::K7BaseHashInternalGetContextFromHandle(HashHandle);
    if (!Context)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
    }

    *HashSize = Context->HashSize;
    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_RESULT MOAPI K7BaseHashDuplicate(
    _In_ K7_BASE_HASH_HANDLE SourceHashHandle,
    _Out_ PK7_BASE_HASH_HANDLE DestinationHashHandle)
{
    if (!DestinationHashHandle)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
    }
    *DestinationHashHandle = nullptr;

    PK7_BASE_HASH_CONTEXT SourceContext =
        ::K7BaseHashInternalGetContextFromHandle(SourceHashHandle);
    if (!SourceContext)
    {
        return MO_RESULT_ERROR_INVALID_PARAMETER;
    }

    bool Result = false;

    do
    {
        PK7_BASE_HASH_CONTEXT Context = reinterpret_cast<PK7_BASE_HASH_CONTEXT>(
            ::MileAllocateMemory(sizeof(K7_BASE_HASH_CONTEXT)));
        if (!Context)
        {
            break;
        }
        *DestinationHashHandle = reinterpret_cast<K7_BASE_HASH_HANDLE>(Context);
        Context->ContextSize = sizeof(K7_BASE_HASH_CONTEXT);

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
        ::K7BaseHashDestroy(*DestinationHashHandle);
        *DestinationHashHandle = nullptr;
    }

    return Result ? MO_RESULT_SUCCESS_OK : MO_RESULT_ERROR_FAIL;
}
