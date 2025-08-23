﻿/* sha256.c - an implementation of SHA-256/224 hash functions
 * based on FIPS 180-3 (Federal Information Processing Standart).
 *
 * Copyright (c) 2010, Aleksey Kravchenko <rhash.admin@gmail.com>
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

#include <string.h>
#include "byte_order.h"
#include "sha256.h"

/* SHA-224 and SHA-256 constants for 64 rounds. These words represent
 * the first 32 bits of the fractional parts of the cube
 * roots of the first 64 prime numbers. */
static const unsigned rhash_k256[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* The SHA256/224 functions defined by FIPS 180-3, 4.1.2 */
/* Optimized version of Ch(x,y,z)=((x & y) | (~x & z)) */
#define Ch(x,y,z)  ((z) ^ ((x) & ((y) ^ (z))))
/* Optimized version of Maj(x,y,z)=((x & y) ^ (x & z) ^ (y & z)) */
#define Maj(x,y,z) (((x) & (y)) ^ ((z) & ((x) ^ (y))))

#define Sigma0(x) (ROTR32((x), 2) ^ ROTR32((x), 13) ^ ROTR32((x), 22))
#define Sigma1(x) (ROTR32((x), 6) ^ ROTR32((x), 11) ^ ROTR32((x), 25))
#define sigma0(x) (ROTR32((x), 7) ^ ROTR32((x), 18) ^ ((x) >>  3))
#define sigma1(x) (ROTR32((x),17) ^ ROTR32((x), 19) ^ ((x) >> 10))

/* Recalculate element n-th of circular buffer W using formula
 *   W[n] = sigma1(W[n - 2]) + W[n - 7] + sigma0(W[n - 15]) + W[n - 16]; */
#define RECALCULATE_W(W,n) (W[n] += \
	(sigma1(W[(n - 2) & 15]) + W[(n - 7) & 15] + sigma0(W[(n - 15) & 15])))

#define ROUND(a,b,c,d,e,f,g,h,k,data) { \
	unsigned T1 = h + Sigma1(e) + Ch(e,f,g) + k + (data); \
	d += T1, h = T1 + Sigma0(a) + Maj(a,b,c); }
#define ROUND_1_16(a,b,c,d,e,f,g,h,n) \
	ROUND(a,b,c,d,e,f,g,h, rhash_k256[n], W[n] = be2me_32(block[n]))
