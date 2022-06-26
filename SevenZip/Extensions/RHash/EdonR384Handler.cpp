/*
 * PROJECT:   NanaZip
 * FILE:      EdonR384Handler.cpp
 * PURPOSE:   Implementation for EDON-R 384 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/edonr.h"

class CEdonR384Handler:
  public IHasher,
  public CMyUnknownImp
{
    edonr_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEdonR384Handler()
    {
        this->Init();
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CEdonR384Handler::Init() throw()
{
    ::rhash_edonr384_init(&this->Context);
}

STDMETHODIMP_(void) CEdonR384Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_edonr512_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEdonR384Handler::Final(Byte *digest) throw()
{
    ::rhash_edonr512_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEdonR384Handler,
    0x333,
    "EDON-R-384",
    edonr384_hash_size)
