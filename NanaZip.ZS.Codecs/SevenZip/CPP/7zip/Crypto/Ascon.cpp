// Ascon.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "Ascon.h"
#include "AsconSimd.h"

namespace NCrypto {
namespace NAscon {

#ifdef MY_CPU_X86_OR_AMD64

bool g_SSE2Enabled = false;
bool g_SIMDInitialized = false;

#ifdef MY_CPU_AMD64
bool g_AVX512Enabled = false;

static UInt64 Ascon_xgetbv(UInt32 num)
{
#if defined(_MSC_VER)
  return _xgetbv(num);
#elif defined(__GNUC__) || defined(__clang__)
  UInt32 a, d;
  __asm__ __volatile__("xgetbv" : "=a"(a), "=d"(d) : "c"(num) : "cc");
  return ((UInt64)d << 32) | a;
#else
  return 0;
#endif
}
#endif

void InitSIMD()
{
  if (g_SIMDInitialized)
    return;
  g_SIMDInitialized = true;

#ifdef MY_CPU_AMD64
  g_SSE2Enabled = true;
#else
  g_SSE2Enabled = CPU_IsSupported_SSE2() != 0;
#endif

#ifdef MY_CPU_AMD64
  if (CPU_IsSupported_AVX())
  {
    if (z7_x86_cpuid_GetMaxFunc() >= 7)
    {
      UInt32 d[4];
      z7_x86_cpuid(d, 7);
      BoolInt avx512f = (d[1] >> 16) & 1;
      BoolInt avx512vl = (d[1] >> 31) & 1;
      if (avx512f && avx512vl)
      {
        const UInt32 bm = (UInt32)Ascon_xgetbv(0);
        if ((bm & 0xE0) == 0xE0)
          g_AVX512Enabled = true;
      }
    }
  }
#endif
}

#endif

#ifdef MY_CPU_ARM_OR_ARM64

bool g_NEONEnabled = false;
bool g_SIMDARMInitialized = false;

void InitSIMD()
{
  if (g_SIMDARMInitialized)
    return;
  g_SIMDARMInitialized = true;
  g_NEONEnabled = CPU_IsSupported_NEON() != 0;
}

#endif

#define RC0 0xf0
#define RC1 0xe1
#define RC2 0xd2
#define RC3 0xc3
#define RC4 0xb4
#define RC5 0xa5
#define RC6 0x96
#define RC7 0x87
#define RC8 0x78
#define RC9 0x69
#define RCa 0x5a
#define RCb 0x4b

static Z7_FORCE_INLINE void AsconRound(UInt64 *st, UInt64 C)
{
  UInt64 x0 = st[0];
  UInt64 x1 = st[1];
  UInt64 x2 = st[2];
  UInt64 x3 = st[3];
  UInt64 x4 = st[4];

  x2 ^= C;
  x0 ^= x4;
  x4 ^= x3;
  x2 ^= x1;

  UInt64 t0 = x0 ^ (~x1 & x2);
  UInt64 t2 = x2 ^ (~x3 & x4);
  UInt64 t4 = x4 ^ (~x0 & x1);
  UInt64 t1 = x1 ^ (~x2 & x3);
  UInt64 t3 = x3 ^ (~x4 & x0);
  t1 ^= t0;
  t3 ^= t2;
  t0 ^= t4;
  t2 = ~t2;

  x0 = t0 ^ ASCON_ROR64(t0, 19) ^ ASCON_ROR64(t0, 28);
  x1 = t1 ^ ASCON_ROR64(t1, 61) ^ ASCON_ROR64(t1, 39);
  x2 = t2 ^ ASCON_ROR64(t2, 1)  ^ ASCON_ROR64(t2, 6);
  x3 = t3 ^ ASCON_ROR64(t3, 10) ^ ASCON_ROR64(t3, 17);
  x4 = t4 ^ ASCON_ROR64(t4, 7)  ^ ASCON_ROR64(t4, 41);

  st[0] = x0;
  st[1] = x1;
  st[2] = x2;
  st[3] = x3;
  st[4] = x4;
}

void AsconP12(UInt64 state[5])
{
#ifdef MY_CPU_AMD64
  InitSIMD();
  if (g_AVX512Enabled)
  {
    UInt64 st[8] = { state[0], state[1], state[2], state[3], state[4] };
    AsconP12_AVX512(st);
    state[0] = st[0];
    state[1] = st[1];
    state[2] = st[2];
    state[3] = st[3];
    state[4] = st[4];
    return;
  }
#endif
  AsconRound(state, RC0);  AsconRound(state, RC1);
  AsconRound(state, RC2);  AsconRound(state, RC3);
  AsconRound(state, RC4);  AsconRound(state, RC5);
  AsconRound(state, RC6);  AsconRound(state, RC7);
  AsconRound(state, RC8);  AsconRound(state, RC9);
  AsconRound(state, RCa);  AsconRound(state, RCb);
}

void AsconP8(UInt64 state[5])
{
#ifdef MY_CPU_AMD64
  InitSIMD();
  if (g_AVX512Enabled)
  {
    UInt64 st[8] = { state[0], state[1], state[2], state[3], state[4] };
    AsconP8_AVX512(st);
    state[0] = st[0];
    state[1] = st[1];
    state[2] = st[2];
    state[3] = st[3];
    state[4] = st[4];
    return;
  }
#endif
  AsconRound(state, RC4);  AsconRound(state, RC5);
  AsconRound(state, RC6);  AsconRound(state, RC7);
  AsconRound(state, RC8);  AsconRound(state, RC9);
  AsconRound(state, RCa);  AsconRound(state, RCb);
}

}}
