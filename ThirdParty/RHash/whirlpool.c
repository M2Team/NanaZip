/* whirlpool.c - an implementation of the Whirlpool Hash Function.
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
 * Documentation:
 * P. S. L. M. Barreto, V. Rijmen, ``The Whirlpool hashing function,''
 * NESSIE submission, 2000 (tweaked version, 2001)
 *
 * The algorithm is named after the Whirlpool Galaxy in Canes Venatici.
 */

#include <string.h>
#include "byte_order.h"
#include "whirlpool.h"

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_whirlpool_init(struct whirlpool_ctx* ctx)
{
	ctx->length = 0;
	memset(ctx->hash, 0, sizeof(ctx->hash));
}

/* Algorithm S-Box */
extern uint64_t rhash_whirlpool_sbox[8][256];

#define WHIRLPOOL_OP(src, shift) ( \
	rhash_whirlpool_sbox[0][(int)(src[ shift      & 7] >> 56)       ] ^ \
	rhash_whirlpool_sbox[1][(int)(src[(shift + 7) & 7] >> 48) & 0xff] ^ \
	rhash_whirlpool_sbox[2][(int)(src[(shift + 6) & 7] >> 40) & 0xff] ^ \
	rhash_whirlpool_sbox[3][(int)(src[(shift + 5) & 7] >> 32) & 0xff] ^ \
	rhash_whirlpool_sbox[4][(int)(src[(shift + 4) & 7] >> 24) & 0xff] ^ \
	rhash_whirlpool_sbox[5][(int)(src[(shift + 3) & 7] >> 16) & 0xff] ^ \
	rhash_whirlpool_sbox[6][(int)(src[(shift + 2) & 7] >>  8) & 0xff] ^ \
	rhash_whirlpool_sbox[7][(int)(src[(shift + 1) & 7]      ) & 0xff])

/**
 * The core transformation. Process a 512-bit block.
 *
 * @param hash algorithm state
 * @param block the message block to process
 */
static void rhash_whirlpool_process_block(uint64_t* hash, uint64_t* p_block)
{
	int i;                /* loop counter */
	uint64_t K[2][8];       /* key */
	uint64_t state[2][8];   /* state */

	/* alternating binary flags */
	unsigned int m = 0;

	/* the number of rounds of the internal dedicated block cipher */
	const int number_of_rounds = 10;

	/* array used in the rounds */
	static const uint64_t rc[10] = {
		I64(0x1823c6e887b8014f),
		I64(0x36a6d2f5796f9152),
		I64(0x60bc9b8ea30c7b35),
		I64(0x1de0d7c22e4bfe57),
		I64(0x157737e59ff04ada),
		I64(0x58c9290ab1a06b85),
		I64(0xbd5d10f4cb3e0567),
		I64(0xe427418ba77d95d8),
		I64(0xfbee7c66dd17479e),
		I64(0xca2dbf07ad5a8333)
	};

	/* map the message buffer to a block */
	for (i = 0; i < 8; i++) {
		/* store K^0 and xor it with the intermediate hash state */
		K[0][i] = hash[i];
		state[0][i] = be2me_64(p_block[i]) ^ hash[i];
		hash[i] = state[0][i];
	}

	/* iterate over algorithm rounds */
	for (i = 0; i < number_of_rounds; i++)
	{
		/* compute K^i from K^{i-1} */
		K[m ^ 1][0] = WHIRLPOOL_OP(K[m], 0) ^ rc[i];
		K[m ^ 1][1] = WHIRLPOOL_OP(K[m], 1);
		K[m ^ 1][2] = WHIRLPOOL_OP(K[m], 2);
		K[m ^ 1][3] = WHIRLPOOL_OP(K[m], 3);
		K[m ^ 1][4] = WHIRLPOOL_OP(K[m], 4);
		K[m ^ 1][5] = WHIRLPOOL_OP(K[m], 5);
		K[m ^ 1][6] = WHIRLPOOL_OP(K[m], 6);
		K[m ^ 1][7] = WHIRLPOOL_OP(K[m], 7);

		/* apply the i-th round transformation */
		state[m ^ 1][0] = WHIRLPOOL_OP(state[m], 0) ^ K[m ^ 1][0];
		state[m ^ 1][1] = WHIRLPOOL_OP(state[m], 1) ^ K[m ^ 1][1];
		state[m ^ 1][2] = WHIRLPOOL_OP(state[m], 2) ^ K[m ^ 1][2];
		state[m ^ 1][3] = WHIRLPOOL_OP(state[m], 3) ^ K[m ^ 1][3];
		state[m ^ 1][4] = WHIRLPOOL_OP(state[m], 4) ^ K[m ^ 1][4];
		state[m ^ 1][5] = WHIRLPOOL_OP(state[m], 5) ^ K[m ^ 1][5];
		state[m ^ 1][6] = WHIRLPOOL_OP(state[m], 6) ^ K[m ^ 1][6];
		state[m ^ 1][7] = WHIRLPOOL_OP(state[m], 7) ^ K[m ^ 1][7];

		m = m ^ 1;
	}

	/* apply the Miyaguchi-Preneel compression function */
	hash[0] ^= state[0][0];
	hash[1] ^= state[0][1];
	hash[2] ^= state[0][2];
	hash[3] ^= state[0][3];
	hash[4] ^= state[0][4];
	hash[5] ^= state[0][5];
	hash[6] ^= state[0][6];
	hash[7] ^= state[0][7];
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_whirlpool_update(whirlpool_ctx* ctx, const unsigned char* msg, size_t size)
{
	unsigned index = (unsigned)ctx->length & 63;
	unsigned left;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		left = whirlpool_block_size - index;
		memcpy(ctx->message + index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_whirlpool_process_block(ctx->hash, (uint64_t*)ctx->message);
		msg  += left;
		size -= left;
	}
	while (size >= whirlpool_block_size) {
		uint64_t* aligned_message_block;
		if (IS_ALIGNED_64(msg)) {
			/* the most common case is processing of an already aligned message
			without copying it */
			aligned_message_block = (uint64_t*)msg;
		} else {
			memcpy(ctx->message, msg, whirlpool_block_size);
			aligned_message_block = (uint64_t*)ctx->message;
		}

		rhash_whirlpool_process_block(ctx->hash, aligned_message_block);
		msg += whirlpool_block_size;
		size -= whirlpool_block_size;
	}
	if (size) {
		/* save leftovers */
		memcpy(ctx->message, msg, size);
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_whirlpool_final(whirlpool_ctx* ctx, unsigned char* result)
{
	unsigned index = (unsigned)ctx->length & 63;
	uint64_t* msg64 = (uint64_t*)ctx->message;

	/* pad message and run for last block */
	ctx->message[index++] = 0x80;

	/* if no room left in the message to store 256-bit message length */
	if (index > 32) {
		/* then pad the rest with zeros and process it */
		while (index < 64) {
			ctx->message[index++] = 0;
		}
		rhash_whirlpool_process_block(ctx->hash, msg64);
		index = 0;
	}
	/* due to optimization actually only 64-bit of message length are stored */
	while (index < 56) {
		ctx->message[index++] = 0;
	}
	msg64[7] = be2me_64(ctx->length << 3);
	rhash_whirlpool_process_block(ctx->hash, msg64);

	/* save result hash */
	be64_copy(result, 0, ctx->hash, 64);
}
