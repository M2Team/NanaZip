/*
 * PROJECT:   NanaZip
 * FILE:      Sha384Handler.cpp
 * PURPOSE:   Implementation for SHA-384 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/sha512.h"

class CSha384Handler :
    public IHasher,
    public CMyUnknownImp
{
    sha512_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSha384Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CSha384Handler::Init() throw()
{
    ::rhash_sha384_init(&this->Context);
}

STDMETHODIMP_(void) CSha384Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_sha512_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSha384Handler::Final(Byte* digest) throw()
{
    ::rhash_sha512_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSha384Handler,
    0x382,
    "SHA384",
    sha384_hash_size)
