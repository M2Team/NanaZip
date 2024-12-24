/*
 * PROJECT:   NanaZip
 * FILE:      Sha1Wrapper.cpp
 * PURPOSE:   Implementation for SHA-256 wrapper for 7-Zip
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "Sha256Wrapper.h"

BoolInt Sha256_SetFunction(
    CSha256* p,
    unsigned algo)
{
    UNREFERENCED_PARAMETER(p);
    UNREFERENCED_PARAMETER(algo);
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

    if (!p->HashHandle)
    {
        ::K7PalHashCreate(
            &p->HashHandle,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0);
    }

    if (p->HashHandle)
    {
        ::K7PalHashReset(p->HashHandle);
    }
}

void Sha256_Init(
    CSha256* p)
{
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
