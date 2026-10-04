// ChaCha20Simd.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_CRYPTO_CHACHA20_SIMD_H
#define ZIP7_CRYPTO_CHACHA20_SIMD_H

#ifdef MY_CPU_X86_OR_AMD64
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__)
#include <x86intrin.h>
#endif
#endif

#ifdef MY_CPU_ARM_OR_ARM64
#include <arm_neon.h>
#endif

static const Byte kSigma[16] = {
  'e', 'x', 'p', 'a', 'n', 'd', ' ', '3', '2', '-', 'b', 'y', 't', 'e', ' ', 'k'
};

#ifdef MY_CPU_X86_OR_AMD64

#ifdef MY_CPU_SSE2

namespace {

template <unsigned int R>
Z7_FORCE_INLINE __m128i RotateLeft_SSE2(const __m128i val)
{
  return _mm_or_si128(_mm_slli_epi32(val, R), _mm_srli_epi32(val, 32 - R));
}

template <>
Z7_FORCE_INLINE __m128i RotateLeft_SSE2<8>(const __m128i val)
{
#ifdef __SSSE3__
  const __m128i mask = _mm_set_epi8(14,13,12,15, 10,9,8,11, 6,5,4,7, 2,1,0,3);
  return _mm_shuffle_epi8(val, mask);
#else
  return _mm_or_si128(_mm_slli_epi32(val, 8), _mm_srli_epi32(val, 24));
#endif
}

template <>
Z7_FORCE_INLINE __m128i RotateLeft_SSE2<16>(const __m128i val)
{
#ifdef __SSSE3__
  const __m128i mask = _mm_set_epi8(13,12,15,14, 9,8,11,10, 5,4,7,6, 1,0,3,2);
  return _mm_shuffle_epi8(val, mask);
#else
  return _mm_or_si128(_mm_slli_epi32(val, 16), _mm_srli_epi32(val, 16));
#endif
}

#define SSE2_QUARTERROUND(a, b, c, d) \
  a = _mm_add_epi32(a, b); \
  d = _mm_xor_si128(d, a); \
  d = RotateLeft_SSE2<16>(d); \
  c = _mm_add_epi32(c, d); \
  b = _mm_xor_si128(b, c); \
  b = RotateLeft_SSE2<12>(b); \
  a = _mm_add_epi32(a, b); \
  d = _mm_xor_si128(d, a); \
  d = RotateLeft_SSE2<8>(d); \
  c = _mm_add_epi32(c, d); \
  b = _mm_xor_si128(b, c); \
  b = RotateLeft_SSE2<7>(b);

Z7_NO_INLINE void ChaCha20_OperateKeystream_SSE2(
    const UInt32 *state,
    const Byte *input,
    Byte *output)
{
  const __m128i state0 = _mm_loadu_si128((const __m128i *)(state + 0));
  const __m128i state1 = _mm_loadu_si128((const __m128i *)(state + 4));
  const __m128i state2 = _mm_loadu_si128((const __m128i *)(state + 8));
  const __m128i state3 = _mm_loadu_si128((const __m128i *)(state + 12));

  __m128i r0_0 = state0;
  __m128i r0_1 = state1;
  __m128i r0_2 = state2;
  __m128i r0_3 = state3;

  __m128i r1_0 = state0;
  __m128i r1_1 = state1;
  __m128i r1_2 = state2;
  __m128i r1_3 = _mm_add_epi64(state3, _mm_set_epi32(0, 0, 0, 1));

  __m128i r2_0 = state0;
  __m128i r2_1 = state1;
  __m128i r2_2 = state2;
  __m128i r2_3 = _mm_add_epi64(state3, _mm_set_epi32(0, 0, 0, 2));

  __m128i r3_0 = state0;
  __m128i r3_1 = state1;
  __m128i r3_2 = state2;
  __m128i r3_3 = _mm_add_epi64(state3, _mm_set_epi32(0, 0, 0, 3));

  for (int i = 0; i < 10; i++)
  {
    SSE2_QUARTERROUND(r0_0, r0_1, r0_2, r0_3);
    SSE2_QUARTERROUND(r1_0, r1_1, r1_2, r1_3);
    SSE2_QUARTERROUND(r2_0, r2_1, r2_2, r2_3);
    SSE2_QUARTERROUND(r3_0, r3_1, r3_2, r3_3);

    r0_1 = _mm_shuffle_epi32(r0_1, _MM_SHUFFLE(0, 3, 2, 1));
    r0_2 = _mm_shuffle_epi32(r0_2, _MM_SHUFFLE(1, 0, 3, 2));
    r0_3 = _mm_shuffle_epi32(r0_3, _MM_SHUFFLE(2, 1, 0, 3));

    r1_1 = _mm_shuffle_epi32(r1_1, _MM_SHUFFLE(0, 3, 2, 1));
    r1_2 = _mm_shuffle_epi32(r1_2, _MM_SHUFFLE(1, 0, 3, 2));
    r1_3 = _mm_shuffle_epi32(r1_3, _MM_SHUFFLE(2, 1, 0, 3));

    r2_1 = _mm_shuffle_epi32(r2_1, _MM_SHUFFLE(0, 3, 2, 1));
    r2_2 = _mm_shuffle_epi32(r2_2, _MM_SHUFFLE(1, 0, 3, 2));
    r2_3 = _mm_shuffle_epi32(r2_3, _MM_SHUFFLE(2, 1, 0, 3));

    r3_1 = _mm_shuffle_epi32(r3_1, _MM_SHUFFLE(0, 3, 2, 1));
    r3_2 = _mm_shuffle_epi32(r3_2, _MM_SHUFFLE(1, 0, 3, 2));
    r3_3 = _mm_shuffle_epi32(r3_3, _MM_SHUFFLE(2, 1, 0, 3));

    SSE2_QUARTERROUND(r0_0, r0_1, r0_2, r0_3);
    SSE2_QUARTERROUND(r1_0, r1_1, r1_2, r1_3);
    SSE2_QUARTERROUND(r2_0, r2_1, r2_2, r2_3);
    SSE2_QUARTERROUND(r3_0, r3_1, r3_2, r3_3);

    r0_1 = _mm_shuffle_epi32(r0_1, _MM_SHUFFLE(2, 1, 0, 3));
    r0_2 = _mm_shuffle_epi32(r0_2, _MM_SHUFFLE(1, 0, 3, 2));
    r0_3 = _mm_shuffle_epi32(r0_3, _MM_SHUFFLE(0, 3, 2, 1));

    r1_1 = _mm_shuffle_epi32(r1_1, _MM_SHUFFLE(2, 1, 0, 3));
    r1_2 = _mm_shuffle_epi32(r1_2, _MM_SHUFFLE(1, 0, 3, 2));
    r1_3 = _mm_shuffle_epi32(r1_3, _MM_SHUFFLE(0, 3, 2, 1));

    r2_1 = _mm_shuffle_epi32(r2_1, _MM_SHUFFLE(2, 1, 0, 3));
    r2_2 = _mm_shuffle_epi32(r2_2, _MM_SHUFFLE(1, 0, 3, 2));
    r2_3 = _mm_shuffle_epi32(r2_3, _MM_SHUFFLE(0, 3, 2, 1));

    r3_1 = _mm_shuffle_epi32(r3_1, _MM_SHUFFLE(2, 1, 0, 3));
    r3_2 = _mm_shuffle_epi32(r3_2, _MM_SHUFFLE(1, 0, 3, 2));
    r3_3 = _mm_shuffle_epi32(r3_3, _MM_SHUFFLE(0, 3, 2, 1));
  }

  r0_0 = _mm_add_epi32(r0_0, state0);
  r0_1 = _mm_add_epi32(r0_1, state1);
  r0_2 = _mm_add_epi32(r0_2, state2);
  r0_3 = _mm_add_epi32(r0_3, state3);

  r1_0 = _mm_add_epi32(r1_0, state0);
  r1_1 = _mm_add_epi32(r1_1, state1);
  r1_2 = _mm_add_epi32(r1_2, state2);
  r1_3 = _mm_add_epi32(r1_3, state3);
  r1_3 = _mm_add_epi64(r1_3, _mm_set_epi32(0, 0, 0, 1));

  r2_0 = _mm_add_epi32(r2_0, state0);
  r2_1 = _mm_add_epi32(r2_1, state1);
  r2_2 = _mm_add_epi32(r2_2, state2);
  r2_3 = _mm_add_epi32(r2_3, state3);
  r2_3 = _mm_add_epi64(r2_3, _mm_set_epi32(0, 0, 0, 2));

  r3_0 = _mm_add_epi32(r3_0, state0);
  r3_1 = _mm_add_epi32(r3_1, state1);
  r3_2 = _mm_add_epi32(r3_2, state2);
  r3_3 = _mm_add_epi32(r3_3, state3);
  r3_3 = _mm_add_epi64(r3_3, _mm_set_epi32(0, 0, 0, 3));

  if (input)
  {
    r0_0 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 0*16)), r0_0);
    r0_1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 1*16)), r0_1);
    r0_2 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 2*16)), r0_2);
    r0_3 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 3*16)), r0_3);
  }

  _mm_storeu_si128((__m128i *)(output + 0*16), r0_0);
  _mm_storeu_si128((__m128i *)(output + 1*16), r0_1);
  _mm_storeu_si128((__m128i *)(output + 2*16), r0_2);
  _mm_storeu_si128((__m128i *)(output + 3*16), r0_3);

  if (input)
  {
    r1_0 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 4*16)), r1_0);
    r1_1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 5*16)), r1_1);
    r1_2 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 6*16)), r1_2);
    r1_3 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 7*16)), r1_3);
  }

  _mm_storeu_si128((__m128i *)(output + 4*16), r1_0);
  _mm_storeu_si128((__m128i *)(output + 5*16), r1_1);
  _mm_storeu_si128((__m128i *)(output + 6*16), r1_2);
  _mm_storeu_si128((__m128i *)(output + 7*16), r1_3);

  if (input)
  {
    r2_0 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 8*16)), r2_0);
    r2_1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 9*16)), r2_1);
    r2_2 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 10*16)), r2_2);
    r2_3 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 11*16)), r2_3);
  }

  _mm_storeu_si128((__m128i *)(output + 8*16), r2_0);
  _mm_storeu_si128((__m128i *)(output + 9*16), r2_1);
  _mm_storeu_si128((__m128i *)(output + 10*16), r2_2);
  _mm_storeu_si128((__m128i *)(output + 11*16), r2_3);

  if (input)
  {
    r3_0 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 12*16)), r3_0);
    r3_1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 13*16)), r3_1);
    r3_2 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 14*16)), r3_2);
    r3_3 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(input + 15*16)), r3_3);
  }

  _mm_storeu_si128((__m128i *)(output + 12*16), r3_0);
  _mm_storeu_si128((__m128i *)(output + 13*16), r3_1);
  _mm_storeu_si128((__m128i *)(output + 14*16), r3_2);
  _mm_storeu_si128((__m128i *)(output + 15*16), r3_3);
}

