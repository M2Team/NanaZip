/* ripemd-160.c - an implementation of RIPEMD-160 Hash function
 * based on the original aritcle:
 * H. Dobbertin, A. Bosselaers, B. Preneel, RIPEMD-160: A strengthened version
 * of RIPEMD, Lecture Notes in Computer, 1996, V.1039, pp.71-82
 *
 * Copyright (c) 2009, Aleksey Kravchenko <rhash.admin@gmail.com>
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
#include "ripemd-160.h"

/**
 * Initialize algorithm context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_ripemd160_init(ripemd160_ctx* ctx)
{
	memset(ctx->message, 0, sizeof(ctx->message));
	ctx->length = 0;

	/* initialize state */
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->hash[4] = 0xc3d2e1f0;
}

/* five boolean functions */
#define F1(x, y, z) ((x) ^ (y) ^ (z))
#define F2(x, y, z) ((((y) ^ (z)) & (x)) ^ (z))
#define F3(x, y, z) (((x) | ~(y)) ^ (z))
#define F4(x, y, z) ((((x) ^ (y)) & (z)) ^ (y))
#define F5(x, y, z) ((x) ^ ((y) | ~(z)))

#define RMD_FUNC(FUNC, A, B, C, D, E, X, S, K) \
	(A) += FUNC((B), (C), (D)) + (X) + K; \
	(A) = ROTL32((A), (S)) + (E); \
	(C) = ROTL32((C), 10);

/* steps for the left and right half of algorithm */
#define L1(A, B, C, D, E, X, S) RMD_FUNC(F1, A, B, C, D, E, X, S, 0)
#define L2(A, B, C, D, E, X, S) RMD_FUNC(F2, A, B, C, D, E, X, S, 0x5a827999UL)
#define L3(A, B, C, D, E, X, S) RMD_FUNC(F3, A, B, C, D, E, X, S, 0x6ed9eba1UL)
#define L4(A, B, C, D, E, X, S) RMD_FUNC(F4, A, B, C, D, E, X, S, 0x8f1bbcdcUL)
#define L5(A, B, C, D, E, X, S) RMD_FUNC(F5, A, B, C, D, E, X, S, 0xa953fd4eUL)
#define R1(A, B, C, D, E, X, S) RMD_FUNC(F5, A, B, C, D, E, X, S, 0x50a28be6UL)
#define R2(A, B, C, D, E, X, S) RMD_FUNC(F4, A, B, C, D, E, X, S, 0x5c4dd124UL)
#define R3(A, B, C, D, E, X, S) RMD_FUNC(F3, A, B, C, D, E, X, S, 0x6d703ef3UL)
#define R4(A, B, C, D, E, X, S) RMD_FUNC(F2, A, B, C, D, E, X, S, 0x7a6d76e9UL)
#define R5(A, B, C, D, E, X, S) RMD_FUNC(F1, A, B, C, D, E, X, S, 0)

/**
 * The core transformation. Process a 512-bit block.
 *
 * @param hash algorithm intermediate hash
 * @param X the message block to process
 */
