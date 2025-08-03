/* byte_order.c - byte order related platform dependent routines,
 *
 * Copyright (c) 2008, Aleksey Kravchenko <rhash.admin@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE  INCLUDING ALL IMPLIED WARRANTIES OF  MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT,  OR CONSEQUENTIAL DAMAGES  OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,  NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF  OR IN CONNECTION  WITH THE USE  OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include "byte_order.h"
#include <string.h>

#ifndef rhash_ctz

#  if _MSC_VER >= 1300 && (_M_IX86 || _M_AMD64 || _M_IA64) /* if MSVC++ >= 2002 on x86/x64 */
#  include <intrin.h>
#  pragma intrinsic(_BitScanForward)

/**
 * Returns index of the trailing bit of x.
 *
 * @param x the number to process
 * @return zero-based index of the trailing bit
 */
unsigned rhash_ctz(unsigned x)
{
	unsigned long index;
	unsigned char isNonzero = _BitScanForward(&index, x); /* MSVC intrinsic */
	return (isNonzero ? (unsigned)index : 0);
}
#  else /* _MSC_VER >= 1300... */

/**
 * Returns index of the least significant set bit in a 32-bit number.
 * This operation is also known as Count Trailing Zeros (CTZ).
 *
 * The function is a portable, branch-free equivalent of GCC's __builtin_ctz(),
 * using a De Bruijn sequence for constant-time lookup.
 *
 * @param x 32-bit unsigned integer to analyze (must not be zero)
 * @return zero-based index of the least significant set bit (0 to 31)
 *
 * @note Undefined behavior when `x == 0`. The current implementation
 *       returns 0, but this value must not be relied upon.
 */
unsigned rhash_ctz(unsigned x)
{
	/* array for conversion to bit position */
	static unsigned char bit_pos[32] =  {
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};

	/* The De Bruijn bit-scan was devised in 1997, according to Donald Knuth
	 * by Martin Lauter. The constant 0x077CB531UL is a De Bruijn sequence,
	 * which produces a unique pattern of bits into the high 5 bits for each
	 * possible bit position that it is multiplied against.
	 * See http://graphics.stanford.edu/~seander/bithacks.html
	 * and http://chessprogramming.wikispaces.com/BitScan */
	return (unsigned)bit_pos[((uint32_t)((x & -x) * 0x077CB531U)) >> 27];
}
#  endif /* _MSC_VER >= 1300... */
#endif /* rhash_ctz */

#ifndef rhash_ctz64
/**
 * Returns the zero-based index of the least significant set bit in a 64-bit number.
 * This operation is also known as Count Trailing Zeros (CTZ).
 *
 * The function is a portable, branch-free equivalent of GCC's __builtin_ctzll().
 * Uses a 32-bit optimized implementation with magic constant `0x78291ACF`,
 * based on Matt Taylor's original algorithm (2003).
 *
 * @param x 64-bit unsigned integer to analyze (must not be zero)
 * @return zero-based index of the least significant set bit (0 to 63)
 *
 * @note Undefined behavior when `x == 0`. The current implementation
 *       returns 63, but this value must not be relied upon.
 * @see rhash_ctz() for 32-bit version.
 */
unsigned rhash_ctz64(uint64_t x)
{
	/* lookup table mapping hash values to bit position */
	static unsigned char bit_pos[64] =  {
		63, 30,  3, 32, 59, 14, 11, 33, 60, 24, 50,  9, 55, 19, 21, 34,
		61, 29,  2, 53, 51, 23, 41, 18, 56, 28,  1, 43, 46, 27,  0, 35,
		62, 31, 58,  4,  5, 49, 54,  6, 15, 52, 12, 40,  7, 42, 45, 16,
		25, 57, 48, 13, 10, 39,  8, 44, 20, 47, 38, 22, 17, 37, 36, 26
	};
	/* transform 0b01000 -> 0b01111 (isolate least significant bit) */
	x ^= x - 1;
	/* fold 64-bit value to 32-bit to be efficient on 32-bit systems */
	uint32_t folded = (uint32_t)((x >> 32) ^ x);
	/* Use Matt Taylor's multiplication trick (2003):
	 * - multiply by (specially chosen) magic constant 0x78291ACF
	 * - use top 6 bits of result (>>26) as table index
	 * Original discussion:
	 * https://groups.google.com/g/comp.lang.asm.x86/c/3pVGzQGb1ys/m/fPpKBKNi848J
	 * https://groups.google.com/g/comp.lang.asm.x86/c/3pVGzQGb1ys/m/230qffQJYvQJ
	 */
	return bit_pos[folded * 0x78291ACF >> 26];
}
#endif /* rhash_ctz64 */

#ifndef rhash_popcount
/**
 * Returns the number of 1-bits in x.
 *
 * @param x the value to process
 * @return the number of 1-bits
 */
unsigned rhash_popcount(unsigned x)
{
	x -= (x >>1) & 0x55555555;
	x = ((x >> 2) & 0x33333333) + (x & 0x33333333);
	x = ((x >> 4) + x) & 0x0f0f0f0f;
	return (x * 0x01010101) >> 24;
}
#endif /* rhash_popcount */

/**
 * Copy a memory block with simultaneous exchanging byte order.
 * The byte order is changed from little-endian 32-bit integers
 * to big-endian (or vice-versa).
 *
 * @param to the pointer where to copy memory block
 * @param index the index to start writing from
 * @param from  the source block to copy
 * @param length length of the memory block
 */
