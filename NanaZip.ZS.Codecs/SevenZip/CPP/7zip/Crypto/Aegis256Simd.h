// Aegis256Simd.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+
//
// Hardware AES round for AEGIS-256:
//   x86/x64  : AES-NI      (aesenc)
//   ARM/ARM64: ARMv8 Crypto Extensions (aese + aesmc)
// AEGIS-256 uses only the AES encryption round, so no decryption keys are needed.

#ifndef ZIP7_INC_CRYPTO_AEGIS256_SIMD_H
#define ZIP7_INC_CRYPTO_AEGIS256_SIMD_H

#include "Aegis256.h"

#include "../../../C/CpuArch.h"

#ifdef MY_CPU_X86_OR_AMD64

  #if defined(__INTEL_COMPILER)
    #if (__INTEL_COMPILER >= 1110)
      #define AEGIS256_USE_HW_AES
    #endif
  #elif defined(Z7_CLANG_VERSION) && (Z7_CLANG_VERSION >= 30800) \
     || defined(Z7_GCC_VERSION)   && (Z7_GCC_VERSION   >= 40400)
      #define AEGIS256_USE_HW_AES
  #elif defined(_MSC_VER)
      #define AEGIS256_USE_HW_AES
  #endif

  #ifdef AEGIS256_USE_HW_AES
    #if defined(__clang__) || defined(__GNUC__)
      #if !defined(__AES__)
        #define AEGIS256_ATTRIB_AES __attribute__((__target__("aes")))
      #endif
    #endif
    #include <wmmintrin.h>
  #endif

#elif defined(MY_CPU_ARM_OR_ARM64) && defined(MY_CPU_LE)

  #if   defined(__ARM_FEATURE_AES) \
     || defined(__ARM_FEATURE_CRYPTO)
    #define AEGIS256_USE_HW_AES
  #else
    #if  defined(MY_CPU_ARM64) \
      || defined(__ARM_ARCH) && (__ARM_ARCH >= 4) \
      || defined(Z7_MSC_VER_ORIGINAL)
    #if  defined(__ARM_FP) && \
          (   defined(Z7_CLANG_VERSION) && (Z7_CLANG_VERSION >= 30800) \
           || defined(__GNUC__) && (__GNUC__ >= 6) \
          ) \
      || defined(Z7_MSC_VER_ORIGINAL) && (_MSC_VER >= 1910)
    #if  defined(MY_CPU_ARM64) \
      || !defined(Z7_CLANG_VERSION) \
      || defined(__ARM_NEON) && \
          (Z7_CLANG_VERSION < 170000 || \
           Z7_CLANG_VERSION > 170001)
      #define AEGIS256_USE_HW_AES
    #endif
    #endif
    #endif
  #endif

  #ifdef AEGIS256_USE_HW_AES

    #if defined(__clang__) || defined(__GNUC__)
      #if !defined(__ARM_FEATURE_AES) && \
          !defined(__ARM_FEATURE_CRYPTO)
        #ifdef MY_CPU_ARM64
          #if defined(__clang__)
            #define AEGIS256_ATTRIB_AES __attribute__((__target__("crypto")))
          #else
            #define AEGIS256_ATTRIB_AES __attribute__((__target__("+crypto")))
          #endif
        #else
          #if defined(__clang__)
            #define AEGIS256_ATTRIB_AES __attribute__((__target__("armv8-a,aes")))
          #else
            #define AEGIS256_ATTRIB_AES __attribute__((__target__("fpu=crypto-neon-fp-armv8")))
          #endif
        #endif
      #endif
    #else
      // for arm32
      #define _ARM_USE_NEW_NEON_INTRINSICS
    #endif

    #if defined(Z7_MSC_VER_ORIGINAL) && defined(MY_CPU_ARM64)
      #include <arm64_neon.h>
    #else
      // old clang versions require these macros to declare the AES intrinsics
      #if defined(__clang__) && __clang_major__ < 16
        #if !defined(__ARM_FEATURE_AES) && \
            !defined(__ARM_FEATURE_CRYPTO)
          #define Z7_AEGIS256_ARM_CRYPTO_WAS_SET 1
          #define __ARM_FEATURE_CRYPTO 1
        #endif
      #endif

      #if defined(__clang__)
        #if defined(__ARM_ARCH) && __ARM_ARCH < 8
          #undef  __ARM_ARCH
          #define __ARM_ARCH 8
        #endif
      #endif

      #include <arm_neon.h>

      #if defined(Z7_AEGIS256_ARM_CRYPTO_WAS_SET) && \
          defined(__ARM_FEATURE_CRYPTO) && \
          defined(__ARM_FEATURE_AES)
        #undef __ARM_FEATURE_CRYPTO
        #undef __ARM_FEATURE_AES
        #undef Z7_AEGIS256_ARM_CRYPTO_WAS_SET
      #endif
    #endif

  #endif
