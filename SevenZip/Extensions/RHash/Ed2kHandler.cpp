/*
 * PROJECT:   NanaZip
 * FILE:      Ed2kHandler.cpp
 * PURPOSE:   Implementation for EDonkey 2000 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/ed2k.h"

class CEd2kHandler:
  public IHasher,
  public CMyUnknownImp
{
    ed2k_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CEd2kHandler()
    {
        this->Init();
    }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CEd2kHandler::Init() throw()
{
    ::rhash_ed2k_init(&this->Context);
}

STDMETHODIMP_(void) CEd2kHandler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_ed2k_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CEd2kHandler::Final(Byte *digest) throw()
{
    ::rhash_ed2k_final(&this->Context, digest);
}

REGISTER_HASHER(
    CEd2kHandler,
    0x303,
    "ED2K",
    16)
