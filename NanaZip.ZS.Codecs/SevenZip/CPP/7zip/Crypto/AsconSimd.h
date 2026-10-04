// AsconSimd.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_INC_CRYPTO_ASCON_SIMD_H
#define ZIP7_INC_CRYPTO_ASCON_SIMD_H

#include "Ascon.h"

#ifdef MY_CPU_SSE2
#include <emmintrin.h>
#endif

#ifdef MY_CPU_ARM_OR_ARM64
#include <arm_neon.h>
#endif

namespace NCrypto {
namespace NAscon {

#ifdef MY_CPU_X86_OR_AMD64

extern bool g_SSE2Enabled;
extern bool g_SIMDInitialized;

#ifdef MY_CPU_AMD64
extern bool g_AVX512Enabled;
#endif

void InitSIMD();

#endif

#ifdef MY_CPU_ARM_OR_ARM64

extern bool g_NEONEnabled;
extern bool g_SIMDARMInitialized;

void InitSIMD();

#endif

#ifdef MY_CPU_SSE2

static Z7_FORCE_INLINE void AsconEncBlock_SSE2(UInt64 state[5], Byte *data)
{
  __m128i ks = _mm_set_epi64x(state[1], state[0]);
  __m128i ct = _mm_xor_si128(_mm_loadu_si128((const __m128i*)data), ks);
  _mm_storeu_si128((__m128i*)data, ct);
  _mm_storel_epi64((__m128i*)&state[0], ct);
  _mm_storel_epi64((__m128i*)&state[1], _mm_srli_si128(ct, 8));
}

static Z7_FORCE_INLINE void AsconDecBlock_SSE2(UInt64 state[5], Byte *data)
{
  __m128i ks = _mm_set_epi64x(state[1], state[0]);
  __m128i ct = _mm_loadu_si128((const __m128i*)data);
  __m128i pt = _mm_xor_si128(ct, ks);
  _mm_storeu_si128((__m128i*)data, pt);
  _mm_storel_epi64((__m128i*)&state[0], ct);
  _mm_storel_epi64((__m128i*)&state[1], _mm_srli_si128(ct, 8));
}

#endif

#ifdef MY_CPU_ARM_OR_ARM64

static Z7_FORCE_INLINE void AsconEncBlock_NEON(UInt64 state[5], Byte *data)
{
  uint64x2_t ks = vld1q_u64((const uint64_t*)state);
  uint64x2_t pt = vld1q_u64((const uint64_t*)data);
  uint64x2_t ct = veorq_u64(pt, ks);
  vst1q_u64((uint64_t*)data, ct);
  state[0] = vgetq_lane_u64(ct, 0);
  state[1] = vgetq_lane_u64(ct, 1);
}

static Z7_FORCE_INLINE void AsconDecBlock_NEON(UInt64 state[5], Byte *data)
{
  uint64x2_t ks = vld1q_u64((const uint64_t*)state);
  uint64x2_t ct = vld1q_u64((const uint64_t*)data);
  uint64x2_t pt = veorq_u64(ct, ks);
  vst1q_u64((uint64_t*)data, pt);
  state[0] = vgetq_lane_u64(ct, 0);
  state[1] = vgetq_lane_u64(ct, 1);
}

#endif

#ifdef MY_CPU_AMD64

#include <immintrin.h>

#if defined(__GNUC__) || defined(__clang__)
#define Z7_AVX512_TARGET __attribute__((target("avx512f")))
#else
#define Z7_AVX512_TARGET
#endif

static Z7_AVX512_TARGET inline void AsconRound_AVX512(UInt64 *st, UInt64 C)
{
  const UInt64 z = 0;
  const __mmask8 mxor1 = 0x15;
  const __mmask8 mxor2 = 0x0b;
  const __m512i pxor1 = _mm512_set_epi64(z, z, z, 3, z, 1, z, 4);
  const __m512i pxor2 = _mm512_set_epi64(z, z, z, z, 2, z, 0, 4);
  const __m512i rc = _mm512_set_epi64(z, z, z, 0, 0, C, 0, 0);
  const __m512i neg = _mm512_set_epi64(z, z, z, 0, 0, ~(UInt64)0, 0, 0);
  const __m512i pchi1 = _mm512_set_epi64(z, z, z, 0, 4, 3, 2, 1);
  const __m512i pchi2 = _mm512_set_epi64(z, z, z, 1, 0, 4, 3, 2);
  const __m512i rot1 = _mm512_set_epi64(z, z, z, 7, 10, 1, 61, 19);
  const __m512i rot2 = _mm512_set_epi64(z, z, z, 41, 17, 6, 39, 28);

  __m512i s = _mm512_loadu_si512((const void*)st);
  __m512i t0, t1, t2;

  t0 = _mm512_maskz_permutexvar_epi64(mxor1, pxor1, s);
  t0 = _mm512_ternarylogic_epi64(s, t0, rc, 0x96);

  t1 = _mm512_permutexvar_epi64(pchi1, t0);
  t2 = _mm512_permutexvar_epi64(pchi2, t0);
  t0 = _mm512_ternarylogic_epi64(t0, t1, t2, 0xd2);

  t1 = _mm512_maskz_permutexvar_epi64(mxor2, pxor2, t0);
  t0 = _mm512_ternarylogic_epi64(t0, t1, neg, 0x96);

  t1 = _mm512_rorv_epi64(t0, rot1);
  t2 = _mm512_rorv_epi64(t0, rot2);
  s = _mm512_ternarylogic_epi64(t0, t1, t2, 0x96);

  _mm512_storeu_si512((void*)st, s);
}

static Z7_AVX512_TARGET inline void AsconP12_AVX512(UInt64 state[5])
{
  AsconRound_AVX512(state, 0xf0);  AsconRound_AVX512(state, 0xe1);
  AsconRound_AVX512(state, 0xd2);  AsconRound_AVX512(state, 0xc3);
  AsconRound_AVX512(state, 0xb4);  AsconRound_AVX512(state, 0xa5);
  AsconRound_AVX512(state, 0x96);  AsconRound_AVX512(state, 0x87);
  AsconRound_AVX512(state, 0x78);  AsconRound_AVX512(state, 0x69);
  AsconRound_AVX512(state, 0x5a);  AsconRound_AVX512(state, 0x4b);
}

static Z7_AVX512_TARGET inline void AsconP8_AVX512(UInt64 state[5])
{
  AsconRound_AVX512(state, 0xb4);  AsconRound_AVX512(state, 0xa5);
  AsconRound_AVX512(state, 0x96);  AsconRound_AVX512(state, 0x87);
  AsconRound_AVX512(state, 0x78);  AsconRound_AVX512(state, 0x69);
  AsconRound_AVX512(state, 0x5a);  AsconRound_AVX512(state, 0x4b);
}

#endif

}}

#endif
