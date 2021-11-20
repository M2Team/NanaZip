// Blake3Reg.cpp /TR 2021-04-06

#include "StdAfx.h"

#include "../../C/CpuArch.h"

EXTERN_C_BEGIN
#include "../../C/hashes/blake3.h"
EXTERN_C_END

#include "../Common/MyCom.h"
#include "../7zip/Common/RegisterCodec.h"

// BLAKE3
class CBLAKE3Hasher:
  public IHasher,
  public CMyUnknownImp
{
  blake3_hasher _ctx;
  Byte mtDummy[1 << 7];

public:
  CBLAKE3Hasher() { blake3_hasher_init(&_ctx); }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CBLAKE3Hasher::Init() throw()
{
  blake3_hasher_init(&_ctx);
}

STDMETHODIMP_(void) CBLAKE3Hasher::Update(const void *data, UInt32 size) throw()
{
  blake3_hasher_update(&_ctx, data, size);
}

STDMETHODIMP_(void) CBLAKE3Hasher::Final(Byte *digest) throw()
{
  blake3_hasher_finalize(&_ctx, digest, BLAKE3_OUT_LEN);
}
REGISTER_HASHER(CBLAKE3Hasher, 0x20a, "BLAKE3", BLAKE3_OUT_LEN)
