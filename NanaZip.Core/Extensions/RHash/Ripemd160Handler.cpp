/*
 * PROJECT:   NanaZip
 * FILE:      Ripemd160Handler.cpp
 * PURPOSE:   Implementation for RIPEMD-160 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <ripemd-160.h>

class CRipemd160Handler final :
    public IHasher,
    public CMyUnknownImp
{
    ripemd160_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CRipemd160Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CRipemd160Handler::Init() throw()
{
    ::rhash_ripemd160_init(&this->Context);
}

STDMETHODIMP_(void) CRipemd160Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_ripemd160_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CRipemd160Handler::Final(Byte* digest) throw()
{
    ::rhash_ripemd160_final(&this->Context, digest);
}

REGISTER_HASHER(
    CRipemd160Handler,
    0x371,
    "RIPEMD-160",
    ripemd160_hash_size)
