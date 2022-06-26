/*
 * PROJECT:   NanaZip
 * FILE:      EdonR512Handler.cpp
 * PURPOSE:   Implementation for EDON-R 512 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/edonr.h"

class CEdonR512Handler:
  public IHasher,
  public CMyUnknownImp
{
    edonr_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEdonR512Handler()
    {
        this->Init();
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CEdonR512Handler::Init() throw()
{
    ::rhash_edonr512_init(&this->Context);
}

STDMETHODIMP_(void) CEdonR512Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_edonr512_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEdonR512Handler::Final(Byte *digest) throw()
{
    ::rhash_edonr512_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEdonR512Handler,
    0x334,
    "EDON-R-512",
    edonr512_hash_size)
