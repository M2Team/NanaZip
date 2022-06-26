/* edonr.c - an implementation of EDON-R 256/224/384/512 hash functions
 *
 * Copyright (c) 2011, Aleksey Kravchenko <rhash.admin@gmail.com>
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
 *
 * This implementation is based on the article:
 *    D. Gligoroski, R. S. Odegard, M. Mihova, S. J. Knapskog, ...,
 *    Cryptographic Hash Function EDON-R - Submission to NIST, 2008
 *
 * EDON-R has been designed to be much more efficient than SHA-2
 * cryptographic hash functions, offering the same or better security.
 */

#include <string.h>
#include "byte_order.h"
#include "edonr.h"

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_edonr256_init(edonr_ctx* ctx)
{
	static const unsigned EDONR256_H0[16] = {
		0x40414243, 0x44454647, 0x48494a4b, 0x4c4d4e4f, 0x50515253, 0x54555657,
		0x58595a5b, 0x5c5d5e5f, 0x60616263, 0x64656667, 0x68696a6b, 0x6c6d6e6f,
		0x70717273, 0x74757677, 0x78797a7b, 0x7c7d7e7f
	};

#if FULL_CTX_INITIALIZATION
	memset(ctx, 0, sizeof(*ctx));
#else
	ctx->length = 0;
#endif
	ctx->digest_length = edonr256_hash_size;
	memcpy(ctx->u.data256.hash, EDONR256_H0, sizeof(EDONR256_H0));
}

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_edonr224_init(struct edonr_ctx* ctx)
{
	static const unsigned EDONR224_H0[16] = {
		0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f, 0x10111213, 0x14151617,
		0x18191a1b, 0x1c1d1e1f, 0x20212223, 0x24252627, 0x28292a2b, 0x2c2d2e2f,
		0x30313233, 0x24353637, 0x38393a3b, 0x3c3d3e3f
	};

#if FULL_CTX_INITIALIZATION
	memset(ctx, 0, sizeof(*ctx));
#else
	ctx->length = 0;
#endif
	ctx->digest_length = edonr224_hash_size;
	memcpy(ctx->u.data256.hash, EDONR224_H0, sizeof(EDONR224_H0));
}

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_edonr512_init(edonr_ctx* ctx)
{
	static const uint64_t EDONR512_H0[16] = {
		I64(0x8081828384858687), I64(0x88898a8b8c8d8e8f), I64(0x9091929394959697),
		I64(0x98999a9b9c9d9e9f), I64(0xa0a1a2a3a4a5a6a7), I64(0xa8a9aaabacadaeaf),
		I64(0xb0b1b2b3b4b5b6b7), I64(0xb8b9babbbcbdbebf), I64(0xc0c1c2c3c4c5c6c7),
		I64(0xc8c9cacbcccdcecf), I64(0xd0d1d2d3d4d5d6d7), I64(0xd8d9dadbdcdddedf),
		I64(0xe0e1e2e3e4e5e6e7), I64(0xe8e9eaebecedeeef), I64(0xf0f1f2f3f4f5f6f7),
		I64(0xf8f9fafbfcfdfeff)
	};

#if FULL_CTX_INITIALIZATION
	memset(ctx, 0, sizeof(*ctx));
#else
	ctx->length = 0;
#endif
	ctx->digest_length = edonr512_hash_size;
	memcpy(ctx->u.data512.hash, EDONR512_H0, sizeof(EDONR512_H0));
}

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_edonr384_init(struct edonr_ctx* ctx)
{
	static const uint64_t EDONR384_H0[16] = {
		I64(0x0001020304050607), I64(0x08090a0b0c0d0e0f), I64(0x1011121314151617),
		I64(0x18191a1b1c1d1e1f), I64(0x2021222324252627), I64(0x28292a2b2c2d2e2f),
		I64(0x3031323324353637), I64(0x38393a3b3c3d3e3f), I64(0x4041424344454647),
		I64(0x48494a4b4c4d4e4f), I64(0x5051525354555657), I64(0x58595a5b5c5d5e5f),
		I64(0x6061626364656667), I64(0x68696a6b6c6d6e6f), I64(0x7071727374757677),
		I64(0x78797a7b7c7d7e7f)
	};

#if FULL_CTX_INITIALIZATION
	memset(ctx, 0, sizeof(*ctx));
#else
	ctx->length = 0;
#endif
	ctx->digest_length = edonr384_hash_size;
	memcpy(ctx->u.data512.hash, EDONR384_H0, sizeof(EDONR384_H0));
}

