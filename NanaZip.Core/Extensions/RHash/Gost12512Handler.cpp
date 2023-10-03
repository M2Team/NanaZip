/*
 * PROJECT:   NanaZip
 * FILE:      Gost12512Handler.cpp
 * PURPOSE:   Implementation for GOST R 34.11-2012 512 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <gost12.h>

class CGost12512Handler final :
    public IHasher,
    public CMyUnknownImp
{
    gost12_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CGost12512Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CGost12512Handler::Init() throw()
{
    ::rhash_gost12_512_init(&this->Context);
}

STDMETHODIMP_(void) CGost12512Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_gost12_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CGost12512Handler::Final(Byte *digest) throw()
{
    ::rhash_gost12_final(&this->Context, digest);
}

REGISTER_HASHER(
    CGost12512Handler,
    0x344,
    "GOST12-512",
    gost12_512_hash_size)
