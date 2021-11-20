// Md4Reg.cpp /TR 2018-11-02

#include "StdAfx.h"

#include "../../C/CpuArch.h"

EXTERN_C_BEGIN
#include "../../C/hashes/md4.h"
EXTERN_C_END

#include "../Common/MyCom.h"
#include "../7zip/Common/RegisterCodec.h"

// MD4
class CMD4Hasher:
  public IHasher,
  public CMyUnknownImp
{
  MD4_CTX _ctx;
  Byte mtDummy[1 << 7];

public:
  CMD4Hasher() { MD4_Init(&_ctx); }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CMD4Hasher::Init() throw()
{
  MD4_Init(&_ctx);
}

STDMETHODIMP_(void) CMD4Hasher::Update(const void *data, UInt32 size) throw()
{
  MD4_Update(&_ctx, (const Byte *)data, size);
}

STDMETHODIMP_(void) CMD4Hasher::Final(Byte *digest) throw()
{
  MD4_Final(digest, &_ctx);
}
REGISTER_HASHER(CMD4Hasher, 0x206, "MD4", MD4_DIGEST_LENGTH)
