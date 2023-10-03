/*
 * PROJECT:   NanaZip
 * FILE:      Xxh64Handler.cpp
 * PURPOSE:   Implementation for XXH64 hash algorithm
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

class CXxh64Handler final :
    public IHasher,
    public CMyUnknownImp
{
    XXH64_state_t* Context;
    Byte mtDummy[1 << 7];

public:

    CXxh64Handler()
    {
        this->Context = ::XXH64_createState();
    }

    ~CXxh64Handler()
    {
        ::XXH64_freeState(this->Context);
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CXxh64Handler::Init() throw()
{
    ::XXH64_reset(this->Context, 0);
}

STDMETHODIMP_(void) CXxh64Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::XXH64_update(this->Context, data, size);
}

STDMETHODIMP_(void) CXxh64Handler::Final(Byte* digest) throw()
{
    XXH64_hash_t FinalDigest = ::XXH64_digest(this->Context);
    std::memcpy(digest, &FinalDigest, 8);
}

REGISTER_HASHER(
    CXxh64Handler,
    0x2F2,
    "XXH64",
    8)
