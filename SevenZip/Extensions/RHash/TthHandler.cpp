/*
 * PROJECT:   NanaZip
 * FILE:      TthHandler.cpp
 * PURPOSE:   Implementation for Tiger Tree Hash (TTH) hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/tth.h"

class CTthHandler :
    public IHasher,
    public CMyUnknownImp
{
    tth_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CTthHandler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CTthHandler::Init() throw()
{
    ::rhash_tth_init(&this->Context);
}

STDMETHODIMP_(void) CTthHandler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_tth_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CTthHandler::Final(Byte* digest) throw()
{
    ::rhash_tth_final(&this->Context, digest);
}

REGISTER_HASHER(
    CTthHandler,
    0x3B3,
    "TTH",
    24)