#endif

#ifdef AEGIS256_NO_HW_AES
  // define AEGIS256_NO_HW_AES to test the portable implementation
  #undef AEGIS256_USE_HW_AES
#endif

#ifndef AEGIS256_ATTRIB_AES
  #define AEGIS256_ATTRIB_AES
#endif

#ifdef AEGIS256_USE_HW_AES

#ifdef MY_CPU_X86_OR_AMD64

  #define AEGIS256_LOAD_128(p)      _mm_loadu_si128((const __m128i *)(const void *)(p))
  #define AEGIS256_STORE_128(p, v)  _mm_storeu_si128((__m128i *)(void *)(p), (v))
  #define AEGIS256_XOR_128(a, b)    _mm_xor_si128((a), (b))
  #define AEGIS256_AND_128(a, b)    _mm_and_si128((a), (b))
  #define AEGIS256_AES_ENC(a, b)    _mm_aesenc_si128((a), (b))
  typedef __m128i Aegis256V128;

#else

  #define AEGIS256_LOAD_128(p)      vld1q_u8((const uint8_t *)(const void *)(p))
  #define AEGIS256_STORE_128(p, v)  vst1q_u8((uint8_t *)(void *)(p), (v))
  #define AEGIS256_XOR_128(a, b)    veorq_u8((a), (b))
  #define AEGIS256_AND_128(a, b)    vandq_u8((a), (b))
  // AESE(a, b) = SubBytes(ShiftRows(a ^ b)), so we get AESRound(a, b)
  #define AEGIS256_AES_ENC(a, b)    veorq_u8(vaesmcq_u8(vaeseq_u8(vmovq_n_u8(0), (a))), (b))
  typedef uint8x16_t Aegis256V128;

#endif


static void Z7_FASTCALL Aegis256_UpdateBlock_HW(Byte *state, const Byte *m);
AEGIS256_ATTRIB_AES
static void Z7_FASTCALL Aegis256_UpdateBlock_HW(Byte *state, const Byte *m)
{
  Aegis256V128 s0 = AEGIS256_LOAD_128(state + 0 * 16);
  Aegis256V128 s1 = AEGIS256_LOAD_128(state + 1 * 16);
  Aegis256V128 s2 = AEGIS256_LOAD_128(state + 2 * 16);
  Aegis256V128 s3 = AEGIS256_LOAD_128(state + 3 * 16);
  Aegis256V128 s4 = AEGIS256_LOAD_128(state + 4 * 16);
  Aegis256V128 s5 = AEGIS256_LOAD_128(state + 5 * 16);

  const Aegis256V128 t = s5;
  s5 = AEGIS256_AES_ENC(s4, s5);
  s4 = AEGIS256_AES_ENC(s3, s4);
  s3 = AEGIS256_AES_ENC(s2, s3);
  s2 = AEGIS256_AES_ENC(s1, s2);
  s1 = AEGIS256_AES_ENC(s0, s1);
  s0 = AEGIS256_XOR_128(AEGIS256_AES_ENC(t, s0), AEGIS256_LOAD_128(m));

  AEGIS256_STORE_128(state + 0 * 16, s0);
  AEGIS256_STORE_128(state + 1 * 16, s1);
  AEGIS256_STORE_128(state + 2 * 16, s2);
  AEGIS256_STORE_128(state + 3 * 16, s3);
  AEGIS256_STORE_128(state + 4 * 16, s4);
  AEGIS256_STORE_128(state + 5 * 16, s5);
}


