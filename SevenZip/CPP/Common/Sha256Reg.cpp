// Sha256Reg.cpp

#include "StdAfx.h"

#include "../../C/Sha256.h"

#include "../Common/MyBuffer2.h"
#include "../Common/MyCom.h"

#include "../7zip/Common/RegisterCodec.h"

class CSha256Hasher:
  public IHasher,
  public ICompressSetCoderProperties,
  public CMyUnknownImp
{
  CAlignedBuffer _buf;
  Byte mtDummy[1 << 7];

  CSha256 *Sha() { return (CSha256 *)(void *)(Byte *)_buf; }
public:
  CSha256Hasher():
    _buf(sizeof(CSha256))
  {
    Sha256_SetFunction(Sha(), 0);
    Sha256_InitState(Sha());
  }

  MY_UNKNOWN_IMP2(IHasher, ICompressSetCoderProperties)
  INTERFACE_IHasher(;)
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
};

STDMETHODIMP_(void) CSha256Hasher::Init() throw()
{
  Sha256_InitState(Sha());
}

STDMETHODIMP_(void) CSha256Hasher::Update(const void *data, UInt32 size) throw()
{
  Sha256_Update(Sha(), (const Byte *)data, size);
}

STDMETHODIMP_(void) CSha256Hasher::Final(Byte *digest) throw()
{
  Sha256_Final(Sha(), digest);
}


STDMETHODIMP CSha256Hasher::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps)
{
  unsigned algo = 0;
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    if (propIDs[i] == NCoderPropID::kDefaultProp)
    {
      if (prop.vt != VT_UI4)
        return E_INVALIDARG;
      if (prop.ulVal > 2)
        return E_NOTIMPL;
      algo = (unsigned)prop.ulVal;
    }
  }
  if (!Sha256_SetFunction(Sha(), algo))
    return E_NOTIMPL;
  return S_OK;
}

REGISTER_HASHER(CSha256Hasher, 0xA, "SHA256", SHA256_DIGEST_SIZE)
