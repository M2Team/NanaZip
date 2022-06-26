/* aich.c - an implementation of EMule AICH Algorithm.
 * Description: http://www.amule.org/wiki/index.php/AICH.
 *
 * Copyright (c) 2008, Aleksey Kravchenko <rhash.admin@gmail.com>
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
 * The AICH Algorithm:
 *
 * Each ed2k chunk (9728000 bytes) is divided into 53 parts (52x 180KB and
 * 1x 140KB) and each of these parts are hashed using the SHA1 algorithm.
 * Each of these hashes is called a Block Hash. By combining pairs of Block
 * Hashes (i.e. each part with the part next to it) algorithm will get a whole
 * tree of hashes (this tree which is therefore a hashset made of all of the
 * other Block Hashes is called the AICH Hashset). Each hash which is neither
 * a Block Hash nor the Root Hash, is a Verifying Hash. The hash at the top
 * level is the Root Hash and it is supposed to be provided by the ed2k link
 * when releasing.
 */

#include "aich.h"
#include "util.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(USE_OPENSSL)
#define SHA1_INIT(ctx) ((pinit_t)ctx->sha1_methods.init)(&ctx->sha1_context)
#define SHA1_UPDATE(ctx, msg, size) ((pupdate_t)ctx->sha1_methods.update)(&ctx->sha1_context, (msg), (size))
#define SHA1_FINAL(ctx, result) ((pfinal_t)ctx->sha1_methods.final)(&ctx->sha1_context, (result))
#else
# define SHA1_INIT(ctx) rhash_sha1_init(&ctx->sha1_context)
# define SHA1_UPDATE(ctx, msg, size) rhash_sha1_update(&ctx->sha1_context, (msg), (size))
# define SHA1_FINAL(ctx, result) rhash_sha1_final(&ctx->sha1_context, (result))
#endif

#define ED2K_CHUNK_SIZE  9728000
#define FULL_BLOCK_SIZE  184320
#define LAST_BLOCK_SIZE  143360
#define BLOCKS_PER_CHUNK 53
#define BLOCK_HASHES_SIZE (BLOCKS_PER_CHUNK * sha1_hash_size)

/*
 * AICH algorithm could be implemented a bit faster if it knows
 * a hashed message size beforehand. It would allow
 * to build balanced tree while hashing the message.
 *
 * This AICH implementation works with unknown
 * message size like other well-known hash algorithms.
 * So, it just stores sha1 hashes and builds balanced tree
 * only on the last step, when the full message processed
 * and its size got to be known.
 */

/**
 * Initialize algorithm context before calculating hash.
 *
 * @param ctx context to initialize
 */
void rhash_aich_init(aich_ctx* ctx)
{
	memset(ctx, 0, sizeof(aich_ctx));

#if defined(USE_OPENSSL)
	assert(rhash_info_table[3].info->hash_id == RHASH_SHA1);
	assert(rhash_info_table[3].context_size <= (sizeof(sha1_ctx) + sizeof(unsigned long)));
	rhash_load_sha1_methods(&ctx->sha1_methods, METHODS_SELECTED);
#endif

	SHA1_INIT(ctx);
}

/* define macrosses to access chunk table */
#define CT_BITS 8
#define CT_GROUP_SIZE (1 << CT_BITS)
typedef unsigned char hash_pair_t[2][sha1_hash_size];
typedef hash_pair_t hash_pairs_group_t[CT_GROUP_SIZE];

#define CT_INDEX(chunk_num) ((chunk_num) & (CT_GROUP_SIZE - 1))
#define GET_HASH_PAIR(ctx, chunk_num) \
	(((hash_pair_t*)(ctx->chunk_table[chunk_num >> CT_BITS]))[CT_INDEX(chunk_num)])

/**
 * Resize the table if needed to ensure it contains space for given chunk_num.
 * and allocate hash_pairs_group_t element at this index.
 *
 * @param ctx algorithm context
 * @param chunk_num the number of chunks required
 */