#ifdef MY_CPU_AMD64

#if defined(__GNUC__) || defined(__clang__)
#define Z7_AVX2_TARGET_ATTR __attribute__((target("avx2")))
#else
#define Z7_AVX2_TARGET_ATTR
#endif

template <unsigned int R>
Z7_AVX2_TARGET_ATTR Z7_FORCE_INLINE __m256i RotateLeft_AVX2(const __m256i val)
{
  return _mm256_or_si256(_mm256_slli_epi32(val, R), _mm256_srli_epi32(val, 32 - R));
}

template <>
Z7_AVX2_TARGET_ATTR Z7_FORCE_INLINE __m256i RotateLeft_AVX2<8>(const __m256i val)
{
  const __m256i mask = _mm256_set_epi8(
    14,13,12,15, 10,9,8,11, 6,5,4,7, 2,1,0,3,
    14,13,12,15, 10,9,8,11, 6,5,4,7, 2,1,0,3);
  return _mm256_shuffle_epi8(val, mask);
}

template <>
Z7_AVX2_TARGET_ATTR Z7_FORCE_INLINE __m256i RotateLeft_AVX2<16>(const __m256i val)
{
  const __m256i mask = _mm256_set_epi8(
    13,12,15,14, 9,8,11,10, 5,4,7,6, 1,0,3,2,
    13,12,15,14, 9,8,11,10, 5,4,7,6, 1,0,3,2);
  return _mm256_shuffle_epi8(val, mask);
}

