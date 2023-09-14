/*
 * PROJECT:   NanaZip
 * FILE:      Snefru256Handler.cpp
 * PURPOSE:   Implementation for Snefru-256 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <snefru.h>

class CSnefru256Handler final :
    public IHasher,
    public CMyUnknownImp
{
    snefru_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSnefru256Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CSnefru256Handler::Init() throw()
{
    ::rhash_snefru256_init(&this->Context);
}

STDMETHODIMP_(void) CSnefru256Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_snefru_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSnefru256Handler::Final(Byte* digest) throw()
{
    ::rhash_snefru_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSnefru256Handler,
    0x3A2,
    "SNEFRU-256",
    snefru256_hash_length)
