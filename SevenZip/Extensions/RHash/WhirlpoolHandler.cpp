/*
 * PROJECT:   NanaZip
 * FILE:      WhirlpoolHandler.cpp
 * PURPOSE:   Implementation for Whirlpool hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/whirlpool.h"

class CWhirlpoolHandler :
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

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
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
