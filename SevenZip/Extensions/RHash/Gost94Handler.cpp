/*
 * PROJECT:   NanaZip
 * FILE:      Gost94Handler.cpp
 * PURPOSE:   Implementation for GOST R 34.11-94 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/gost94.h"

class CGost94Handler:
  public IHasher,
  public CMyUnknownImp
{
    gost94_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CGost94Handler()
    {
        this->Init();
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CGost94Handler::Init() throw()
{
    ::rhash_gost94_init(&this->Context);
}

STDMETHODIMP_(void) CGost94Handler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_gost94_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CGost94Handler::Final(Byte *digest) throw()
{
    ::rhash_gost94_final(&this->Context, digest);
}

REGISTER_HASHER(
    CGost94Handler,
    0x30A,
    "GOST94",
    gost94_hash_length)
