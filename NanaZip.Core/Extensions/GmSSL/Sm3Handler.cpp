/*
 * PROJECT:   NanaZip
 * FILE:      Sm3Handler.cpp
 * PURPOSE:   Implementation for SM3 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <sm3.h>

class CSm3Handler final :
    public IHasher,
    public CMyUnknownImp
{
    SM3_CTX Context;
    Byte mtDummy[1 << 7];

public:

    CSm3Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CSm3Handler::Init() throw()
{
    ::sm3_init(&this->Context);
}

STDMETHODIMP_(void) CSm3Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::sm3_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CSm3Handler::Final(Byte* digest) throw()
{
    ::sm3_finish(&this->Context, digest);
}

REGISTER_HASHER(
    CSm3Handler,
    0x2D1,
    "SM3",
    SM3_DIGEST_SIZE)
