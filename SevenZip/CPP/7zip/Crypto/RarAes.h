// Crypto/RarAes.h

#ifndef __CRYPTO_RAR_AES_H
#define __CRYPTO_RAR_AES_H

#include "../../../../ThirdParty/LZMA/C/Aes.h"

#include "../../../../ThirdParty/LZMA/CPP/Common/MyBuffer.h"

#include "../../../../ThirdParty/LZMA/CPP/7zip/IPassword.h"

#include "../../../../ThirdParty/LZMA/CPP/7zip/Crypto/MyAes.h"

namespace NCrypto {
namespace NRar3 {

const unsigned kAesKeySize = 16;

class CDecoder:
  public CAesCbcDecoder
  // public ICompressSetDecoderProperties2,
  // public ICryptoSetPassword
{
  Byte _salt[8];
  bool _thereIsSalt;
  bool _needCalc;
  // bool _rar350Mode;

  CByteBuffer _password;

  Byte _key[kAesKeySize];
  Byte _iv[AES_BLOCK_SIZE];

  void CalcKey();
public:
  /*
  MY_UNKNOWN_IMP1(
    ICryptoSetPassword
    // ICompressSetDecoderProperties2
  */
  STDMETHOD(Init)();

  void SetPassword(const Byte *data, unsigned size);
  HRESULT SetDecoderProperties2(const Byte *data, UInt32 size);

  CDecoder();

  ~CDecoder() { Wipe(); }
  void Wipe()
  {
    _password.Wipe();
    MY_memset_0_ARRAY(_salt);
    MY_memset_0_ARRAY(_key);
    MY_memset_0_ARRAY(_iv);
  }
  // void SetRar350Mode(bool rar350Mode) { _rar350Mode = rar350Mode; }
};

}}

#endif
