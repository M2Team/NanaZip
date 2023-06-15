/*
 * PROJECT:   NanaZip
 * FILE:      EdonR256Handler.cpp
 * PURPOSE:   Implementation for EDON-R 256 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/edonr.h"

class CEdonR256Handler:
  public IHasher,
  public CMyUnknownImp
{
    edonr_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEdonR256Handler()
    {
        this->Init();
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CEdonR256Handler::Init() throw()
{
    ::rhash_edonr256_init(&this->Context);
}

STDMETHODIMP_(void) CEdonR256Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_edonr256_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEdonR256Handler::Final(Byte *digest) throw()
{
    ::rhash_edonr256_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEdonR256Handler,
    0x332,
    "EDON-R-256",
    edonr256_hash_size)
