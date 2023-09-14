/*
 * PROJECT:   NanaZip
 * FILE:      Blake2bHandler.cpp
 * PURPOSE:   Implementation for BLAKE2b hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <blake2b.h>

class CBlake2bHandler final :
    public IHasher,
    public CMyUnknownImp
{
    blake2b_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CBlake2bHandler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CBlake2bHandler::Init() throw()
{
    ::rhash_blake2b_init(&this->Context);
}

STDMETHODIMP_(void) CBlake2bHandler::Update(
    const void *data,
    UInt32 size) throw()
{
    ::rhash_blake2b_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CBlake2bHandler::Final(Byte *digest) throw()
{
    ::rhash_blake2b_final(&this->Context, digest);
}

REGISTER_HASHER(
    CBlake2bHandler,
    0x311,
    "BLAKE2b",
    blake2b_hash_size)
