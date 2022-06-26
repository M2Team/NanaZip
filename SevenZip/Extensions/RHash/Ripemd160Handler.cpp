/*
 * PROJECT:   NanaZip
 * FILE:      Ripemd160Handler.cpp
 * PURPOSE:   Implementation for RIPEMD-160 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/ripemd-160.h"

class CRipemd160Handler :
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

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
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
