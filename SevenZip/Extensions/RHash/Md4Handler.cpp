/*
 * PROJECT:   NanaZip
 * FILE:      Md4Handler.cpp
 * PURPOSE:   Implementation for MD4 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/md4.h"

class CMd4Handler :
    public IHasher,
    public CMyUnknownImp
{
    md4_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CMd4Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CMd4Handler::Init() throw()
{
    ::rhash_md4_init(&this->Context);
}

STDMETHODIMP_(void) CMd4Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_md4_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CMd4Handler::Final(Byte* digest) throw()
{
    ::rhash_md4_final(&this->Context, digest);
}

REGISTER_HASHER(
    CMd4Handler,
    0x361,
    "MD4",
    md4_hash_size)
