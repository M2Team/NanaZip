/*
 * PROJECT:   NanaZip
 * FILE:      Xxh64Handler.cpp
 * PURPOSE:   Implementation for XXH64 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include <cstring>

#define XXH_STATIC_LINKING_ONLY
#include <xxhash.h>

class CXxh64Handler :
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

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
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
