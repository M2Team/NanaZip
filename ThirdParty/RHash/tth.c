/* tth.c - calculate TTH (Tiger Tree Hash) function.
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

#include "tth.h"
#include "byte_order.h"
#include <stddef.h>
#include <string.h>

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_tth_init(tth_ctx* ctx)
{
	rhash_tiger_init(&ctx->tiger);
	ctx->tiger.message[ ctx->tiger.length++ ] = 0x00;
	ctx->block_count = 0;
}

/**
 * The core transformation.
 *
 * @param ctx algorithm state
 */
static void rhash_tth_process_block(tth_ctx* ctx)
{
	uint64_t it;
	unsigned pos = 0;
	unsigned char msg[24];

	for (it = 1; it & ctx->block_count; it <<= 1) {
		rhash_tiger_final(&ctx->tiger, msg);
		rhash_tiger_init(&ctx->tiger);
		ctx->tiger.message[ctx->tiger.length++] = 0x01;
		rhash_tiger_update(&ctx->tiger, (unsigned char*)(ctx->stack + pos), 24);
		/* note: we can cut this step, if the previous rhash_tiger_final saves directly to ctx->tiger.message+25; */
		rhash_tiger_update(&ctx->tiger, msg, 24);
		pos += 3;
	}
	rhash_tiger_final(&ctx->tiger, (unsigned char*)(ctx->stack + pos));
	ctx->block_count++;
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_tth_update(tth_ctx* ctx, const unsigned char* msg, size_t size)
{
	size_t rest = 1025 - (size_t)ctx->tiger.length;
	for (;;) {
		if (size < rest) rest = size;
		rhash_tiger_update(&ctx->tiger, msg, rest);
		msg += rest;
		size -= rest;
		if (ctx->tiger.length < 1025) {
			return;
		}

		/* process block hash */
		rhash_tth_process_block(ctx);

		/* init block hash */
		rhash_tiger_init(&ctx->tiger);
		ctx->tiger.message[ ctx->tiger.length++ ] = 0x00;
		rest = 1024;
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_tth_final(tth_ctx* ctx, unsigned char result[24])
{
	uint64_t it = 1;
	unsigned pos = 0;
	unsigned char msg[24];
	const unsigned char* last_message;

	/* process the bytes left in the context buffer */
	if (ctx->tiger.length > 1 || ctx->block_count == 0) {
		rhash_tth_process_block(ctx);
	}

	for (; it < ctx->block_count && (it & ctx->block_count) == 0; it <<= 1) pos += 3;
	last_message = (unsigned char*)(ctx->stack + pos);

	for (it <<= 1; it <= ctx->block_count; it <<= 1) {
		/* merge TTH sums in the tree */
		pos += 3;
		if (it & ctx->block_count) {
			rhash_tiger_init(&ctx->tiger);
			ctx->tiger.message[ ctx->tiger.length++ ] = 0x01;
			rhash_tiger_update(&ctx->tiger, (unsigned char*)(ctx->stack + pos), 24);
			rhash_tiger_update(&ctx->tiger, last_message, 24);

			rhash_tiger_final(&ctx->tiger, msg);
			last_message = msg;
		}
	}

	/* save result hash */
	memcpy(ctx->tiger.hash, last_message, tiger_hash_length);
	if (result) memcpy(result, last_message, tiger_hash_length);
}

static size_t tth_get_stack_size(uint64_t block_count)
{
	size_t stack_size = 0;
	for (; block_count; block_count >>= 1)
		stack_size += 24;
	return stack_size;
}

#if !defined(NO_IMPORT_EXPORT)
/**
 * Export tth context to a memory region, or calculate the
 * size required for context export.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param out pointer to the memory region or NULL
 * @param size size of memory region
 * @return the size of the exported data on success, 0 on fail.
 */
size_t rhash_tth_export(const tth_ctx* ctx, void* out, size_t size)
{
	size_t export_size = offsetof(tth_ctx, stack) +
		tth_get_stack_size(ctx->block_count);
	if (out != NULL) {
		if (size < export_size)
			return 0;
		memcpy(out, ctx, export_size);
	}
	return export_size;
}

/**
 * Import tth context from a memory region.
 *
 * @param ctx pointer to the algorithm context
 * @param in pointer to the data to import
 * @param size size of data to import
 * @return the size of the imported data on success, 0 on fail.
 */
size_t rhash_tth_import(tth_ctx* ctx, const void* in, size_t size)
{
	const size_t head_size = offsetof(tth_ctx, stack);
	size_t stack_size;
	if (size < head_size)
		return 0;
	memset(ctx, 0, sizeof(tth_ctx));
	memcpy(ctx, in, head_size);
	stack_size = tth_get_stack_size(ctx->block_count);
	if (size < (head_size + stack_size))
		return 0;
	memcpy(ctx->stack, (const char*)in + head_size, stack_size);
	return head_size + stack_size;
}
#endif /* !defined(NO_IMPORT_EXPORT) */
