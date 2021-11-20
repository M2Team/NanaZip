// Sha384Reg.cpp /TR 2018-11-02

#include "StdAfx.h"

#include "../../C/CpuArch.h"

EXTERN_C_BEGIN
#include "../../C/hashes/sha.h"
EXTERN_C_END

#include "../Common/MyCom.h"
#include "../7zip/Common/RegisterCodec.h"

// SHA384
class CSHA384Hasher:
  public IHasher,
  public CMyUnknownImp
{
  SHA384_CTX _ctx;
  Byte mtDummy[1 << 7];

public:
  CSHA384Hasher() { SHA384_Init(&_ctx); }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CSHA384Hasher::Init() throw()
{
  SHA384_Init(&_ctx);
}

STDMETHODIMP_(void) CSHA384Hasher::Update(const void *data, UInt32 size) throw()
{
  SHA384_Update(&_ctx, (const Byte *)data, size);
}

STDMETHODIMP_(void) CSHA384Hasher::Final(Byte *digest) throw()
{
  SHA384_Final(digest, &_ctx);
}
REGISTER_HASHER(CSHA384Hasher, 0x208, "SHA384", SHA384_DIGEST_LENGTH)
