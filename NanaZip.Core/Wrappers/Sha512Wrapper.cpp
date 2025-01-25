/*
 * PROJECT:   NanaZip
 * FILE:      Sha512Wrapper.cpp
 * PURPOSE:   Implementation for SHA-384/SHA-512 wrapper for 7-Zip
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "Sha512Wrapper.h"

#include <cstring>

CSha512& CSha512::operator=(
    CSha512 const& Source)
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

BoolInt Sha512_SetFunction(
    CSha512* p,
    unsigned algo)
{
    UNREFERENCED_PARAMETER(algo);

    if (p)
    {
        // Workaround for using the uninitialized memory.
        std::memset(p, 0, sizeof(CSha512));
    }

    // Only return true because it's a wrapper to K7Pal.
    return True;
}

void Sha512_InitState(
    CSha512* p,
    unsigned digestSize)
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

    if (SHA512_DIGEST_SIZE == digestSize)
    {
        ::K7PalHashCreate(
            &p->HashHandle,
            BCRYPT_SHA512_ALGORITHM,
            nullptr,
            0);
    }
    else if (SHA512_384_DIGEST_SIZE == digestSize)
    {
        ::K7PalHashCreate(
            &p->HashHandle,
            BCRYPT_SHA384_ALGORITHM,
            nullptr,
            0);
    }
}

void Sha512_Init(
    CSha512* p,
    unsigned digestSize)
{
    if (p)
    {
        // Workaround for using the uninitialized memory.
        std::memset(p, 0, sizeof(CSha512));
    }

    ::Sha512_InitState(p, digestSize);
}

void Sha512_Update(
    CSha512* p,
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

void Sha512_Final(
    CSha512* p,
    Byte* digest,
    unsigned digestSize)
{
    if (!p)
    {
        return;
    }

    ::K7PalHashFinal(
        p->HashHandle,
        digest,
        digestSize);
}
