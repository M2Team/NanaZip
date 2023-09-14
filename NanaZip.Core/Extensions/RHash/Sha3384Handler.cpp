/*
 * PROJECT:   NanaZip
 * FILE:      Sha3384Handler.cpp
 * PURPOSE:   Implementation for SHA3-384 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <sha3.h>

class CSha3384Handler final :
    public IHasher,
    public CMyUnknownImp
{
    sha3_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSha3384Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CSha3384Handler::Init() throw()
{
    ::rhash_sha3_384_init(&this->Context);
}

STDMETHODIMP_(void) CSha3384Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_sha3_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSha3384Handler::Final(Byte* digest) throw()
{
    ::rhash_sha3_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSha3384Handler,
    0x393,
    "SHA3-384",
    sha3_384_hash_size)
