/* ed2k.c - an implementation of EDonkey 2000 Hash Algorithm.
 *
 * Copyright (c) 2006, Aleksey Kravchenko <rhash.admin@gmail.com>
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
 * This file implements eMule-compatible version of algorithm.
 * Note that eDonkey and eMule ed2k hashes are different only for
 * files containing exactly multiple of 9728000 bytes.
 *
 * The file data is divided into full chunks of 9500 KiB (9728000 bytes) plus
 * a remainder chunk, and a separate 128-bit MD4 hash is computed for each.
 * If the file length is an exact multiple of 9500 KiB, the remainder zero
 * size chunk is still used at the end of the hash list. The ed2k hash is
 * computed by concatenating the chunks' MD4 hashes in order and hashing the
 * result using MD4. Although, if the file is composed of a single non-full
 * chunk, its MD4 hash is returned with no further modifications.
 *
 * See http://en.wikipedia.org/wiki/EDonkey_network for algorithm description.
 */

#include <string.h>
#include "ed2k.h"

/* each hashed file is divided into 9500 KiB sized chunks */
#define ED2K_CHUNK_SIZE 9728000

/**
 * Initialize context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_ed2k_init(ed2k_ctx* ctx)
{
	rhash_md4_init(&ctx->md4_context);
	rhash_md4_init(&ctx->md4_context_inner);
	ctx->not_emule = 0;
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void rhash_ed2k_update(ed2k_ctx* ctx, const unsigned char* msg, size_t size)
{
	unsigned char chunk_md4_hash[16];
	unsigned blockleft = ED2K_CHUNK_SIZE - (unsigned)ctx->md4_context_inner.length;

	/* note: eMule-compatible algorithm hashes by md4_inner
	* the messages which sizes are multiple of 9728000
	* and then processes obtained hash by external md4 */

	while ( size >= blockleft )
	{
		if (size == blockleft && ctx->not_emule) break;

		/* if internal ed2k chunk is full, then finalize it */
		rhash_md4_update(&ctx->md4_context_inner, msg, blockleft);
		msg += blockleft;
		size -= blockleft;
		blockleft = ED2K_CHUNK_SIZE;

		/* just finished an ed2k chunk, updating md4_external context */
		rhash_md4_final(&ctx->md4_context_inner, chunk_md4_hash);
		rhash_md4_update(&ctx->md4_context, chunk_md4_hash, 16);
		rhash_md4_init(&ctx->md4_context_inner);
	}

	if (size) {
		/* hash leftovers */
		rhash_md4_update(&ctx->md4_context_inner, msg, size);
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_ed2k_final(ed2k_ctx* ctx, unsigned char result[16])
{
	/* check if hashed message size is greater or equal to ED2K_CHUNK_SIZE */
	if ( ctx->md4_context.length ) {

		/* note: weird eMule algorithm always processes the inner
		 * md4 context, no matter if it contains data or is empty */

		/* if any data are left in the md4_context_inner */
		if ( (size_t)ctx->md4_context_inner.length > 0 || !ctx->not_emule)
		{
			/* emule algorithm processes aditional block, even if it's empty */
			unsigned char md4_digest_inner[16];
			rhash_md4_final(&ctx->md4_context_inner, md4_digest_inner);
			rhash_md4_update(&ctx->md4_context, md4_digest_inner, 16);
		}
		/* first call final to flush md4 buffer and finalize the hash value */
		rhash_md4_final(&ctx->md4_context, result);
		/* store the calculated ed2k hash in the md4_context_inner.hash */
		memcpy(&ctx->md4_context_inner.hash, &ctx->md4_context.hash, md4_hash_size);
	} else {
		/* return just the message MD4 hash */
		if (result) rhash_md4_final(&ctx->md4_context_inner, result);
	}
}