#define AVX2_QUARTERROUND(a, b, c, d) \
  a = _mm256_add_epi32(a, b); \
  d = _mm256_xor_si256(d, a); \
  d = RotateLeft_AVX2<16>(d); \
  c = _mm256_add_epi32(c, d); \
  b = _mm256_xor_si256(b, c); \
  b = RotateLeft_AVX2<12>(b); \
  a = _mm256_add_epi32(a, b); \
  d = _mm256_xor_si256(d, a); \
  d = RotateLeft_AVX2<8>(d); \
  c = _mm256_add_epi32(c, d); \
  b = _mm256_xor_si256(b, c); \
  b = RotateLeft_AVX2<7>(b);

Z7_AVX2_TARGET_ATTR Z7_NO_INLINE void ChaCha20_OperateKeystream_AVX2(
    const UInt32 *state,
    const Byte *input,
    Byte *output)
{
  const __m256i state0 = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)(state + 0)));
  const __m256i state1 = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)(state + 4)));
  const __m256i state2 = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)(state + 8)));
  const __m256i state3 = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)(state + 12)));

  const UInt32 C = 0xFFFFFFFFu - state[12];
  const __m256i CTR0 = _mm256_set_epi32(0, 0,     0, 0, 0, 0, C < 4, 4);
  const __m256i CTR1 = _mm256_set_epi32(0, 0, C < 1, 1, 0, 0, C < 5, 5);
  const __m256i CTR2 = _mm256_set_epi32(0, 0, C < 2, 2, 0, 0, C < 6, 6);
  const __m256i CTR3 = _mm256_set_epi32(0, 0, C < 3, 3, 0, 0, C < 7, 7);

  __m256i X0_0 = state0;
  __m256i X0_1 = state1;
  __m256i X0_2 = state2;
  __m256i X0_3 = _mm256_add_epi32(state3, CTR0);

  __m256i X1_0 = state0;
  __m256i X1_1 = state1;
  __m256i X1_2 = state2;
  __m256i X1_3 = _mm256_add_epi32(state3, CTR1);

  __m256i X2_0 = state0;
  __m256i X2_1 = state1;
  __m256i X2_2 = state2;
  __m256i X2_3 = _mm256_add_epi32(state3, CTR2);

  __m256i X3_0 = state0;
  __m256i X3_1 = state1;
  __m256i X3_2 = state2;
  __m256i X3_3 = _mm256_add_epi32(state3, CTR3);

  for (int i = 0; i < 10; i++)
  {
    AVX2_QUARTERROUND(X0_0, X0_1, X0_2, X0_3);
    AVX2_QUARTERROUND(X1_0, X1_1, X1_2, X1_3);
    AVX2_QUARTERROUND(X2_0, X2_1, X2_2, X2_3);
    AVX2_QUARTERROUND(X3_0, X3_1, X3_2, X3_3);

    X0_1 = _mm256_shuffle_epi32(X0_1, _MM_SHUFFLE(0, 3, 2, 1));
    X0_2 = _mm256_shuffle_epi32(X0_2, _MM_SHUFFLE(1, 0, 3, 2));
    X0_3 = _mm256_shuffle_epi32(X0_3, _MM_SHUFFLE(2, 1, 0, 3));

    X1_1 = _mm256_shuffle_epi32(X1_1, _MM_SHUFFLE(0, 3, 2, 1));
    X1_2 = _mm256_shuffle_epi32(X1_2, _MM_SHUFFLE(1, 0, 3, 2));
    X1_3 = _mm256_shuffle_epi32(X1_3, _MM_SHUFFLE(2, 1, 0, 3));

    X2_1 = _mm256_shuffle_epi32(X2_1, _MM_SHUFFLE(0, 3, 2, 1));
    X2_2 = _mm256_shuffle_epi32(X2_2, _MM_SHUFFLE(1, 0, 3, 2));
    X2_3 = _mm256_shuffle_epi32(X2_3, _MM_SHUFFLE(2, 1, 0, 3));

    X3_1 = _mm256_shuffle_epi32(X3_1, _MM_SHUFFLE(0, 3, 2, 1));
    X3_2 = _mm256_shuffle_epi32(X3_2, _MM_SHUFFLE(1, 0, 3, 2));
    X3_3 = _mm256_shuffle_epi32(X3_3, _MM_SHUFFLE(2, 1, 0, 3));

    AVX2_QUARTERROUND(X0_0, X0_1, X0_2, X0_3);
    AVX2_QUARTERROUND(X1_0, X1_1, X1_2, X1_3);
    AVX2_QUARTERROUND(X2_0, X2_1, X2_2, X2_3);
    AVX2_QUARTERROUND(X3_0, X3_1, X3_2, X3_3);

    X0_1 = _mm256_shuffle_epi32(X0_1, _MM_SHUFFLE(2, 1, 0, 3));
    X0_2 = _mm256_shuffle_epi32(X0_2, _MM_SHUFFLE(1, 0, 3, 2));
    X0_3 = _mm256_shuffle_epi32(X0_3, _MM_SHUFFLE(0, 3, 2, 1));

    X1_1 = _mm256_shuffle_epi32(X1_1, _MM_SHUFFLE(2, 1, 0, 3));
    X1_2 = _mm256_shuffle_epi32(X1_2, _MM_SHUFFLE(1, 0, 3, 2));
    X1_3 = _mm256_shuffle_epi32(X1_3, _MM_SHUFFLE(0, 3, 2, 1));

    X2_1 = _mm256_shuffle_epi32(X2_1, _MM_SHUFFLE(2, 1, 0, 3));
    X2_2 = _mm256_shuffle_epi32(X2_2, _MM_SHUFFLE(1, 0, 3, 2));
    X2_3 = _mm256_shuffle_epi32(X2_3, _MM_SHUFFLE(0, 3, 2, 1));

    X3_1 = _mm256_shuffle_epi32(X3_1, _MM_SHUFFLE(2, 1, 0, 3));
    X3_2 = _mm256_shuffle_epi32(X3_2, _MM_SHUFFLE(1, 0, 3, 2));
    X3_3 = _mm256_shuffle_epi32(X3_3, _MM_SHUFFLE(0, 3, 2, 1));
  }

  X0_0 = _mm256_add_epi32(X0_0, state0);
  X0_1 = _mm256_add_epi32(X0_1, state1);
  X0_2 = _mm256_add_epi32(X0_2, state2);
  X0_3 = _mm256_add_epi32(X0_3, state3);
  X0_3 = _mm256_add_epi32(X0_3, CTR0);

  X1_0 = _mm256_add_epi32(X1_0, state0);
  X1_1 = _mm256_add_epi32(X1_1, state1);
  X1_2 = _mm256_add_epi32(X1_2, state2);
  X1_3 = _mm256_add_epi32(X1_3, state3);
  X1_3 = _mm256_add_epi32(X1_3, CTR1);

  X2_0 = _mm256_add_epi32(X2_0, state0);
  X2_1 = _mm256_add_epi32(X2_1, state1);
  X2_2 = _mm256_add_epi32(X2_2, state2);
  X2_3 = _mm256_add_epi32(X2_3, state3);
  X2_3 = _mm256_add_epi32(X2_3, CTR2);

  X3_0 = _mm256_add_epi32(X3_0, state0);
  X3_1 = _mm256_add_epi32(X3_1, state1);
  X3_2 = _mm256_add_epi32(X3_2, state2);
  X3_3 = _mm256_add_epi32(X3_3, state3);
  X3_3 = _mm256_add_epi32(X3_3, CTR3);

  if (input)
  {
    _mm256_storeu_si256((__m256i *)(output + 0*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X0_0, X0_1, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 0*32))));
    _mm256_storeu_si256((__m256i *)(output + 1*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X0_2, X0_3, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 1*32))));
    _mm256_storeu_si256((__m256i *)(output + 2*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X1_0, X1_1, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 2*32))));
    _mm256_storeu_si256((__m256i *)(output + 3*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X1_2, X1_3, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 3*32))));
  }
  else
  {
    _mm256_storeu_si256((__m256i *)(output + 0*32),
      _mm256_permute2x128_si256(X0_0, X0_1, 1 + (3 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 1*32),
      _mm256_permute2x128_si256(X0_2, X0_3, 1 + (3 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 2*32),
      _mm256_permute2x128_si256(X1_0, X1_1, 1 + (3 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 3*32),
      _mm256_permute2x128_si256(X1_2, X1_3, 1 + (3 << 4)));
  }

  if (input)
  {
    _mm256_storeu_si256((__m256i *)(output + 4*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X2_0, X2_1, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 4*32))));
    _mm256_storeu_si256((__m256i *)(output + 5*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X2_2, X2_3, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 5*32))));
    _mm256_storeu_si256((__m256i *)(output + 6*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X3_0, X3_1, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 6*32))));
    _mm256_storeu_si256((__m256i *)(output + 7*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X3_2, X3_3, 1 + (3 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 7*32))));
  }
  else
  {
    _mm256_storeu_si256((__m256i *)(output + 4*32),
      _mm256_permute2x128_si256(X2_0, X2_1, 1 + (3 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 5*32),
      _mm256_permute2x128_si256(X2_2, X2_3, 1 + (3 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 6*32),
      _mm256_permute2x128_si256(X3_0, X3_1, 1 + (3 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 7*32),
      _mm256_permute2x128_si256(X3_2, X3_3, 1 + (3 << 4)));
  }

  if (input)
  {
    _mm256_storeu_si256((__m256i *)(output + 8*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X0_0, X0_1, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 8*32))));
    _mm256_storeu_si256((__m256i *)(output + 9*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X0_2, X0_3, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 9*32))));
    _mm256_storeu_si256((__m256i *)(output + 10*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X1_0, X1_1, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 10*32))));
    _mm256_storeu_si256((__m256i *)(output + 11*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X1_2, X1_3, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 11*32))));
  }
  else
  {
    _mm256_storeu_si256((__m256i *)(output + 8*32),
      _mm256_permute2x128_si256(X0_0, X0_1, 0 + (2 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 9*32),
      _mm256_permute2x128_si256(X0_2, X0_3, 0 + (2 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 10*32),
      _mm256_permute2x128_si256(X1_0, X1_1, 0 + (2 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 11*32),
      _mm256_permute2x128_si256(X1_2, X1_3, 0 + (2 << 4)));
  }

  if (input)
  {
    _mm256_storeu_si256((__m256i *)(output + 12*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X2_0, X2_1, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 12*32))));
    _mm256_storeu_si256((__m256i *)(output + 13*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X2_2, X2_3, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 13*32))));
    _mm256_storeu_si256((__m256i *)(output + 14*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X3_0, X3_1, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 14*32))));
    _mm256_storeu_si256((__m256i *)(output + 15*32),
      _mm256_xor_si256(_mm256_permute2x128_si256(X3_2, X3_3, 0 + (2 << 4)),
      _mm256_loadu_si256((const __m256i *)(input + 15*32))));
  }
  else
  {
    _mm256_storeu_si256((__m256i *)(output + 12*32),
      _mm256_permute2x128_si256(X2_0, X2_1, 0 + (2 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 13*32),
      _mm256_permute2x128_si256(X2_2, X2_3, 0 + (2 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 14*32),
      _mm256_permute2x128_si256(X3_0, X3_1, 0 + (2 << 4)));
    _mm256_storeu_si256((__m256i *)(output + 15*32),
      _mm256_permute2x128_si256(X3_2, X3_3, 0 + (2 << 4)));
  }

  _mm256_zeroupper();
}

#endif

}

