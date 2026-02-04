/*
 * PROJECT:    NanaZip
 * FILE:       Sha1Wrapper.cpp
 * PURPOSE:    Implementation for SHA-1 wrapper for 7-Zip
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "Sha1Wrapper.h"

#include <Windows.h>

#include <cstring>

CSha1& CSha1::operator=(
    CSha1 const& Source)
{
    if (this != &Source)
    {
        if (this->HashHandle)
        {
            ::K7BaseHashDestroy(this->HashHandle);
            this->HashHandle = nullptr;
        }
        if (Source.HashHandle)
        {
            ::K7BaseHashDuplicate(Source.HashHandle, &this->HashHandle);
        }
    }
    return *this;
}

BoolInt Sha1_SetFunction(
    CSha1* p,
    unsigned algo)
{
    UNREFERENCED_PARAMETER(algo);

    if (p)
    {
        // Workaround for using the uninitialized memory.
        std::memset(p, 0, sizeof(CSha1));
    }

    // Only return true because it's a wrapper to K7Pal.
    return True;
}

void Sha1_InitState(
    CSha1* p)
{
    if (!p)
    {
        return;
    }

    if (p->HashHandle)
    {
        ::K7BaseHashDestroy(p->HashHandle);
        p->HashHandle = nullptr;
    }

    ::K7BaseHashCreate(
        &p->HashHandle,
        K7_BASE_HASH_ALGORITHM_SHA1,
        nullptr,
        0);
}

void Sha1_Init(
    CSha1* p)
{
    if (p)
    {
        // Workaround for using the uninitialized memory.
        std::memset(p, 0, sizeof(CSha1));
    }

    ::Sha1_InitState(p);
}

void Sha1_Update(
    CSha1* p,
    const Byte* data,
    size_t size)
{
    if (!p)
    {
        return;
    }

    ::K7BaseHashUpdate(
        p->HashHandle,
        const_cast<LPVOID>(reinterpret_cast<LPCVOID>(data)),
        static_cast<UINT32>(size));
}

void Sha1_Final(
    CSha1* p,
    Byte* digest)
{
    if (!p)
    {
        return;
    }

    ::K7BaseHashFinal(
        p->HashHandle,
        digest,
        SHA1_DIGEST_SIZE);
}

void Sha1_PrepareBlock(
    const CSha1* p,
    Byte* block,
    unsigned size)
{
    UNREFERENCED_PARAMETER(p);
    UNREFERENCED_PARAMETER(block);
    UNREFERENCED_PARAMETER(size);

    // Empty implementation because it's a wrapper to K7Pal.
}

void Sha1_GetBlockDigest(
    const CSha1* p,
    const Byte* data,
    Byte* destDigest)
{
    if (!p)
    {
        return;
    }

    K7_BASE_HASH_HANDLE CurrentHashHandle = nullptr;
    if (MO_RESULT_SUCCESS_OK == ::K7BaseHashDuplicate(
        p->HashHandle,
        &CurrentHashHandle))
    {
        if (MO_RESULT_SUCCESS_OK == ::K7BaseHashUpdate(
            CurrentHashHandle,
            const_cast<LPVOID>(reinterpret_cast<LPCVOID>(data)),
            SHA1_DIGEST_SIZE))
        {
            ::K7BaseHashFinal(
                CurrentHashHandle,
                destDigest,
                SHA1_DIGEST_SIZE);
        }
        ::K7BaseHashDestroy(CurrentHashHandle);
    }
}
