// Crypto/Rar5Aes.h

#ifndef ZIP7_INC_CRYPTO_RAR5_AES_H
#define ZIP7_INC_CRYPTO_RAR5_AES_H

#include "../../../C/Sha256.h"

#include "../../Common/MyBuffer.h"

#include "MyAes.h"

namespace NCrypto {
namespace NRar5 {

const unsigned kSaltSize = 16;
const unsigned kPswCheckSize = 8;
const unsigned kAesKeySize = 32;

namespace NCryptoFlags
{
  const unsigned kPswCheck = 1 << 0;
  const unsigned kUseMAC   = 1 << 1;
}

struct CKey
{
  bool _needCalc;

  unsigned _numIterationsLog;
  Byte _salt[kSaltSize];
  CByteBuffer _password;
  
  Byte _key[kAesKeySize];
  Byte _check_Calced[kPswCheckSize];
  Byte _hashKey[SHA256_DIGEST_SIZE];

  void CopyCalcedKeysFrom(const CKey &k)
  {
    memcpy(_key, k._key, sizeof(_key));
    memcpy(_check_Calced, k._check_Calced, sizeof(_check_Calced));
    memcpy(_hashKey, k._hashKey, sizeof(_hashKey));
  }

  bool IsKeyEqualTo(const CKey &key)
  {
    return (_numIterationsLog == key._numIterationsLog
        && memcmp(_salt, key._salt, sizeof(_salt)) == 0
        && _password == key._password);
  }
  
  CKey();

  void Wipe()
  {
    _password.Wipe();
    Z7_memset_0_ARRAY(_salt);
    Z7_memset_0_ARRAY(_key);
    Z7_memset_0_ARRAY(_check_Calced);
    Z7_memset_0_ARRAY(_hashKey);
  }

#ifdef Z7_CPP_IS_SUPPORTED_default
  // CKey(const CKey &) = default;
  CKey& operator =(const CKey &) = default;
#endif
  ~CKey() { Wipe(); }
};


class CDecoder Z7_final:
  public CAesCbcDecoder,
  public CKey
{
  Byte _check[kPswCheckSize];
  bool _canCheck;
  UInt64 Flags;

  bool IsThereCheck() const { return ((Flags & NCryptoFlags::kPswCheck) != 0); }
public:
  Byte _iv[AES_BLOCK_SIZE];
  
  CDecoder();

  Z7_COM7F_IMP(Init())

  void SetPassword(const Byte *data, size_t size);
  HRESULT SetDecoderProps(const Byte *data, unsigned size, bool includeIV, bool isService);

  bool CalcKey_and_CheckPassword();

  bool UseMAC() const { return (Flags & NCryptoFlags::kUseMAC) != 0; }
  UInt32 Hmac_Convert_Crc32(UInt32 crc) const;
  void Hmac_Convert_32Bytes(Byte *data) const;
};

}}

#endif
