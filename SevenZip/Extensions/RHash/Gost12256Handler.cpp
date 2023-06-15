/*
 * PROJECT:   NanaZip
 * FILE:      Gost12256Handler.cpp
 * PURPOSE:   Implementation for GOST R 34.11-2012 256 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/gost12.h"

class CGost12256Handler:
  public IHasher,
  public CMyUnknownImp
{
    gost12_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CGost12256Handler()
    {
        this->Init();
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CGost12256Handler::Init() throw()
{
    ::rhash_gost12_256_init(&this->Context);
}

STDMETHODIMP_(void) CGost12256Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_gost12_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CGost12256Handler::Final(Byte *digest) throw()
{
    ::rhash_gost12_final(&this->Context, digest);
}

REGISTER_HASHER(
    CGost12256Handler,
    0x343,
    "GOST12-256",
    gost12_256_hash_size)
