/*
 * PROJECT:   NanaZip
 * FILE:      Gost94CryptoProHandler.cpp
 * PURPOSE:   Implementation for GOST R 34.11-94 CryptoPro hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <gost94.h>

class CGost94CryptoProHandler final :
    public IHasher,
    public CMyUnknownImp
{
    gost94_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CGost94CryptoProHandler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CGost94CryptoProHandler::Init() throw()
{
    ::rhash_gost94_cryptopro_init(&this->Context);
}

STDMETHODIMP_(void) CGost94CryptoProHandler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_gost94_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CGost94CryptoProHandler::Final(Byte* digest) throw()
{
    ::rhash_gost94_final(&this->Context, digest);
}

REGISTER_HASHER(
    CGost94CryptoProHandler,
    0x342,
    "GOST94CryptoPro",
    gost94_hash_length)