/* Q256 macro, taken from eBASH submission */
#define Q256(x0,x1,x2,x3,x4,x5,x6,x7,y0,y1,y2,y3,y4,y5,y6,y7,z0,z1,z2,z3,z4,z5,z6,z7) \
{\
	/* The First Latin Square\
	0   7   1   3   2   4   6   5\
	4   1   7   6   3   0   5   2\
	7   0   4   2   5   3   1   6\
	1   4   0   5   6   2   7   3\
	2   3   6   7   1   5   0   4\
	5   2   3   1   7   6   4   0\
	3   6   5   0   4   7   2   1\
	6   5   2   4   0   1   3   7\
	*/\
	t8  = x0 + x4; \
	t9  = x1 + x7; \
	t12 = t8 + t9; \
	t10 = x2 + x3; \
	t11 = x5 + x6; \
	t13 = t10 + t11; \
	t0  = 0xaaaaaaaa + t12 + x2; \
	t1  = t12 + x3; \
	t1  = ROTL32((t1), 5); \
	t2  = t12 + x6; \
	t2  = ROTL32((t2), 11); \
	t3  = t13 + x7; \
	t3  = ROTL32((t3), 13); \
	t4  = x1 + t13; \
	t4  = ROTL32((t4), 17); \
	t5  = t8 + t10 + x5; \
	t5  = ROTL32((t5), 19); \
	t6  = x0 + t9 + t11; \
	t6  = ROTL32((t6), 29); \
	t7  = t13 + x4; \
	t7  = ROTL32((t7), 31); \
	\
	t16 = t0 ^ t4; \
	t17 = t1 ^ t7; \
	t18 = t2 ^ t3; \
	t19 = t5 ^ t6; \
	t8  = t3 ^ t19; \
	t9  = t2 ^ t19; \
	t10 = t18 ^ t5; \
	t11 = t16 ^ t1; \
	t12 = t16 ^ t7; \
	t13 = t17 ^ t6; \
	t14 = t18 ^ t4; \
	t15 = t0 ^ t17; \
	\
	/* The Second Orthogonal Latin Square\
	0   4   2   3   1   6   5   7\
	7   6   3   2   5   4   1   0\
	5   3   1   6   0   2   7   4\
	1   0   5   4   3   7   2   6\
	2   1   0   7   4   5   6   3\
	3   5   7   0   6   1   4   2\
	4   7   6   1   2   0   3   5\
	6   2   4   5   7   3   0   1\
	*/\
	t16 = y0  + y1; \
	t17 = y2  + y5; \
	t20 = t16 + t17; \
	t18 = y3  + y4; \
	t22 = t16 + t18; \
	t19 = y6  + y7; \
	t21 = t18 + t19; \
	t23 = t17 + t19; \
	t0  = 0x55555555 + t20 + y7; \
	t1  = t22 + y6; \
	t1  = ROTL32((t1), 3); \
	t2  = t20 + y3; \
	t2  = ROTL32((t2), 7); \
	t3  = y2 + t21; \
	t3  = ROTL32((t3), 11); \
	t4  = t22 + y5; \
	t4  = ROTL32((t4), 17); \
	t5  = t23 + y4; \
	t5  = ROTL32((t5), 19); \
	t6  = y1 + t23; \
	t6  = ROTL32((t6), 23); \
	t7  = y0 + t21; \
	t7  = ROTL32((t7), 29); \
	\
	t16  = t0 ^ t1; \
	t17  = t2 ^ t5; \
	t18  = t3 ^ t4; \
	t19  = t6 ^ t7; \
	z5   = t8  + (t18 ^ t6); \
	z6   = t9  + (t17 ^ t7); \
	z7   = t10 + (t4 ^ t19); \
	z0   = t11 + (t16 ^ t5); \
	z1   = t12 + (t2 ^ t19); \
	z2   = t13 + (t16 ^ t3); \
	z3   = t14 + (t0 ^ t18); \
	z4   = t15 + (t1 ^ t17); \
}

