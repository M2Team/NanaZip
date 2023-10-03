/*
 * PROJECT:   NanaZip
 * FILE:      EdonR384Handler.cpp
 * PURPOSE:   Implementation for EDON-R 384 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <edonr.h>

class CEdonR384Handler final :
    public IHasher,
    public CMyUnknownImp
{
    edonr_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEdonR384Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CEdonR384Handler::Init() throw()
{
    ::rhash_edonr384_init(&this->Context);
}

STDMETHODIMP_(void) CEdonR384Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_edonr512_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEdonR384Handler::Final(Byte *digest) throw()
{
    ::rhash_edonr512_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEdonR384Handler,
    0x333,
    "EDON-R-384",
    edonr384_hash_size)
