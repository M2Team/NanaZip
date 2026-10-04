// HkdfBlake2sp.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include "HkdfBlake2sp.h"

namespace NCrypto {
namespace NHkdfBlake2sp {

#define BLAKE2SP_BLOCK_SIZE 64

#define Z7_HKDF_MAX_OUT_SIZE (255 * Z7_BLAKE2S_DIGEST_SIZE)

static struct CBlake2sp_Prepare
{
  CBlake2sp_Prepare() { z7_Black2sp_Prepare(); }
} g_Blake2sp_Prepare;

static void CloneBlake2spState(CBlake2sp *dest, const CBlake2sp *src)
{
  memcpy(dest, src, sizeof(CBlake2sp));
}

void Derive(const Byte *prk, unsigned prkSize,
    const char *info, unsigned infoLen,
    Byte *output, unsigned outSize)
{
  if (outSize > Z7_HKDF_MAX_OUT_SIZE)
    return;

  Byte processedKey[Z7_BLAKE2S_DIGEST_SIZE];
  const Byte *effectiveKey;
  unsigned effectiveKeySize;

  if (prkSize > BLAKE2SP_BLOCK_SIZE)
  {
    CAlignedBuffer1 bufHash(sizeof(CBlake2sp));
    CBlake2sp *blake2spHash = (CBlake2sp *)(void *)(Byte *)bufHash;
    Blake2sp_Init(blake2spHash);
    Blake2sp_SetFunction(blake2spHash, 0);
    Blake2sp_Update(blake2spHash, prk, prkSize);
    Blake2sp_Final(blake2spHash, processedKey);

    effectiveKey = processedKey;
    effectiveKeySize = Z7_BLAKE2S_DIGEST_SIZE;
  }
  else
  {
    effectiveKey = prk;
    effectiveKeySize = prkSize;
  }

  Byte ipad[BLAKE2SP_BLOCK_SIZE];
  Byte opad[BLAKE2SP_BLOCK_SIZE];
  memset(ipad, 0x36, BLAKE2SP_BLOCK_SIZE);
  memset(opad, 0x5c, BLAKE2SP_BLOCK_SIZE);
  for (unsigned i = 0; i < effectiveKeySize; i++)
  {
    ipad[i] ^= effectiveKey[i];
    opad[i] ^= effectiveKey[i];
  }

  CAlignedBuffer1 bufInnerState(sizeof(CBlake2sp));
  CBlake2sp *innerState = (CBlake2sp *)(void *)(Byte *)bufInnerState;
  Blake2sp_Init(innerState);
  Blake2sp_SetFunction(innerState, 0);
  Blake2sp_Update(innerState, ipad, BLAKE2SP_BLOCK_SIZE);

  CAlignedBuffer1 bufOuterState(sizeof(CBlake2sp));
  CBlake2sp *outerState = (CBlake2sp *)(void *)(Byte *)bufOuterState;
  Blake2sp_Init(outerState);
  Blake2sp_SetFunction(outerState, 0);
  Blake2sp_Update(outerState, opad, BLAKE2SP_BLOCK_SIZE);

  const unsigned n = (outSize + Z7_BLAKE2S_DIGEST_SIZE - 1) / Z7_BLAKE2S_DIGEST_SIZE;

  Byte prevT[Z7_BLAKE2S_DIGEST_SIZE];
  unsigned prevTSize = 0;

  Byte *outPtr = output;
  unsigned remaining = outSize;

  CAlignedBuffer1 bufInnerTmp(sizeof(CBlake2sp));
  CAlignedBuffer1 bufOuterTmp(sizeof(CBlake2sp));
  CBlake2sp *innerTmp = (CBlake2sp *)(void *)(Byte *)bufInnerTmp;
  CBlake2sp *outerTmp = (CBlake2sp *)(void *)(Byte *)bufOuterTmp;

  for (unsigned i = 1; i <= n; i++)
  {
    CloneBlake2spState(innerTmp, innerState);
    if (prevTSize > 0)
      Blake2sp_Update(innerTmp, prevT, prevTSize);
    if (infoLen > 0)
      Blake2sp_Update(innerTmp, (const Byte *)info, infoLen);
    const Byte counter = (Byte)i;
    Blake2sp_Update(innerTmp, &counter, 1);

    Byte innerHash[Z7_BLAKE2S_DIGEST_SIZE];
    Blake2sp_Final(innerTmp, innerHash);
    CloneBlake2spState(outerTmp, outerState);
    Blake2sp_Update(outerTmp, innerHash, Z7_BLAKE2S_DIGEST_SIZE);

    Byte ti[Z7_BLAKE2S_DIGEST_SIZE];
    Blake2sp_Final(outerTmp, ti);

    const unsigned copySize = remaining < Z7_BLAKE2S_DIGEST_SIZE ? remaining : Z7_BLAKE2S_DIGEST_SIZE;
    memcpy(outPtr, ti, copySize);
    outPtr += copySize;
    remaining -= copySize;

    memcpy(prevT, ti, Z7_BLAKE2S_DIGEST_SIZE);
    prevTSize = Z7_BLAKE2S_DIGEST_SIZE;

    Z7_memset_0_ARRAY(ti);
    Z7_memset_0_ARRAY(innerHash);
  }

  Z7_memset_0_ARRAY(prevT);
  Z7_memset_0_ARRAY(ipad);
  Z7_memset_0_ARRAY(opad);
  Z7_memset_0_ARRAY(processedKey);
}

}}