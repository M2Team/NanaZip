// Crypto/RarAes.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/RotateDefs.h"

#include "RarAes.h"
#include "Sha1Cls.h"

namespace NCrypto {
namespace NRar3 {

CDecoder::CDecoder():
    CAesCbcDecoder(kAesKeySize),
    _thereIsSalt(false),
    _needCalc(true)
    // _rar350Mode(false)
{
  for (unsigned i = 0; i < sizeof(_salt); i++)
    _salt[i] = 0;
}

HRESULT CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size)
{
  bool prev = _thereIsSalt;
  _thereIsSalt = false;
  if (size == 0)
  {
    if (!_needCalc && prev)
      _needCalc = true;
    return S_OK;
  }
  if (size < 8)
    return E_INVALIDARG;
  _thereIsSalt = true;
  bool same = false;
  if (_thereIsSalt == prev)
  {
    same = true;
    if (_thereIsSalt)
    {
      for (unsigned i = 0; i < sizeof(_salt); i++)
        if (_salt[i] != data[i])
        {
          same = false;
          break;
        }
    }
  }
  for (unsigned i = 0; i < sizeof(_salt); i++)
    _salt[i] = data[i];
  if (!_needCalc && !same)
    _needCalc = true;
  return S_OK;
}

static const unsigned kPasswordLen_Bytes_MAX = 127 * 2;

void CDecoder::SetPassword(const Byte *data, unsigned size)
{
  if (size > kPasswordLen_Bytes_MAX)
    size = kPasswordLen_Bytes_MAX;
  bool same = false;
  if (size == _password.Size())
  {
    same = true;
    for (UInt32 i = 0; i < size; i++)
      if (data[i] != _password[i])
      {
        same = false;
        break;
      }
  }
  if (!_needCalc && !same)
    _needCalc = true;
  _password.Wipe();
  _password.CopyFrom(data, (size_t)size);
}

Z7_COM7F_IMF(CDecoder::Init())
{
  CalcKey();
  RINOK(SetKey(_key, kAesKeySize))
  RINOK(SetInitVector(_iv, AES_BLOCK_SIZE))
  return CAesCoder::Init();
}


// if (password_size_in_bytes + SaltSize > 64),
// the original rar code updates password_with_salt buffer
// with some generated data from SHA1 code.
 
// #define RAR_SHA1_REDUCE

#ifdef RAR_SHA1_REDUCE
  #define kNumW 16
  #define WW(i) W[(i)&15]
#else
  #define kNumW 80
  #define WW(i) W[i]
#endif

static void UpdatePswDataSha1(Byte *data)
{
  UInt32 W[kNumW];
  size_t i;
  
  for (i = 0; i < SHA1_NUM_BLOCK_WORDS; i++)
    W[i] = GetBe32(data + i * 4);
  
  for (i = 16; i < 80; i++)
  {
    WW(i) = rotlFixed(WW((i)-3) ^ WW((i)-8) ^ WW((i)-14) ^ WW((i)-16), 1);
  }
  
  for (i = 0; i < SHA1_NUM_BLOCK_WORDS; i++)
  {
    SetUi32(data + i * 4, W[kNumW - SHA1_NUM_BLOCK_WORDS + i])
  }
}


void CDecoder::CalcKey()
{
  if (!_needCalc)
    return;

  const unsigned kSaltSize = 8;
  
  Byte buf[kPasswordLen_Bytes_MAX + kSaltSize];
  
  if (_password.Size() != 0)
    memcpy(buf, _password, _password.Size());
  
  size_t rawSize = _password.Size();
  
  if (_thereIsSalt)
  {
    memcpy(buf + rawSize, _salt, kSaltSize);
    rawSize += kSaltSize;
  }
  
  MY_ALIGN (16)
  NSha1::CContext sha;
  sha.Init();
  
  MY_ALIGN (16)
  Byte digest[NSha1::kDigestSize];
  // rar reverts hash for sha.
  const UInt32 kNumRounds = ((UInt32)1 << 18);
  UInt32 pos = 0;
  UInt32 i;
  for (i = 0; i < kNumRounds; i++)
  {
    sha.Update(buf, rawSize);
    // if (_rar350Mode)
    {
      const UInt32 kBlockSize = 64;
      const UInt32 endPos = (pos + (UInt32)rawSize) & ~(kBlockSize - 1);
      if (endPos > pos + kBlockSize)
      {
        UInt32 curPos = pos & ~(kBlockSize - 1);
        curPos += kBlockSize;
        do
        {
          UpdatePswDataSha1(buf + (curPos - pos));
          curPos += kBlockSize;
        }
        while (curPos != endPos);
      }
    }
    pos += (UInt32)rawSize;
    Byte pswNum[3] = { (Byte)i, (Byte)(i >> 8), (Byte)(i >> 16) };
    sha.Update(pswNum, 3);
    pos += 3;
    if (i % (kNumRounds / 16) == 0)
    {
      MY_ALIGN (16)
      NSha1::CContext shaTemp = sha;
      shaTemp.Final(digest);
      _iv[i / (kNumRounds / 16)] = (Byte)digest[4 * 4 + 3];
    }
  }
  
  sha.Final(digest);
  for (i = 0; i < 4; i++)
    for (unsigned j = 0; j < 4; j++)
      _key[i * 4 + j] = (digest[i * 4 + 3 - j]);
    
  _needCalc = false;
}

}}
