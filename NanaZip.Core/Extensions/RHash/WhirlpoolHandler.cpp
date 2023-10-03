/*
 * PROJECT:   NanaZip
 * FILE:      WhirlpoolHandler.cpp
 * PURPOSE:   Implementation for Whirlpool hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <whirlpool.h>

class CWhirlpoolHandler final :
    public IHasher,
    public CMyUnknownImp
{
    whirlpool_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CWhirlpoolHandler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CWhirlpoolHandler::Init() throw()
{
    ::rhash_whirlpool_init(&this->Context);
}

STDMETHODIMP_(void) CWhirlpoolHandler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_whirlpool_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CWhirlpoolHandler::Final(Byte* digest) throw()
{
    ::rhash_whirlpool_final(&this->Context, digest);
}

REGISTER_HASHER(
    CWhirlpoolHandler,
    0x3D1,
    "WHIRLPOOL",
    64)
