// Sha1Reg.cpp

#include "StdAfx.h"

#include "../../C/Sha1.h"

#include "../Common/MyBuffer2.h"
#include "../Common/MyCom.h"

#include "../7zip/Common/RegisterCodec.h"

class CSha1Hasher:
  public IHasher,
  public ICompressSetCoderProperties,
  public CMyUnknownImp
{
  CAlignedBuffer _buf;
  Byte mtDummy[1 << 7];
  
  CSha1 *Sha() { return (CSha1 *)(void *)(Byte *)_buf; }
public:
  CSha1Hasher():
    _buf(sizeof(CSha1))
  {
    Sha1_SetFunction(Sha(), 0);
    Sha1_InitState(Sha());
  }

  MY_UNKNOWN_IMP2(IHasher, ICompressSetCoderProperties)
  INTERFACE_IHasher(;)
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
};

STDMETHODIMP_(void) CSha1Hasher::Init() throw()
{
  Sha1_InitState(Sha());
}

STDMETHODIMP_(void) CSha1Hasher::Update(const void *data, UInt32 size) throw()
{
  Sha1_Update(Sha(), (const Byte *)data, size);
}

STDMETHODIMP_(void) CSha1Hasher::Final(Byte *digest) throw()
{
  Sha1_Final(Sha(), digest);
}


STDMETHODIMP CSha1Hasher::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps)
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
  if (!Sha1_SetFunction(Sha(), algo))
    return E_NOTIMPL;
  return S_OK;
}

REGISTER_HASHER(CSha1Hasher, 0x201, "SHA1", SHA1_DIGEST_SIZE)
