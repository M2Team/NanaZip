// XChaCha20Poly1305.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_INC_CRYPTO_XCHACHA20_POLY1305_H
#define ZIP7_INC_CRYPTO_XCHACHA20_POLY1305_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"
#include "../IPassword.h"

#include "XChaCha20.h"

namespace NCrypto {
namespace NXChaCha20Poly1305 {

using NXChaCha20::kNonceSize;
using NXChaCha20::k_NumCyclesPower_Supported_MAX;

const unsigned kTagSize = 16;
const unsigned kPolyKeySize = 32;

class CPoly1305
{
  UInt32 _r[4];
  UInt32 _s[4];
  UInt32 _h[5];
  UInt32 _n[4];
  Byte _block[16];
  unsigned _blockPos;
  UInt64 _totalLen;
  bool _finalized;
  Byte _aadBlock[16];
  unsigned _aadBlockPos;
  UInt64 _aadLen;

  void PadAndProcessBlock(Byte *buf, unsigned bufPos);
  void ProcessBlocks(Byte *buf, unsigned &bufPos, UInt64 &len, const Byte *data, UInt32 size);
public:
  CPoly1305();
  void SetKey(const Byte *key);
  void Update(const Byte *data, UInt32 size);
  void UpdateAad(const Byte *data, UInt32 size);
  void Final(Byte *tag);
  void Reset();
};

class CBaseCoder:
  public NXChaCha20::CBaseCoder
{
  Z7_COM7F_IMP(Init())
protected:
  virtual ~CBaseCoder()
  {
    Z7_memset_0_ARRAY(_polyKey);
    Z7_memset_0_ARRAY(_aad);
  }

  Byte _polyKey[kPolyKeySize];
  CPoly1305 _poly1305;
  Byte _aad[2 + 16 + kNonceSize];
  unsigned _aadSize;

  void DeriveKey() override;
  void ComputePolyKey();
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