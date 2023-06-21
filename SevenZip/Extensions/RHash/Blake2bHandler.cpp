/*
 * PROJECT:   NanaZip
 * FILE:      Blake2bHandler.cpp
 * PURPOSE:   Implementation for BLAKE2b hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/blake2b.h"

class CBlake2bHandler:
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

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
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