/* Q512 macro, taken from eBASH submission */
#define Q512(x0,x1,x2,x3,x4,x5,x6,x7,y0,y1,y2,y3,y4,y5,y6,y7,z0,z1,z2,z3,z4,z5,z6,z7) \
{\
	/* First Latin Square\
	0   7   1   3   2   4   6   5\
	4   1   7   6   3   0   5   2\
	7   0   4   2   5   3   1   6\
	1   4   0   5   6   2   7   3\
	2   3   6   7   1   5   0   4\
	5   2   3   1   7   6   4   0\
	3   6   5   0   4   7   2   1\
	6   5   2   4   0   1   3   7\
	*/\
	t8  = x0 + x4; \
	t9  = x1 + x7; \
	t12 = t8 + t9; \
	t10 = x2 + x3; \
	t11 = x5 + x6; \
	t13 = t10 + t11; \
	t0  = I64(0xaaaaaaaaaaaaaaaa) + t12 + x2; \
	t1  = t12 + x3; \
	t1  = ROTL64((t1), 5); \
	t2  = t12 + x6; \
	t2  = ROTL64((t2),19); \
	t3  = t13 + x7; \
	t3  = ROTL64((t3),29); \
	t4  = x1 + t13; \
	t4  = ROTL64((t4),31); \
	t5  = t8 + t10 + x5; \
	t5  = ROTL64((t5),41); \
	t6  = x0 + t9 + t11; \
	t6  = ROTL64((t6),57); \
	t7  = t13 + x4; \
	t7  = ROTL64((t7),61); \
	\
	t16 = t0 ^ t4; \
	t17 = t1 ^ t7; \
	t18 = t2 ^ t3; \
	t19 = t5 ^ t6; \
	t8  = t3 ^ t19; \
	t9  = t2 ^ t19; \
	t10 = t18 ^ t5; \
	t11 = t16 ^ t1; \
	t12 = t16 ^ t7; \
	t13 = t17 ^ t6; \
	t14 = t18 ^ t4; \
	t15 = t0 ^ t17; \
	\
	/* Second Orthogonal Latin Square\
	0   4   2   3   1   6   5   7\
	7   6   3   2   5   4   1   0\
	5   3   1   6   0   2   7   4\
	1   0   5   4   3   7   2   6\
	2   1   0   7   4   5   6   3\
	3   5   7   0   6   1   4   2\
	4   7   6   1   2   0   3   5\
	6   2   4   5   7   3   0   1\
	*/\
	t16 = y0  + y1; \
	t17 = y2  + y5; \
	t20 = t16 + t17; \
	t18 = y3  + y4; \
	t22 = t16 + t18; \
	t19 = y6  + y7; \
	t21 = t18 + t19; \
	t23 = t17 + t19; \
	t0  = I64(0x5555555555555555) + t20 + y7; \
	t1  = t22 + y6; \
	t1  = ROTL64((t1), 3); \
	t2  = t20 + y3; \
	t2  = ROTL64((t2), 17); \
	t3  = y2 + t21; \
	t3  = ROTL64((t3), 23); \
	t4  = t22 + y5; \
	t4  = ROTL64((t4), 31); \
	t5  = t23 + y4; \
	t5  = ROTL64((t5), 37); \
	t6  = y1 + t23; \
	t6  = ROTL64((t6), 45); \
	t7  = y0 + t21; \
	t7  = ROTL64((t7), 59); \
	\
	t16  = t0 ^ t1; \
	t17  = t2 ^ t5; \
	t18  = t3 ^ t4; \
	t19  = t6 ^ t7; \
	z5   = t8  + (t18 ^ t6); \
	z6   = t9  + (t17 ^ t7); \
	z7   = t10 + (t4 ^ t19); \
	z0   = t11 + (t16 ^ t5); \
	z1   = t12 + (t2 ^ t19); \
	z2   = t13 + (t16 ^ t3); \
	z3   = t14 + (t0 ^ t18); \
	z4   = t15 + (t1 ^ t17); \
}

/**
 * The core transformation. Process a 512-bit block.
 *
 * @param hash algorithm state
 * @param block the message block to process
 */
static void rhash_edonr256_process_block(unsigned hash[16], const unsigned* block, size_t count)
{
	while (1) {
		uint32_t t0,  t1,  t2,  t3,  t4,  t5,  t6,  t7;
		uint32_t t8,  t9, t10, t11, t12, t13, t14, t15;
		uint32_t t16, t17,t18, t19, t20, t21, t22, t23;
		uint32_t p16, p17, p18, p19, p20, p21, p22, p23;
		uint32_t p24, p25, p26, p27, p28, p29, p30, p31;

		/* First row of quasigroup e-transformations */
		Q256(block[15], block[14], block[13], block[12], block[11], block[10], block[ 9], block[ 8],
			block[ 0], block[ 1], block[ 2], block[ 3], block[ 4], block[ 5], block[ 6], block[ 7],
			p16, p17, p18, p19, p20, p21, p22, p23);
		Q256(p16, p17, p18, p19, p20, p21, p22, p23,
			block[ 8], block[ 9], block[10], block[11], block[12], block[13], block[14], block[15],
			p24, p25, p26, p27, p28, p29, p30, p31);

		/* Second row of quasigroup e-transformations */
		Q256(hash[ 8], hash[ 9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15],
			p16,  p17, p18, p19, p20, p21, p22, p23,
			p16,  p17, p18, p19, p20, p21, p22, p23);
		Q256(p16,  p17, p18, p19, p20, p21, p22, p23,
			p24, p25, p26, p27, p28, p29, p30, p31,
			p24, p25, p26, p27, p28, p29, p30, p31);

		/* Third row of quasigroup e-transformations */
		Q256(p16,  p17, p18, p19, p20, p21, p22, p23,
			hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
			p16,  p17, p18, p19, p20, p21, p22, p23);
		Q256(p24, p25, p26, p27, p28, p29, p30, p31,
			p16,  p17, p18, p19, p20, p21, p22, p23,
			p24, p25, p26, p27, p28, p29, p30, p31);

		/* Fourth row of quasigroup e-transformations */
		Q256(block[ 7], block[ 6], block[ 5], block[ 4], block[ 3], block[ 2], block[ 1], block[ 0],
			p16,  p17, p18, p19, p20, p21, p22, p23,
			hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7]);
		Q256(hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
			p24, p25, p26, p27, p28, p29, p30, p31,
			hash[ 8], hash[ 9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);

		if (!--count) return;
		block += edonr256_block_size / sizeof(unsigned);
	};
}

