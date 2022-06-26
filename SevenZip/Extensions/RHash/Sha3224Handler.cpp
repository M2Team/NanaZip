/*
 * PROJECT:   NanaZip
 * FILE:      Sha3224Handler.cpp
 * PURPOSE:   Implementation for SHA3 224 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/sha3.h"

class CSha3224Handler :
    public IHasher,
    public CMyUnknownImp
{
    sha3_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSha3224Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CSha3224Handler::Init() throw()
{
    ::rhash_sha3_224_init(&this->Context);
}

STDMETHODIMP_(void) CSha3224Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_sha3_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSha3224Handler::Final(Byte* digest) throw()
{
    ::rhash_sha3_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSha3224Handler,
    0x391,
    "SHA3-224",
    sha3_224_hash_size)
