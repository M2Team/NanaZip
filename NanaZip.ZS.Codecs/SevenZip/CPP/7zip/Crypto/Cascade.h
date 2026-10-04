// Cascade.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_INC_CRYPTO_CASCADE_H
#define ZIP7_INC_CRYPTO_CASCADE_H

#include "../../Common/MyCom.h"
#include "../../Common/MyBuffer2.h"

#include "../ICoder.h"
#include "../IPassword.h"

#include "7zKeyDerivation.h"
#include "Aegis256.h"
#include "Ascon.h"

#define AES_BLOCK_SIZE 16
#define AES_NUM_IVMRK_WORDS ((1 + 1 + 15) * 4)

namespace NCrypto {
namespace NAXACascade {

using CKeyInfo = N7zKeyDerivation::CKeyInfo;
using CKeyInfoCache = N7zKeyDerivation::CKeyInfoCache;

using N7zKeyDerivation::kKeySize;
using N7zKeyDerivation::kCascadeKeySize;

const unsigned k_NumCyclesPower_Supported_MAX = 24;

static const unsigned kXcBlockSize = 64;

class CBase
{
  CKeyInfoCache _cachedKeys;
protected:
  CKeyInfo _key;
  Byte _nonce[NAscon::kNonceSize];
  Byte _keyAscon[NAscon::kKeySize_Ascon];
  bool _keyDerived;

  Byte _keyAes[32];
  Byte _aesIv[16];
  CAlignedBuffer1 _aesKeys;

  Byte _keyXChaCha20[32];
  Byte _xcNonce[24];
  Byte _xcDerivedKey[32];
  Byte _xcBlock[64];
  unsigned _xcBlockPos;
  UInt64 _xcCounter;

  UInt32 *AesKeys() { return (UInt32 *)(void *)(Byte *)_aesKeys; }

  void PrepareKey();
  void DeriveCascadeKeys();
  void XChaCha20XorData(Byte *data, UInt32 size);
  CBase();
  ~CBase()
  {
    Z7_memset_0_ARRAY(_nonce);
    Z7_memset_0_ARRAY(_keyAscon);
    Z7_memset_0_ARRAY(_keyAes);
    Z7_memset_0_ARRAY(_aesIv);
    memset(_aesKeys, 0, AES_NUM_IVMRK_WORDS * sizeof(UInt32) + AES_BLOCK_SIZE);
    Z7_memset_0_ARRAY(_keyXChaCha20);
    Z7_memset_0_ARRAY(_xcNonce);
    Z7_memset_0_ARRAY(_xcDerivedKey);
    Z7_memset_0_ARRAY(_xcBlock);
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
    Z7_memset_0_ARRAY(_aad);
    Z7_memset_0_ARRAY(_state);
  }

  Byte _aad[2 + 32 + NAscon::kNonceSize + 16 + 24];
  unsigned _aadSize;

  UInt64 _state[5];
  Byte _stateBuf[NAscon::kRateSize];
  unsigned _stateBufPos;
  bool _finalized;
  bool _authOk;

  void InitState();
  void ProcessAad(const Byte *aad, UInt64 aadLen);
  void ProcessEnc(Byte *data, UInt32 size);
  void ProcessDec(Byte *data, UInt32 size);
  void Finalize(Byte *tag);
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

  Byte _computedTag[NAscon::kTagSize];
  bool _tagReady;
  bool _propsWritten;
  Z7_COM7F_IMP2(UInt32, Filter(Byte *data, UInt32 size))
public:
  CEncoder();
};

#endif

class CDecoder Z7_final:
  public CBaseCoder,
  public ICompressSetDecoderProperties2,
  public ICryptoAuthVerify
{
  Z7_COM_UNKNOWN_IMP_4(
      ICompressFilter,
      ICryptoSetPassword,
      ICompressSetDecoderProperties2,
      ICryptoAuthVerify)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
  Z7_IFACE_COM7_IMP(ICryptoAuthVerify)

  Byte _expectedTag[NAscon::kTagSize];
  Z7_COM7F_IMP2(UInt32, Filter(Byte *data, UInt32 size))
public:
  CDecoder();
};

}}

#include "XChaCha20Poly1305.h"

namespace NCrypto {
namespace NAXPCascade {

using NAXACascade::CKeyInfo;
using NAXACascade::CKeyInfoCache;
using NAXACascade::k_NumCyclesPower_Supported_MAX;
using NAXACascade::kCascadeKeySize;
using NAXACascade::kXcBlockSize;

using NXChaCha20Poly1305::kTagSize;
using NXChaCha20Poly1305::kPolyKeySize;

class CAXPBase
{
  CKeyInfoCache _cachedKeys;
protected:
  CKeyInfo _key;
  bool _keyDerived;

  Byte _keyAes[32];
  Byte _aesIv[16];
  CAlignedBuffer1 _aesKeys;

  Byte _keyXChaCha20[32];
  Byte _xcNonce[24];
  Byte _xcDerivedKey[32];
  Byte _xcBlock[64];
  unsigned _xcBlockPos;
  UInt64 _xcCounter;

  Byte _polyKey[kPolyKeySize];
  NXChaCha20Poly1305::CPoly1305 _poly1305;
  Byte _aad[2 + 32 + 24 + 16];
  unsigned _aadSize;
  bool _finalized;
  bool _authOk;

  UInt32 *AesKeys() { return (UInt32 *)(void *)(Byte *)_aesKeys; }

  void PrepareKey();
  void DeriveAXPKeys();
  void ComputePolyKey();
  void XChaCha20XorData(Byte *data, UInt32 size);