static void Z7_FASTCALL Aegis256_EncryptBlocks_HW(Byte *state, Byte *data, size_t numBlocks);
AEGIS256_ATTRIB_AES
static void Z7_FASTCALL Aegis256_EncryptBlocks_HW(Byte *state, Byte *data, size_t numBlocks)
{
  Aegis256V128 s0 = AEGIS256_LOAD_128(state + 0 * 16);
  Aegis256V128 s1 = AEGIS256_LOAD_128(state + 1 * 16);
  Aegis256V128 s2 = AEGIS256_LOAD_128(state + 2 * 16);
  Aegis256V128 s3 = AEGIS256_LOAD_128(state + 3 * 16);
  Aegis256V128 s4 = AEGIS256_LOAD_128(state + 4 * 16);
  Aegis256V128 s5 = AEGIS256_LOAD_128(state + 5 * 16);

  do
  {
    const Aegis256V128 m = AEGIS256_LOAD_128(data);
    const Aegis256V128 z = AEGIS256_XOR_128(
        AEGIS256_XOR_128(s1, s4),
        AEGIS256_XOR_128(s5, AEGIS256_AND_128(s2, s3)));
    AEGIS256_STORE_128(data, AEGIS256_XOR_128(m, z));

    const Aegis256V128 t = s5;
    s5 = AEGIS256_AES_ENC(s4, s5);
    s4 = AEGIS256_AES_ENC(s3, s4);
    s3 = AEGIS256_AES_ENC(s2, s3);
    s2 = AEGIS256_AES_ENC(s1, s2);
    s1 = AEGIS256_AES_ENC(s0, s1);
    s0 = AEGIS256_XOR_128(AEGIS256_AES_ENC(t, s0), m);

    data += 16;
  }
  while (--numBlocks);

  AEGIS256_STORE_128(state + 0 * 16, s0);
  AEGIS256_STORE_128(state + 1 * 16, s1);
  AEGIS256_STORE_128(state + 2 * 16, s2);
  AEGIS256_STORE_128(state + 3 * 16, s3);
  AEGIS256_STORE_128(state + 4 * 16, s4);
  AEGIS256_STORE_128(state + 5 * 16, s5);
}


static void Z7_FASTCALL Aegis256_DecryptBlocks_HW(Byte *state, Byte *data, size_t numBlocks);
AEGIS256_ATTRIB_AES
static void Z7_FASTCALL Aegis256_DecryptBlocks_HW(Byte *state, Byte *data, size_t numBlocks)
{
  Aegis256V128 s0 = AEGIS256_LOAD_128(state + 0 * 16);
  Aegis256V128 s1 = AEGIS256_LOAD_128(state + 1 * 16);
  Aegis256V128 s2 = AEGIS256_LOAD_128(state + 2 * 16);
  Aegis256V128 s3 = AEGIS256_LOAD_128(state + 3 * 16);
  Aegis256V128 s4 = AEGIS256_LOAD_128(state + 4 * 16);
  Aegis256V128 s5 = AEGIS256_LOAD_128(state + 5 * 16);

  do
  {
    const Aegis256V128 c = AEGIS256_LOAD_128(data);
    const Aegis256V128 z = AEGIS256_XOR_128(
        AEGIS256_XOR_128(s1, s4),
        AEGIS256_XOR_128(s5, AEGIS256_AND_128(s2, s3)));
    const Aegis256V128 m = AEGIS256_XOR_128(c, z);
    AEGIS256_STORE_128(data, m);

    const Aegis256V128 t = s5;
    s5 = AEGIS256_AES_ENC(s4, s5);
    s4 = AEGIS256_AES_ENC(s3, s4);
    s3 = AEGIS256_AES_ENC(s2, s3);
    s2 = AEGIS256_AES_ENC(s1, s2);
    s1 = AEGIS256_AES_ENC(s0, s1);
    s0 = AEGIS256_XOR_128(AEGIS256_AES_ENC(t, s0), m);

    data += 16;
  }
  while (--numBlocks);

  AEGIS256_STORE_128(state + 0 * 16, s0);
  AEGIS256_STORE_128(state + 1 * 16, s1);
  AEGIS256_STORE_128(state + 2 * 16, s2);
  AEGIS256_STORE_128(state + 3 * 16, s3);
  AEGIS256_STORE_128(state + 4 * 16, s4);
  AEGIS256_STORE_128(state + 5 * 16, s5);
}

#endif // AEGIS256_USE_HW_AES

#endif
