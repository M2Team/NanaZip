/* hash.c - an implementation of HAS-160 Algorithm.
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
 *
 * HAS-160 is a cryptographic hash function designed for use with the
 * Korean KCDSA digital signature algorithm. It derives from SHA-1,
 * with assorted changes intended to increase its security.
 * It produces a 160-bit message digest.
 *
 * HAS-160 was developed in 1998 by KISA
 * (Korea Information Security Agency) + Academic.
 */

#include <string.h>
#include "byte_order.h"
#include "has160.h"

/**
 * Initialize algorithm context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_has160_init(has160_ctx* ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	/* initialize algorithm state */
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->hash[4] = 0xc3d2e1f0;
}

/* HAS-160 boolean functions:
 * F1(x,y,z) == (x AND y) OR ((NOT x) AND z) = ((y XOR z) AND x) XOR z
 * F2(x,y,z) == x XOR y XOR z
 * F3(x,y,z) == y XOR (x OR (NOT Z))
 * F4(x,y,z) == x XOR y XOR z                 */
#define STEP_F1(A, B, C, D, E, msg, rot) \
	E += ROTL32(A, rot) + (D ^ (B & (C ^ D))) + msg; \
	B  = ROTL32(B, 10);
#define STEP_F2(A, B, C, D, E, msg, rot) \
	E += ROTL32(A, rot) + (B ^ C ^ D) + msg + 0x5A827999; \
	B  = ROTL32(B, 17);
#define STEP_F3(A, B, C, D, E, msg, rot) \
	E += ROTL32(A, rot) + (C ^ (B | ~D)) + msg + 0x6ED9EBA1; \
	B  = ROTL32(B, 25);
#define STEP_F4(A, B, C, D, E, msg, rot) \
	E += ROTL32(A, rot) + (B ^ C ^ D) + msg + 0x8F1BBCDC; \
	B  = ROTL32(B, 30);

/**
 * The core transformation. Process a 512-bit block.
 *
 * @param hash algorithm state
 * @param block the message block to process
 */
