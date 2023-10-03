/*
 * PROJECT:   NanaZip
 * FILE:      Md4Handler.cpp
 * PURPOSE:   Implementation for MD4 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <md4.h>

class CMd4Handler final :
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

    Z7_IFACES_IMP_UNK_1(IHasher)
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
