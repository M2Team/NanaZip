// XChaCha20Poly1305.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"

#ifndef Z7_ST
#include "../../Windows/Synchronization.h"
#endif

#include "../Common/StreamUtils.h"

#include "XChaCha20Poly1305.h"

#ifndef Z7_EXTRACT_ONLY
#include "RandGen.h"
#endif

#if defined(MY_CPU_AMD64)
  #ifdef _MSC_VER
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
  #define Z7_POLY1305_128BIT
#endif

namespace NCrypto {
namespace NXChaCha20Poly1305 {

void CBaseCoder::ComputePolyKey()
{
  Byte polyBlock[64];
  NXChaCha20::XChaCha20Block_Core(polyBlock, _derivedKey, _nonce + 16, 0);
  memcpy(_polyKey, polyBlock, kPolyKeySize);
  Z7_memset_0_ARRAY(polyBlock);
}

CPoly1305::CPoly1305()
{
  Reset();
}

void CPoly1305::Reset()
{
  memset(_r, 0, sizeof(_r));
  memset(_s, 0, sizeof(_s));
  memset(_h, 0, sizeof(_h));
  memset(_n, 0, sizeof(_n));
  memset(_block, 0, sizeof(_block));
  _blockPos = 0;
  _totalLen = 0;
  memset(_aadBlock, 0, sizeof(_aadBlock));
  _aadBlockPos = 0;
  _aadLen = 0;
  _finalized = false;
}

void CPoly1305::SetKey(const Byte *key)
{
  Reset();

  _r[0] = GetUi32(key) & 0x0fffffff;
  _r[1] = GetUi32(key + 4) & 0x0ffffffc;
  _r[2] = GetUi32(key + 8) & 0x0ffffffc;
  _r[3] = GetUi32(key + 12) & 0x0ffffffc;

  _s[1] = _r[1] + (_r[1] >> 2);
  _s[2] = _r[2] + (_r[2] >> 2);
  _s[3] = _r[3] + (_r[3] >> 2);

  _n[0] = GetUi32(key + 16);
  _n[1] = GetUi32(key + 20);
  _n[2] = GetUi32(key + 24);
  _n[3] = GetUi32(key + 28);
}

static inline UInt32 CONSTANT_TIME_CARRY(UInt32 a, UInt32 b)
{
  return ((a ^ ((a ^ b) | ((a - b) ^ b))) >> 31);
}

static void Poly1305_ProcessBlock(UInt32 h[5], const UInt32 r[4], const UInt32 s[4], const Byte block[16], bool hasHighBit)
{
  UInt32 r0 = r[0], r1 = r[1], r2 = r[2], r3 = r[3];
  UInt32 s1 = s[1], s2 = s[2], s3 = s[3];
  UInt32 h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4];
  UInt64 d0, d1, d2, d3;
  UInt32 c;

  h0 = (UInt32)(d0 = (UInt64)h0 + GetUi32(block));
  h1 = (UInt32)(d1 = (UInt64)h1 + (d0 >> 32) + GetUi32(block + 4));
  h2 = (UInt32)(d2 = (UInt64)h2 + (d1 >> 32) + GetUi32(block + 8));
  h3 = (UInt32)(d3 = (UInt64)h3 + (d2 >> 32) + GetUi32(block + 12));
  h4 += (UInt32)(d3 >> 32) + (hasHighBit ? 1 : 0);

  d0 = ((UInt64)h0 * r0) +
       ((UInt64)h1 * s3) +
       ((UInt64)h2 * s2) +
       ((UInt64)h3 * s1);
  d1 = ((UInt64)h0 * r1) +
       ((UInt64)h1 * r0) +
       ((UInt64)h2 * s3) +
       ((UInt64)h3 * s2) +
       ((UInt64)h4 * s1);
  d2 = ((UInt64)h0 * r2) +
       ((UInt64)h1 * r1) +
       ((UInt64)h2 * r0) +
       ((UInt64)h3 * s3) +
       ((UInt64)h4 * s2);
  d3 = ((UInt64)h0 * r3) +
       ((UInt64)h1 * r2) +
       ((UInt64)h2 * r1) +
       ((UInt64)h3 * r0) +
       ((UInt64)h4 * s3);
  h4 = (UInt32)(h4 * r0);