static bool g_SSE2Enabled = false;
static bool g_SIMDInitialized = false;
#ifdef MY_CPU_AMD64
static bool g_AVX2Enabled = false;
#endif

static void InitSIMD()
{
  if (g_SIMDInitialized)
    return;
  g_SIMDInitialized = true;
  
#ifdef MY_CPU_AMD64
  g_SSE2Enabled = true;
  g_AVX2Enabled = CPU_IsSupported_AVX2() != 0;
#elif defined(MY_CPU_X86)
  g_SSE2Enabled = CPU_IsSupported_SSE2() != 0;
#endif
}

#endif

#elif defined(MY_CPU_ARM_OR_ARM64)

namespace {

template <unsigned int R>
Z7_FORCE_INLINE uint32x4_t RotateLeft_NEON(const uint32x4_t val)
{
  return vorrq_u32(vshlq_n_u32(val, R), vshrq_n_u32(val, 32 - R));
}

template <>
Z7_FORCE_INLINE uint32x4_t RotateLeft_NEON<8>(const uint32x4_t val)
{
#if defined(__aarch64__) || defined(_M_ARM64)
  static const uint8_t kMaskRot8[16] = {3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14};
  const uint8x16_t mask = vld1q_u8(kMaskRot8);
  return vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(val), mask));
