// Md2Reg.cpp /TR 2018-11-02

#include "../../../ThirdParty/LZMA/CPP/Common/StdAfx.h"

#include "../../../ThirdParty/LZMA/C/CpuArch.h"

EXTERN_C_BEGIN
#include "../../C/hashes/md2.h"
EXTERN_C_END

#include "../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

// MD2
class CMD2Hasher:
  public IHasher,
  public CMyUnknownImp
{
  MD2_CTX _ctx;
  Byte mtDummy[1 << 7];

public:
  CMD2Hasher() { MD2_Init(&_ctx); }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CMD2Hasher::Init() throw()
{
  MD2_Init(&_ctx);
}

STDMETHODIMP_(void) CMD2Hasher::Update(const void *data, UInt32 size) throw()
{
  MD2_Update(&_ctx, (const Byte *)data, size);
}

STDMETHODIMP_(void) CMD2Hasher::Final(Byte *digest) throw()
{
  MD2_Final(digest, &_ctx);
}
REGISTER_HASHER(CMD2Hasher, 0x205, "MD2", MD2_DIGEST_LENGTH)
