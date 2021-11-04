// CksumReg.cpp

#include "StdAfx.h"

#include "../../C/CpuArch.h"

#include "../Common/MyCom.h"

#include "../7zip/Common/RegisterCodec.h"

#include "../7zip/Compress/BZip2Crc.h"

class CCksumHasher:
  public IHasher,
  public CMyUnknownImp
{
  CBZip2Crc _crc;
  UInt64 _size;
  Byte mtDummy[1 << 7];
  
public:
  CCksumHasher()
  {
    _crc.Init(0);
    _size = 0;
  }

  MY_UNKNOWN_IMP1(IHasher)
  INTERFACE_IHasher(;)
};

STDMETHODIMP_(void) CCksumHasher::Init() throw()
{
  _crc.Init(0);
  _size = 0;
}

STDMETHODIMP_(void) CCksumHasher::Update(const void *data, UInt32 size) throw()
{
  _size += size;
  CBZip2Crc crc = _crc;
  for (UInt32 i = 0; i < size; i++)
    crc.UpdateByte(((const Byte *)data)[i]);
  _crc = crc;
}

STDMETHODIMP_(void) CCksumHasher::Final(Byte *digest) throw()
{
  UInt64 size = _size;
  CBZip2Crc crc = _crc;
  while (size)
  {
    crc.UpdateByte((Byte)size);
    size >>= 8;
  }
  const UInt32 val = crc.GetDigest();
  SetUi32(digest, val);
}

REGISTER_HASHER(CCksumHasher, 0x203, "CKSUM", 4)
