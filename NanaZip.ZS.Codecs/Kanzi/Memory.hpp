/*
Copyright 2011-2026 Frederic Langlet
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
you may obtain a copy of the License at

                http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#ifndef knz_Memory
#define knz_Memory

#if __cplusplus >= 202002L
    #include <bit>
#endif
#include <cstring>
#include "types.hpp"

#if defined(USE_INTRINSICS)
    #if defined(__ARM_NEON) || defined(__aarch64__)
        #include <arm_neon.h>
    #elif defined(__AVX512F__) || defined(__AVX2__) || defined(__SSE2__)
        #include <immintrin.h>
    #endif
#endif


namespace kanzi {

// Prefetch helpers

static KANZI_ALWAYS_INLINE void prefetchRead(const void* ptr) {
#if defined(__GNUG__) || defined(__clang__)
    __builtin_prefetch(ptr, 0, 1);
#elif defined(__x86_64__) || defined(_M_AMD64)
    _mm_prefetch((const char*)ptr, _MM_HINT_T0);
#elif defined(_M_ARM)
    __prefetch(ptr);
#elif defined(_M_ARM64)
    __prefetch2(ptr, 1);
#endif
}

static KANZI_ALWAYS_INLINE void prefetchWrite(const void* ptr) {
#if defined(__GNUG__) || defined(__clang__)
    __builtin_prefetch(ptr, 1, 1);
#elif defined(__x86_64__) || defined(_M_AMD64)
    _mm_prefetch((const char*)ptr, _MM_HINT_T0);
#elif defined(_M_ARM)
    __prefetchw(ptr);
#elif defined(_M_ARM64)
    __prefetch2(ptr, 17);
#endif
}

// Byte-swap helpers

static KANZI_ALWAYS_INLINE uint16 knz_bswap16(uint16 x) {
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 5)
    return __builtin_bswap16(x);
#elif defined(_MSC_VER)
    return _byteswap_ushort(x);
#else
    return (uint16)((x >> 8) | (x << 8));
#endif
}

static KANZI_ALWAYS_INLINE uint32 knz_bswap32(uint32 x) {
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 5)
    return __builtin_bswap32(x);
#elif defined(_MSC_VER)
    return _byteswap_ulong(x);
#else
    return ((x >> 24) |
           ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) |
           (x << 24));
#endif
}

static KANZI_ALWAYS_INLINE uint64 knz_bswap64(uint64 x) {
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 5)
    return __builtin_bswap64(x);
#elif defined(_MSC_VER)
    return _byteswap_uint64(x);
#else
    x = ((x & 0xFFFFFFFF00000000ull) >> 32) |
        ((x & 0xFFFFFFFFull) << 32);
    x = ((x & 0xFFFF0000FFFF0000ull) >> 16) |
        ((x & 0xFFFF0000FFFFull) << 16);
    x = ((x & 0xFF00FF00FF00FF00ull) >> 8) |
        ((x & 0xFF00FF00FF00FFull) << 8);
    return x;
#endif
}

#if defined(USE_INTRINSICS) && (defined(__ARM_NEON) || defined(__aarch64__))

    static KANZI_ALWAYS_INLINE bool memEq4(const byte* x, const byte* y)
    {
        const uint32x2_t a = vld1_dup_u32(reinterpret_cast<const uint32_t*>(x));
        const uint32x2_t b = vld1_dup_u32(reinterpret_cast<const uint32_t*>(y));
        return vget_lane_u32(vceq_u32(a, b), 0) != 0;
    }

    static KANZI_ALWAYS_INLINE bool memEq8(const byte* x, const byte* y)
    {
#if defined(__aarch64__)
        const uint64x1_t a = vld1_u64(reinterpret_cast<const uint64_t*>(x));
        const uint64x1_t b = vld1_u64(reinterpret_cast<const uint64_t*>(y));
        return vget_lane_u64(vceq_u64(a, b), 0) != 0;
#else
        const uint32x2_t a = vld1_u32(reinterpret_cast<const uint32_t*>(x));
        const uint32x2_t b = vld1_u32(reinterpret_cast<const uint32_t*>(y));
        const uint32x2_t eq = vceq_u32(a, b);
        return (vget_lane_u32(eq, 0) != 0) && (vget_lane_u32(eq, 1) != 0);
#endif
    }

    static KANZI_ALWAYS_INLINE void memCp8(byte* dst, const byte* src)
    {
        vst1_u8(reinterpret_cast<uint8_t*>(dst), vld1_u8(reinterpret_cast<const uint8_t*>(src)));
    }

    static KANZI_ALWAYS_INLINE void memCp16(byte* dst, const byte* src)
    {
        vst1q_u8(reinterpret_cast<uint8_t*>(dst), vld1q_u8(reinterpret_cast<const uint8_t*>(src)));
    }

#elif defined(USE_INTRINSICS) && defined(__AVX512F__)

    static KANZI_ALWAYS_INLINE bool memEq4(const byte* x, const byte* y)
    {
        const __mmask16 mask = 0x0001;
        const __m512i a = _mm512_maskz_loadu_epi32(mask, x);
        const __m512i b = _mm512_maskz_loadu_epi32(mask, y);
        return _mm512_mask_cmpeq_epi32_mask(mask, a, b) == mask;
    }

    static KANZI_ALWAYS_INLINE bool memEq8(const byte* x, const byte* y)
    {
        const __mmask8 mask = 0x01;
        const __m512i a = _mm512_maskz_loadu_epi64(mask, x);
        const __m512i b = _mm512_maskz_loadu_epi64(mask, y);
        return _mm512_mask_cmpeq_epi64_mask(mask, a, b) == mask;
    }

    static KANZI_ALWAYS_INLINE void memCp8(byte* dst, const byte* src)
    {
        const __m512i value = _mm512_maskz_loadu_epi64(0x01, src);
        _mm512_mask_storeu_epi64(dst, 0x01, value);
    }

    static KANZI_ALWAYS_INLINE void memCp16(byte* dst, const byte* src)
    {
        const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), value);
    }

#elif defined(USE_INTRINSICS) && defined(__AVX2__)

    static KANZI_ALWAYS_INLINE bool memEq4(const byte* x, const byte* y)
    {
        const __m256i mask = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, -1);
        const __m256i a = _mm256_maskload_epi32(reinterpret_cast<const int*>(x), mask);
        const __m256i b = _mm256_maskload_epi32(reinterpret_cast<const int*>(y), mask);
        return (_mm256_movemask_epi8(_mm256_cmpeq_epi32(a, b)) & 0x0F) == 0x0F;
    }

    static KANZI_ALWAYS_INLINE bool memEq8(const byte* x, const byte* y)
    {
        const __m256i mask = _mm256_set_epi64x(0, 0, 0, -1);
        const __m256i a = _mm256_maskload_epi64(reinterpret_cast<const long long*>(x), mask);
        const __m256i b = _mm256_maskload_epi64(reinterpret_cast<const long long*>(y), mask);
        return (_mm256_movemask_epi8(_mm256_cmpeq_epi64(a, b)) & 0xFF) == 0xFF;
    }

    static KANZI_ALWAYS_INLINE void memCp8(byte* dst, const byte* src)
    {
        const __m256i mask = _mm256_set_epi64x(0, 0, 0, -1);
        const __m256i value = _mm256_maskload_epi64(reinterpret_cast<const long long*>(src), mask);
        _mm256_maskstore_epi64(reinterpret_cast<long long*>(dst), mask, value);
    }

    static KANZI_ALWAYS_INLINE void memCp16(byte* dst, const byte* src)
    {
        const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), value);
    }

#elif defined(USE_INTRINSICS) && defined(__SSE2__)

    static KANZI_ALWAYS_INLINE bool memEq4(const byte* x, const byte* y)
    {
        const __m128i va = _mm_loadu_si32(x);
        const __m128i vb = _mm_loadu_si32(y);
        return _mm_cvtsi128_si32(_mm_cmpeq_epi32(va, vb)) != 0;
    }

    static KANZI_ALWAYS_INLINE bool memEq8(const byte* x, const byte* y)
    {
        const __m128i a = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(x));
        const __m128i b = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(y));
        return _mm_movemask_epi8(_mm_cmpeq_epi8(a, b)) == 0xFF;
    }

    static KANZI_ALWAYS_INLINE void memCp8(byte* dst, const byte* src)
    {
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst),
                         _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src)));
    }

    static KANZI_ALWAYS_INLINE void memCp16(byte* dst, const byte* src)
    {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst),
                         _mm_loadu_si128(reinterpret_cast<const __m128i*>(src)));
    }

#else

    static KANZI_ALWAYS_INLINE bool memEq4(const byte* x, const byte* y)
    {
        return std::memcmp(x, y, 4) == 0;
    }

    static KANZI_ALWAYS_INLINE bool memEq8(const byte* x, const byte* y)
    {
        return std::memcmp(x, y, 8) == 0;
    }

    static KANZI_ALWAYS_INLINE void memCp8(byte* dst, const byte* src)
    {
        memcpy(dst, src, 8);
    }

    static KANZI_ALWAYS_INLINE void memCp16(byte* dst, const byte* src)
    {
        memcpy(dst, src, 16);
    }

#endif

static KANZI_ALWAYS_INLINE void memXor8(byte* dst, const byte* x, const byte* y)
{
#if defined(USE_INTRINSICS) && (defined(__ARM_NEON) || defined(__aarch64__))
    vst1_u8(reinterpret_cast<uint8_t*>(dst),
            veor_u8(vld1_u8(reinterpret_cast<const uint8_t*>(x)),
                    vld1_u8(reinterpret_cast<const uint8_t*>(y))));
#elif defined(USE_INTRINSICS) && defined(__AVX512F__)
    const __mmask8 mask = 0x01;
    const __m512i a = _mm512_maskz_loadu_epi64(mask, x);
    const __m512i b = _mm512_maskz_loadu_epi64(mask, y);
    _mm512_mask_storeu_epi64(dst, mask, _mm512_xor_si512(a, b));
#elif defined(USE_INTRINSICS) && defined(__AVX2__)
    const __m256i mask = _mm256_set_epi64x(0, 0, 0, -1);
    const __m256i a = _mm256_maskload_epi64(reinterpret_cast<const long long*>(x), mask);
    const __m256i b = _mm256_maskload_epi64(reinterpret_cast<const long long*>(y), mask);
    _mm256_maskstore_epi64(reinterpret_cast<long long*>(dst), mask, _mm256_xor_si256(a, b));
#elif defined(USE_INTRINSICS) && defined(__SSE2__)
    const __m128i a = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(x));
    const __m128i b = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(y));
    _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), _mm_xor_si128(a, b));
#else
    uint64 a;
    uint64 b;
    memcpy(&a, x, sizeof(uint64));
    memcpy(&b, y, sizeof(uint64));
    a ^= b;
    memcpy(dst, &a, sizeof(uint64));
#endif
}

static KANZI_ALWAYS_INLINE void memXor16(byte* dst, const byte* x, const byte* y)
{
#if defined(USE_INTRINSICS) && (defined(__ARM_NEON) || defined(__aarch64__))
    vst1q_u8(reinterpret_cast<uint8_t*>(dst),
             veorq_u8(vld1q_u8(reinterpret_cast<const uint8_t*>(x)),
                      vld1q_u8(reinterpret_cast<const uint8_t*>(y))));
#elif defined(USE_INTRINSICS) && defined(__AVX512F__)
    const __mmask8 mask = 0x03;
    const __m512i a = _mm512_maskz_loadu_epi64(mask, x);
    const __m512i b = _mm512_maskz_loadu_epi64(mask, y);
    _mm512_mask_storeu_epi64(dst, mask, _mm512_xor_si512(a, b));
#elif defined(USE_INTRINSICS) && defined(__AVX2__)
    const __m256i mask = _mm256_set_epi64x(0, 0, -1, -1);
    const __m256i a = _mm256_maskload_epi64(reinterpret_cast<const long long*>(x), mask);
    const __m256i b = _mm256_maskload_epi64(reinterpret_cast<const long long*>(y), mask);
    _mm256_maskstore_epi64(reinterpret_cast<long long*>(dst), mask, _mm256_xor_si256(a, b));
#elif defined(USE_INTRINSICS) && defined(__SSE2__)
    const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(x));
    const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(y));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), _mm_xor_si128(a, b));
#else
    memXor8(dst, x, y);
    memXor8(dst + 8, x + 8, y + 8);
#endif
}

#define KANZI_MEM_EQ4(x, y) (::kanzi::memEq4((x), (y)))
#define KANZI_MEM_EQ8(x, y) (::kanzi::memEq8((x), (y)))
#define KANZI_MEM_CP8(dst, src) (::kanzi::memCp8((dst), (src)))
#define KANZI_MEM_CP16(dst, src) (::kanzi::memCp16((dst), (src)))
#define KANZI_MEM_XOR8(dst, x, y) (::kanzi::memXor8((dst), (x), (y)))
#define KANZI_MEM_XOR16(dst, x, y) (::kanzi::memXor16((dst), (x), (y)))

// Detect host endianness

#if __cplusplus >= 202002L
    static_assert(std::endian::native == std::endian::little ||
                  std::endian::native == std::endian::big,
                  "Kanzi supports only little- and big-endian hosts");
#else
    #ifndef HOST_IS_LITTLE
        #if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(__BIG_ENDIAN__)
            #define HOST_IS_LITTLE 0
        #else
            #define HOST_IS_LITTLE 1
        #endif
    #endif
#endif


template <typename T, bool SourceIsBigEndian>
static KANZI_ALWAYS_INLINE T readEndian(const byte* p) {
    T val;

    memcpy(&val, p, sizeof(T));

    // Swap if host and source endianness differ
#if __cplusplus >= 202002L
    if constexpr (SourceIsBigEndian !=
                  (std::endian::native == std::endian::big)) {
#elif HOST_IS_LITTLE
    if (SourceIsBigEndian) {
#else
    if (!SourceIsBigEndian) {
#endif
        if (sizeof(T) == 2)
            val = (T)knz_bswap16((uint16)val);
        else if (sizeof(T) == 4)
            val = (T)knz_bswap32((uint32)val);
        else if (sizeof(T) == 8)
            val = (T)knz_bswap64((uint64)val);
    }

    return val;
}

template <typename T, bool TargetIsBigEndian>
static KANZI_ALWAYS_INLINE void writeEndian(byte* p, T val) {

#if __cplusplus >= 202002L
    if constexpr (TargetIsBigEndian !=
                  (std::endian::native == std::endian::big)) {
#elif HOST_IS_LITTLE
    if (TargetIsBigEndian) {
#else
    if (!TargetIsBigEndian) {
#endif
        if (sizeof(T) == 2)
            val = (T)knz_bswap16((uint16)val);
        else if (sizeof(T) == 4)
            val = (T)knz_bswap32((uint32)val);
        else if (sizeof(T) == 8)
            val = (T)knz_bswap64((uint64)val);
    }

    memcpy(p, &val, sizeof(T));
}


class BigEndian {
public:
    static int64 readLong64(const byte* p) { return readEndian<int64, true>(p); }
    static int32 readInt32(const byte* p)  { return readEndian<int32, true>(p); }
    static int16 readInt16(const byte* p)  { return readEndian<int16, true>(p); }

    static void writeLong64(byte* p, int64 v) { writeEndian<int64, true>(p, v); }
    static void writeInt32(byte* p, int32 v)  { writeEndian<int32, true>(p, v); }
    static void writeInt16(byte* p, int16 v)  { writeEndian<int16, true>(p, v); }
};

class LittleEndian {
public:
    static int64 readLong64(const byte* p) { return readEndian<int64, false>(p); }
    static int32 readInt32(const byte* p)  { return readEndian<int32, false>(p); }
    static int16 readInt16(const byte* p)  { return readEndian<int16, false>(p); }

    static void writeLong64(byte* p, int64 v) { writeEndian<int64, false>(p, v); }
    static void writeInt32(byte* p, int32 v)  { writeEndian<int32, false>(p, v); }
    static void writeInt16(byte* p, int16 v)  { writeEndian<int16, false>(p, v); }
};

} // namespace kanzi
#endif
