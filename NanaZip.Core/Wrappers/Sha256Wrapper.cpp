/*
 * PROJECT:   NanaZip
 * FILE:      Sha256Wrapper.cpp
 * PURPOSE:   Implementation for SHA-256 wrapper for 7-Zip
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "Sha256Wrapper.h"

#include <cstring>

CSha256& CSha256::operator=(
    CSha256 const& Source)
{
    if (this != &Source)
    {
        if (this->HashHandle)
        {
            ::K7PalHashDestroy(this->HashHandle);
            this->HashHandle = nullptr;
        }
        if (Source.HashHandle)
        {
            ::K7PalHashDuplicate(Source.HashHandle, &this->HashHandle);
        }
    }
    return *this;
}

BoolInt Sha256_SetFunction(
    CSha256* p,
    unsigned algo)
{
    UNREFERENCED_PARAMETER(algo);

    if (p)
    {
        // Workaround for using the uninitialized memory.
        std::memset(p, 0, sizeof(CSha256));
    }

    // Only return true because it's a wrapper to K7Pal.
    return True;
}

void Sha256_InitState(
    CSha256* p)
{
    if (!p)
    {
        return;
    }

    if (p->HashHandle)
    {
        ::K7PalHashDestroy(p->HashHandle);
        p->HashHandle = nullptr;
    }

    ::K7PalHashCreate(
        &p->HashHandle,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0);
}

void Sha256_Init(
    CSha256* p)
{
    if (p)
    {
        // Workaround for using the uninitialized memory.
        std::memset(p, 0, sizeof(CSha256));
    }

    ::Sha256_InitState(p);
}

void Sha256_Update(
    CSha256* p,
    const Byte* data,
    size_t size)
{
    if (!p)
    {
        return;
    }

    ::K7PalHashUpdate(
        p->HashHandle,
        const_cast<LPVOID>(reinterpret_cast<LPCVOID>(data)),
        static_cast<UINT32>(size));
}

void Sha256_Final(
    CSha256* p,
    Byte* digest)
{
    if (!p)
    {
        return;
    }

    ::K7PalHashFinal(
        p->HashHandle,
        digest,
        SHA256_DIGEST_SIZE);
}
