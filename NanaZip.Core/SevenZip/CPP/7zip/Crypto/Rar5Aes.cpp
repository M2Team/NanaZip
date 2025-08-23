// Crypto/Rar5Aes.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#ifndef Z7_ST
#include "../../Windows/Synchronization.h"
#endif

#include "HmacSha256.h"
#include "Rar5Aes.h"

#define MY_ALIGN_FOR_SHA256  MY_ALIGN(16)

namespace NCrypto {
namespace NRar5 {

static const unsigned kNumIterationsLog_Max = 24;
static const unsigned kPswCheckCsumSize32 = 1;
static const unsigned kCheckSize32 = kPswCheckSize32 + kPswCheckCsumSize32;

CKey::CKey():
    _needCalc(true),
    _numIterationsLog(0)
{
  for (unsigned i = 0; i < sizeof(_salt); i++)
    _salt[i] = 0;
}

CKey::~CKey()
{
  Wipe();
}

void CKey::Wipe()
{
  _password.Wipe();
  Z7_memset_0_ARRAY(_salt);
  // Z7_memset_0_ARRAY(_key32);
  // Z7_memset_0_ARRAY(_check_Calced32);
  // Z7_memset_0_ARRAY(_hashKey32);
  CKeyBase::Wipe();
}

CDecoder::CDecoder(): CAesCbcDecoder(kAesKeySize) {}

static unsigned ReadVarInt(const Byte *p, unsigned maxSize, UInt64 *val)
{
  *val = 0;
  for (unsigned i = 0; i < maxSize && i < 10;)
  {
    const Byte b = p[i];
    *val |= (UInt64)(b & 0x7F) << (7 * i);
    i++;
    if ((b & 0x80) == 0)
      return i;
  }
  return 0;
}

HRESULT CDecoder::SetDecoderProps(const Byte *p, unsigned size, bool includeIV, bool isService)
{
  UInt64 Version;
  
  unsigned num = ReadVarInt(p, size, &Version);
  if (num == 0)
    return E_NOTIMPL;
  p += num;
  size -= num;

  if (Version != 0)
    return E_NOTIMPL;
  
  num = ReadVarInt(p, size, &Flags);
  if (num == 0)
    return E_NOTIMPL;
  p += num;
  size -= num;

  bool isCheck = IsThereCheck();
  if (size != 1 + kSaltSize + (includeIV ? AES_BLOCK_SIZE : 0) + (unsigned)(isCheck ? kCheckSize32 * 4 : 0))
    return E_NOTIMPL;

  if (_numIterationsLog != p[0])
  {
    _numIterationsLog = p[0];
    _needCalc = true;
  }

  p++;
    
  if (memcmp(_salt, p, kSaltSize) != 0)
  {
    memcpy(_salt, p, kSaltSize);
    _needCalc = true;
  }
  
  p += kSaltSize;
  
  if (includeIV)
  {
    memcpy(_iv, p, AES_BLOCK_SIZE);
    p += AES_BLOCK_SIZE;
  }
  
  _canCheck = true;
  
  if (isCheck)
  {
    memcpy(_check32, p, sizeof(_check32));
    MY_ALIGN_FOR_SHA256
    CSha256 sha;
    MY_ALIGN_FOR_SHA256
    Byte digest[SHA256_DIGEST_SIZE];
    Sha256_Init(&sha);
    Sha256_Update(&sha, (const Byte *)_check32, sizeof(_check32));
    Sha256_Final(&sha, digest);
    _canCheck = (memcmp(digest, p + sizeof(_check32), kPswCheckCsumSize32 * 4) == 0);
    if (_canCheck && isService)
    {
      // There was bug in RAR 5.21- : PswCheck field in service records ("QO") contained zeros.
      // so we disable password checking for such bad records.
      _canCheck = false;
      for (unsigned i = 0; i < kPswCheckSize32 * 4; i++)
        if (p[i] != 0)
        {
          _canCheck = true;
          break;
        }
    }
  }

  return (_numIterationsLog <= kNumIterationsLog_Max ? S_OK : E_NOTIMPL);
}


void CDecoder::SetPassword(const Byte *data, size_t size)
{
  if (size != _password.Size() || memcmp(data, _password, size) != 0)
  {
    _needCalc = true;
    _password.Wipe();
    _password.CopyFrom(data, size);
  }
}


Z7_COM7F_IMF(CDecoder::Init())
{
  CalcKey_and_CheckPassword();
  RINOK(SetKey((const Byte *)_key32, kAesKeySize))
  RINOK(SetInitVector(_iv, AES_BLOCK_SIZE))
  return CAesCoder::Init();
}


UInt32 CDecoder::Hmac_Convert_Crc32(UInt32 crc) const
{
  MY_ALIGN_FOR_SHA256
  NSha256::CHmac ctx;
  ctx.SetKey((const Byte *)_hashKey32, NSha256::kDigestSize);
  UInt32 v;
  SetUi32a(&v, crc)
  ctx.Update((const Byte *)&v, 4);
  MY_ALIGN_FOR_SHA256
  UInt32 h[SHA256_NUM_DIGEST_WORDS];
  ctx.Final((Byte *)h);
  crc = 0;
  for (unsigned i = 0; i < SHA256_NUM_DIGEST_WORDS; i++)
    crc ^= (UInt32)GetUi32a(h + i);
  return crc;
}


void CDecoder::Hmac_Convert_32Bytes(Byte *data) const
{
  MY_ALIGN_FOR_SHA256
  NSha256::CHmac ctx;
  ctx.SetKey((const Byte *)_hashKey32, NSha256::kDigestSize);
  ctx.Update(data, NSha256::kDigestSize);
  ctx.Final(data);
}


static CKey g_Key;

#ifndef Z7_ST
  static NWindows::NSynchronization::CCriticalSection g_GlobalKeyCacheCriticalSection;
  #define MT_LOCK NWindows::NSynchronization::CCriticalSectionLock lock(g_GlobalKeyCacheCriticalSection);
#else
  #define MT_LOCK
#endif

bool CDecoder::CalcKey_and_CheckPassword()
{
  if (_needCalc)
  {
    {
      MT_LOCK
      if (!g_Key._needCalc && IsKeyEqualTo(g_Key))
      {
        CopyCalcedKeysFrom(g_Key);
        _needCalc = false;
      }
    }
    
    if (_needCalc)
    {
      MY_ALIGN_FOR_SHA256
      UInt32 pswCheck[SHA256_NUM_DIGEST_WORDS];
      {
        // Pbkdf HMAC-SHA-256
        MY_ALIGN_FOR_SHA256
        NSha256::CHmac baseCtx;
        baseCtx.SetKey(_password, _password.Size());
        MY_ALIGN_FOR_SHA256
        NSha256::CHmac ctx;
        ctx = baseCtx;
        ctx.Update(_salt, sizeof(_salt));
        
        MY_ALIGN_FOR_SHA256
        UInt32 u[SHA256_NUM_DIGEST_WORDS];
        MY_ALIGN_FOR_SHA256
        UInt32 key[SHA256_NUM_DIGEST_WORDS];
        
        // u[0] = 0;
        // u[1] = 0;
        // u[2] = 0;
        // u[3] = 1;
        SetUi32a(u, 0x1000000)
        
        ctx.Update((const Byte *)(const void *)u, 4);
        ctx.Final((Byte *)(void *)u);
        
        memcpy(key, u, NSha256::kDigestSize);
        
        UInt32 numIterations = ((UInt32)1 << _numIterationsLog) - 1;
        
        for (unsigned i = 0; i < 3; i++)
        {
          for (; numIterations != 0; numIterations--)
          {
            ctx = baseCtx;
            ctx.Update((const Byte *)(const void *)u, NSha256::kDigestSize);
            ctx.Final((Byte *)(void *)u);
            for (unsigned s = 0; s < Z7_ARRAY_SIZE(u); s++)
              key[s] ^= u[s];
          }
          
          // RAR uses additional iterations for additional keys
          memcpy(i == 0 ? _key32 : i == 1 ? _hashKey32 : pswCheck,
              key, NSha256::kDigestSize);
          numIterations = 16;
        }
      }
     _check_Calced32[0] = pswCheck[0] ^ pswCheck[2] ^ pswCheck[4] ^ pswCheck[6];
     _check_Calced32[1] = pswCheck[1] ^ pswCheck[3] ^ pswCheck[5] ^ pswCheck[7];
      _needCalc = false;
      {
        MT_LOCK
        g_Key = *this;
      }
    }
  }
  
  if (IsThereCheck() && _canCheck)
    return memcmp(_check_Calced32, _check32, sizeof(_check32)) == 0;
  return true;
}

}}
