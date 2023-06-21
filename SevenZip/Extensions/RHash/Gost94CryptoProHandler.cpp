/*
 * PROJECT:   NanaZip
 * FILE:      Gost94CryptoProHandler.cpp
 * PURPOSE:   Implementation for GOST R 34.11-94 CryptoPro hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/gost94.h"

class CGost94CryptoProHandler :
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

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
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
