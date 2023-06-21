/*
 * PROJECT:   NanaZip
 * FILE:      AichHandler.cpp
 * PURPOSE:   Implementation for EMule AICH hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/aich.h"

class CAichHandler :
  public IHasher,
  public CMyUnknownImp
{
    aich_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CAichHandler()
    {
        this->Init();
    }

    ~CAichHandler()
    {
        ::rhash_aich_cleanup(&this->Context);
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CAichHandler::Init() throw()
{
    ::rhash_aich_init(&this->Context);
}

STDMETHODIMP_(void) CAichHandler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_aich_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CAichHandler::Final(Byte *digest) throw()
{
    ::rhash_aich_final(&this->Context, digest);
}

REGISTER_HASHER(
    CAichHandler,
    0x301,
    "AICH",
    20)