static void rhash_has160_process_block(unsigned* hash, const unsigned* block)
{
	unsigned X[32];
	{
		unsigned j;
		for (j = 0; j < 16; j++) {
			X[j] = le2me_32(block[j]);
		}

		X[16] = X[ 0] ^ X[ 1] ^ X[ 2] ^ X[ 3]; /* for rounds  1..20 */
		X[17] = X[ 4] ^ X[ 5] ^ X[ 6] ^ X[ 7];
		X[18] = X[ 8] ^ X[ 9] ^ X[10] ^ X[11];
		X[19] = X[12] ^ X[13] ^ X[14] ^ X[15];
		X[20] = X[ 3] ^ X[ 6] ^ X[ 9] ^ X[12]; /* for rounds 21..40 */
		X[21] = X[ 2] ^ X[ 5] ^ X[ 8] ^ X[15];
		X[22] = X[ 1] ^ X[ 4] ^ X[11] ^ X[14];
		X[23] = X[ 0] ^ X[ 7] ^ X[10] ^ X[13];
		X[24] = X[ 5] ^ X[ 7] ^ X[12] ^ X[14]; /* for rounds 41..60 */
		X[25] = X[ 0] ^ X[ 2] ^ X[ 9] ^ X[11];
		X[26] = X[ 4] ^ X[ 6] ^ X[13] ^ X[15];
		X[27] = X[ 1] ^ X[ 3] ^ X[ 8] ^ X[10];
		X[28] = X[ 2] ^ X[ 7] ^ X[ 8] ^ X[13]; /* for rounds 61..80 */
		X[29] = X[ 3] ^ X[ 4] ^ X[ 9] ^ X[14];
		X[30] = X[ 0] ^ X[ 5] ^ X[10] ^ X[15];
		X[31] = X[ 1] ^ X[ 6] ^ X[11] ^ X[12];
	}


	{
		unsigned A, B, C, D, E;

		A = hash[0];
		B = hash[1];
		C = hash[2];
		D = hash[3];
		E = hash[4];

		STEP_F1(A,B,C,D,E,X[18], 5);
		STEP_F1(E,A,B,C,D,X[ 0],11);
		STEP_F1(D,E,A,B,C,X[ 1], 7);
		STEP_F1(C,D,E,A,B,X[ 2],15);
		STEP_F1(B,C,D,E,A,X[ 3], 6);
		STEP_F1(A,B,C,D,E,X[19],13);
		STEP_F1(E,A,B,C,D,X[ 4], 8);
		STEP_F1(D,E,A,B,C,X[ 5],14);
		STEP_F1(C,D,E,A,B,X[ 6], 7);
		STEP_F1(B,C,D,E,A,X[ 7],12);
		STEP_F1(A,B,C,D,E,X[16], 9);
		STEP_F1(E,A,B,C,D,X[ 8],11);
		STEP_F1(D,E,A,B,C,X[ 9], 8);
		STEP_F1(C,D,E,A,B,X[10],15);
		STEP_F1(B,C,D,E,A,X[11], 6);
		STEP_F1(A,B,C,D,E,X[17],12);
		STEP_F1(E,A,B,C,D,X[12], 9);
		STEP_F1(D,E,A,B,C,X[13],14);
		STEP_F1(C,D,E,A,B,X[14], 5);
		STEP_F1(B,C,D,E,A,X[15],13);

		STEP_F2(A,B,C,D,E,X[22], 5);
		STEP_F2(E,A,B,C,D,X[ 3],11);
		STEP_F2(D,E,A,B,C,X[ 6], 7);
		STEP_F2(C,D,E,A,B,X[ 9],15);
		STEP_F2(B,C,D,E,A,X[12], 6);
		STEP_F2(A,B,C,D,E,X[23],13);
		STEP_F2(E,A,B,C,D,X[15], 8);
		STEP_F2(D,E,A,B,C,X[ 2],14);
		STEP_F2(C,D,E,A,B,X[ 5], 7);
		STEP_F2(B,C,D,E,A,X[ 8],12);
		STEP_F2(A,B,C,D,E,X[20], 9);
		STEP_F2(E,A,B,C,D,X[11],11);
		STEP_F2(D,E,A,B,C,X[14], 8);
		STEP_F2(C,D,E,A,B,X[ 1],15);
		STEP_F2(B,C,D,E,A,X[ 4], 6);
		STEP_F2(A,B,C,D,E,X[21],12);
		STEP_F2(E,A,B,C,D,X[ 7], 9);
		STEP_F2(D,E,A,B,C,X[10],14);
		STEP_F2(C,D,E,A,B,X[13], 5);
		STEP_F2(B,C,D,E,A,X[ 0],13);

		STEP_F3(A,B,C,D,E,X[26], 5);
		STEP_F3(E,A,B,C,D,X[12],11);
		STEP_F3(D,E,A,B,C,X[ 5], 7);
		STEP_F3(C,D,E,A,B,X[14],15);
		STEP_F3(B,C,D,E,A,X[ 7], 6);
		STEP_F3(A,B,C,D,E,X[27],13);
		STEP_F3(E,A,B,C,D,X[ 0], 8);
		STEP_F3(D,E,A,B,C,X[ 9],14);
		STEP_F3(C,D,E,A,B,X[ 2], 7);
		STEP_F3(B,C,D,E,A,X[11],12);
		STEP_F3(A,B,C,D,E,X[24], 9);
		STEP_F3(E,A,B,C,D,X[ 4],11);
		STEP_F3(D,E,A,B,C,X[13], 8);
		STEP_F3(C,D,E,A,B,X[ 6],15);
		STEP_F3(B,C,D,E,A,X[15], 6);
		STEP_F3(A,B,C,D,E,X[25],12);
		STEP_F3(E,A,B,C,D,X[ 8], 9);
		STEP_F3(D,E,A,B,C,X[ 1],14);
		STEP_F3(C,D,E,A,B,X[10], 5);
		STEP_F3(B,C,D,E,A,X[ 3],13);

		STEP_F4(A,B,C,D,E,X[30], 5);
		STEP_F4(E,A,B,C,D,X[ 7],11);
		STEP_F4(D,E,A,B,C,X[ 2], 7);
		STEP_F4(C,D,E,A,B,X[13],15);
		STEP_F4(B,C,D,E,A,X[ 8], 6);
		STEP_F4(A,B,C,D,E,X[31],13);
		STEP_F4(E,A,B,C,D,X[ 3], 8);
		STEP_F4(D,E,A,B,C,X[14],14);
		STEP_F4(C,D,E,A,B,X[ 9], 7);
		STEP_F4(B,C,D,E,A,X[ 4],12);
		STEP_F4(A,B,C,D,E,X[28], 9);
		STEP_F4(E,A,B,C,D,X[15],11);
		STEP_F4(D,E,A,B,C,X[10], 8);
		STEP_F4(C,D,E,A,B,X[ 5],15);
		STEP_F4(B,C,D,E,A,X[ 0], 6);
		STEP_F4(A,B,C,D,E,X[29],12);
		STEP_F4(E,A,B,C,D,X[11], 9);
		STEP_F4(D,E,A,B,C,X[ 6],14);
		STEP_F4(C,D,E,A,B,X[ 1], 5);
		STEP_F4(B,C,D,E,A,X[12],13);

		hash[0] += A;
		hash[1] += B;
		hash[2] += C;
		hash[3] += D;
		hash[4] += E;
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
void rhash_has160_update(has160_ctx* ctx, const unsigned char* msg, size_t size)
{
	unsigned index = (unsigned)ctx->length & 63;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		unsigned left = has160_block_size - index;
		memcpy((char*)ctx->message + index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_has160_process_block(ctx->hash, ctx->message);
		msg  += left;
		size -= left;
	}
	while (size >= has160_block_size) {
		unsigned* aligned_message_block;
		if (IS_ALIGNED_32(msg)) {
			/* the most common case is processing a 32-bit aligned message
			without copying it */
			aligned_message_block = (unsigned*)msg;
		} else {
			memcpy(ctx->message, msg, has160_block_size);
			aligned_message_block = ctx->message;
		}

		rhash_has160_process_block(ctx->hash, aligned_message_block);
		msg  += has160_block_size;
		size -= has160_block_size;
	}
	if (size) {
		/* save leftovers */
		memcpy(ctx->message, msg, size);
	}
}

/**
 * Compute and save calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_has160_final(has160_ctx* ctx, unsigned char* result)
{
	unsigned shift = ((unsigned)ctx->length & 3) * 8;
	unsigned index = ((unsigned)ctx->length & 63) >> 2;

	/* pad message and run for last block */
#if IS_LITTLE_ENDIAN
	ctx->message[index]   &= ~(0xFFFFFFFFu << shift);
	ctx->message[index++] ^= 0x80u << shift;
#else
	ctx->message[index]   &= ~(0xFFFFFFFFu >> shift);
	ctx->message[index++] ^= 0x80000000u >> shift;
#endif

	/* if no room left in the message to store 64-bit message length */
	if (index > 14) {
		/* then fill the rest with zeros and process it */
		while (index < 16) {
			ctx->message[index++] = 0;
		}
		rhash_has160_process_block(ctx->hash, ctx->message);
		index = 0;
	}
	while (index < 14) {
		ctx->message[index++] = 0;
	}
	ctx->message[14] = le2me_32( (unsigned)(ctx->length << 3)  );
	ctx->message[15] = le2me_32( (unsigned)(ctx->length >> 29) );
	rhash_has160_process_block(ctx->hash, ctx->message);

	le32_copy(result, 0, &ctx->hash, has160_hash_size);
}
