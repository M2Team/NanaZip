/*
 * PROJECT:   NanaZip
 * FILE:      Ed2kHandler.cpp
 * PURPOSE:   Implementation for ED2K hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <ed2k.h>

class CEd2kHandler final :
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

    Z7_IFACES_IMP_UNK_1(IHasher)
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
    0x321,
    "ED2K",
    16)
