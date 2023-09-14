// CrcReg.cpp

#include "StdAfx.h"

#include "../../C/7zCrc.h"
#include "../../C/CpuArch.h"

#include "../Common/MyCom.h"

#include "../7zip/Common/RegisterCodec.h"

EXTERN_C_BEGIN

// UInt32 Z7_FASTCALL CrcUpdateT1(UInt32 v, const void *data, size_t size, const UInt32 *table);

extern CRC_FUNC g_CrcUpdate;
// extern CRC_FUNC g_CrcUpdateT4;
extern CRC_FUNC g_CrcUpdateT8;
extern CRC_FUNC g_CrcUpdateT0_32;
extern CRC_FUNC g_CrcUpdateT0_64;

EXTERN_C_END

Z7_CLASS_IMP_COM_2(
  CCrcHasher
  , IHasher
  , ICompressSetCoderProperties
)
  UInt32 _crc;
  CRC_FUNC _updateFunc;

  Z7_CLASS_NO_COPY(CCrcHasher)

  bool SetFunctions(UInt32 tSize);
public:
  Byte _mtDummy[1 << 7];  // it's public to eliminate clang warning: unused private field

  CCrcHasher(): _crc(CRC_INIT_VAL) { SetFunctions(0); }
};

bool CCrcHasher::SetFunctions(UInt32 tSize)
{
  CRC_FUNC f = NULL;
       if (tSize ==  0) f = g_CrcUpdate;
  // else if (tSize ==  1) f = CrcUpdateT1;
  // else if (tSize ==  4) f = g_CrcUpdateT4;
  else if (tSize ==  8) f = g_CrcUpdateT8;
  else if (tSize == 32) f = g_CrcUpdateT0_32;
  else if (tSize == 64) f = g_CrcUpdateT0_64;
  
  if (!f)
  {
    _updateFunc = g_CrcUpdate;
    return false;
  }
  _updateFunc = f;
  return true;
}

Z7_COM7F_IMF(CCrcHasher::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps))
{
  for (UInt32 i = 0; i < numProps; i++)
  {
    if (propIDs[i] == NCoderPropID::kDefaultProp)
    {
      const PROPVARIANT &prop = coderProps[i];
      if (prop.vt != VT_UI4)
        return E_INVALIDARG;
      if (!SetFunctions(prop.ulVal))
        return E_NOTIMPL;
    }
  }
  return S_OK;
}

Z7_COM7F_IMF2(void, CCrcHasher::Init())
{
  _crc = CRC_INIT_VAL;
}

Z7_COM7F_IMF2(void, CCrcHasher::Update(const void *data, UInt32 size))
{
  _crc = _updateFunc(_crc, data, size, g_CrcTable);
}

Z7_COM7F_IMF2(void, CCrcHasher::Final(Byte *digest))
{
  const UInt32 val = CRC_GET_DIGEST(_crc);
  SetUi32(digest, val)
}

REGISTER_HASHER(CCrcHasher, 0x1, "CRC32", 4)