void rhash_swap_copy_str_to_u32(void* to, int index, const void* from, size_t length)
{
	/* if all pointers and length are 32-bits aligned */
	if ( 0 == (( (uintptr_t)to | (uintptr_t)from | (uintptr_t)index | length ) & 3) ) {
		/* copy memory as 32-bit words */
		const uint32_t* src = (const uint32_t*)from;
		const uint32_t* end = (const uint32_t*)((const char*)src + length);
		uint32_t* dst = (uint32_t*)((char*)to + index);
		for (; src < end; dst++, src++)
			*dst = bswap_32(*src);
	} else {
		const char* src = (const char*)from;
		for (length += index; (size_t)index < length; index++)
			((char*)to)[index ^ 3] = *(src++);
	}
}

/**
 * Fill a memory block by a character with changing byte order.
 * The byte order is changed from little-endian 32-bit integers
 * to big-endian (or vice-versa).
 *
 * @param to the pointer where to copy memory block
 * @param index the index to start writing from
 * @param c the character to fill the block with
 * @param length length of the memory block
 */
void rhash_swap_memset_to_u32(void* to, int index, int c, size_t length)
{
	const size_t end = length + (size_t)index;
	for (; (index & 3) && (size_t)index < end; index++)
		((char*)to)[index ^ 3] = (char)c;
	length = end - (size_t)index;
	memset((char*)to + index, c, length & ~3);
	for (; (size_t)index < end; index++)
		((char*)to)[index ^ 3] = (char)c;
}

/**
 * Copy a memory block with changed byte order.
 * The byte order is changed from little-endian 64-bit integers
 * to big-endian (or vice-versa).
 *
 * @param to     the pointer where to copy memory block
 * @param index  the index to start writing from
 * @param from   the source block to copy
 * @param length length of the memory block
 */
void rhash_swap_copy_str_to_u64(void* to, int index, const void* from, size_t length)
{
	/* if all pointers and length are 64-bits aligned */
	if ( 0 == (( (uintptr_t)to | (uintptr_t)from | (uintptr_t)index | length ) & 7) ) {
		/* copy aligned memory block as 64-bit integers */
		const uint64_t* src = (const uint64_t*)from;
		const uint64_t* end = (const uint64_t*)((const char*)src + length);
		uint64_t* dst = (uint64_t*)((char*)to + index);
		while (src < end) *(dst++) = bswap_64( *(src++) );
	} else {
		const char* src = (const char*)from;
		for (length += index; (size_t)index < length; index++) ((char*)to)[index ^ 7] = *(src++);
	}
}

/**
 * Copy data from a sequence of 64-bit words to a binary string of given length,
 * while changing byte order.
 *
 * @param to     the binary string to receive data
 * @param from   the source sequence of 64-bit words
 * @param length the size in bytes of the data being copied
 */
void rhash_swap_copy_u64_to_str(void* to, const void* from, size_t length)
{
	/* if all pointers and length are 64-bits aligned */
	if ( 0 == (( (uintptr_t)to | (uintptr_t)from | length ) & 7) ) {
		/* copy aligned memory block as 64-bit integers */
		const uint64_t* src = (const uint64_t*)from;
		const uint64_t* end = (const uint64_t*)((const char*)src + length);
		uint64_t* dst = (uint64_t*)to;
		while (src < end) *(dst++) = bswap_64( *(src++) );
	} else {
		size_t index;
		char* dst = (char*)to;
		for (index = 0; index < length; index++) *(dst++) = ((char*)from)[index ^ 7];
	}
}

/**
 * Exchange byte order in the given array of 32-bit integers.
 *
 * @param arr    the array to process
 * @param length array length
 */
void rhash_u32_mem_swap(unsigned* arr, int length)
{
	unsigned* end = arr + length;
	for (; arr < end; arr++) {
		*arr = bswap_32(*arr);
	}
}

#if !defined(has_cpu_feature)
# if defined(HAS_GCC_INTEL_CPUID)
#  include <cpuid.h>
#  define RHASH_CPUID(id, regs) \
	__get_cpuid(id, &(regs[0]), &(regs[1]), &(regs[2]), &(regs[3]));
#  if HAS_GNUC(6, 3)
#   define RHASH_CPUIDEX(id, sub_id, regs) \
	__get_cpuid_count(id, sub_id, &regs[0], &regs[1], &regs[2], &regs[3]);
#  endif
# elif defined(HAS_MSVC_INTEL_CPUID)
#  define RHASH_CPUID(id, regs) __cpuid((int*)regs, id)
#  if _MSC_VER >= 1600
#   define RHASH_CPUIDEX(id, sub_id, regs) __cpuidex((int*)regs, id, sub_id);
#  endif
# else
#  error "Unsupported platform"
#endif /* HAS_GCC_INTEL_CPUID */

static uint64_t get_cpuid_features(void)
{
	uint32_t cpu_info[4] = {0};
	uint64_t result = 0;
	/* Request basic CPU functions */
	RHASH_CPUID(1, cpu_info);
	/* Store features, but clear bit 29 to store SHANI bit later */
	result = ((((uint64_t)cpu_info[2]) << 32) ^
		(cpu_info[3] & ~(1 << 29)));
#ifdef RHASH_CPUIDEX
	/* Check if CPUID requests for feature_id >= 7 are supported */
	RHASH_CPUID(0, cpu_info);
	if (cpu_info[0] >= 7)
	{
		/* Request CPUID AX=7 CX=0 to get SHANI bit */
		RHASH_CPUIDEX(7, 0, cpu_info);
		result |= (cpu_info[1] & (1 << 29));
	}
#endif
	return result;
}

int has_cpu_feature(unsigned feature_bit)
{
	static uint64_t features;
	const uint64_t feature = I64(1) << feature_bit;
	if (!features)
		features = (get_cpuid_features() | 1);
	return !!(features & feature);
}
#endif /* has_cpu_feature */
