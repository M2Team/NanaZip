/* md4.c - an implementation of MD4 Message-Digest Algorithm based on RFC 1320.
 *
 * Copyright (c) 2007, Aleksey Kravchenko <rhash.admin@gmail.com>
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
#include "md4.h"

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_md4_init(md4_ctx* ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* initialize state */
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
}

/* First, define three auxiliary functions that each take as input
 * three 32-bit words and returns a 32-bit word.
 *  F(x,y,z) = XY v not(X) Z = ((Y xor Z) X) xor Z (the last form is faster)
 *  G(X,Y,Z) = XY v XZ v YZ
 *  H(X,Y,Z) = X xor Y xor Z */

#define MD4_F(x, y, z) ((((y) ^ (z)) & (x)) ^ (z))
#define MD4_G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define MD4_H(x, y, z) ((x) ^ (y) ^ (z))

/* transformations for rounds 1, 2, and 3. */
#define MD4_ROUND1(a, b, c, d, x, s) { \
	(a) += MD4_F((b), (c), (d)) + (x); \
	(a) = ROTL32((a), (s)); \
}
#define MD4_ROUND2(a, b, c, d, x, s) { \
	(a) += MD4_G((b), (c), (d)) + (x) + 0x5a827999; \
	(a) = ROTL32((a), (s)); \
}
#define MD4_ROUND3(a, b, c, d, x, s) { \
	(a) += MD4_H((b), (c), (d)) + (x) + 0x6ed9eba1; \
	(a) = ROTL32((a), (s)); \
}

/**
 * The core transformation. Process a 512-bit block.
 * The function has been taken from RFC 1320 with little changes.
 *
 * @param state algorithm state
 * @param x the message block to process
 */
static void rhash_md4_process_block(unsigned state[4], const unsigned* x)
{
	register unsigned a, b, c, d;
	a = state[0], b = state[1], c = state[2], d = state[3];

	MD4_ROUND1(a, b, c, d, x[ 0],  3);
	MD4_ROUND1(d, a, b, c, x[ 1],  7);
	MD4_ROUND1(c, d, a, b, x[ 2], 11);
	MD4_ROUND1(b, c, d, a, x[ 3], 19);
	MD4_ROUND1(a, b, c, d, x[ 4],  3);
	MD4_ROUND1(d, a, b, c, x[ 5],  7);
	MD4_ROUND1(c, d, a, b, x[ 6], 11);
	MD4_ROUND1(b, c, d, a, x[ 7], 19);
	MD4_ROUND1(a, b, c, d, x[ 8],  3);
	MD4_ROUND1(d, a, b, c, x[ 9],  7);
	MD4_ROUND1(c, d, a, b, x[10], 11);
	MD4_ROUND1(b, c, d, a, x[11], 19);
	MD4_ROUND1(a, b, c, d, x[12],  3);
	MD4_ROUND1(d, a, b, c, x[13],  7);
	MD4_ROUND1(c, d, a, b, x[14], 11);
	MD4_ROUND1(b, c, d, a, x[15], 19);

	MD4_ROUND2(a, b, c, d, x[ 0],  3);
	MD4_ROUND2(d, a, b, c, x[ 4],  5);
	MD4_ROUND2(c, d, a, b, x[ 8],  9);
	MD4_ROUND2(b, c, d, a, x[12], 13);
	MD4_ROUND2(a, b, c, d, x[ 1],  3);
	MD4_ROUND2(d, a, b, c, x[ 5],  5);
	MD4_ROUND2(c, d, a, b, x[ 9],  9);
	MD4_ROUND2(b, c, d, a, x[13], 13);
	MD4_ROUND2(a, b, c, d, x[ 2],  3);
	MD4_ROUND2(d, a, b, c, x[ 6],  5);
	MD4_ROUND2(c, d, a, b, x[10],  9);
	MD4_ROUND2(b, c, d, a, x[14], 13);
	MD4_ROUND2(a, b, c, d, x[ 3],  3);
	MD4_ROUND2(d, a, b, c, x[ 7],  5);
	MD4_ROUND2(c, d, a, b, x[11],  9);
	MD4_ROUND2(b, c, d, a, x[15], 13);

	MD4_ROUND3(a, b, c, d, x[ 0],  3);
	MD4_ROUND3(d, a, b, c, x[ 8],  9);
	MD4_ROUND3(c, d, a, b, x[ 4], 11);
	MD4_ROUND3(b, c, d, a, x[12], 15);
	MD4_ROUND3(a, b, c, d, x[ 2],  3);
	MD4_ROUND3(d, a, b, c, x[10],  9);
	MD4_ROUND3(c, d, a, b, x[ 6], 11);
	MD4_ROUND3(b, c, d, a, x[14], 15);
	MD4_ROUND3(a, b, c, d, x[ 1],  3);
	MD4_ROUND3(d, a, b, c, x[ 9],  9);
	MD4_ROUND3(c, d, a, b, x[ 5], 11);
	MD4_ROUND3(b, c, d, a, x[13], 15);
	MD4_ROUND3(a, b, c, d, x[ 3],  3);
	MD4_ROUND3(d, a, b, c, x[11],  9);
	MD4_ROUND3(c, d, a, b, x[ 7], 11);
	MD4_ROUND3(b, c, d, a, x[15], 15);

	state[0] += a, state[1] += b, state[2] += c, state[3] += d;
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_md4_update(md4_ctx* ctx, const unsigned char* msg, size_t size)
{
	unsigned index = (unsigned)ctx->length & 63;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		unsigned left = md4_block_size - index;
		le32_copy(ctx->message, index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_md4_process_block(ctx->hash, ctx->message);
		msg  += left;
		size -= left;
	}
	while (size >= md4_block_size) {
		unsigned* aligned_message_block;
		if (IS_LITTLE_ENDIAN && IS_ALIGNED_32(msg)) {
			/* the most common case is processing a 32-bit aligned message
			on a little-endian CPU without copying it */
			aligned_message_block = (unsigned*)msg;
		} else {
			le32_copy(ctx->message, 0, msg, md4_block_size);
			aligned_message_block = ctx->message;
		}

		rhash_md4_process_block(ctx->hash, aligned_message_block);
		msg  += md4_block_size;
		size -= md4_block_size;
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
void rhash_md4_final(md4_ctx* ctx, unsigned char result[16])
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
		rhash_md4_process_block(ctx->hash, ctx->message);
		index = 0;
	}
	while (index < 14) {
		ctx->message[index++] = 0;
	}
	ctx->message[14] = (unsigned)(ctx->length << 3);
	ctx->message[15] = (unsigned)(ctx->length >> 29);
	rhash_md4_process_block(ctx->hash, ctx->message);

	if (result) le32_copy(result, 0, &ctx->hash, 16);
}
