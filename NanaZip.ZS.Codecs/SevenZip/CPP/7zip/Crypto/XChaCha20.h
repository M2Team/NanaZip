// XChaCha20.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_INC_CRYPTO_XCHACHA20_H
#define ZIP7_INC_CRYPTO_XCHACHA20_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"
#include "../IPassword.h"

#include "7zKeyDerivation.h"

namespace NCrypto {
namespace NXChaCha20 {

using CKeyInfo = N7zKeyDerivation::CKeyInfo;
using CKeyInfoCache = N7zKeyDerivation::CKeyInfoCache;

using N7zKeyDerivation::kKeySize;

const unsigned kNonceSize = 24;
const unsigned k_NumCyclesPower_Supported_MAX = 24;
const unsigned kBlockBytes = 64;

void XChaCha20Block_Core(Byte *output, const Byte *key, const Byte *nonce, UInt64 counter);
void XHChaCha20Block_Core(Byte *output, const Byte *key, const Byte *nonce);
void XChaCha20ProcessData(Byte *data, UInt32 size, const Byte *derivedKey, const Byte *nonce, UInt64 &counter, Byte *block, unsigned &blockPos);

class CBase
{
  CKeyInfoCache _cachedKeys;
protected:
  CKeyInfo _key;
  Byte _nonce[kNonceSize];
  UInt64 _counter;
  
  void PrepareKey();
  CBase();
  ~CBase()
  {
    Z7_memset_0_ARRAY(_nonce);
  }
};

class CBaseCoder:
  public ICompressFilter,
  public ICryptoSetPassword,
  public CMyUnknownImp,
  public CBase
{
  Z7_IFACE_COM7_IMP_NONFINAL(ICompressFilter)
  Z7_IFACE_COM7_IMP_NONFINAL(ICryptoSetPassword)
protected:
  virtual ~CBaseCoder()
  {
    Z7_memset_0_ARRAY(_block);
    Z7_memset_0_ARRAY(_derivedKey);
  }
  
  static const unsigned kBlockSize = 64;
  Byte _block[kBlockSize];
  unsigned _blockPos;
  Byte _derivedKey[kKeySize];
  bool _derivedKeyValid;
  
  void ProcessData(Byte *data, UInt32 size);
  virtual void DeriveKey();
};

#ifndef Z7_EXTRACT_ONLY

class CEncoder Z7_final:
  public CBaseCoder,
  public ICompressWriteCoderProperties,
  public ICryptoResetInitVector
{
  Z7_COM_UNKNOWN_IMP_4(
      ICompressFilter,
      ICryptoSetPassword,
      ICompressWriteCoderProperties,
      ICryptoResetInitVector)
  Z7_IFACE_COM7_IMP(ICompressWriteCoderProperties)
  Z7_IFACE_COM7_IMP(ICryptoResetInitVector)
public:
  CEncoder();
};

#endif

class CDecoder Z7_final:
  public CBaseCoder,
  public ICompressSetDecoderProperties2
{
  Z7_COM_UNKNOWN_IMP_3(
      ICompressFilter,
      ICryptoSetPassword,
      ICompressSetDecoderProperties2)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
public:
  CDecoder();
};

}}

#endif
