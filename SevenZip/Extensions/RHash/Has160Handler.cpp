/*
 * PROJECT:   NanaZip
 * FILE:      Has160Handler.cpp
 * PURPOSE:   Implementation for HAS-160 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/has160.h"

class CHas160Handler :
    public IHasher,
    public CMyUnknownImp
{
    has160_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CHas160Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CHas160Handler::Init() throw()
{
    ::rhash_has160_init(&this->Context);
}

STDMETHODIMP_(void) CHas160Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_has160_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CHas160Handler::Final(Byte* digest) throw()
{
    ::rhash_has160_final(&this->Context, digest);
}

REGISTER_HASHER(
    CHas160Handler,
    0x351,
    "HAS-160",
    has160_hash_size)
