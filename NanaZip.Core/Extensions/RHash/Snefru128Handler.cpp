/*
 * PROJECT:   NanaZip
 * FILE:      Snefru128Handler.cpp
 * PURPOSE:   Implementation for Snefru-128 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <snefru.h>

class CSnefru128Handler final :
    public IHasher,
    public CMyUnknownImp
{
    snefru_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSnefru128Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CSnefru128Handler::Init() throw()
{
    ::rhash_snefru128_init(&this->Context);
}

STDMETHODIMP_(void) CSnefru128Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_snefru_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSnefru128Handler::Final(Byte* digest) throw()
{
    ::rhash_snefru_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSnefru128Handler,
    0x3A1,
    "SNEFRU-128",
    snefru128_hash_length)
