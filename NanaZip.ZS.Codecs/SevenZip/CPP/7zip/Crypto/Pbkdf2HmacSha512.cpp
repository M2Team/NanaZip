// Pbkdf2HmacSha512.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include <string.h>

#include "../../../C/CpuArch.h"

#include "HmacSha512.h"
#include "Pbkdf2HmacSha512.h"

namespace NCrypto {
namespace NSha512 {

void Pbkdf2Hmac(const Byte *pwd, size_t pwdSize,
    const Byte *salt, size_t saltSize,
    UInt32 numIterations,
    Byte *key, size_t keySize)
{
  MY_ALIGN (16)
  CHmac baseCtx;
  baseCtx.SetKey(pwd, pwdSize);

  for (UInt32 i = 1; keySize != 0; i++)
  {
    MY_ALIGN (16)
    CHmac ctx;
    ctx = baseCtx;
    ctx.Update(salt, saltSize);

    MY_ALIGN (16)
    UInt32 be_i[1];
    SetBe32(be_i, i)

    ctx.Update((const Byte *)be_i, 4);

    MY_ALIGN (16)
    Byte u[kDigestSize];
    ctx.Final(u);

    MY_ALIGN (16)
    Byte t[kDigestSize];
    memcpy(t, u, kDigestSize);

    for (UInt32 j = 1; j < numIterations; j++)
    {
      ctx = baseCtx;
      ctx.Update(u, kDigestSize);
      ctx.Final(u);
      for (unsigned k = 0; k < kDigestSize; k++)
        t[k] ^= u[k];
    }

    const unsigned curSize = (keySize < kDigestSize) ? (unsigned)keySize : kDigestSize;
    memcpy(key, t, curSize);
    key += curSize;
    keySize -= curSize;
  }
}

}}
