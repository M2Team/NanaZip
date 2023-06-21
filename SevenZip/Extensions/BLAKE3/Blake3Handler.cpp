/*
 * PROJECT:   NanaZip
 * FILE:      Blake3Handler.cpp
 * PURPOSE:   Implementation for BLAKE3 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../../ThirdParty/LZMA/C/CpuArch.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include <cstring>

EXTERN_C_BEGIN
#include <blake3.h>
EXTERN_C_END

class CBlake3Handler :
    public IHasher,
    public CMyUnknownImp
{
    blake3_hasher Context;
    Byte mtDummy[1 << 7];

public:

    CBlake3Handler()
    {
        this->Init();
    }

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CBlake3Handler::Init() throw()
{
    ::blake3_hasher_init(&this->Context);
}

STDMETHODIMP_(void) CBlake3Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::blake3_hasher_update(&this->Context, data, size);
}

STDMETHODIMP_(void) CBlake3Handler::Final(Byte* digest) throw()
{
    ::blake3_hasher_finalize(&this->Context, digest, BLAKE3_OUT_LEN);
}

REGISTER_HASHER(
    CBlake3Handler,
    0x2E1,
    "BLAKE3",
    BLAKE3_OUT_LEN)
