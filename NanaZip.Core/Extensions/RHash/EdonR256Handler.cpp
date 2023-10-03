/*
 * PROJECT:   NanaZip
 * FILE:      EdonR256Handler.cpp
 * PURPOSE:   Implementation for EDON-R 256 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <edonr.h>

class CEdonR256Handler final :
    public IHasher,
    public CMyUnknownImp
{
    edonr_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEdonR256Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CEdonR256Handler::Init() throw()
{
    ::rhash_edonr256_init(&this->Context);
}

STDMETHODIMP_(void) CEdonR256Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_edonr256_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEdonR256Handler::Final(Byte *digest) throw()
{
    ::rhash_edonr256_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEdonR256Handler,
    0x332,
    "EDON-R-256",
    edonr256_hash_size)
