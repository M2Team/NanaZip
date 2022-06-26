/*
 * PROJECT:   NanaZip
 * FILE:      Snefru128Handler.cpp
 * PURPOSE:   Implementation for Snefru-128 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/snefru.h"

class CSnefru128Handler :
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

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
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
