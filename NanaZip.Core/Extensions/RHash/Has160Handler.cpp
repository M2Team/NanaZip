/*
 * PROJECT:   NanaZip
 * FILE:      Has160Handler.cpp
 * PURPOSE:   Implementation for HAS-160 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <has160.h>

class CHas160Handler final :
    public IHasher,
    public CMyUnknownImp
{
    has160_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CHas160Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CHas160Handler::Init() throw()
{
    ::rhash_has160_init(&this->Context);
}

STDMETHODIMP_(void) CHas160Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_has160_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CHas160Handler::Final(Byte* digest) throw()
{
    ::rhash_has160_final(&this->Context, digest);
}

REGISTER_HASHER(
    CHas160Handler,
    0x351,
    "HAS-160",
    has160_hash_size)