/**
 * The core transformation. Process a 1024-bit block.
 *
 * @param hash algorithm state
 * @param block the message block to process
 */
static void rhash_edonr512_process_block(uint64_t hash[16], const uint64_t* block, size_t count)
{
	while (1) {
		uint64_t t0,  t1,  t2,  t3,  t4,  t5,  t6,  t7;
		uint64_t t8,  t9, t10, t11, t12, t13, t14, t15;
		uint64_t t16, t17,t18, t19, t20, t21, t22, t23;
		uint64_t p16, p17, p18, p19, p20, p21, p22, p23;
		uint64_t p24, p25, p26, p27, p28, p29, p30, p31;

		/* First row of quasigroup e-transformations */
		Q512(block[15], block[14], block[13], block[12], block[11], block[10], block[ 9], block[ 8],
			block[ 0], block[ 1], block[ 2], block[ 3], block[ 4], block[ 5], block[ 6], block[ 7],
			p16, p17, p18, p19, p20, p21, p22, p23);
		Q512(p16, p17, p18, p19, p20, p21, p22, p23,
			block[ 8], block[ 9], block[10], block[11], block[12], block[13], block[14], block[15],
			p24, p25, p26, p27, p28, p29, p30, p31);

		/* Second row of quasigroup e-transformations */
		Q512(hash[ 8], hash[ 9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15],
			p16,  p17, p18, p19, p20, p21, p22, p23,
			p16,  p17, p18, p19, p20, p21, p22, p23);
		Q512(p16,  p17, p18, p19, p20, p21, p22, p23,
			p24, p25, p26, p27, p28, p29, p30, p31,
			p24, p25, p26, p27, p28, p29, p30, p31);

		/* Third row of quasigroup e-transformations */
		Q512(p16,  p17, p18, p19, p20, p21, p22, p23,
			hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
			p16,  p17, p18, p19, p20, p21, p22, p23);
		Q512(p24, p25, p26, p27, p28, p29, p30, p31,
			p16,  p17, p18, p19, p20, p21, p22, p23,
			p24, p25, p26, p27, p28, p29, p30, p31);

		/* Fourth row of quasigroup e-transformations */
		Q512(block[ 7], block[ 6], block[ 5], block[ 4], block[ 3], block[ 2], block[ 1], block[ 0],
			p16,  p17, p18, p19, p20, p21, p22, p23,
			hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7]);
		Q512(hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
			p24, p25, p26, p27, p28, p29, p30, p31,
			hash[ 8], hash[ 9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);

		if (!--count) return;
		block += edonr512_block_size / sizeof(uint64_t);
	};
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_edonr256_update(edonr_ctx* ctx, const unsigned char* msg, size_t size)
{
	size_t index = (size_t)ctx->length & 63;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		size_t left = edonr256_block_size - index;
		le32_copy(ctx->u.data256.message, index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_edonr256_process_block(ctx->u.data256.hash, ctx->u.data256.message, 1);
		msg  += left;
		size -= left;
	}

	if (size >= edonr256_block_size) {
#if defined(CPU_IA32) || defined(CPU_X64)
		if (1)
#else
		if (IS_LITTLE_ENDIAN && IS_ALIGNED_32(msg))
#endif
		{
			/* the most common case is processing a 32-bit aligned message
			on a little-endian CPU without copying it */
			size_t count = size / edonr256_block_size;
			rhash_edonr256_process_block(ctx->u.data256.hash, (unsigned*)msg, count);
			msg  += edonr256_block_size * count;
			size -= edonr256_block_size * count;
		} else {
			do {
				le32_copy(ctx->u.data256.message, 0, msg, edonr256_block_size);
				rhash_edonr256_process_block(ctx->u.data256.hash, ctx->u.data256.message, 1);
				msg  += edonr256_block_size;
				size -= edonr256_block_size;
			} while (size >= edonr256_block_size);
		}
	}

	if (size) {
		le32_copy(ctx->u.data256.message, 0, msg, size); /* save leftovers */
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_edonr256_final(edonr_ctx* ctx, unsigned char* result)
{
	size_t index = ((unsigned)ctx->length & 63) >> 2;
	unsigned shift = ((unsigned)ctx->length & 3) * 8;

	/* pad message and run for the last block */

	/* append the byte 0x80 to the message */
	ctx->u.data256.message[index]   &= ~(0xFFFFFFFFu << shift);
	ctx->u.data256.message[index++] ^= 0x80u << shift;

	/* if no room left in the message to store 64-bit message length */
	if (index > 14) {
		/* then fill the rest with zeros and process it */
		while (index < 16) {
			ctx->u.data256.message[index++] = 0;
		}
		rhash_edonr256_process_block(ctx->u.data256.hash, ctx->u.data256.message, 1);
		index = 0;
	}
	while (index < 14) {
		ctx->u.data256.message[index++] = 0;
	}
	/* store message length in bits */
	ctx->u.data256.message[14] = (unsigned)(ctx->length << 3);
	ctx->u.data256.message[15] = (unsigned)(ctx->length >> 29);
	rhash_edonr256_process_block(ctx->u.data256.hash, ctx->u.data256.message, 1);

	if (result) {
		/* copy last bytes of intermidiate hash */
		int off = (ctx->digest_length <= 256 ? 64 : 128) - ctx->digest_length;
		le32_copy(result, 0, ((char*)ctx->u.data256.hash) + off, ctx->digest_length);
	}
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_edonr512_update(edonr_ctx* ctx, const unsigned char* msg, size_t size)
{
	size_t index = (size_t)ctx->length & 127;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		size_t left = edonr512_block_size - index;
		le64_copy(ctx->u.data512.message, index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_edonr512_process_block(ctx->u.data512.hash, ctx->u.data512.message, 1);
		msg  += left;
		size -= left;
	}
	if (size >= edonr512_block_size) {
#if defined(CPU_IA32) || defined(CPU_X64)
		if (1)
#else
		if (IS_LITTLE_ENDIAN && IS_ALIGNED_64(msg))
#endif
		{
			/* the most common case is processing a 64-bit aligned message
			on a little-endian CPU without copying it */
			size_t count = size / edonr512_block_size;
			rhash_edonr512_process_block(ctx->u.data512.hash, (uint64_t*)msg, count);
			msg  += edonr512_block_size * count;
			size -= edonr512_block_size * count;
		} else {
			do {
				le64_copy(ctx->u.data512.message, 0, msg, edonr512_block_size);
				rhash_edonr512_process_block(ctx->u.data512.hash, ctx->u.data512.message, 1);
				msg  += edonr512_block_size;
				size -= edonr512_block_size;
			} while (size >= edonr512_block_size);
		}
	}

	if (size) {
		le64_copy(ctx->u.data512.message, 0, msg, size); /* save leftovers */
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_edonr512_final(edonr_ctx* ctx, unsigned char* result)
{
	size_t index = ((unsigned)ctx->length & 127) >> 3;
	unsigned shift = ((unsigned)ctx->length & 7) * 8;

	/* pad message and run for the last block */

	/* append the byte 0x80 to the message */
	ctx->u.data512.message[index]   &= ~(I64(0xFFFFFFFFFFFFFFFF) << shift);
	ctx->u.data512.message[index++] ^= I64(0x80) << shift;

	/* if no room left in the message to store 64-bit message length */
	if (index == 16) {
		rhash_edonr512_process_block(ctx->u.data512.hash, ctx->u.data512.message, 1);
		index = 0;
	}
	while (index < 15) {
		ctx->u.data512.message[index++] = 0;
	}
	/* store message length in bits */
	ctx->u.data512.message[15] = ctx->length << 3;
	rhash_edonr512_process_block(ctx->u.data512.hash, ctx->u.data512.message, 1);

	if (result) {
		/* copy last bytes of intermidiate hash */
		int off = edonr512_block_size - ctx->digest_length;
		le64_copy(result, 0, ((char*)ctx->u.data512.hash) + off, ctx->digest_length);
	}
}
