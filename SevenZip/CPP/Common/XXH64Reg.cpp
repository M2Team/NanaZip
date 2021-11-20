// XXH64Reg.cpp /TR 2018-11-02

#include "StdAfx.h"

#include "../../C/CpuArch.h"

#define XXH_STATIC_LINKING_ONLY
#include "../../C/zstd/xxhash.h"

#include "../Common/MyCom.h"
#include "../7zip/Common/RegisterCodec.h"

// XXH64
class CXXH64Hasher:
  public IHasher,
  public CMyUnknownImp
{
  XXH64_state_t *_ctx;
  Byte mtDummy[1 << 7];

public:
  CXXH64Hasher() { _ctx = XXH64_createState(); }
  ~CXXH64Hasher() { XXH64_freeState(_ctx); }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CXXH64Hasher::Init() throw()
{
  XXH64_reset(_ctx, 0);
}

STDMETHODIMP_(void) CXXH64Hasher::Update(const void *data, UInt32 size) throw()
{
  XXH64_update(_ctx, data, size);
}

STDMETHODIMP_(void) CXXH64Hasher::Final(Byte *digest) throw()
{
  UInt64 val = XXH64_digest(_ctx);
  SetUi64(digest, val);
}
REGISTER_HASHER(CXXH64Hasher, 0x204, "XXH64", 8)
