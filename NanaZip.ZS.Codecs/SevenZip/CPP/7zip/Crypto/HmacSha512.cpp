// HmacSha512.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include <string.h>

#include "../../../C/CpuArch.h"

#include "HmacSha512.h"

namespace NCrypto {
namespace NSha512 {

void CHmac::SetKey(const Byte *key, size_t keySize)
{
  MY_ALIGN (16)
  UInt64 temp[SHA512_NUM_BLOCK_WORDS];
  size_t i;

  for (i = 0; i < SHA512_NUM_BLOCK_WORDS; i++)
    temp[i] = 0;

  if (keySize > kBlockSize)
  {
    Sha512_Init(&_sha, SHA512_DIGEST_SIZE);
    Sha512_Update(&_sha, key, keySize);
    Sha512_Final(&_sha, (Byte *)temp, SHA512_DIGEST_SIZE);
  }
  else
    memcpy(temp, key, keySize);

  for (i = 0; i < SHA512_NUM_BLOCK_WORDS; i++)
    temp[i] ^= UINT64_CONST(0x3636363636363636);

  Sha512_Init(&_sha, SHA512_DIGEST_SIZE);
  Sha512_Update(&_sha, (const Byte *)temp, kBlockSize);

  for (i = 0; i < SHA512_NUM_BLOCK_WORDS; i++)
    temp[i] ^= UINT64_CONST(0x3636363636363636) ^ UINT64_CONST(0x5C5C5C5C5C5C5C5C);

  Sha512_Init(&_sha2, SHA512_DIGEST_SIZE);
  Sha512_Update(&_sha2, (const Byte *)temp, kBlockSize);
}


void CHmac::Final(Byte *mac)
{
  Sha512_Final(&_sha, mac, SHA512_DIGEST_SIZE);
  Sha512_Update(&_sha2, mac, SHA512_DIGEST_SIZE);
  Sha512_Final(&_sha2, mac, SHA512_DIGEST_SIZE);
}

}}
