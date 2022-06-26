/*
 * PROJECT:   NanaZip
 * FILE:      EdonR224Handler.cpp
 * PURPOSE:   Implementation for EDON-R 224 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/edonr.h"

class CEdonR224Handler:
  public IHasher,
  public CMyUnknownImp
{
    edonr_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEdonR224Handler()
    {
        this->Init();
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CEdonR224Handler::Init() throw()
{
    ::rhash_edonr224_init(&this->Context);
}

STDMETHODIMP_(void) CEdonR224Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_edonr256_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEdonR224Handler::Final(Byte *digest) throw()
{
    ::rhash_edonr256_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEdonR224Handler,
    0x304,
    "EDON-R-224",
    edonr224_hash_size)