  h0 = (UInt32)d0;
  h1 = (UInt32)(d1 += d0 >> 32);
  h2 = (UInt32)(d2 += d1 >> 32);
  h3 = (UInt32)(d3 += d2 >> 32);
  h4 += (UInt32)(d3 >> 32);

  c = (h4 >> 2) + (h4 & ~3U);
  h4 &= 3;
  h0 += c;
  h1 += (c = CONSTANT_TIME_CARRY(h0, c));
  h2 += (c = CONSTANT_TIME_CARRY(h1, c));
  h3 += (c = CONSTANT_TIME_CARRY(h2, c));
  h4 += CONSTANT_TIME_CARRY(h3, c);

  h[0] = h0; h[1] = h1; h[2] = h2;
  h[3] = h3; h[4] = h4;
}

void CPoly1305::ProcessBlocks(Byte *buf, unsigned &bufPos, UInt64 &len, const Byte *data, UInt32 size)
{
  len += size;

  if (bufPos > 0)
  {
    unsigned n = 16 - bufPos;
    if (n > size) n = size;
    memcpy(buf + bufPos, data, n);
    bufPos += n;
    data += n;
    size -= n;
    if (bufPos == 16)
    {
      Poly1305_ProcessBlock(_h, _r, _s, buf, true);
      bufPos = 0;
    }
  }

  while (size >= 16)
  {
    Poly1305_ProcessBlock(_h, _r, _s, data, true);
    data += 16;
    size -= 16;
  }

  if (size > 0)
  {
    memcpy(buf, data, size);
    bufPos = size;
  }
}

void CPoly1305::Update(const Byte *data, UInt32 size)
{
  if (_finalized) return;
  ProcessBlocks(_block, _blockPos, _totalLen, data, size);
}

void CPoly1305::UpdateAad(const Byte *data, UInt32 size)
{
  if (_finalized) return;
  ProcessBlocks(_aadBlock, _aadBlockPos, _aadLen, data, size);
}

void CPoly1305::PadAndProcessBlock(Byte *buf, unsigned bufPos)
{
  if (bufPos != 0)
  {
    memset(buf + bufPos, 0, 16 - bufPos);
    buf[bufPos] = 1;
    Poly1305_ProcessBlock(_h, _r, _s, buf, false);
  }
}

void CPoly1305::Final(Byte *tag)
{
  if (_finalized)
    return;
  _finalized = true;

  PadAndProcessBlock(_aadBlock, _aadBlockPos);
  PadAndProcessBlock(_block, _blockPos);

  {
    Byte lenBlock[16];
    for (unsigned i = 0; i < 8; i++)
      lenBlock[i] = (Byte)(_aadLen >> (i * 8));
    for (unsigned i = 0; i < 8; i++)
      lenBlock[8 + i] = (Byte)(_totalLen >> (i * 8));
    Poly1305_ProcessBlock(_h, _r, _s, lenBlock, true);
  }

  UInt32 h0 = _h[0], h1 = _h[1], h2 = _h[2], h3 = _h[3], h4 = _h[4];
  UInt32 g0, g1, g2, g3, g4;
  UInt32 mask;
  UInt64 t;

  g0 = (UInt32)(t = (UInt64)h0 + 5);
  g1 = (UInt32)(t = (UInt64)h1 + (t >> 32));
  g2 = (UInt32)(t = (UInt64)h2 + (t >> 32));
  g3 = (UInt32)(t = (UInt64)h3 + (t >> 32));
  g4 = h4 + (UInt32)(t >> 32);

  mask = 0 - (g4 >> 2);
  g0 &= mask; g1 &= mask;
  g2 &= mask; g3 &= mask;
  mask = ~mask;
  h0 = (h0 & mask) | g0;
  h1 = (h1 & mask) | g1;
  h2 = (h2 & mask) | g2;
  h3 = (h3 & mask) | g3;

  h0 = (UInt32)(t = (UInt64)h0 + _n[0]);
  h1 = (UInt32)(t = (UInt64)h1 + (t >> 32) + _n[1]);
  h2 = (UInt32)(t = (UInt64)h2 + (t >> 32) + _n[2]);
  h3 = (UInt32)(t = (UInt64)h3 + (t >> 32) + _n[3]);

  SetUi32(tag, h0);
  SetUi32(tag + 4, h1);
  SetUi32(tag + 8, h2);
  SetUi32(tag + 12, h3);
}

void CBaseCoder::DeriveKey()
{
  NXChaCha20::XHChaCha20Block_Core(_derivedKey, _key.Key, _nonce);
  ComputePolyKey();
  _poly1305.SetKey(_polyKey);
  if (_aadSize > 0)
  {
    _poly1305.UpdateAad(_aad, _aadSize);
  }
  _derivedKeyValid = true;
}

Z7_COM7F_IMF(CBaseCoder::Init())
{
  COM_TRY_BEGIN

  PrepareKey();
  _counter = 1;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  _poly1305.Reset();

  return S_OK;

  COM_TRY_END
}

#ifndef Z7_EXTRACT_ONLY

Z7_COM7F_IMF(CEncoder::ResetInitVector())
{
  for (unsigned i = 0; i < sizeof(_nonce); i++)
    _nonce[i] = 0;
  MY_RAND_GEN(_nonce, kNonceSize);
  _counter = 1;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  _poly1305.Reset();

  _aadSize = 1;
  const unsigned nonceSizeMinus1 = kNonceSize - 1;
  const unsigned nonceHigh = (nonceSizeMinus1 >= 16) ? (1 << 6) : 0;
  const unsigned nonceLow = nonceSizeMinus1 & 0x0F;
  _aad[0] = (Byte)(_key.NumCyclesPower
      | (_key.SaltSize == 0 ? 0 : (1 << 7))
      | nonceHigh);
  if (_key.SaltSize != 0)
  {
    _aad[1] = (Byte)(((_key.SaltSize - 1) << 4) | nonceLow);
    memcpy(_aad + 2, _key.Salt, _key.SaltSize);
    _aadSize = 2 + _key.SaltSize;
    memcpy(_aad + _aadSize, _nonce, kNonceSize);
    _aadSize += kNonceSize;
  }
  else
  {
    _aad[1] = (Byte)(nonceLow);
    _aadSize = 2;
    memcpy(_aad + _aadSize, _nonce, kNonceSize);
    _aadSize += kNonceSize;
  }

  _tagReady = false;
  _propsWritten = false;
  memset(_computedTag, 0, kTagSize);
  return S_OK;
}

Z7_COM7F_IMF2(UInt32, CEncoder::Filter(Byte *data, UInt32 size))
{
  ProcessData(data, size);
  _poly1305.Update(data, size);
  return size;
}

Z7_COM7F_IMF(CEncoder::WriteCoderProperties(ISequentialOutStream *outStream))
{
  Byte props[2 + sizeof(_key.Salt) + kNonceSize + kTagSize];
  unsigned propsSize = 1;

  const unsigned nonceSizeMinus1 = kNonceSize - 1;
  const unsigned nonceHigh = (nonceSizeMinus1 >= 16) ? (1 << 6) : 0;
  const unsigned nonceLow = nonceSizeMinus1 & 0x0F;

  props[0] = (Byte)(_key.NumCyclesPower
      | (_key.SaltSize == 0 ? 0 : (1 << 7))
      | nonceHigh);

  if (_key.SaltSize != 0)
  {
    props[1] = (Byte)(
        ((_key.SaltSize - 1) << 4)
        | nonceLow);
    memcpy(props + 2, _key.Salt, _key.SaltSize);
    propsSize = 2 + _key.SaltSize;
    memcpy(props + propsSize, _nonce, kNonceSize);
    propsSize += kNonceSize;
  }
  else
  {
    props[1] = (Byte)(nonceLow);
    propsSize = 2;
    memcpy(props + propsSize, _nonce, kNonceSize);
    propsSize += kNonceSize;
  }

  if (!_tagReady)
  {
    if (!_derivedKeyValid && _propsWritten)
      DeriveKey();
    if (_derivedKeyValid)
    {
      _poly1305.Final(_computedTag);
      _tagReady = true;
    }
    else
    {
      memset(_computedTag, 0, kTagSize);
    }
  }
  _propsWritten = true;

  memcpy(props + propsSize, _computedTag, kTagSize);
  propsSize += kTagSize;

  return WriteStream(outStream, props, propsSize);
}

CEncoder::CEncoder()
{
  _key.NumCyclesPower = 19;
  _counter = 1;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  _aadSize = 0;
  _tagReady = false;
  _propsWritten = false;
  memset(_computedTag, 0, kTagSize);
}

#endif

CDecoder::CDecoder()
{
  _counter = 1;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  _aadSize = 0;
  _authChecked = false;
  _authResult = 0;
  memset(_expectedTag, 0, kTagSize);
}

Z7_COM7F_IMF2(UInt32, CDecoder::Filter(Byte *data, UInt32 size))
{
  if (!_derivedKeyValid)
    DeriveKey();
  _poly1305.Update(data, size);
  ProcessData(data, size);
  return size;
}

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size))
{
  _key.ClearProps();

  _counter = 1;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  _poly1305.Reset();
  _authChecked = false;
  _authResult = 0;
  memset(_expectedTag, 0, kTagSize);

  for (unsigned i = 0; i < sizeof(_nonce); i++)
    _nonce[i] = 0;

  if (size == 0)
    return S_OK;

  const unsigned b0 = data[0];
  _key.NumCyclesPower = b0 & 0x3F;
  if ((b0 & 0xC0) == 0)
    return size == 1 ? S_OK : E_INVALIDARG;
  if (size <= 1)
    return E_INVALIDARG;

  const unsigned b1 = data[1];
  const unsigned saltSize = ((b0 >> 7) & 1) + (b1 >> 4);
  const unsigned nonceSizeMinus1 = ((b0 >> 6) & 1) * 16 + (b1 & 0x0F);
  const unsigned nonceSize = nonceSizeMinus1 + 1;

  if (nonceSize > kNonceSize)
    return E_INVALIDARG;

  const unsigned totalSize = 2 + saltSize + nonceSize + kTagSize;

  if (size != totalSize)
  {
    return E_INVALIDARG;
  }

  _aadSize = totalSize - kTagSize;
  memcpy(_aad, data, _aadSize);

  _key.SaltSize = saltSize;
  data += 2;
  for (unsigned i = 0; i < saltSize; i++)
    _key.Salt[i] = *data++;
  for (unsigned i = 0; i < nonceSize && i < kNonceSize; i++)
    _nonce[i] = *data++;

  memcpy(_expectedTag, data, kTagSize);

  return (_key.NumCyclesPower <= k_NumCyclesPower_Supported_MAX
      || _key.NumCyclesPower == 0x3F) ? S_OK : E_NOTIMPL;
}

Z7_COM7F_IMF(CDecoder::CryptoAuthVerify(Int32 *result))
{
  if (_authChecked)
  {
    *result = _authResult;
    return S_OK;
  }
  _authChecked = true;
  if (!_derivedKeyValid)
    DeriveKey();

  Byte computedTag[kTagSize];
  _poly1305.Final(computedTag);

  {
    volatile Byte diff = 0;
    for (unsigned i = 0; i < kTagSize; i++)
      diff |= computedTag[i] ^ _expectedTag[i];
    _authResult = (diff == 0) ? 0 : 1;
  }
  *result = _authResult;

  Z7_memset_0_ARRAY(computedTag);
  return S_OK;
}

}}