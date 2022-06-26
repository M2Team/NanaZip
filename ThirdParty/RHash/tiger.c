/* tiger.c - an implementation of Tiger Hash Function
 * based on the article by
 * Ross Anderson and Eli Biham "Tiger: A Fast New Hash Function".
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
#include "tiger.h"

#ifdef NO_TIGER2
# define TIGER2_FLAG I64(0)
#else
# define TIGER2_FLAG I64(0x8000000000000000)
#endif /* NO_TIGER2 */

#define LENGTH_MASK (~TIGER2_FLAG)
#define INITIALIZE_TIGER_STATE(state) \
	state[0] = I64(0x0123456789ABCDEF); \
	state[1] = I64(0xFEDCBA9876543210); \
	state[2] = I64(0xF096A5B4C3B2E187);

/**
 * Initialize Tiger context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_tiger_init(tiger_ctx* ctx)
{
	ctx->length = 0;
	INITIALIZE_TIGER_STATE(ctx->hash);
}

#ifndef NO_TIGER2
/**
 * Initialize Tiger2 context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_tiger2_init(tiger_ctx* ctx)
{
	ctx->length = TIGER2_FLAG;
	INITIALIZE_TIGER_STATE(ctx->hash);
}
#endif /* NO_TIGER2 */

/* lookup tables */
extern uint64_t rhash_tiger_sboxes[4][256];
#define t1 rhash_tiger_sboxes[0]
#define t2 rhash_tiger_sboxes[1]
#define t3 rhash_tiger_sboxes[2]
#define t4 rhash_tiger_sboxes[3]

#ifdef CPU_X64 /* for x86-64 */
#define round(a,b,c,x,mul) \
	c ^= x; \
	a -= t1[(uint8_t)(c)] ^ \
		t2[(uint8_t)((c) >> (2 * 8))] ^ \
		t3[(uint8_t)((c) >> (4 * 8))] ^ \
		t4[(uint8_t)((c) >> (6 * 8))] ; \
	b += t4[(uint8_t)((c) >> (1 * 8))] ^ \
		t3[(uint8_t)((c) >> (3 * 8))] ^ \
		t2[(uint8_t)((c) >> (5 * 8))] ^ \
		t1[(uint8_t)((c) >> (7 * 8))]; \
	b *= mul;

#else /* for IA32 */

#define round(a,b,c,x,mul) \
	c ^= x; \
	a -= t1[(uint8_t)(c)] ^ \
		t2[(uint8_t)(((uint32_t)(c)) >> (2 * 8))] ^ \
		t3[(uint8_t)((c) >> (4 * 8))] ^ \
		t4[(uint8_t)(((uint32_t)((c) >> (4 * 8))) >> (2 * 8))] ; \
	b += t4[(uint8_t)(((uint32_t)(c)) >> (1 * 8))] ^ \
		t3[(uint8_t)(((uint32_t)(c)) >> (3 * 8))] ^ \
		t2[(uint8_t)(((uint32_t)((c) >> (4 * 8))) >> (1 * 8))] ^ \
		t1[(uint8_t)(((uint32_t)((c) >> (4 * 8))) >> (3 * 8))]; \
	b *= mul;
#endif /* CPU_X64 */

#define pass(a,b,c,mul) \
	round(a,b,c,x0,mul) \
	round(b,c,a,x1,mul) \
	round(c,a,b,x2,mul) \
	round(a,b,c,x3,mul) \
	round(b,c,a,x4,mul) \
	round(c,a,b,x5,mul) \
	round(a,b,c,x6,mul) \
	round(b,c,a,x7,mul)

#define key_schedule { \
	x0 -= x7 ^ I64(0xA5A5A5A5A5A5A5A5); \
	x1 ^= x0; \
	x2 += x1; \
	x3 -= x2 ^ ((~x1)<<19); \
	x4 ^= x3; \
	x5 += x4; \
	x6 -= x5 ^ ((~x4)>>23); \
	x7 ^= x6; \
	x0 += x7; \
	x1 -= x0 ^ ((~x7)<<19); \
	x2 ^= x1; \
	x3 += x2; \
	x4 -= x3 ^ ((~x2)>>23); \
	x5 ^= x4; \
	x6 += x5; \
	x7 -= x6 ^ I64(0x0123456789ABCDEF); \
}

/**
 * The core transformation. Process a 512-bit block.
 *
 * @param state the algorithm state
 * @param block the message block to process
 */
static void rhash_tiger_process_block(uint64_t state[3], uint64_t* block)
{
	/* Optimized for GCC IA32.
	   The order of declarations is important for compiler. */
	uint64_t a, b, c;
	uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
#ifndef CPU_X64
	uint64_t tmp;
	char i;
#endif

	x0 = le2me_64(block[0]); x1 = le2me_64(block[1]);
	x2 = le2me_64(block[2]); x3 = le2me_64(block[3]);
	x4 = le2me_64(block[4]); x5 = le2me_64(block[5]);
	x6 = le2me_64(block[6]); x7 = le2me_64(block[7]);

	a = state[0];
	b = state[1];
	c = state[2];

	/* passes and key shedules */
#ifndef CPU_X64
	for (i = 0; i < 3; i++) {
		if (i != 0) key_schedule;
		pass(a, b, c, (i == 0 ? 5 : i == 1 ? 7 : 9));
		tmp = a;
		a = c;
		c = b;
		b = tmp;
	}
#else
	pass(a, b, c, 5);
	key_schedule;
	pass(c, a, b, 7);
	key_schedule;
	pass(b, c, a, 9);
#endif

	/* feedforward operation */
	state[0] = a ^ state[0];
	state[1] = b - state[1];
	state[2] = c + state[2];
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_tiger_update(tiger_ctx* ctx, const unsigned char* msg, size_t size)
{
	size_t index = (size_t)ctx->length & 63;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		size_t left = tiger_block_size - index;
		if (size < left) {
			if (size > 0)
				memcpy(ctx->message + index, msg, size);
			return;
		} else {
			memcpy(ctx->message + index, msg, left);
			rhash_tiger_process_block(ctx->hash, (uint64_t*)ctx->message);
			msg += left;
			size -= left;
		}
	}
	while (size >= tiger_block_size) {
		if (IS_ALIGNED_64(msg)) {
			/* the most common case is processing of an already aligned message
			without copying it */
			rhash_tiger_process_block(ctx->hash, (uint64_t*)msg);
		} else {
			memcpy(ctx->message, msg, tiger_block_size);
			rhash_tiger_process_block(ctx->hash, (uint64_t*)ctx->message);
		}

		msg += tiger_block_size;
		size -= tiger_block_size;
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
void rhash_tiger_final(tiger_ctx* ctx, unsigned char result[24])
{
	unsigned index = (unsigned)ctx->length & 63;
	uint64_t* msg64 = (uint64_t*)ctx->message;

	/* pad message and run for last block */

	/* append the byte 0x01 to the message */
	ctx->message[index++] = (ctx->length & TIGER2_FLAG ? 0x80 : 0x01);

	/* if no room left in the message to store 64-bit message length */
	if (index > 56) {
		/* then fill the rest with zeros and process it */
		while (index < 64) {
			ctx->message[index++] = 0;
		}
		rhash_tiger_process_block(ctx->hash, msg64);
		index = 0;
	}
	while (index < 56) {
		ctx->message[index++] = 0;
	}
	msg64[7] = le2me_64((ctx->length & LENGTH_MASK) << 3);
	rhash_tiger_process_block(ctx->hash, msg64);

	/* save result hash */
	le64_copy(result, 0, &ctx->hash, 24);
}