static void rhash_aich_chunk_table_extend(aich_ctx* ctx, unsigned chunk_num)
{
	unsigned index = (chunk_num >> CT_BITS);
	assert(CT_INDEX(chunk_num) == 0);
	RHASH_ASSERT(sizeof(hash_pair_t) == 40);
	RHASH_ASSERT(sizeof(hash_pairs_group_t) == (40 * CT_GROUP_SIZE)); /* 10KiB */
	RHASH_ASSERT(CT_GROUP_SIZE == 256);

	/* check main assumptions */
	assert(ctx->chunk_table == 0 || ctx->chunk_table[index - 1] != 0); /* table is empty or full */
	assert(index <= ctx->allocated);

	/* check if there is enough space allocated */
	if (index >= ctx->allocated) {
		/* resize the table by allocating some extra space */
		size_t new_size = (ctx->allocated == 0 ? 64 : ctx->allocated * 2);
		void** new_block;
		assert(index == ctx->allocated);

		/* re-size the chunk table to new_size */
		new_block = (void**)realloc(ctx->chunk_table, new_size * sizeof(void*));
		if (new_block == 0) {
			free(ctx->chunk_table);
			ctx->chunk_table = 0;
			ctx->error = 1;
			return;
		}

		memset(new_block + ctx->allocated, 0, (new_size - ctx->allocated) * sizeof(void*));
		ctx->chunk_table = new_block;
		ctx->allocated = new_size;
	}

	/* add new hash_pairs_group_t block to the table */
	assert(index < ctx->allocated);
	assert(ctx->chunk_table != 0);
	assert(ctx->chunk_table[index] == 0);

	ctx->chunk_table[index] = malloc(sizeof(hash_pairs_group_t));
	if (ctx->chunk_table[index] == 0)
		ctx->error = 1;
}

/**
 * Free dynamically allocated memory for internal structures
 * used by hashing algorithm.
 *
 * @param ctx AICH algorithm context to cleanup
 */
void rhash_aich_cleanup(aich_ctx* ctx)
{
	size_t i;
	size_t table_size = (ctx->chunks_count + CT_GROUP_SIZE - 1) / CT_GROUP_SIZE;

	if (ctx->chunk_table != 0) {
		assert(table_size <= ctx->allocated);
		assert(table_size == ctx->allocated || ctx->chunk_table[table_size] == 0);
		for (i = 0;  i < table_size; i++)
			free(ctx->chunk_table[i]);
		free(ctx->chunk_table);
		ctx->chunk_table = 0;
	}

	free(ctx->block_hashes);
	ctx->block_hashes = 0;
}

#define AICH_HASH_FULL_TREE 0
#define AICH_HASH_LEFT_BRANCH 1
#define AICH_HASH_RIGHT_BRANCH 2

/**
 * Calculate an AICH tree hash, based ether on hashes of 180KB parts
 * (for an ed2k chunk) or on stored ed2k chunks (for the whole tree hash).
 *
 * @param ctx algorithm context
 * @param result pointer to receive calculated tree hash
 * @param type the type of hash to calculate, can be one of constants
 *   AICH_HASH_LEFT_BRANCH, AICH_HASH_RIGHT_BRANCH or AICH_HASH_FULL_TREE.
 */
static void rhash_aich_hash_tree(aich_ctx* ctx, unsigned char* result, int type)
{
	unsigned index = 0; /* leaf index */
	unsigned blocks;
	int      level = 0;
	unsigned is_left_branch = (type == AICH_HASH_RIGHT_BRANCH ? 0x0 : 0x1);
	uint64_t path  = is_left_branch;
	unsigned blocks_stack[56];
	unsigned char sha1_stack[56][sha1_hash_size];

	if (ctx->error)
		return;
	assert(ctx->index <= ED2K_CHUNK_SIZE);
	assert(type == AICH_HASH_FULL_TREE ? ctx->chunk_table != 0 : ctx->block_hashes != 0);

	/* calculate number of leafs in the tree */
	blocks_stack[0] = blocks = (unsigned)(type == AICH_HASH_FULL_TREE ?
		ctx->chunks_count : (ctx->index + FULL_BLOCK_SIZE - 1) / FULL_BLOCK_SIZE);

	while (1) {
		unsigned char sha1_message[sha1_hash_size];
		unsigned char* leaf_hash;

		/* go into the left branches until a leaf block is reached */
		while (blocks > 1) {
			/* step down into the left branch */
			blocks = (blocks + ((unsigned)path & 0x1)) / 2;
			level++;
			assert(level < 56); /* assumption filesize < (2^56 * 9MiB) */
			blocks_stack[level] = blocks;
			path = (path << 1) | 0x1; /* mark branch as left */
		}

		/* read a leaf hash */
		leaf_hash = &(ctx->block_hashes[index][0]);

		if (type == AICH_HASH_FULL_TREE) {
			is_left_branch = (unsigned)path & 0x1;

			leaf_hash = GET_HASH_PAIR(ctx, index)[is_left_branch];
		}
		index++;

		/* climb up the tree until a left branch is reached */
		for (; level > 0 && (path & 0x01) == 0; path >>= 1) {
			SHA1_INIT(ctx);
			SHA1_UPDATE(ctx, sha1_stack[level], sha1_hash_size);
			SHA1_UPDATE(ctx, leaf_hash, sha1_hash_size);
			SHA1_FINAL(ctx, sha1_message);
			leaf_hash = sha1_message;
			level--;
		}
		memcpy((level > 0 ? sha1_stack[level] : result), leaf_hash, 20);

		if (level == 0) break;

		/* jump at the current level from left to right branch */
		path &= ~0x1; /* mark branch as right */
		is_left_branch = ((unsigned)path >> 1) & 1;

		/* calculate number of blocks at right branch of the current level */
		blocks_stack[level] =
			(blocks_stack[level - 1] + 1 - is_left_branch) / 2;
		blocks = blocks_stack[level];
	}
}

