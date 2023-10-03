/*
 * PROJECT:   NanaZip
 * FILE:      Md2Handler.cpp
 * PURPOSE:   Implementation for MD2 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

EXTERN_C_BEGIN
#include <md2.h>
EXTERN_C_END

class CMd2Handler final :
    public IHasher,
    public CMyUnknownImp
{
    MD2_CTX Context;
    Byte mtDummy[1 << 7];

public:

    CMd2Handler()
    {
        this->Init();
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CMd2Handler::Init() throw()
{
    ::MD2_Init(&this->Context);
}

STDMETHODIMP_(void) CMd2Handler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::MD2_Update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CMd2Handler::Final(Byte* digest) throw()
{
    ::MD2_Final(digest, &this->Context);
}

REGISTER_HASHER(
    CMd2Handler,
    0x2C1,
    "MD2",
    MD2_DIGEST_LENGTH)
