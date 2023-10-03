/*
 * PROJECT:   NanaZip
 * FILE:      Gost94Handler.cpp
 * PURPOSE:   Implementation for GOST R 34.11-94 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <gost94.h>

class CGost94Handler final :
    public IHasher,
    public CMyUnknownImp
{
    gost94_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CGost94Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CGost94Handler::Init() throw()
{
    ::rhash_gost94_init(&this->Context);
}

STDMETHODIMP_(void) CGost94Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_gost94_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CGost94Handler::Final(Byte *digest) throw()
{
    ::rhash_gost94_final(&this->Context, digest);
}

REGISTER_HASHER(
    CGost94Handler,
    0x341,
    "GOST94",
    gost94_hash_length)
