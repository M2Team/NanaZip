/*
 * PROJECT:   NanaZip
 * FILE:      Xxh364Handler.cpp
 * PURPOSE:   Implementation for XXH3_64bits hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <cstring>

#define XXH_STATIC_LINKING_ONLY
#include <xxhash.h>

class CXxh364Handler final :
    public IHasher,
    public CMyUnknownImp
{
    XXH3_state_t* Context;
    Byte mtDummy[1 << 7];

public:

    CXxh364Handler()
    {
        this->Context = ::XXH3_createState();
    }

    ~CXxh364Handler()
    {
        ::XXH3_freeState(this->Context);
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CXxh364Handler::Init() throw()
{
    ::XXH3_64bits_reset(this->Context);
}

STDMETHODIMP_(void) CXxh364Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::XXH3_64bits_update(this->Context, data, size);
}

STDMETHODIMP_(void) CXxh364Handler::Final(Byte* digest) throw()
{
    XXH64_hash_t FinalDigest = ::XXH3_64bits_digest(this->Context);
    std::memcpy(digest, &FinalDigest, 8);
}

REGISTER_HASHER(
    CXxh364Handler,
    0x2F3,
    "XXH3_64bits",
    8)
