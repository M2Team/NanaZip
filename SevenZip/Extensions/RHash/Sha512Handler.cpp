/*
 * PROJECT:   NanaZip
 * FILE:      Sha512Handler.cpp
 * PURPOSE:   Implementation for SHA-512 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/sha512.h"

class CSha512Handler :
    public IHasher,
    public CMyUnknownImp
{
    sha512_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSha512Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CSha512Handler::Init() throw()
{
    ::rhash_sha512_init(&this->Context);
}

STDMETHODIMP_(void) CSha512Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_sha512_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSha512Handler::Final(Byte* digest) throw()
{
    ::rhash_sha512_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSha512Handler,
    0x383,
    "SHA512",
    sha512_hash_size)