#define ROUND_17_64(a,b,c,d,e,f,g,h,n) \
	ROUND(a,b,c,d,e,f,g,h, k[n], RECALCULATE_W(W, n))

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_sha256_init(sha256_ctx* ctx)
{
	/* Initial values. These words were obtained by taking the first 32
	 * bits of the fractional parts of the square roots of the first
	 * eight prime numbers. */
	static const unsigned SHA256_H0[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	memset(ctx->message, 0, sizeof(ctx->message));
	ctx->length = 0;
	ctx->digest_length = sha256_hash_size;

	/* initialize algorithm state */
	memcpy(ctx->hash, SHA256_H0, sizeof(ctx->hash));
}

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_sha224_init(struct sha256_ctx* ctx)
{
	/* Initial values from FIPS 180-3. These words were obtained by taking
	 * bits from 33th to 64th of the fractional parts of the square
	 * roots of ninth through sixteenth prime numbers. */
	static const unsigned SHA224_H0[8] = {
		0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
		0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4
	};
	memset(ctx->message, 0, sizeof(ctx->message));
	ctx->length = 0;
	ctx->digest_length = sha224_hash_size;

	memcpy(ctx->hash, SHA224_H0, sizeof(ctx->hash));
}

/**
 * The core transformation. Process a 512-bit block.
 *
 * @param hash algorithm state
 * @param block the message block to process
 */
static void rhash_sha256_process_block(unsigned hash[8], unsigned block[16])
{
	unsigned A, B, C, D, E, F, G, H;
	unsigned W[16];
	const unsigned* k;
	int i;

	A = hash[0], B = hash[1], C = hash[2], D = hash[3];
	E = hash[4], F = hash[5], G = hash[6], H = hash[7];

	/* Compute SHA using alternate Method: FIPS 180-3 6.1.3 */
	ROUND_1_16(A, B, C, D, E, F, G, H, 0);
	ROUND_1_16(H, A, B, C, D, E, F, G, 1);
	ROUND_1_16(G, H, A, B, C, D, E, F, 2);
	ROUND_1_16(F, G, H, A, B, C, D, E, 3);
	ROUND_1_16(E, F, G, H, A, B, C, D, 4);
	ROUND_1_16(D, E, F, G, H, A, B, C, 5);
	ROUND_1_16(C, D, E, F, G, H, A, B, 6);
	ROUND_1_16(B, C, D, E, F, G, H, A, 7);
	ROUND_1_16(A, B, C, D, E, F, G, H, 8);
	ROUND_1_16(H, A, B, C, D, E, F, G, 9);
	ROUND_1_16(G, H, A, B, C, D, E, F, 10);
	ROUND_1_16(F, G, H, A, B, C, D, E, 11);
	ROUND_1_16(E, F, G, H, A, B, C, D, 12);
	ROUND_1_16(D, E, F, G, H, A, B, C, 13);
	ROUND_1_16(C, D, E, F, G, H, A, B, 14);
	ROUND_1_16(B, C, D, E, F, G, H, A, 15);

	for (i = 16, k = &rhash_k256[16]; i < 64; i += 16, k += 16) {
		ROUND_17_64(A, B, C, D, E, F, G, H,  0);
		ROUND_17_64(H, A, B, C, D, E, F, G,  1);
		ROUND_17_64(G, H, A, B, C, D, E, F,  2);
		ROUND_17_64(F, G, H, A, B, C, D, E,  3);
		ROUND_17_64(E, F, G, H, A, B, C, D,  4);
		ROUND_17_64(D, E, F, G, H, A, B, C,  5);
		ROUND_17_64(C, D, E, F, G, H, A, B,  6);
		ROUND_17_64(B, C, D, E, F, G, H, A,  7);
		ROUND_17_64(A, B, C, D, E, F, G, H,  8);
		ROUND_17_64(H, A, B, C, D, E, F, G,  9);
		ROUND_17_64(G, H, A, B, C, D, E, F, 10);
		ROUND_17_64(F, G, H, A, B, C, D, E, 11);
		ROUND_17_64(E, F, G, H, A, B, C, D, 12);
		ROUND_17_64(D, E, F, G, H, A, B, C, 13);
		ROUND_17_64(C, D, E, F, G, H, A, B, 14);
		ROUND_17_64(B, C, D, E, F, G, H, A, 15);
	}

	hash[0] += A, hash[1] += B, hash[2] += C, hash[3] += D;
	hash[4] += E, hash[5] += F, hash[6] += G, hash[7] += H;
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_sha256_update(sha256_ctx* ctx, const unsigned char* msg, size_t size)
{
	size_t index = (size_t)ctx->length & 63;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		size_t left = sha256_block_size - index;
		memcpy((char*)ctx->message + index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_sha256_process_block(ctx->hash, (unsigned*)ctx->message);
		msg  += left;
		size -= left;
	}
	while (size >= sha256_block_size) {
		unsigned* aligned_message_block;
		if (IS_ALIGNED_32(msg)) {
			/* the most common case is processing of an already aligned message
			without copying it */
			aligned_message_block = (unsigned*)msg;
		} else {
			memcpy(ctx->message, msg, sha256_block_size);
			aligned_message_block = (unsigned*)ctx->message;
		}

		rhash_sha256_process_block(ctx->hash, aligned_message_block);
		msg  += sha256_block_size;
		size -= sha256_block_size;
	}
	if (size) {
		memcpy(ctx->message, msg, size); /* save leftovers */
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_sha256_final(sha256_ctx* ctx, unsigned char* result)
{
	size_t index = ((unsigned)ctx->length & 63) >> 2;
	unsigned shift = ((unsigned)ctx->length & 3) * 8;

	/* pad message and run for last block */

	/* append the byte 0x80 to the message */
	ctx->message[index]   &= le2me_32(~(0xFFFFFFFFu << shift));
	ctx->message[index++] ^= le2me_32(0x80u << shift);

	/* if no room left in the message to store 64-bit message length */
	if (index > 14) {
		/* then fill the rest with zeros and process it */
		while (index < 16) {
			ctx->message[index++] = 0;
		}
		rhash_sha256_process_block(ctx->hash, ctx->message);
		index = 0;
	}
	while (index < 14) {
		ctx->message[index++] = 0;
	}
	ctx->message[14] = be2me_32( (unsigned)(ctx->length >> 29) );
	ctx->message[15] = be2me_32( (unsigned)(ctx->length << 3) );
	rhash_sha256_process_block(ctx->hash, ctx->message);

	if (result) be32_copy(result, 0, ctx->hash, ctx->digest_length);
}
