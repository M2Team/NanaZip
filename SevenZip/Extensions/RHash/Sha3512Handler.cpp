/*
 * PROJECT:   NanaZip
 * FILE:      Sha3512Handler.cpp
 * PURPOSE:   Implementation for SHA3 512 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/sha3.h"

class CSha3512Handler :
    public IHasher,
    public CMyUnknownImp
{
    sha3_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CSha3512Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CSha3512Handler::Init() throw()
{
    ::rhash_sha3_512_init(&this->Context);
}

STDMETHODIMP_(void) CSha3512Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_sha3_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSha3512Handler::Final(Byte* digest) throw()
{
    ::rhash_sha3_final(&this->Context, digest);
}

REGISTER_HASHER(
    CSha3512Handler,
    0x394,
    "SHA3-512",
    sha3_512_hash_size)
