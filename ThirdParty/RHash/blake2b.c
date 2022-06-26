/* blake2b.c - an implementation of blake2b hash function.
 *
 * Copyright (c) 2012, Samuel Neves <sneves@dei.uc.pt>
 * Copyright (c) 2021, Aleksey Kravchenko <rhash.admin@gmail.com>
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

#include "blake2b.h"
#include "byte_order.h"
#include <string.h>

static const uint64_t blake2b_IV[8] =
{
	I64(0x6a09e667f3bcc908), I64(0xbb67ae8584caa73b),
	I64(0x3c6ef372fe94f82b), I64(0xa54ff53a5f1d36f1),
	I64(0x510e527fade682d1), I64(0x9b05688c2b3e6c1f),
	I64(0x1f83d9abfb41bd6b), I64(0x5be0cd19137e2179)
};

static const uint8_t blake2b_sigma[12][16] =
{
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};

void rhash_blake2b_init(blake2b_ctx* ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	/* init state by xoring IV with blake2b input parameter block */
	ctx->hash[0] = blake2b_IV[0] ^ I64(0x01010040);
	ctx->hash[1] = blake2b_IV[1];
	ctx->hash[2] = blake2b_IV[2];
	ctx->hash[3] = blake2b_IV[3];
	ctx->hash[4] = blake2b_IV[4];
	ctx->hash[5] = blake2b_IV[5];
	ctx->hash[6] = blake2b_IV[6];
	ctx->hash[7] = blake2b_IV[7];
}

#define G(r,i,a,b,c,d) \
	do { \
		a = a + b + m[blake2b_sigma[r][2*i+0]]; \
		d = ROTR64(d ^ a, 32); \
		c = c + d; \
		b = ROTR64(b ^ c, 24); \
		a = a + b + m[blake2b_sigma[r][2*i+1]]; \
		d = ROTR64(d ^ a, 16); \
		c = c + d; \
		b = ROTR64(b ^ c, 63); \
	} while(0)

#define ROUND(r) \
	do { \
		G(r,0,v[0],v[4],v[ 8],v[12]); \
		G(r,1,v[1],v[5],v[ 9],v[13]); \
		G(r,2,v[2],v[6],v[10],v[14]); \
		G(r,3,v[3],v[7],v[11],v[15]); \
		G(r,4,v[0],v[5],v[10],v[15]); \
		G(r,5,v[1],v[6],v[11],v[12]); \
		G(r,6,v[2],v[7],v[ 8],v[13]); \
		G(r,7,v[3],v[4],v[ 9],v[14]); \
	} while(0)

static void rhash_blake2b_process_block(blake2b_ctx* ctx, const uint64_t* m, uint64_t finalization_flag)
{
	uint64_t v[16];
	size_t i;

	memcpy(v, ctx->hash, sizeof(uint64_t) * 8);
	v[ 8] = blake2b_IV[0];
	v[ 9] = blake2b_IV[1];
	v[10] = blake2b_IV[2];
	v[11] = blake2b_IV[3];
	v[12] = blake2b_IV[4] ^ ctx->length; /* length correction */
	v[13] = blake2b_IV[5];
	v[14] = blake2b_IV[6] ^ finalization_flag;
	v[15] = blake2b_IV[7];

	ROUND(0);
	ROUND(1);
	ROUND(2);
	ROUND(3);
	ROUND(4);
	ROUND(5);
	ROUND(6);
	ROUND(7);
	ROUND(8);
	ROUND(9);
	ROUND(10);
	ROUND(11);

	for(i = 0; i < 8; ++i)
		ctx->hash[i] ^= v[i] ^ v[i + 8];
}

void rhash_blake2b_update(blake2b_ctx* ctx, const unsigned char* msg, size_t size)
{
	if(size > 0)
	{
		size_t index = (size_t)ctx->length & 127;
		if(index)
		{
			size_t rest = blake2b_block_size - index;
			if (size > rest) {
				le64_copy(ctx->message, index, msg, rest); /* fill the block */

				/* process the block */
				size -= rest;
				msg += rest;
				ctx->length += rest;
				index = 0;
				rhash_blake2b_process_block(ctx, ctx->message, I64(0));
			}
		} else if (ctx->length) {
			rhash_blake2b_process_block(ctx, ctx->message, I64(0));
		}
		while(size > blake2b_block_size) {
			uint64_t* aligned_message_block;
			if (IS_LITTLE_ENDIAN && IS_ALIGNED_64(msg)) {
				aligned_message_block = (uint64_t*)msg;
			} else {
				le64_copy(ctx->message, 0, msg, blake2b_block_size);
				aligned_message_block = ctx->message;
			}
			size -= blake2b_block_size;
			msg += blake2b_block_size;
			ctx->length += blake2b_block_size;
			rhash_blake2b_process_block(ctx, aligned_message_block, I64(0));
		}
		le64_copy(ctx->message, index, msg, size); /* save leftovers */
		ctx->length += size;
	}
}

void rhash_blake2b_final(blake2b_ctx* ctx, unsigned char *result)
{
	size_t length = (size_t)ctx->length & 127;
	if (length)
	{
		/* pad the message with zeros */
		size_t index = length >> 3;
		size_t shift = (length & 7) * 8;
		ctx->message[index] &= ~(I64(0xFFFFFFFFFFFFFFFF) << shift);
		for(index++; index < 16; index++)
			ctx->message[index] = 0;
	}
	rhash_blake2b_process_block(ctx, ctx->message, I64(0xFFFFFFFFFFFFFFFF));

	/* convert hash state to result bytes */
	le64_copy(result, 0, ctx->hash, blake2b_hash_size);
}