  CAXPBase();
  ~CAXPBase()
  {
    Z7_memset_0_ARRAY(_keyAes);
    Z7_memset_0_ARRAY(_aesIv);
    memset(_aesKeys, 0, AES_NUM_IVMRK_WORDS * sizeof(UInt32) + AES_BLOCK_SIZE);
    Z7_memset_0_ARRAY(_keyXChaCha20);
    Z7_memset_0_ARRAY(_xcNonce);
    Z7_memset_0_ARRAY(_xcDerivedKey);
    Z7_memset_0_ARRAY(_xcBlock);
    Z7_memset_0_ARRAY(_polyKey);
    Z7_memset_0_ARRAY(_aad);
  }
};

class CAXPBaseCoder:
  public ICompressFilter,
  public ICryptoSetPassword,
  public CMyUnknownImp,
  public CAXPBase
{
  Z7_IFACE_COM7_IMP_NONFINAL(ICompressFilter)
  Z7_IFACE_COM7_IMP_NONFINAL(ICryptoSetPassword)
protected:
  virtual ~CAXPBaseCoder() {}
  void ProcessEnc(Byte *data, UInt32 size);
  void ProcessDec(Byte *data, UInt32 size);
};

#ifndef Z7_EXTRACT_ONLY

class CAXPEncoder Z7_final:
  public CAXPBaseCoder,
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

  Byte _computedTag[kTagSize];
  bool _tagReady;
  bool _propsWritten;
  Z7_COM7F_IMP2(UInt32, Filter(Byte *data, UInt32 size))
public:
  CAXPEncoder();
};

#endif

class CAXPDecoder Z7_final:
  public CAXPBaseCoder,
  public ICompressSetDecoderProperties2,
  public ICryptoAuthVerify
{
  Z7_COM_UNKNOWN_IMP_4(
      ICompressFilter,
      ICryptoSetPassword,
      ICompressSetDecoderProperties2,
      ICryptoAuthVerify)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
  Z7_IFACE_COM7_IMP(ICryptoAuthVerify)

  Byte _expectedTag[kTagSize];
  Z7_COM7F_IMP2(UInt32, Filter(Byte *data, UInt32 size))
public:
  CAXPDecoder();
};

}}

namespace NCrypto {
namespace NAXECascade {

using NAXACascade::CKeyInfo;
using NAXACascade::CKeyInfoCache;
using NAXACascade::k_NumCyclesPower_Supported_MAX;
using NAXACascade::kCascadeKeySize;
using NAXACascade::kXcBlockSize;

using NAegis256::kTagSize;

const unsigned kAesIvSize = 16;
const unsigned kXcNonceSize = 24;

class CBase
{
  CKeyInfoCache _cachedKeys;
protected:
  CKeyInfo _key;
  bool _keyDerived;

  Byte _keyAes[32];
  Byte _aesIv[kAesIvSize];
  CAlignedBuffer1 _aesKeys;

  Byte _keyXChaCha20[32];
  Byte _xcNonce[kXcNonceSize];
  Byte _xcDerivedKey[32];
  Byte _xcBlock[kXcBlockSize];
  unsigned _xcBlockPos;
  UInt64 _xcCounter;

  Byte _keyAegis[NAegis256::kKeySize];
  Byte _aegisNonce[NAegis256::kNonceSize];
  NAegis256::CCipher _cipher;
  Byte _aad[2 + 32 + kAesIvSize + kXcNonceSize + NAegis256::kNonceSize];
  unsigned _aadSize;

  UInt32 *AesKeys() { return (UInt32 *)(void *)(Byte *)_aesKeys; }

  void PrepareKey();
  void DeriveKeys();
  void XChaCha20XorData(Byte *data, UInt32 size);

  CBase();
  ~CBase()
  {
    Z7_memset_0_ARRAY(_keyAes);
    Z7_memset_0_ARRAY(_aesIv);
    memset(_aesKeys, 0, AES_NUM_IVMRK_WORDS * sizeof(UInt32) + AES_BLOCK_SIZE);
    Z7_memset_0_ARRAY(_keyXChaCha20);
    Z7_memset_0_ARRAY(_xcNonce);
    Z7_memset_0_ARRAY(_xcDerivedKey);
    Z7_memset_0_ARRAY(_xcBlock);
    Z7_memset_0_ARRAY(_keyAegis);
    Z7_memset_0_ARRAY(_aegisNonce);
    Z7_memset_0_ARRAY(_aad);
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
  virtual ~CBaseCoder() {}
  void ProcessEnc(Byte *data, UInt32 size);
  void ProcessDec(Byte *data, UInt32 size);
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

  Byte _computedTag[kTagSize];
  bool _tagReady;
  bool _propsWritten;
  Z7_COM7F_IMP2(UInt32, Filter(Byte *data, UInt32 size))
public:
  CEncoder();
};

#endif

class CDecoder Z7_final:
  public CBaseCoder,
  public ICompressSetDecoderProperties2,
  public ICryptoAuthVerify
{
  Z7_COM_UNKNOWN_IMP_4(
      ICompressFilter,
      ICryptoSetPassword,
      ICompressSetDecoderProperties2,
      ICryptoAuthVerify)
  Z7_IFACE_COM7_IMP(ICompressSetDecoderProperties2)
  Z7_IFACE_COM7_IMP(ICryptoAuthVerify)

  Byte _expectedTag[kTagSize];
  bool _authChecked;
  Int32 _authResult;
  Z7_COM7F_IMP2(UInt32, Filter(Byte *data, UInt32 size))
public:
  CDecoder();
};

}}

#endif
