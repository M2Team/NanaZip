/*
 * PROJECT:   NanaZip
 * FILE:      Md5Handler.cpp
 * PURPOSE:   Implementation for MD5 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <md5.h>

class CMd5Handler final :
    public IHasher,
    public CMyUnknownImp
{
    md5_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CMd5Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CMd5Handler::Init() throw()
{
    ::rhash_md5_init(&this->Context);
}

STDMETHODIMP_(void) CMd5Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_md5_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CMd5Handler::Final(Byte* digest) throw()
{
    ::rhash_md5_final(&this->Context, digest);
}

REGISTER_HASHER(
    CMd5Handler,
    0x362,
    "MD5",
    md5_hash_size)
