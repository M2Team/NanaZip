/*
 * PROJECT:   NanaZip
 * FILE:      TthHandler.cpp
 * PURPOSE:   Implementation for Tiger Tree Hash (TTH) hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <tth.h>

class CTthHandler final :
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

    Z7_IFACES_IMP_UNK_1(IHasher)
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
