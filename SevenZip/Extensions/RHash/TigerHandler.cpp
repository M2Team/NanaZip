/*
 * PROJECT:   NanaZip
 * FILE:      TigerHandler.cpp
 * PURPOSE:   Implementation for Tiger hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/tiger.h"

class CTigerHandler :
    public IHasher,
    public CMyUnknownImp
{
    tiger_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CTigerHandler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CTigerHandler::Init() throw()
{
    ::rhash_tiger_init(&this->Context);
}

STDMETHODIMP_(void) CTigerHandler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::rhash_tiger_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CTigerHandler::Final(Byte* digest) throw()
{
    ::rhash_tiger_final(&this->Context, digest);
}

REGISTER_HASHER(
    CTigerHandler,
    0x3B1,
    "TIGER",
    tiger_hash_length)
