// 7zKeyDerivation.h
// Key derivation common module for 7z format

#ifndef ZIP7_INC_CRYPTO_7Z_KEY_DERIVATION_H
#define ZIP7_INC_CRYPTO_7Z_KEY_DERIVATION_H

#include "../../Common/MyBuffer.h"
#include "../../Common/MyVector.h"

namespace NCrypto {
namespace N7zKeyDerivation {

const unsigned kKeySize = 32;
const unsigned kSaltSizeMax = 32;

// for cascade mode
const unsigned kCascadeKeySize = 96;

// random salt for cascade mode
const unsigned kCascadeSaltSize = 32;

enum EDerivationMode
{
  kDeriv_Single = 0,    // SHA-256 iterative
  kDeriv_Cascade = 1    // PBKDF2-HMAC-SHA512
};

class CKeyInfo
{
public:
  unsigned NumCyclesPower;
  unsigned SaltSize;
  Byte Salt[kSaltSizeMax];
  CByteBuffer Password;
  Byte Key[kKeySize];
  EDerivationMode DerivMode;
  Byte CascadeKey[kCascadeKeySize];

  bool IsEqualTo(const CKeyInfo &a) const;
  void CalcKey();

  CKeyInfo() { ClearProps(); }
  void ClearProps()
  {
    NumCyclesPower = 0;
    SaltSize = 0;
    DerivMode = kDeriv_Single;
    for (unsigned i = 0; i < sizeof(Salt); i++)
      Salt[i] = 0;
  }

  void Wipe()
  {
    Password.Wipe();
    NumCyclesPower = 0;
    SaltSize = 0;
    Z7_memset_0_ARRAY(Salt);
    Z7_memset_0_ARRAY(Key);
    Z7_memset_0_ARRAY(CascadeKey);
  }

#ifdef Z7_CPP_IS_SUPPORTED_default
  CKeyInfo(const CKeyInfo &) = default;
#endif
  ~CKeyInfo() { Wipe(); }
};

class CKeyInfoCache
{
  unsigned Size;
  CObjectVector<CKeyInfo> Keys;
public:
  CKeyInfoCache(unsigned size): Size(size) {}
  bool GetKey(CKeyInfo &key);
  void Add(const CKeyInfo &key);
  void FindAndAdd(const CKeyInfo &key);
};

}}

#endif