#else
  return vorrq_u32(vshlq_n_u32(val, 8), vshrq_n_u32(val, 32 - 8));
#endif
}

template <>
Z7_FORCE_INLINE uint32x4_t RotateLeft_NEON<16>(const uint32x4_t val)
{
  return vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(val)));
}

Z7_FORCE_INLINE uint32x4_t Add64_NEON(const uint32x4_t a, const uint32x4_t b)
{
  return vreinterpretq_u32_u64(vaddq_u64(vreinterpretq_u64_u32(a), vreinterpretq_u64_u32(b)));
}

template <unsigned int S>
Z7_FORCE_INLINE uint32x4_t Extract_NEON(const uint32x4_t val)
{
  return vextq_u32(val, val, S);
}

#define NEON_QUARTERROUND(a, b, c, d) \
  a = vaddq_u32(a, b); \
  d = veorq_u32(d, a); \
  d = RotateLeft_NEON<16>(d); \
  c = vaddq_u32(c, d); \
  b = veorq_u32(b, c); \
  b = RotateLeft_NEON<12>(b); \
  a = vaddq_u32(a, b); \
  d = veorq_u32(d, a); \
  d = RotateLeft_NEON<8>(d); \
  c = vaddq_u32(c, d); \
  b = veorq_u32(b, c); \
  b = RotateLeft_NEON<7>(b);

