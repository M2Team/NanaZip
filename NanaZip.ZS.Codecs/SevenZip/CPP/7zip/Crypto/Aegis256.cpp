// Aegis256.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+
//
// AEGIS-256 authenticated encryption (RFC 10032).
// The portable implementation uses the AES encryption round, so it can run
// without any AES hardware support.

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "Aegis256.h"

#include "Aegis256Simd.h"

namespace NCrypto {
namespace NAegis256 {

AEGIS256_UPDATE_FUNC g_Aegis256_Update = NULL;
AEGIS256_BLOCKS_FUNC g_Aegis256_EncBlocks = NULL;
AEGIS256_BLOCKS_FUNC g_Aegis256_DecBlocks = NULL;

// AEGIS constants C0 and C1
static const Byte kC0[kBlockSize] =
  { 0x00, 0x01, 0x01, 0x02, 0x03, 0x05, 0x08, 0x0d, 0x15, 0x22, 0x37, 0x59, 0x90, 0xe9, 0x79, 0x62 };

static const Byte kC1[kBlockSize] =
  { 0xdb, 0x3d, 0x18, 0x55, 0x6d, 0xc2, 0x2f, 0xf1, 0x20, 0x11, 0x31, 0x42, 0x73, 0xb5, 0x28, 0xdd };

// ---------- portable AES encryption round ----------

static const Byte kAesSbox[256] =
{
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

#define AEGIS256_XTIME(x) ((Byte)(((x) << 1) ^ ((x) >> 7) * 0x1b))

// AESRound(in, rk) = MixColumns(ShiftRows(SubBytes(in))) ^ rk
static void AesRoundSoft(Byte *out, const Byte *in, const Byte *rk)
{
  Byte t[kBlockSize];
  unsigned r, c;

  for (c = 0; c < 4; c++)
    for (r = 0; r < 4; r++)
      t[r + 4 * c] = kAesSbox[in[r + 4 * ((c + r) & 3)]];

  for (c = 0; c < 4; c++)
  {
    const Byte a0 = t[4 * c + 0];
    const Byte a1 = t[4 * c + 1];
    const Byte a2 = t[4 * c + 2];
    const Byte a3 = t[4 * c + 3];
    const Byte x = (Byte)(a0 ^ a1 ^ a2 ^ a3);
    out[4 * c + 0] = (Byte)(a0 ^ x ^ AEGIS256_XTIME((Byte)(a0 ^ a1)) ^ rk[4 * c + 0]);
    out[4 * c + 1] = (Byte)(a1 ^ x ^ AEGIS256_XTIME((Byte)(a1 ^ a2)) ^ rk[4 * c + 1]);
    out[4 * c + 2] = (Byte)(a2 ^ x ^ AEGIS256_XTIME((Byte)(a2 ^ a3)) ^ rk[4 * c + 2]);
    out[4 * c + 3] = (Byte)(a3 ^ x ^ AEGIS256_XTIME((Byte)(a3 ^ a0)) ^ rk[4 * c + 3]);
  }

  Z7_memset_0_ARRAY(t);
}

static void Z7_FASTCALL Aegis256_UpdateBlock_Soft(Byte *state, const Byte *m)
{
  Byte t[kBlockSize];

  memcpy(t, state + 5 * kBlockSize, kBlockSize);
  AesRoundSoft(state + 5 * kBlockSize, state + 4 * kBlockSize, state + 5 * kBlockSize);
  AesRoundSoft(state + 4 * kBlockSize, state + 3 * kBlockSize, state + 4 * kBlockSize);
  AesRoundSoft(state + 3 * kBlockSize, state + 2 * kBlockSize, state + 3 * kBlockSize);
  AesRoundSoft(state + 2 * kBlockSize, state + 1 * kBlockSize, state + 2 * kBlockSize);
  AesRoundSoft(state + 1 * kBlockSize, state + 0 * kBlockSize, state + 1 * kBlockSize);
  AesRoundSoft(state + 0 * kBlockSize, t, state + 0 * kBlockSize);

  for (unsigned i = 0; i < kBlockSize; i++)
    state[i] ^= m[i];

  Z7_memset_0_ARRAY(t);
}

// z = S1 ^ S4 ^ S5 ^ (S2 & S3)
static Z7_FORCE_INLINE void XorKeyStream(Byte *dest, const Byte *src, const Byte *state)
{
  const Byte *s1 = state + 1 * kBlockSize;
  const Byte *s2 = state + 2 * kBlockSize;
  const Byte *s3 = state + 3 * kBlockSize;
  const Byte *s4 = state + 4 * kBlockSize;
  const Byte *s5 = state + 5 * kBlockSize;

  for (unsigned i = 0; i < 2; i++)
  {
    const UInt64 z = GetUi64(s1 + i * 8) ^ GetUi64(s4 + i * 8)
        ^ GetUi64(s5 + i * 8) ^ (GetUi64(s2 + i * 8) & GetUi64(s3 + i * 8));
    SetUi64(dest + i * 8, GetUi64(src + i * 8) ^ z);
  }
}

static void Z7_FASTCALL Aegis256_EncryptBlocks_Soft(Byte *state, Byte *data, size_t numBlocks)
{
  do
  {
    Byte tmp[kBlockSize];
    XorKeyStream(tmp, data, state);
    Aegis256_UpdateBlock_Soft(state, data);
    memcpy(data, tmp, kBlockSize);
    Z7_memset_0_ARRAY(tmp);
    data += kBlockSize;
  }
  while (--numBlocks);
}

static void Z7_FASTCALL Aegis256_DecryptBlocks_Soft(Byte *state, Byte *data, size_t numBlocks)
{
  do
  {
    Byte tmp[kBlockSize];
    XorKeyStream(tmp, data, state);
    Aegis256_UpdateBlock_Soft(state, tmp);
    memcpy(data, tmp, kBlockSize);
    Z7_memset_0_ARRAY(tmp);
    data += kBlockSize;
  }
  while (--numBlocks);
}

void Aegis256_InitFuncs(void)
{
  if (g_Aegis256_Update)
    return;

  g_Aegis256_Update = Aegis256_UpdateBlock_Soft;
  g_Aegis256_EncBlocks = Aegis256_EncryptBlocks_Soft;
  g_Aegis256_DecBlocks = Aegis256_DecryptBlocks_Soft;

#ifdef AEGIS256_USE_HW_AES
  if (CPU_IsSupported_AES())
  {
    g_Aegis256_Update = Aegis256_UpdateBlock_HW;
    g_Aegis256_EncBlocks = Aegis256_EncryptBlocks_HW;
    g_Aegis256_DecBlocks = Aegis256_DecryptBlocks_HW;
  }
#endif
}

// ---------- cipher ----------

CCipher::CCipher():
  _adLen(0),
  _msgLen(0)
{
  Aegis256_InitFuncs();
  memset(_state, 0, sizeof(_state));
}

void CCipher::Clear()
{
  Z7_memset_0_ARRAY(_state);
  _adLen = 0;
  _msgLen = 0;
}

void CCipher::Init(const Byte *key, const Byte *nonce)
{
  const Byte *k0 = key;
  const Byte *k1 = key + kBlockSize;
  const Byte *n0 = nonce;
  const Byte *n1 = nonce + kBlockSize;

  Byte k0n0[kBlockSize];
  Byte k1n1[kBlockSize];
  unsigned i;

  for (i = 0; i < kBlockSize; i++)
  {
    _state[0 * kBlockSize + i] = (Byte)(k0[i] ^ n0[i]);
    _state[1 * kBlockSize + i] = (Byte)(k1[i] ^ n1[i]);
    _state[2 * kBlockSize + i] = kC1[i];
    _state[3 * kBlockSize + i] = kC0[i];
    _state[4 * kBlockSize + i] = (Byte)(k0[i] ^ kC0[i]);
    _state[5 * kBlockSize + i] = (Byte)(k1[i] ^ kC1[i]);
    k0n0[i] = (Byte)(k0[i] ^ n0[i]);
    k1n1[i] = (Byte)(k1[i] ^ n1[i]);
  }

  for (i = 0; i < 4; i++)
  {
    Absorb(k0);
    Absorb(k1);
    Absorb(k0n0);
    Absorb(k1n1);
  }

  Z7_memset_0_ARRAY(k0n0);
  Z7_memset_0_ARRAY(k1n1);

  _adLen = 0;
  _msgLen = 0;
}

void CCipher::AbsorbAad(const Byte *data, size_t size)
{
  size_t i = 0;
  _adLen += size;

  while (size - i >= kBlockSize)
  {
    Absorb(data + i);
    i += kBlockSize;
  }

  if (i < size)
  {
    // ZeroPad(ad, 128)
    Byte pad[kBlockSize];
    memset(pad, 0, kBlockSize);
    memcpy(pad, data + i, size - i);
    Absorb(pad);
    Z7_memset_0_ARRAY(pad);
  }
}

void CCipher::EncBlock(Byte *dest, const Byte *src)
{
  Byte tmp[kBlockSize];
  XorKeyStream(tmp, src, _state);
  Absorb(src);
  memcpy(dest, tmp, kBlockSize);
  Z7_memset_0_ARRAY(tmp);
}

void CCipher::EncryptData(Byte *data, size_t size)
{
  _msgLen += size;

  size_t numBlocks = size / kBlockSize;
  if (numBlocks)
  {
    g_Aegis256_EncBlocks(_state, data, numBlocks);
    data += numBlocks * kBlockSize;
    size -= numBlocks * kBlockSize;
  }

  if (size)
  {
    // the incomplete last block is zero padded, only (size) bytes are used
    Byte pad[kBlockSize];
    Byte out[kBlockSize];
    memset(pad, 0, kBlockSize);
    memcpy(pad, data, size);
    EncBlock(out, pad);
    memcpy(data, out, size);
    Z7_memset_0_ARRAY(pad);
    Z7_memset_0_ARRAY(out);
  }
}

void CCipher::DecryptData(Byte *data, size_t size)
{
  _msgLen += size;

  size_t numBlocks = size / kBlockSize;
  if (numBlocks)
  {
    g_Aegis256_DecBlocks(_state, data, numBlocks);
    data += numBlocks * kBlockSize;
    size -= numBlocks * kBlockSize;
  }

  if (size)
  {
    // DecPartial: the plaintext of the incomplete block is absorbed zero padded
    Byte pad[kBlockSize];
    Byte out[kBlockSize];
    memset(pad, 0, kBlockSize);
    memcpy(pad, data, size);
    XorKeyStream(out, pad, _state);
    memset(out + size, 0, kBlockSize - size);
    Absorb(out);
    memcpy(data, out, size);
    Z7_memset_0_ARRAY(pad);
    Z7_memset_0_ARRAY(out);
  }
}

void CCipher::FinalTag(Byte *tag)
{
  Byte t[kBlockSize];
  const UInt64 adLenBits = _adLen << 3;
  const UInt64 msgLenBits = _msgLen << 3;
  unsigned i;

  for (i = 0; i < 8; i++)
  {
    t[i] = (Byte)(adLenBits >> (i * 8));
    t[8 + i] = (Byte)(msgLenBits >> (i * 8));
  }

  for (i = 0; i < kBlockSize; i++)
    t[i] ^= _state[3 * kBlockSize + i];

  for (i = 0; i < 7; i++)
    Absorb(t);

  // 256-bit tag: tag = (S0 ^ S1 ^ S2) || (S3 ^ S4 ^ S5)
  for (i = 0; i < kBlockSize; i++)
  {
    tag[i] = (Byte)(_state[0 * kBlockSize + i] ^ _state[1 * kBlockSize + i]
        ^ _state[2 * kBlockSize + i]);
    tag[kBlockSize + i] = (Byte)(_state[3 * kBlockSize + i] ^ _state[4 * kBlockSize + i]
        ^ _state[5 * kBlockSize + i]);
  }

  Z7_memset_0_ARRAY(t);
}

}}
