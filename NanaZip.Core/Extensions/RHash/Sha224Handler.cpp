/*
 * PROJECT:   NanaZip
 * FILE:      Sha224Handler.cpp
 * PURPOSE:   Implementation for SHA-224 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <sha256.h>

class CSha224Handler final :
    public IHasher,
    public CMyUnknownImp
{
    sha256_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSha224Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CSha224Handler::Init() throw()
{
    ::rhash_sha224_init(&this->Context);
}

STDMETHODIMP_(void) CSha224Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_sha256_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSha224Handler::Final(Byte* digest) throw()
{
    ::rhash_sha256_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSha224Handler,
    0x381,
    "SHA224",
    sha224_hash_size)
