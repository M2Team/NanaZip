/*
 * PROJECT:   NanaZip
 * FILE:      Tiger2Handler.cpp
 * PURPOSE:   Implementation for Tiger2 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/tiger.h"

class CTiger2Handler :
    public IHasher,
    public CMyUnknownImp
{
    tiger_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CTiger2Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CTiger2Handler::Init() throw()
{
    ::rhash_tiger2_init(&this->Context);
}

STDMETHODIMP_(void) CTiger2Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_tiger_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CTiger2Handler::Final(Byte* digest) throw()
{
    ::rhash_tiger_final(&this->Context, digest);
}

REGISTER_HASHER(
    CTiger2Handler,
    0x3B2,
    "TIGER2",
    tiger_hash_length)