#define AICH_PROCESS_FINAL_BLOCK 1
#define AICH_PROCESS_FLUSH_BLOCK 2

/**
 * Calculate and store a hash for a 180K/140K block.
 * Also, if it is the last block of a 9.2MiB ed2k chunk or of the hashed message,
 * then also calculate the AICH tree-hash of the current ed2k chunk.
 *
 * @param ctx algorithm context
 * @param type the actions to take, can be combination of bits AICH_PROCESS_FINAL_BLOCK
 *             and AICH_PROCESS_FLUSH_BLOCK
 */
static void rhash_aich_process_block(aich_ctx* ctx, int type)
{
	assert(type != 0);
	assert(ctx->index <= ED2K_CHUNK_SIZE);

	/* if there is unprocessed data left in the current 180K block. */
	if ((type & AICH_PROCESS_FLUSH_BLOCK) != 0)
	{
		/* ensure that the block_hashes array is allocated to save the result */
		if (ctx->block_hashes == NULL) {
			ctx->block_hashes = (unsigned char (*)[sha1_hash_size])malloc(BLOCK_HASHES_SIZE);
			if (ctx->block_hashes == NULL) {
				ctx->error = 1;
				return;
			}
		}

		/* store the 180-KiB block hash to the block_hashes array */
		assert(((ctx->index - 1) / FULL_BLOCK_SIZE) < BLOCKS_PER_CHUNK);
		SHA1_FINAL(ctx, ctx->block_hashes[(ctx->index - 1) / FULL_BLOCK_SIZE]);
	}

	/* check, if it's time to calculate the tree hash for the current ed2k chunk */
	if (ctx->index >= ED2K_CHUNK_SIZE || (type & AICH_PROCESS_FINAL_BLOCK)) {
		unsigned char (*pair)[sha1_hash_size];

		/* ensure, that we have the space to store tree hash */
		if (CT_INDEX(ctx->chunks_count) == 0) {
			rhash_aich_chunk_table_extend(ctx, (unsigned)ctx->chunks_count);
			if (ctx->error)
				return;
		}
		assert(ctx->chunk_table  != 0);
		assert(ctx->block_hashes != 0);

		/* calculate tree hash and save results to chunk_table */
		pair = GET_HASH_PAIR(ctx, ctx->chunks_count);

		/* small optimization: skip a left-branch-hash for the last chunk */
		if (!(type & AICH_PROCESS_FINAL_BLOCK) || ctx->chunks_count == 0) {
			/* calculate a tree hash to be used in left branch */
			rhash_aich_hash_tree(ctx, pair[1], AICH_HASH_LEFT_BRANCH);
		}

		/* small optimization: skip right-branch-hash for the very first chunk */
		if (ctx->chunks_count > 0) {
			/* calculate a tree hash to be used in right branch */
			rhash_aich_hash_tree(ctx, pair[0], AICH_HASH_RIGHT_BRANCH);
		}

		ctx->index = 0; /* mark that the entire ed2k chunk has been processed */
		ctx->chunks_count++;
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
void rhash_aich_update(aich_ctx* ctx, const unsigned char* msg, size_t size)
{
	if (ctx->error)
		return;
	while (size > 0) {
		unsigned left_in_chunk = ED2K_CHUNK_SIZE - ctx->index;
		unsigned block_left = (left_in_chunk <= LAST_BLOCK_SIZE ? left_in_chunk :
			FULL_BLOCK_SIZE - ctx->index % FULL_BLOCK_SIZE);
		assert(block_left > 0);

		if (size >= block_left) {
			SHA1_UPDATE(ctx, msg, block_left);
			msg  += block_left;
			size -= block_left;
			ctx->index += block_left;

			/* process a 180KiB-blok */
			rhash_aich_process_block(ctx, AICH_PROCESS_FLUSH_BLOCK);
			SHA1_INIT(ctx);
		} else {
			/* add to a leaf block */
			SHA1_UPDATE(ctx, msg, size);
			ctx->index += (unsigned)size;
			break;
		}
	}
	assert(ctx->index < ED2K_CHUNK_SIZE);
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void rhash_aich_final(aich_ctx* ctx, unsigned char result[20])
{
	uint64_t total_size =
		((uint64_t)ctx->chunks_count * ED2K_CHUNK_SIZE) + ctx->index;
	unsigned char* const hash = (unsigned char*)ctx->sha1_context.hash;

	if (ctx->chunks_count == 0 && ctx->block_hashes == NULL) {
		assert(ctx->index < FULL_BLOCK_SIZE);
#if defined(USE_OPENSSL)
		SHA1_FINAL(ctx, hash); /* return just sha1 hash */
#else
		SHA1_FINAL(ctx, 0); /* return just sha1 hash */
#if IS_LITTLE_ENDIAN
		rhash_u32_mem_swap(ctx->sha1_context.hash, 5);
#endif
#endif
		if (result) memcpy(result, hash, sha1_hash_size);
		return;
	}

	/* if there is unprocessed data left in the last 180K block */
	if ((ctx->index % FULL_BLOCK_SIZE) > 0) {
		/* then process the last block */
		rhash_aich_process_block(ctx, ctx->block_hashes != NULL ?
			AICH_PROCESS_FINAL_BLOCK | AICH_PROCESS_FLUSH_BLOCK : AICH_PROCESS_FLUSH_BLOCK);
	}

	/* if processed message was shorter than a ed2k chunk */
	if (ctx->chunks_count == 0) {
		/* then return the aich hash for the first chunk */
		rhash_aich_hash_tree(ctx, hash, AICH_HASH_LEFT_BRANCH);
	} else {
		if (ctx->index > 0) {
			/* process the last block of the message */
			rhash_aich_process_block(ctx, AICH_PROCESS_FINAL_BLOCK);
		}
		assert(ctx->chunks_count > 0);
		assert(ctx->block_hashes != NULL);

		rhash_aich_hash_tree(ctx, hash, AICH_HASH_FULL_TREE);
	}

	rhash_aich_cleanup(ctx);
	ctx->sha1_context.length = total_size; /* store total message size  */
	if (result) memcpy(result, hash, sha1_hash_size);
}

#if !defined(NO_IMPORT_EXPORT)
# define AICH_CTX_OSSL_FLAG 0x10

/**
 * Export aich context to a memory region, or calculate the
 * size required for context export.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param out pointer to the memory region or NULL
 * @param size size of memory region
 * @return the size of the exported data on success, 0 on fail.
 */
size_t rhash_aich_export(const aich_ctx* ctx, void* out, size_t size)
{
	const size_t head_size = sizeof(size_t);
	const size_t ctx_head_size = offsetof(aich_ctx, block_hashes);
	const size_t block_hashes_size = (ctx->block_hashes ? BLOCK_HASHES_SIZE : 0);
	const size_t chunk_table_size = sizeof(hash_pair_t) * ctx->chunks_count;
	const size_t exported_size = head_size + ctx_head_size + block_hashes_size + chunk_table_size;
	char* out_ptr = (char*)out;
	if (!out)
		return exported_size;
	if (size < exported_size)
		return 0;
	*(size_t*)out_ptr = sizeof(aich_ctx);
	out_ptr += head_size;
	memcpy(out_ptr, ctx, ctx_head_size);
	out_ptr += ctx_head_size;
	if (ctx->block_hashes) {
		memcpy(out_ptr, ctx->block_hashes, BLOCK_HASHES_SIZE);
		out_ptr += BLOCK_HASHES_SIZE;
	}
	if (chunk_table_size > 0) {
		size_t left_size = chunk_table_size;
		size_t index;
		assert(ctx->chunk_table != NULL);
		for (index = 0; left_size > 0; index++) {
			size_t group_size = (left_size < sizeof(hash_pairs_group_t) ?
				left_size : sizeof(hash_pairs_group_t));
			memcpy(out_ptr, ctx->chunk_table[index], group_size);
			out_ptr += group_size;
			left_size -= group_size;
		}
		assert(left_size == 0);
	}
	assert(!out || (size_t)(out_ptr - (char*)out) == exported_size);
#if defined(USE_OPENSSL)
	if (out_ptr && ARE_OPENSSL_METHODS(ctx->sha1_methods)) {
		int* error_ptr = (int*)((char*)out + head_size + offsetof(aich_ctx, error));
		*error_ptr |= AICH_CTX_OSSL_FLAG;
	}
#endif
	return exported_size;
}

/**
 * Import aich context from a memory region.
 *
 * @param ctx pointer to the algorithm context
 * @param in pointer to the data to import
 * @param size size of data to import
 * @return the size of the imported data on success, 0 on fail.
 */
size_t rhash_aich_import(aich_ctx* ctx, const void* in, size_t size)
{
	const size_t head_size = sizeof(size_t);
	const size_t ctx_head_size = offsetof(aich_ctx, block_hashes);
	const char* in_ptr = (const char*)in;
	size_t imported_size = head_size + ctx_head_size;
	size_t block_hashes_size;
	size_t chunk_table_size;
	if (size < imported_size)
		return 0;
	if(*(size_t*)in_ptr != sizeof(aich_ctx))
		return 0;
	in_ptr += head_size;
	memset(ctx, 0, sizeof(aich_ctx));
	memcpy(ctx, in_ptr, ctx_head_size);
	in_ptr += ctx_head_size;
	block_hashes_size = (ctx->block_hashes ? BLOCK_HASHES_SIZE : 0);
	chunk_table_size = sizeof(hash_pair_t) * ctx->chunks_count;
	imported_size += block_hashes_size + chunk_table_size;
	if (size < imported_size)
		return 0;
	if (ctx->block_hashes != NULL) {
		ctx->block_hashes = (unsigned char (*)[sha1_hash_size])malloc(BLOCK_HASHES_SIZE);
		if (!ctx->block_hashes)
			return 0;
		memcpy(ctx->block_hashes, in_ptr, BLOCK_HASHES_SIZE);
		in_ptr += BLOCK_HASHES_SIZE;
	}
	if (ctx->allocated > 0) {
		size_t index;
		ctx->chunk_table = (void**)malloc(ctx->allocated * sizeof(void*));
		if (!ctx->chunk_table) {
			ctx->error = 1;
			return 0;
		}
		memset(ctx->chunk_table, 0, ctx->allocated * sizeof(void*));
		for (index = 0; chunk_table_size > 0; index++) {
			size_t group_size = (chunk_table_size < sizeof(hash_pairs_group_t) ?
				chunk_table_size : sizeof(hash_pairs_group_t));
			assert(index < ctx->allocated);
			ctx->chunk_table[index] = malloc(sizeof(hash_pairs_group_t));
			if (ctx->chunk_table[index] == 0) {
				ctx->error = 1;
				return 0;
			}
			memcpy(ctx->chunk_table[index], in_ptr, group_size);
			chunk_table_size -= group_size;
			in_ptr += group_size;
		}
	}
	assert((size_t)(in_ptr - (char*)in) == imported_size);
#if defined(USE_OPENSSL)
	if ((ctx->error & AICH_CTX_OSSL_FLAG) != 0) {
		ctx->error &= ~AICH_CTX_OSSL_FLAG;
		rhash_load_sha1_methods(&ctx->sha1_methods, METHODS_OPENSSL);
	} else {
		rhash_load_sha1_methods(&ctx->sha1_methods, METHODS_RHASH);
	}
#endif
	return imported_size;
}
#endif /* !defined(NO_IMPORT_EXPORT) */
