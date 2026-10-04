// XChaCha20.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"

#ifndef Z7_ST
#include "../../Windows/Synchronization.h"
#endif

#include "../Common/StreamUtils.h"

#include "XChaCha20.h"

#ifndef Z7_EXTRACT_ONLY
#include "RandGen.h"
#endif

#include "ChaCha20Simd.h"

namespace NCrypto {
namespace NXChaCha20 {

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

#define QUARTERROUND(a, b, c, d) \
  a += b; d ^= a; d = ROTL32(d, 16); \
  c += d; b ^= c; b = ROTL32(b, 12); \
  a += b; d ^= a; d = ROTL32(d, 8); \
  c += d; b ^= c; b = ROTL32(b, 7);

#define CHACHA20_10_DOUBLE_ROUNDS \
  DOUBLE_ROUND; DOUBLE_ROUND; \
  DOUBLE_ROUND; DOUBLE_ROUND; \
  DOUBLE_ROUND; DOUBLE_ROUND; \
  DOUBLE_ROUND; DOUBLE_ROUND; \
  DOUBLE_ROUND; DOUBLE_ROUND;

static CKeyInfoCache g_GlobalKeyCache(32);

#ifndef Z7_ST
  static NWindows::NSynchronization::CCriticalSection g_GlobalKeyCacheCriticalSection;
  #define MT_LOCK NWindows::NSynchronization::CCriticalSectionLock lock(g_GlobalKeyCacheCriticalSection);
#else
  #define MT_LOCK
#endif

CBase::CBase():
  _cachedKeys(16),
  _counter(0)
{
  for (unsigned i = 0; i < sizeof(_nonce); i++)
    _nonce[i] = 0;
}

void CBaseCoder::DeriveKey()
{
  XHChaCha20Block_Core(_derivedKey, _key.Key, _nonce);
  _derivedKeyValid = true;
}

void CBase::PrepareKey()
{
  MT_LOCK
  
  bool finded = false;
  if (!_cachedKeys.GetKey(_key))
  {
    finded = g_GlobalKeyCache.GetKey(_key);
    if (!finded)
      _key.CalcKey();
    _cachedKeys.Add(_key);
  }
  if (!finded)
    g_GlobalKeyCache.FindAndAdd(_key);
}

#define DOUBLE_ROUND \
  QUARTERROUND(x0, x4, x8,  x12) \
  QUARTERROUND(x1, x5, x9,  x13) \
  QUARTERROUND(x2, x6, x10, x14) \
  QUARTERROUND(x3, x7, x11, x15) \
  QUARTERROUND(x0, x5, x10, x15) \
  QUARTERROUND(x1, x6, x11, x12) \
  QUARTERROUND(x2, x7, x8,  x13) \
  QUARTERROUND(x3, x4, x9,  x14)

void XHChaCha20Block_Core(Byte *output, const Byte *key, const Byte *nonce)
{
  UInt32 x0, x1, x2, x3, x4, x5, x6, x7;
  UInt32 x8, x9, x10, x11, x12, x13, x14, x15;
  
  x0 = GetUi32(kSigma);
  x1 = GetUi32(kSigma + 4);
  x2 = GetUi32(kSigma + 8);
  x3 = GetUi32(kSigma + 12);
  
  x4 = GetUi32(key);
  x5 = GetUi32(key + 4);
  x6 = GetUi32(key + 8);
  x7 = GetUi32(key + 12);
  x8 = GetUi32(key + 16);
  x9 = GetUi32(key + 20);
  x10 = GetUi32(key + 24);
  x11 = GetUi32(key + 28);
  
  x12 = GetUi32(nonce);
  x13 = GetUi32(nonce + 4);
  x14 = GetUi32(nonce + 8);
  x15 = GetUi32(nonce + 12);
  
  CHACHA20_10_DOUBLE_ROUNDS

  SetUi32(output, x0);
  SetUi32(output + 4, x1);
  SetUi32(output + 8, x2);
  SetUi32(output + 12, x3);
  SetUi32(output + 16, x12);
  SetUi32(output + 20, x13);
  SetUi32(output + 24, x14);
  SetUi32(output + 28, x15);
}

void XChaCha20Block_Core(Byte *output, const Byte *key, const Byte *nonce, UInt64 counter)
{
  UInt32 x0, x1, x2, x3, x4, x5, x6, x7;
  UInt32 x8, x9, x10, x11, x12, x13, x14, x15;
  
  x0 = GetUi32(kSigma);
  x1 = GetUi32(kSigma + 4);
  x2 = GetUi32(kSigma + 8);
  x3 = GetUi32(kSigma + 12);
  
  x4 = GetUi32(key);
  x5 = GetUi32(key + 4);
  x6 = GetUi32(key + 8);
  x7 = GetUi32(key + 12);
  x8 = GetUi32(key + 16);
  x9 = GetUi32(key + 20);
  x10 = GetUi32(key + 24);
  x11 = GetUi32(key + 28);
  
  x12 = (UInt32)(counter & 0xFFFFFFFF);
  x13 = (UInt32)(counter >> 32);
  x14 = GetUi32(nonce);
  x15 = GetUi32(nonce + 4);
  
  CHACHA20_10_DOUBLE_ROUNDS

  x0 += GetUi32(kSigma);
  x1 += GetUi32(kSigma + 4);
  x2 += GetUi32(kSigma + 8);
  x3 += GetUi32(kSigma + 12);
  x4 += GetUi32(key);
  x5 += GetUi32(key + 4);
  x6 += GetUi32(key + 8);
  x7 += GetUi32(key + 12);
  x8 += GetUi32(key + 16);
  x9 += GetUi32(key + 20);
  x10 += GetUi32(key + 24);
  x11 += GetUi32(key + 28);
  x12 += (UInt32)(counter & 0xFFFFFFFF);
  x13 += (UInt32)(counter >> 32);
  x14 += GetUi32(nonce);
  x15 += GetUi32(nonce + 4);
  
  SetUi32(output, x0);
  SetUi32(output + 4, x1);
  SetUi32(output + 8, x2);
  SetUi32(output + 12, x3);
  SetUi32(output + 16, x4);
  SetUi32(output + 20, x5);
  SetUi32(output + 24, x6);
  SetUi32(output + 28, x7);
  SetUi32(output + 32, x8);
  SetUi32(output + 36, x9);
  SetUi32(output + 40, x10);
  SetUi32(output + 44, x11);
  SetUi32(output + 48, x12);
  SetUi32(output + 52, x13);
  SetUi32(output + 56, x14);
  SetUi32(output + 60, x15);
}

#undef DOUBLE_ROUND

void XChaCha20ProcessData(Byte *data, UInt32 size, const Byte *derivedKey, const Byte *nonce, UInt64 &counter, Byte *block, unsigned &blockPos)
{
  if (blockPos > 0 && blockPos < kBlockBytes)
  {
    UInt32 remaining = kBlockBytes - blockPos;
    UInt32 toProcess = (size < remaining) ? size : remaining;
    Byte *dataPtr = data;
    const Byte *blockPtr = block + blockPos;
    UInt32 count = toProcess;
#ifdef MY_CPU_LE_UNALIGN_64
    while (count >= 8)
    {
      *(UInt64 *)dataPtr ^= *(const UInt64 *)blockPtr;
      dataPtr += 8;
      blockPtr += 8;
      count -= 8;
    }
#endif
#ifdef MY_CPU_LE_UNALIGN
    while (count >= 4)
    {
      *(UInt32 *)dataPtr ^= *(const UInt32 *)blockPtr;
      dataPtr += 4;
      blockPtr += 4;
      count -= 4;
    }
#endif
    while (count--)
      *dataPtr++ ^= *blockPtr++;
    data += toProcess;
    size -= toProcess;
    blockPos += toProcess;
  }

#ifdef MY_CPU_X86_OR_AMD64
#ifdef MY_CPU_SSE2
  InitSIMD();
  
  if (size >= kBlockBytes * 4)
  {
    UInt32 state[16];
    state[0] = GetUi32(kSigma);
    state[1] = GetUi32(kSigma + 4);
    state[2] = GetUi32(kSigma + 8);
    state[3] = GetUi32(kSigma + 12);
    state[4] = GetUi32(derivedKey);
    state[5] = GetUi32(derivedKey + 4);
    state[6] = GetUi32(derivedKey + 8);
    state[7] = GetUi32(derivedKey + 12);
    state[8] = GetUi32(derivedKey + 16);
    state[9] = GetUi32(derivedKey + 20);
    state[10] = GetUi32(derivedKey + 24);
    state[11] = GetUi32(derivedKey + 28);
    state[12] = (UInt32)(counter & 0xFFFFFFFF);
    state[13] = (UInt32)(counter >> 32);
    state[14] = GetUi32(nonce + 16);
    state[15] = GetUi32(nonce + 20);
    
#ifdef MY_CPU_AMD64
    if (g_AVX2Enabled && size >= kBlockBytes * 8)
    {
      while (size >= kBlockBytes * 8)
      {
        ChaCha20_OperateKeystream_AVX2(state, data, data);
        state[12] += 8;
        if (state[12] < 8)
          state[13]++;
        data += kBlockBytes * 8;
        size -= kBlockBytes * 8;
      }
    }
#endif
    
    if (g_SSE2Enabled && size >= kBlockBytes * 4)
    {
      while (size >= kBlockBytes * 4)
      {
        ChaCha20_OperateKeystream_SSE2(state, data, data);
        state[12] += 4;
        if (state[12] < 4)
          state[13]++;
        data += kBlockBytes * 4;
        size -= kBlockBytes * 4;
      }
    }
    
    counter = (UInt64)state[13] << 32 | state[12];
    blockPos = kBlockBytes;
  }
#endif
#endif

#ifdef MY_CPU_ARM_OR_ARM64
  InitSIMD();

  if (g_NEONEnabled && size >= kBlockBytes * 4)
  {
    UInt32 state[16];
    state[0] = GetUi32(kSigma);
    state[1] = GetUi32(kSigma + 4);
    state[2] = GetUi32(kSigma + 8);
    state[3] = GetUi32(kSigma + 12);
    state[4] = GetUi32(derivedKey);
    state[5] = GetUi32(derivedKey + 4);
    state[6] = GetUi32(derivedKey + 8);
    state[7] = GetUi32(derivedKey + 12);
    state[8] = GetUi32(derivedKey + 16);
    state[9] = GetUi32(derivedKey + 20);
    state[10] = GetUi32(derivedKey + 24);
    state[11] = GetUi32(derivedKey + 28);
    state[12] = (UInt32)(counter & 0xFFFFFFFF);
    state[13] = (UInt32)(counter >> 32);
    state[14] = GetUi32(nonce + 16);
    state[15] = GetUi32(nonce + 20);

    while (size >= kBlockBytes * 4)
    {
      ChaCha20_OperateKeystream_NEON(state, data, data);
      state[12] += 4;
      if (state[12] < 4)
        state[13]++;
      data += kBlockBytes * 4;
      size -= kBlockBytes * 4;
    }

    counter = (UInt64)state[13] << 32 | state[12];
    blockPos = kBlockBytes;
  }
#endif
  
  while (size > 0)
  {
    if (blockPos == 0 || blockPos >= kBlockBytes)
    {
      XChaCha20Block_Core(block, derivedKey, nonce + 16, counter);
      blockPos = 0;
      counter++;
    }
    
    UInt32 remaining = kBlockBytes - blockPos;
    UInt32 toProcess = (size < remaining) ? size : remaining;
    
    Byte *dataPtr = data;
    const Byte *blockPtr = block + blockPos;
    UInt32 count = toProcess;
    
#ifdef MY_CPU_LE_UNALIGN_64
    while (count >= 8)
    {
      *(UInt64 *)dataPtr ^= *(const UInt64 *)blockPtr;
      dataPtr += 8;
      blockPtr += 8;
      count -= 8;
    }
#endif
    
#ifdef MY_CPU_LE_UNALIGN
    while (count >= 4)
    {
      *(UInt32 *)dataPtr ^= *(const UInt32 *)blockPtr;
      dataPtr += 4;
      blockPtr += 4;
      count -= 4;
    }
#endif
    
    while (count--)
      *dataPtr++ ^= *blockPtr++;
    
    data += toProcess;
    size -= toProcess;
    blockPos += toProcess;
  }
}

void CBaseCoder::ProcessData(Byte *data, UInt32 size)
{
  if (!_derivedKeyValid)
  {
    DeriveKey();
  }
  XChaCha20ProcessData(data, size, _derivedKey, _nonce, _counter, _block, _blockPos);
}

#ifndef Z7_EXTRACT_ONLY

Z7_COM7F_IMF(CEncoder::ResetInitVector())
{
  for (unsigned i = 0; i < sizeof(_nonce); i++)
    _nonce[i] = 0;
  MY_RAND_GEN(_nonce, kNonceSize);
  _counter = 0;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  return S_OK;
}

Z7_COM7F_IMF(CEncoder::WriteCoderProperties(ISequentialOutStream *outStream))
{
  Byte props[2 + sizeof(_key.Salt) + kNonceSize];
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

  return WriteStream(outStream, props, propsSize);
}

CEncoder::CEncoder()
{
  _key.NumCyclesPower = 19;
  _counter = 0;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
}

#endif

CDecoder::CDecoder()
{
  _counter = 0;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
}

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size))
{
  _key.ClearProps();
 
  _counter = 0;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  unsigned i;
  for (i = 0; i < sizeof(_nonce); i++)
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
  
  if (size != 2 + saltSize + nonceSize)
    return E_INVALIDARG;
  _key.SaltSize = saltSize;
  data += 2;
  for (i = 0; i < saltSize; i++)
    _key.Salt[i] = *data++;
  for (i = 0; i < nonceSize && i < kNonceSize; i++)
    _nonce[i] = *data++;
  
  return (_key.NumCyclesPower <= k_NumCyclesPower_Supported_MAX
      || _key.NumCyclesPower == 0x3F) ? S_OK : E_NOTIMPL;
}


Z7_COM7F_IMF(CBaseCoder::CryptoSetPassword(const Byte *data, UInt32 size))
{
  COM_TRY_BEGIN
  
  _key.Password.Wipe();
  _key.Password.CopyFrom(data, (size_t)size);
  _derivedKeyValid = false;
  return S_OK;
  
  COM_TRY_END
}

Z7_COM7F_IMF(CBaseCoder::Init())
{
  COM_TRY_BEGIN
  
  PrepareKey();
  _counter = 0;
  _blockPos = kBlockSize;
  _derivedKeyValid = false;
  return S_OK;
  
  COM_TRY_END
}

Z7_COM7F_IMF2(UInt32, CBaseCoder::Filter(Byte *data, UInt32 size))
{
  ProcessData(data, size);
  return size;
}

}}