Z7_NO_INLINE void ChaCha20_OperateKeystream_NEON(
    const UInt32 *state,
    const Byte *input,
    Byte *output)
{
  const uint32x4_t state0 = vld1q_u32(state + 0);
  const uint32x4_t state1 = vld1q_u32(state + 4);
  const uint32x4_t state2 = vld1q_u32(state + 8);
  const uint32x4_t state3 = vld1q_u32(state + 12);

  const UInt32 CTR[12] = {1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0};
  const uint32x4_t CTR1 = vld1q_u32(CTR + 0);
  const uint32x4_t CTR2 = vld1q_u32(CTR + 4);
  const uint32x4_t CTR3 = vld1q_u32(CTR + 8);

  uint32x4_t r0_0 = state0;
  uint32x4_t r0_1 = state1;
  uint32x4_t r0_2 = state2;
  uint32x4_t r0_3 = state3;

  uint32x4_t r1_0 = state0;
  uint32x4_t r1_1 = state1;
  uint32x4_t r1_2 = state2;
  uint32x4_t r1_3 = Add64_NEON(state3, CTR1);

  uint32x4_t r2_0 = state0;
  uint32x4_t r2_1 = state1;
  uint32x4_t r2_2 = state2;
  uint32x4_t r2_3 = Add64_NEON(state3, CTR2);

  uint32x4_t r3_0 = state0;
  uint32x4_t r3_1 = state1;
  uint32x4_t r3_2 = state2;
  uint32x4_t r3_3 = Add64_NEON(state3, CTR3);

  for (int i = 0; i < 10; i++)
  {
    NEON_QUARTERROUND(r0_0, r0_1, r0_2, r0_3);
    NEON_QUARTERROUND(r1_0, r1_1, r1_2, r1_3);
    NEON_QUARTERROUND(r2_0, r2_1, r2_2, r2_3);
    NEON_QUARTERROUND(r3_0, r3_1, r3_2, r3_3);

    r0_1 = Extract_NEON<1>(r0_1);
    r0_2 = Extract_NEON<2>(r0_2);
    r0_3 = Extract_NEON<3>(r0_3);

    r1_1 = Extract_NEON<1>(r1_1);
    r1_2 = Extract_NEON<2>(r1_2);
    r1_3 = Extract_NEON<3>(r1_3);

    r2_1 = Extract_NEON<1>(r2_1);
    r2_2 = Extract_NEON<2>(r2_2);
    r2_3 = Extract_NEON<3>(r2_3);

    r3_1 = Extract_NEON<1>(r3_1);
    r3_2 = Extract_NEON<2>(r3_2);
    r3_3 = Extract_NEON<3>(r3_3);

    NEON_QUARTERROUND(r0_0, r0_1, r0_2, r0_3);
    NEON_QUARTERROUND(r1_0, r1_1, r1_2, r1_3);
    NEON_QUARTERROUND(r2_0, r2_1, r2_2, r2_3);
    NEON_QUARTERROUND(r3_0, r3_1, r3_2, r3_3);

    r0_1 = Extract_NEON<3>(r0_1);
    r0_2 = Extract_NEON<2>(r0_2);
    r0_3 = Extract_NEON<1>(r0_3);

    r1_1 = Extract_NEON<3>(r1_1);
    r1_2 = Extract_NEON<2>(r1_2);
    r1_3 = Extract_NEON<1>(r1_3);

    r2_1 = Extract_NEON<3>(r2_1);
    r2_2 = Extract_NEON<2>(r2_2);
    r2_3 = Extract_NEON<1>(r2_3);

    r3_1 = Extract_NEON<3>(r3_1);
    r3_2 = Extract_NEON<2>(r3_2);
    r3_3 = Extract_NEON<1>(r3_3);
  }

  r0_0 = vaddq_u32(r0_0, state0);
  r0_1 = vaddq_u32(r0_1, state1);
  r0_2 = vaddq_u32(r0_2, state2);
  r0_3 = vaddq_u32(r0_3, state3);

  r1_0 = vaddq_u32(r1_0, state0);
  r1_1 = vaddq_u32(r1_1, state1);
  r1_2 = vaddq_u32(r1_2, state2);
  r1_3 = vaddq_u32(r1_3, state3);
  r1_3 = Add64_NEON(r1_3, CTR1);

  r2_0 = vaddq_u32(r2_0, state0);
  r2_1 = vaddq_u32(r2_1, state1);
  r2_2 = vaddq_u32(r2_2, state2);
  r2_3 = vaddq_u32(r2_3, state3);
  r2_3 = Add64_NEON(r2_3, CTR2);

  r3_0 = vaddq_u32(r3_0, state0);
  r3_1 = vaddq_u32(r3_1, state1);
  r3_2 = vaddq_u32(r3_2, state2);
  r3_3 = vaddq_u32(r3_3, state3);
  r3_3 = Add64_NEON(r3_3, CTR3);

  if (input)
  {
    r0_0 = veorq_u32(vld1q_u32((const UInt32 *)(input + 0*16)), r0_0);
    r0_1 = veorq_u32(vld1q_u32((const UInt32 *)(input + 1*16)), r0_1);
    r0_2 = veorq_u32(vld1q_u32((const UInt32 *)(input + 2*16)), r0_2);
    r0_3 = veorq_u32(vld1q_u32((const UInt32 *)(input + 3*16)), r0_3);
  }

  vst1q_u32((UInt32 *)(output + 0*16), r0_0);
  vst1q_u32((UInt32 *)(output + 1*16), r0_1);
  vst1q_u32((UInt32 *)(output + 2*16), r0_2);
  vst1q_u32((UInt32 *)(output + 3*16), r0_3);

  if (input)
  {
    r1_0 = veorq_u32(vld1q_u32((const UInt32 *)(input + 4*16)), r1_0);
    r1_1 = veorq_u32(vld1q_u32((const UInt32 *)(input + 5*16)), r1_1);
    r1_2 = veorq_u32(vld1q_u32((const UInt32 *)(input + 6*16)), r1_2);
    r1_3 = veorq_u32(vld1q_u32((const UInt32 *)(input + 7*16)), r1_3);
  }

  vst1q_u32((UInt32 *)(output + 4*16), r1_0);
  vst1q_u32((UInt32 *)(output + 5*16), r1_1);
  vst1q_u32((UInt32 *)(output + 6*16), r1_2);
  vst1q_u32((UInt32 *)(output + 7*16), r1_3);

  if (input)
  {
    r2_0 = veorq_u32(vld1q_u32((const UInt32 *)(input + 8*16)), r2_0);
    r2_1 = veorq_u32(vld1q_u32((const UInt32 *)(input + 9*16)), r2_1);
    r2_2 = veorq_u32(vld1q_u32((const UInt32 *)(input + 10*16)), r2_2);
    r2_3 = veorq_u32(vld1q_u32((const UInt32 *)(input + 11*16)), r2_3);
  }

  vst1q_u32((UInt32 *)(output + 8*16), r2_0);
  vst1q_u32((UInt32 *)(output + 9*16), r2_1);
  vst1q_u32((UInt32 *)(output + 10*16), r2_2);
  vst1q_u32((UInt32 *)(output + 11*16), r2_3);

  if (input)
  {
    r3_0 = veorq_u32(vld1q_u32((const UInt32 *)(input + 12*16)), r3_0);
    r3_1 = veorq_u32(vld1q_u32((const UInt32 *)(input + 13*16)), r3_1);
    r3_2 = veorq_u32(vld1q_u32((const UInt32 *)(input + 14*16)), r3_2);
    r3_3 = veorq_u32(vld1q_u32((const UInt32 *)(input + 15*16)), r3_3);
  }

  vst1q_u32((UInt32 *)(output + 12*16), r3_0);
  vst1q_u32((UInt32 *)(output + 13*16), r3_1);
  vst1q_u32((UInt32 *)(output + 14*16), r3_2);
  vst1q_u32((UInt32 *)(output + 15*16), r3_3);
}

}

static bool g_NEONEnabled = false;
static bool g_SIMDARMInitialized = false;

static void InitSIMD()
{
  if (g_SIMDARMInitialized)
    return;
  g_SIMDARMInitialized = true;
  g_NEONEnabled = CPU_IsSupported_NEON() != 0;
}

#endif

#endif