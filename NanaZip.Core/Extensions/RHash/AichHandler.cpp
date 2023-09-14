/*
 * PROJECT:   NanaZip
 * FILE:      AichHandler.cpp
 * PURPOSE:   Implementation for EMule AICH hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <aich.h>

class CAichHandler  final :
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

    Z7_IFACES_IMP_UNK_1(IHasher)
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