static void rhash_ripemd160_process_block(unsigned* hash, const unsigned* X)
{
	register unsigned A = hash[0],  B = hash[1],  C = hash[2], D = hash[3],  E = hash[4];
	register unsigned a1 = hash[0], b1 = hash[1], c1 = hash[2], d1 = hash[3], e1 = hash[4];

	/* rounds of the left and right half interleaved */
	L1(a1, b1, c1, d1, e1, X[ 0], 11);
	R1(A, B, C, D, E, X[ 5],  8);
	L1(e1, a1, b1, c1, d1, X[ 1], 14);
	R1(E, A, B, C, D, X[14],  9);
	L1(d1, e1, a1, b1, c1, X[ 2], 15);
	R1(D, E, A, B, C, X[ 7],  9);
	L1(c1, d1, e1, a1, b1, X[ 3], 12);
	R1(C, D, E, A, B, X[ 0], 11);
	L1(b1, c1, d1, e1, a1, X[ 4],  5);
	R1(B, C, D, E, A, X[ 9], 13);
	L1(a1, b1, c1, d1, e1, X[ 5],  8);
	R1(A, B, C, D, E, X[ 2], 15);
	L1(e1, a1, b1, c1, d1, X[ 6],  7);
	R1(E, A, B, C, D, X[11], 15);
	L1(d1, e1, a1, b1, c1, X[ 7],  9);
	R1(D, E, A, B, C, X[ 4],  5);
	L1(c1, d1, e1, a1, b1, X[ 8], 11);
	R1(C, D, E, A, B, X[13],  7);
	L1(b1, c1, d1, e1, a1, X[ 9], 13);
	R1(B, C, D, E, A, X[ 6],  7);
	L1(a1, b1, c1, d1, e1, X[10], 14);
	R1(A, B, C, D, E, X[15],  8);
	L1(e1, a1, b1, c1, d1, X[11], 15);
	R1(E, A, B, C, D, X[ 8], 11);
	L1(d1, e1, a1, b1, c1, X[12],  6);
	R1(D, E, A, B, C, X[ 1], 14);
	L1(c1, d1, e1, a1, b1, X[13],  7);
	R1(C, D, E, A, B, X[10], 14);
	L1(b1, c1, d1, e1, a1, X[14],  9);
	R1(B, C, D, E, A, X[ 3], 12);
	L1(a1, b1, c1, d1, e1, X[15],  8);
	R1(A, B, C, D, E, X[12],  6);

	L2(e1, a1, b1, c1, d1, X[ 7],  7);
	R2(E, A, B, C, D, X[ 6],  9);
	L2(d1, e1, a1, b1, c1, X[ 4],  6);
	R2(D, E, A, B, C, X[11], 13);
	L2(c1, d1, e1, a1, b1, X[13],  8);
	R2(C, D, E, A, B, X[ 3], 15);
	L2(b1, c1, d1, e1, a1, X[ 1], 13);
	R2(B, C, D, E, A, X[ 7],  7);
	L2(a1, b1, c1, d1, e1, X[10], 11);
	R2(A, B, C, D, E, X[ 0], 12);
	L2(e1, a1, b1, c1, d1, X[ 6],  9);
	R2(E, A, B, C, D, X[13],  8);
	L2(d1, e1, a1, b1, c1, X[15],  7);
	R2(D, E, A, B, C, X[ 5],  9);
	L2(c1, d1, e1, a1, b1, X[ 3], 15);
	R2(C, D, E, A, B, X[10], 11);
	L2(b1, c1, d1, e1, a1, X[12],  7);
	R2(B, C, D, E, A, X[14],  7);
	L2(a1, b1, c1, d1, e1, X[ 0], 12);
	R2(A, B, C, D, E, X[15],  7);
	L2(e1, a1, b1, c1, d1, X[ 9], 15);
	R2(E, A, B, C, D, X[ 8], 12);
	L2(d1, e1, a1, b1, c1, X[ 5],  9);
	R2(D, E, A, B, C, X[12],  7);
	L2(c1, d1, e1, a1, b1, X[ 2], 11);
	R2(C, D, E, A, B, X[ 4],  6);
	L2(b1, c1, d1, e1, a1, X[14],  7);
	R2(B, C, D, E, A, X[ 9], 15);
	L2(a1, b1, c1, d1, e1, X[11], 13);
	R2(A, B, C, D, E, X[ 1], 13);
	L2(e1, a1, b1, c1, d1, X[ 8], 12);
	R2(E, A, B, C, D, X[ 2], 11);

	L3(d1, e1, a1, b1, c1, X[ 3], 11);
	R3(D, E, A, B, C, X[15],  9);
	L3(c1, d1, e1, a1, b1, X[10], 13);
	R3(C, D, E, A, B, X[ 5],  7);
	L3(b1, c1, d1, e1, a1, X[14],  6);
	R3(B, C, D, E, A, X[ 1], 15);
	L3(a1, b1, c1, d1, e1, X[ 4],  7);
	R3(A, B, C, D, E, X[ 3], 11);
	L3(e1, a1, b1, c1, d1, X[ 9], 14);
	R3(E, A, B, C, D, X[ 7],  8);
	L3(d1, e1, a1, b1, c1, X[15],  9);
	R3(D, E, A, B, C, X[14],  6);
	L3(c1, d1, e1, a1, b1, X[ 8], 13);
	R3(C, D, E, A, B, X[ 6],  6);
	L3(b1, c1, d1, e1, a1, X[ 1], 15);
	R3(B, C, D, E, A, X[ 9], 14);
	L3(a1, b1, c1, d1, e1, X[ 2], 14);
	R3(A, B, C, D, E, X[11], 12);
	L3(e1, a1, b1, c1, d1, X[ 7],  8);
	R3(E, A, B, C, D, X[ 8], 13);
	L3(d1, e1, a1, b1, c1, X[ 0], 13);
	R3(D, E, A, B, C, X[12],  5);
	L3(c1, d1, e1, a1, b1, X[ 6],  6);
	R3(C, D, E, A, B, X[ 2], 14);
	L3(b1, c1, d1, e1, a1, X[13],  5);
	R3(B, C, D, E, A, X[10], 13);
	L3(a1, b1, c1, d1, e1, X[11], 12);
	R3(A, B, C, D, E, X[ 0], 13);
	L3(e1, a1, b1, c1, d1, X[ 5],  7);
	R3(E, A, B, C, D, X[ 4],  7);
	L3(d1, e1, a1, b1, c1, X[12],  5);
	R3(D, E, A, B, C, X[13],  5);

	L4(c1, d1, e1, a1, b1, X[ 1], 11);
	R4(C, D, E, A, B, X[ 8], 15);
	L4(b1, c1, d1, e1, a1, X[ 9], 12);
	R4(B, C, D, E, A, X[ 6],  5);
	L4(a1, b1, c1, d1, e1, X[11], 14);
	R4(A, B, C, D, E, X[ 4],  8);
	L4(e1, a1, b1, c1, d1, X[10], 15);
	R4(E, A, B, C, D, X[ 1], 11);
	L4(d1, e1, a1, b1, c1, X[ 0], 14);
	R4(D, E, A, B, C, X[ 3], 14);
	L4(c1, d1, e1, a1, b1, X[ 8], 15);
	R4(C, D, E, A, B, X[11], 14);
	L4(b1, c1, d1, e1, a1, X[12],  9);
	R4(B, C, D, E, A, X[15],  6);
	L4(a1, b1, c1, d1, e1, X[ 4],  8);
	R4(A, B, C, D, E, X[ 0], 14);
	L4(e1, a1, b1, c1, d1, X[13],  9);
	R4(E, A, B, C, D, X[ 5],  6);
	L4(d1, e1, a1, b1, c1, X[ 3], 14);
	R4(D, E, A, B, C, X[12],  9);
	L4(c1, d1, e1, a1, b1, X[ 7],  5);
	R4(C, D, E, A, B, X[ 2], 12);
	L4(b1, c1, d1, e1, a1, X[15],  6);
	R4(B, C, D, E, A, X[13],  9);
	L4(a1, b1, c1, d1, e1, X[14],  8);
	R4(A, B, C, D, E, X[ 9], 12);
	L4(e1, a1, b1, c1, d1, X[ 5],  6);
	R4(E, A, B, C, D, X[ 7],  5);
	L4(d1, e1, a1, b1, c1, X[ 6],  5);
	R4(D, E, A, B, C, X[10], 15);
	L4(c1, d1, e1, a1, b1, X[ 2], 12);
	R4(C, D, E, A, B, X[14],  8);

	L5(b1, c1, d1, e1, a1, X[ 4],  9);
	R5(B, C, D, E, A, X[12] ,  8);
	L5(a1, b1, c1, d1, e1, X[ 0], 15);
	R5(A, B, C, D, E, X[15] ,  5);
	L5(e1, a1, b1, c1, d1, X[ 5],  5);
	R5(E, A, B, C, D, X[10] , 12);
	L5(d1, e1, a1, b1, c1, X[ 9], 11);
	R5(D, E, A, B, C, X[ 4] ,  9);
	L5(c1, d1, e1, a1, b1, X[ 7],  6);
	R5(C, D, E, A, B, X[ 1] , 12);
	L5(b1, c1, d1, e1, a1, X[12],  8);
	R5(B, C, D, E, A, X[ 5] ,  5);
	L5(a1, b1, c1, d1, e1, X[ 2], 13);
	R5(A, B, C, D, E, X[ 8] , 14);
	L5(e1, a1, b1, c1, d1, X[10], 12);
	R5(E, A, B, C, D, X[ 7] ,  6);
	L5(d1, e1, a1, b1, c1, X[14],  5);
	R5(D, E, A, B, C, X[ 6] ,  8);
	L5(c1, d1, e1, a1, b1, X[ 1], 12);
	R5(C, D, E, A, B, X[ 2] , 13);
	L5(b1, c1, d1, e1, a1, X[ 3], 13);
	R5(B, C, D, E, A, X[13] ,  6);
	L5(a1, b1, c1, d1, e1, X[ 8], 14);
	R5(A, B, C, D, E, X[14] ,  5);
	L5(e1, a1, b1, c1, d1, X[11], 11);
	R5(E, A, B, C, D, X[ 0] , 15);
	L5(d1, e1, a1, b1, c1, X[ 6],  8);
	R5(D, E, A, B, C, X[ 3] , 13);
	L5(c1, d1, e1, a1, b1, X[15],  5);
	R5(C, D, E, A, B, X[ 9] , 11);
	L5(b1, c1, d1, e1, a1, X[13],  6);
	R5(B, C, D, E, A, X[11] , 11);

	/* update intermediate hash */
	D += c1 + hash[1];
	hash[1] = hash[2] + d1 + E;
	hash[2] = hash[3] + e1 + A;
	hash[3] = hash[4] + a1 + B;
	hash[4] = hash[0] + b1 + C;
	hash[0] = D;
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_ripemd160_update(ripemd160_ctx* ctx, const unsigned char* msg, size_t size)
{
	unsigned index = (unsigned)ctx->length & 63;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		unsigned left = ripemd160_block_size - index;
		le32_copy(ctx->message, index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_ripemd160_process_block(ctx->hash, (unsigned*)ctx->message);
		msg += left;
		size -= left;
	}
	while (size >= ripemd160_block_size) {
		unsigned* aligned_message_block;
		if (IS_LITTLE_ENDIAN && IS_ALIGNED_32(msg)) {
			/* the most common case is processing of an already aligned message
			on little-endian CPU without copying it */
			aligned_message_block = (unsigned*)msg;
		} else {
			le32_copy(ctx->message, 0, msg, ripemd160_block_size);
			aligned_message_block = ctx->message;
		}

		rhash_ripemd160_process_block(ctx->hash, aligned_message_block);
		msg += ripemd160_block_size;
		size -= ripemd160_block_size;
	}
	if (size) {
		/* save leftovers */
		le32_copy(ctx->message, 0, msg, size);
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_ripemd160_final(ripemd160_ctx* ctx, unsigned char result[20])
{
	unsigned index = ((unsigned)ctx->length & 63) >> 2;
	unsigned shift = ((unsigned)ctx->length & 3) * 8;

	/* pad message and run for last block */

	/* append the byte 0x80 to the message */
	ctx->message[index]   &= ~(0xFFFFFFFFu << shift);
	ctx->message[index++] ^= 0x80u << shift;

	/* if no room left in the message to store 64-bit message length */
	if (index > 14) {
		/* then fill the rest with zeros and process it */
		while (index < 16) {
			ctx->message[index++] = 0;
		}
		rhash_ripemd160_process_block(ctx->hash, ctx->message);
		index = 0;
	}
	while (index < 14) {
		ctx->message[index++] = 0;
	}
	ctx->message[14] = (unsigned)(ctx->length << 3);
	ctx->message[15] = (unsigned)(ctx->length >> 29);
	rhash_ripemd160_process_block(ctx->hash, ctx->message);

	le32_copy(result, 0, &ctx->hash, 20);
}
