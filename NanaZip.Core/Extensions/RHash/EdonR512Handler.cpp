/*
 * PROJECT:   NanaZip
 * FILE:      EdonR512Handler.cpp
 * PURPOSE:   Implementation for EDON-R 512 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <edonr.h>

class CEdonR512Handler final :
    public IHasher,
    public CMyUnknownImp
{
    edonr_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEdonR512Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CEdonR512Handler::Init() throw()
{
    ::rhash_edonr512_init(&this->Context);
}

STDMETHODIMP_(void) CEdonR512Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_edonr512_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEdonR512Handler::Final(Byte *digest) throw()
{
    ::rhash_edonr512_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEdonR512Handler,
    0x334,
    "EDON-R-512",
    edonr512_hash_size)
