/* torrent.c - create BitTorrent files and calculate BitTorrent  InfoHash (BTIH).
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

#include "torrent.h"
#include "hex.h"
#include "util.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>  /* time() */
#include "byte_order.h"

#ifdef USE_OPENSSL
#define SHA1_INIT(ctx) ((pinit_t)ctx->sha1_methods.init)(&ctx->sha1_context)
#define SHA1_UPDATE(ctx, msg, size) ((pupdate_t)ctx->sha1_methods.update)(&ctx->sha1_context, (msg), (size))
#define SHA1_FINAL(ctx, result) ((pfinal_t)ctx->sha1_methods.final)(&ctx->sha1_context, (result))
#else
#define SHA1_INIT(ctx) rhash_sha1_init(&ctx->sha1_context)
#define SHA1_UPDATE(ctx, msg, size) rhash_sha1_update(&ctx->sha1_context, (msg), (size))
#define SHA1_FINAL(ctx, result) rhash_sha1_final(&ctx->sha1_context, (result))
#endif

#define BT_MIN_PIECE_LENGTH 16384
/** size of a SHA1 hash in bytes */
#define BT_HASH_SIZE 20
/** number of SHA1 hashes to store together in one block */
#define BT_BLOCK_SIZE 256
#define BT_BLOCK_SIZE_IN_BYTES (BT_BLOCK_SIZE * BT_HASH_SIZE)

/**
 * Initialize torrent context before calculating hash.
 *
 * @param ctx context to initialize
 */
void bt_init(torrent_ctx* ctx)
{
	memset(ctx, 0, sizeof(torrent_ctx));
	ctx->piece_length = BT_MIN_PIECE_LENGTH;
	assert(BT_MIN_PIECE_LENGTH == bt_default_piece_length(0, 0));

#ifdef USE_OPENSSL
	/* get the methods of the selected SHA1 algorithm */
	assert(rhash_info_table[3].info->hash_id == RHASH_SHA1);
	assert(rhash_info_table[3].context_size <= (sizeof(sha1_ctx) + sizeof(unsigned long)));
	rhash_load_sha1_methods(&ctx->sha1_methods, METHODS_SELECTED);
#endif

	SHA1_INIT(ctx);
}

/**
 * Free memory allocated by properties of torrent_vect structure.
 *
 * @param vect vector to clean
 */
static void bt_vector_clean(torrent_vect* vect)
{
	size_t i;
	for (i = 0; i < vect->size; i++) {
		free(vect->array[i]);
	}
	free(vect->array);
}

/**
 * Clean up torrent context by freeing all dynamically
 * allocated memory.
 *
 * @param ctx torrent algorithm context
 */
void bt_cleanup(torrent_ctx* ctx)
{
	assert(ctx != NULL);

	/* destroy arrays */
	bt_vector_clean(&ctx->hash_blocks);
	bt_vector_clean(&ctx->files);
	bt_vector_clean(&ctx->announce);

	free(ctx->program_name);
	free(ctx->content.str);
	ctx->program_name = 0;
	ctx->content.str = 0;
}

static void bt_generate_torrent(torrent_ctx* ctx);

/**
 * Add an item to vector.
 *
 * @param vect vector to add item to
 * @param item the item to add
 * @return non-zero on success, zero on fail
 */
static int bt_vector_add_ptr(torrent_vect* vect, void* item)
{
	/* check if vector contains enough space for the next item */
	if (vect->size >= vect->allocated) {
		size_t size = (vect->allocated == 0 ? 128 : vect->allocated * 2);
		void* new_array = realloc(vect->array, size * sizeof(void*));
		if (new_array == NULL) return 0; /* failed: no memory */
		vect->array = (void**)new_array;
		vect->allocated = size;
	}
	/* add new item to the vector */
	vect->array[vect->size] = item;
	vect->size++;
	return 1;
}

/**
 * Store a SHA1 hash of a processed file piece.
 *
 * @param ctx torrent algorithm context
 * @return non-zero on success, zero on fail
 */
static int bt_store_piece_sha1(torrent_ctx* ctx)
{
	unsigned char* block;
	unsigned char* hash;

	if ((ctx->piece_count % BT_BLOCK_SIZE) == 0) {
		block = (unsigned char*)malloc(BT_BLOCK_SIZE_IN_BYTES);
		if (!block)
			return 0;
		if (!bt_vector_add_ptr(&ctx->hash_blocks, block)) {
			free(block);
			return 0;
		}
	} else {
		block = (unsigned char*)(ctx->hash_blocks.array[ctx->piece_count / BT_BLOCK_SIZE]);
	}

	hash = &block[BT_HASH_SIZE * (ctx->piece_count % BT_BLOCK_SIZE)];
	SHA1_FINAL(ctx, hash); /* write the hash */
	ctx->piece_count++;
	return 1;
}

/**
 * A filepath and filesize information.
 */
typedef struct bt_file_info
{
	uint64_t size;
	char path[];
} bt_file_info;

/**
 * Add a file info into the batch of files of given torrent.
 *
 * @param ctx torrent algorithm context
 * @param path file path
 * @param filesize file size
 * @return non-zero on success, zero on fail
 */
int bt_add_file(torrent_ctx* ctx, const char* path, uint64_t filesize)
{
	size_t len = strlen(path);
	bt_file_info* info = (bt_file_info*)malloc(sizeof(uint64_t) + len + 1);
	if (info == NULL) {
		ctx->error = 1;
		return 0;
	}

	info->size = filesize;
	memcpy(info->path, path, len + 1);
	if (!bt_vector_add_ptr(&ctx->files, info)) {
		free(info);
		return 0;
	}

	/* recalculate piece length (but only if hashing not started yet) */
	if (ctx->piece_count == 0 && ctx->index == 0) {
		/* note: in case of batch of files should use a total batch size */
		ctx->piece_length = bt_default_piece_length(filesize, ctx->options & BT_OPT_TRANSMISSION);
	}
	return 1;
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void bt_update(torrent_ctx* ctx, const void* msg, size_t size)
{
	const unsigned char* pmsg = (const unsigned char*)msg;
	size_t rest = (size_t)(ctx->piece_length - ctx->index);
	assert(ctx->index < ctx->piece_length);

	while (size > 0) {
		size_t left = (size < rest ? size : rest);
		SHA1_UPDATE(ctx, pmsg, left);
		if (size < rest) {
			ctx->index += left;
			break;
		}
		bt_store_piece_sha1(ctx);
		SHA1_INIT(ctx);
		ctx->index = 0;

		pmsg += rest;
		size -= rest;
		rest = ctx->piece_length;
	}
}

/**
 * Finalize hashing and optionally store calculated hash into the given array.
 * If the result parameter is NULL, the hash is not stored, but it is
 * accessible by bt_get_btih().
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result pointer to the array store message hash into
 */
void bt_final(torrent_ctx* ctx, unsigned char result[20])
{
	if (ctx->index > 0) {
		bt_store_piece_sha1(ctx); /* flush buffered data */
	}

	bt_generate_torrent(ctx);
	if (result) memcpy(result, ctx->btih, btih_hash_size);
}

/* BitTorrent functions */

/**
 * Grow, if needed, the torrent_str buffer to ensure it contains
 * at least (length + 1) characters.
 *
 * @param ctx the torrent algorithm context
 * @param length length of the string, the allocated buffer must contain
 * @return 1 on success, 0 on error
 */
static int bt_str_ensure_length(torrent_ctx* ctx, size_t length)
{
	char* new_str;
    if (ctx->error)
        return 0;
    if (length >= ctx->content.allocated) {
		length++; /* allocate one character more */
		if (length < 64) length = 64;
		else length = (length + 255) & ~255;
		new_str = (char*)realloc(ctx->content.str, length);
		if (new_str == NULL) {
			ctx->error = 1;
			ctx->content.allocated = 0;
			return 0;
		}
		ctx->content.str = new_str;
		ctx->content.allocated = length;
	}
	return 1;
}

/**
 * Append a null-terminated string to the string string buffer.
 *
 * @param ctx the torrent algorithm context
 * @param text the null-terminated string to append
 */
static void bt_str_append(torrent_ctx* ctx, const char* text)
{
	size_t length = strlen(text);
	if (!bt_str_ensure_length(ctx, ctx->content.length + length + 1))
		return;
	assert(ctx->content.str != 0);
	memcpy(ctx->content.str + ctx->content.length, text, length + 1);
	ctx->content.length += length;
}

/**
 * B-encode given integer.
 *
 * @param ctx the torrent algorithm context
 * @param name B-encoded string to prepend the number or NULL
 * @param number the integer to output
 */
static void bt_bencode_int(torrent_ctx* ctx, const char* name, uint64_t number)
{
	char* p;
	if (name)
		bt_str_append(ctx, name);

	/* add up to 20 digits and 2 letters */
	if (!bt_str_ensure_length(ctx, ctx->content.length + 22))
		return;
	p = ctx->content.str + ctx->content.length;
	*(p++) = 'i';
	p += rhash_sprintI64(p, number);
	*(p++) = 'e';
	*p = '\0'; /* terminate string with \0 */

	ctx->content.length = (p - ctx->content.str);
}

/**
 * B-encode a string.
 *
 * @param ctx the torrent algorithm context
 * @param name B-encoded string to prepend or NULL
 * @param str the string to encode
 */
static void bt_bencode_str(torrent_ctx* ctx, const char* name, const char* str)
{
	const size_t string_length = strlen(str);
	int number_length;
	char* p;

	if (name)
		bt_str_append(ctx, name);
	if (!bt_str_ensure_length(ctx, ctx->content.length + string_length + 21))
		return;
	p = ctx->content.str + ctx->content.length;
	p += (number_length = rhash_sprintI64(p, string_length));
	ctx->content.length += string_length + number_length + 1;

	*(p++) = ':';
	memcpy(p, str, string_length + 1); /* copy with trailing '\0' */
}

/**
 * B-encode array of SHA1 hashes of file pieces.
 *
 * @param ctx pointer to the torrent structure containing SHA1 hashes
 */
static void bt_bencode_pieces(torrent_ctx* ctx)
{
	const size_t pieces_length = ctx->piece_count * BT_HASH_SIZE;
	size_t bytes_left, i;
	int number_length;
	char* p;

	if (!bt_str_ensure_length(ctx, ctx->content.length + pieces_length + 21))
		return;
	p = ctx->content.str + ctx->content.length;
	p += (number_length = rhash_sprintI64(p, pieces_length));
	ctx->content.length += pieces_length + number_length + 1;

	*(p++) = ':';
	p[pieces_length] = '\0'; /* terminate with \0 just in case */

	for (bytes_left = pieces_length, i = 0; bytes_left > 0; i++)
	{
		size_t size = (bytes_left < BT_BLOCK_SIZE_IN_BYTES ? bytes_left : BT_BLOCK_SIZE_IN_BYTES);
		memcpy(p, ctx->hash_blocks.array[i], size);
		bytes_left -= size;
		p += size;
	}
}

/**
 * Calculate default torrent piece length, using uTorrent algorithm.
 * Algorithm:
 *   piece_length = 16K for total_size < 16M,
 *   piece_length = 8M for total_size >= 4G,
 *   piece_length = top_bit(total_size) / 512 otherwise.
 *
 * @param total_size total torrent batch size
 * @return piece length used by torrent file
 */
static size_t utorr_piece_length(uint64_t total_size)
{
	size_t size = (size_t)(total_size >> 9) | 16384;
	size_t hi_bit;
	for (hi_bit = 8388608; hi_bit > size; hi_bit >>= 1);
	return hi_bit;
}

#define MB I64(1048576)

/**
 * Calculate default torrent piece length, using transmission algorithm.
 * Algorithm:
 *   piece_length = (size >= 2G ? 2M : size >= 1G ? 1M :
 *       size >= 512M ? 512K : size >= 350M ? 256K :
 *       size >= 150M ? 128K : size >= 50M ? 64K : 32K);
 *
 * @param total_size total torrent batch size
 * @return piece length used by torrent file
 */
static size_t transmission_piece_length(uint64_t total_size)
{
	static const uint64_t sizes[6] = { 50 * MB, 150 * MB, 350 * MB, 512 * MB, 1024 * MB, 2048 * MB };
	int i;
	for (i = 0; i < 6 && total_size >= sizes[i]; i++);
	return (32 * 1024) << i;
}

size_t bt_default_piece_length(uint64_t total_size, int transmission)
{
	return (transmission ?
		transmission_piece_length(total_size) : utorr_piece_length(total_size));
}

/* get file basename */
static const char* bt_get_basename(const char* path)
{
	const char* p = strchr(path, '\0') - 1;
	for (; p >= path && *p != '/' && *p != '\\'; p--);
	return (p + 1);
}

/* extract batchname from the path, modifies the path buffer */
static const char* get_batch_name(char* path)
{
	char* p = (char*)bt_get_basename(path) - 1;
	for (; p > path && (*p == '/' || *p == '\\'); p--) *p = 0;
	if (p <= path) return "BATCH_DIR";
	return bt_get_basename(path);
}

/* write file size and path */
static void bt_file_info_append(torrent_ctx* ctx, const char* length_name,
	const char* path_name, bt_file_info* info)
{
	bt_bencode_int(ctx, length_name, info->size);
	/* store the file basename */
	bt_bencode_str(ctx, path_name, bt_get_basename(info->path));
}

/**
 * Generate torrent file content
 * @see http://wiki.theory.org/BitTorrentSpecification
 *
 * @param ctx the torrent algorithm context
 */
static void bt_generate_torrent(torrent_ctx* ctx)
{
	uint64_t total_size = 0;
	size_t info_start_pos;

	assert(ctx->content.str == NULL);

	if (ctx->piece_length == 0) {
		if (ctx->files.size == 1) {
			total_size = ((bt_file_info*)ctx->files.array[0])->size;
		}
		ctx->piece_length = bt_default_piece_length(total_size, ctx->options & BT_OPT_TRANSMISSION);
	}

	if ((ctx->options & BT_OPT_INFOHASH_ONLY) == 0) {
		/* write the torrent header */
		bt_str_append(ctx, "d");
		if (ctx->announce.array && ctx->announce.size > 0) {
			bt_bencode_str(ctx, "8:announce", ctx->announce.array[0]);

			/* if more than one announce url */
			if (ctx->announce.size > 1) {
				/* add the announce-list key-value pair */
				size_t i;
				bt_str_append(ctx, "13:announce-listll");

				for (i = 0; i < ctx->announce.size; i++) {
					if (i > 0) {
						bt_str_append(ctx, "el");
					}
					bt_bencode_str(ctx, 0, ctx->announce.array[i]);
				}
				bt_str_append(ctx, "ee");
			}
		}

		if (ctx->program_name) {
			bt_bencode_str(ctx, "10:created by", ctx->program_name);
		}
		bt_bencode_int(ctx, "13:creation date", (uint64_t)time(NULL));

		bt_str_append(ctx, "8:encoding5:UTF-8");
	}

	/* write the essential for BTIH part of the torrent file */

	bt_str_append(ctx, "4:infod"); /* start the info dictionary */
	info_start_pos = ctx->content.length - 1;

	if (ctx->files.size > 1) {
		size_t i;

		/* process batch torrent */
		bt_str_append(ctx, "5:filesl"); /* start list of files */

		/* write length and path for each file in the batch */
		for (i = 0; i < ctx->files.size; i++) {
			bt_file_info_append(ctx, "d6:length", "4:pathl",
				(bt_file_info*)ctx->files.array[i]);
			bt_str_append(ctx, "ee");
		}
		/* note: get_batch_name modifies path, so should be called here */
		bt_bencode_str(ctx, "e4:name", get_batch_name(
			((bt_file_info*)ctx->files.array[0])->path));
	}
	else if (ctx->files.size > 0) {
		/* write size and basename of the first file */
		/* in the non-batch mode other files are ignored */
		bt_file_info_append(ctx, "6:length", "4:name",
			(bt_file_info*)ctx->files.array[0]);
	}

	bt_bencode_int(ctx, "12:piece length", ctx->piece_length);
	bt_str_append(ctx, "6:pieces");
	bt_bencode_pieces(ctx);

	if (ctx->options & BT_OPT_PRIVATE) {
		bt_str_append(ctx, "7:privatei1e");
	} else if (ctx->options & BT_OPT_TRANSMISSION) {
		bt_str_append(ctx, "7:privatei0e");
	}
	bt_str_append(ctx, "ee");

	/* calculate BTIH */
	SHA1_INIT(ctx);
    if (ctx->content.str) {
        SHA1_UPDATE(ctx, (unsigned char*)ctx->content.str + info_start_pos,
            ctx->content.length - info_start_pos - 1);
    }
	SHA1_FINAL(ctx, ctx->btih);
}

/* Getters/Setters */

/**
 * Get BTIH (BitTorrent Info Hash) value.
 *
 * @param ctx the torrent algorithm context
 * @return the 20-bytes long BTIH value
 */
unsigned char* bt_get_btih(torrent_ctx* ctx)
{
	return ctx->btih;
}

/**
 * Set the torrent algorithm options.
 *
 * @param ctx the torrent algorithm context
 * @param options the options to set
 */
void bt_set_options(torrent_ctx* ctx, unsigned options)
{
	ctx->options = options;
}

#if defined(__STRICT_ANSI__)
/* define strdup for gcc -ansi */
static char* bt_strdup(const char* str)
{
	size_t len = strlen(str);
	char* res = (char*)malloc(len + 1);
	if (res) memcpy(res, str, len + 1);
	return res;
}
#define strdup bt_strdup
#endif /* __STRICT_ANSI__ */

/**
 * Set optional name of the program generating the torrent
 * for storing into torrent file.
 *
 * @param ctx the torrent algorithm context
 * @param name the program name
 * @return non-zero on success, zero on error
 */
int bt_set_program_name(torrent_ctx* ctx, const char* name)
{
	ctx->program_name = strdup(name);
	return (ctx->program_name != NULL);
}

/**
 * Set length of a file piece.
 *
 * @param ctx the torrent algorithm context
 * @param piece_length the piece length in bytes
 */
void bt_set_piece_length(torrent_ctx* ctx, size_t piece_length)
{
	ctx->piece_length = piece_length;
}

/**
 * Set length of a file piece by the total batch size.
 *
 * @param ctx the torrent algorithm context
 * @param total_size total batch size
 */
void bt_set_total_batch_size(torrent_ctx* ctx, uint64_t total_size)
{
	ctx->piece_length = bt_default_piece_length(total_size, ctx->options & BT_OPT_TRANSMISSION);
}

/**
 * Add a tracker announce-URL to the torrent file.
 *
 * @param ctx the torrent algorithm context
 * @param announce_url the announce URL of the tracker
 * @return non-zero on success, zero on error
 */
int bt_add_announce(torrent_ctx* ctx, const char* announce_url)
{
	char* url_copy;
	if (!announce_url || announce_url[0] == '\0') return 0;
	url_copy = strdup(announce_url);
	if (!url_copy) return 0;
	if (bt_vector_add_ptr(&ctx->announce, url_copy))
		return 1;
	free(url_copy);
	return 0;
}

/**
 * Get the content of generated torrent file.
 *
 * @param ctx the torrent algorithm context
 * @param pstr pointer to pointer receiving the buffer with file content
 * @return length of the torrent file content
 */
size_t bt_get_text(torrent_ctx* ctx, char** pstr)
{
	assert(ctx->content.str);
	*pstr = ctx->content.str;
	return ctx->content.length;
}

#if !defined(NO_IMPORT_EXPORT)

# define EXPORT_ALIGNER 7
# define GET_EXPORT_ALIGNED(size) (((size) + EXPORT_ALIGNER) & ~EXPORT_ALIGNER)
# define GET_EXPORT_PADDING(size) (-(size) & EXPORT_ALIGNER)
# define GET_EXPORT_STR_LEN(length) GET_EXPORT_ALIGNED((length) + 1)
# define GET_EXPORT_SIZED_STR_LEN(length) GET_EXPORT_STR_LEN((length) + sizeof(size_t))
# define IS_EXPORT_ALIGNED(size) (((size) & EXPORT_ALIGNER) == 0)
# define BT_CTX_OSSL_FLAG 0x10

static void bt_export_str(char* out, const char* str, size_t length)
{
	assert(!!out);
	*(size_t*)(out) = length;
	out += sizeof(size_t);
	memcpy(out, str, length + 1);
}

typedef struct bt_export_header {
	size_t torrent_ctx_size;
	size_t files_size;
	size_t announce_size;
	size_t program_name_length;
	size_t content_length;
} bt_export_header;

/**
 * Export algorithm context to a memory region, or calculate the
 * size required for context export.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param out pointer to the memory region or NULL
 * @param size size of memory region
 * @return the size of the exported data on success, 0 on fail.
 */
size_t bt_export(const torrent_ctx* ctx, void* out, size_t size)
{
	const size_t head_size = sizeof(bt_export_header);
	const size_t ctx_head_size = offsetof(torrent_ctx, hash_blocks);
	const size_t hashes_size = ctx->piece_count * BT_HASH_SIZE;
	size_t exported_size = head_size + ctx_head_size + hashes_size;
	const size_t padding_size = GET_EXPORT_PADDING(exported_size);
	const size_t program_name_length = (ctx->program_name ? strlen(ctx->program_name) : 0);
	char* out_ptr = (char*)out;
	size_t i;
	assert((exported_size + padding_size) == GET_EXPORT_ALIGNED(exported_size));
	if (out_ptr) {
		bt_export_header* header = (bt_export_header*)out_ptr;
		size_t hash_data_left = hashes_size;
		if (size < exported_size)
			return 0;
		header->torrent_ctx_size = sizeof(torrent_ctx);
		header->files_size = ctx->files.size;
		header->announce_size = ctx->announce.size;
		header->program_name_length = program_name_length;
		header->content_length = ctx->content.length;
		out_ptr += head_size;

		memcpy(out_ptr, ctx, ctx_head_size);
		out_ptr += ctx_head_size;

		for (i = 0; i < ctx->hash_blocks.size && hash_data_left; i++) {
			size_t left = (hash_data_left < BT_BLOCK_SIZE_IN_BYTES ? hash_data_left : BT_BLOCK_SIZE_IN_BYTES);
			memcpy(out_ptr, ctx->hash_blocks.array[i], left);
			out_ptr += left;
			hash_data_left -= left;
		}
		out_ptr += padding_size;
	}
	exported_size += padding_size;
	assert(IS_EXPORT_ALIGNED(exported_size));

	for (i = 0; i < ctx->files.size; i++) {
		bt_file_info* info = (bt_file_info*)(ctx->files.array[i]);
		size_t length = strlen(info->path);
		const size_t aligned_length = GET_EXPORT_SIZED_STR_LEN(length);
		if (!length)
			continue;
		exported_size += sizeof(uint64_t) + aligned_length;
		if (out_ptr) {
			if (size < exported_size)
				return 0;
			*(uint64_t*)out_ptr = info->size;
			out_ptr += sizeof(uint64_t);
			bt_export_str(out_ptr, info->path, length);
			out_ptr += aligned_length;
		}
	}
	assert(IS_EXPORT_ALIGNED(exported_size));

	for (i = 0; i < ctx->announce.size; i++) {
		size_t length = strlen(ctx->announce.array[i]);
		const size_t aligned_length = GET_EXPORT_SIZED_STR_LEN(length);
		if (!length)
			continue;
		exported_size += aligned_length;
		if (out_ptr) {
			if (size < exported_size)
				return 0;
			bt_export_str(out_ptr, ctx->announce.array[i], length);
			out_ptr += aligned_length;
		}
	}
	assert(IS_EXPORT_ALIGNED(exported_size));

	if (program_name_length > 0) {
		const size_t aligned_length = GET_EXPORT_STR_LEN(program_name_length);
		exported_size += aligned_length;
		if (out_ptr) {
			if (size < exported_size)
				return 0;
			strcpy(out_ptr, ctx->program_name);
			out_ptr += aligned_length;
		}
		assert(IS_EXPORT_ALIGNED(exported_size));
	}

	if (ctx->content.length > 0) {
		const size_t aligned_length = GET_EXPORT_STR_LEN(ctx->content.length);
		exported_size += aligned_length;
		if (out_ptr) {
			if (size < exported_size)
				return 0;
            assert(ctx->content.str != NULL);
			memcpy(out_ptr, ctx->content.str, ctx->content.length + 1);
			out_ptr += aligned_length;
		}
		assert(IS_EXPORT_ALIGNED(exported_size));
	}
	assert(!out || (size_t)(out_ptr - (char*)out) == exported_size);

#if defined(USE_OPENSSL)
    if (out_ptr && ARE_OPENSSL_METHODS(ctx->sha1_methods)) {
        size_t* error_ptr = (size_t*)((char*)out + head_size + offsetof(torrent_ctx, error));
        *error_ptr |= BT_CTX_OSSL_FLAG;
        RHASH_ASSERT(sizeof(*error_ptr) == sizeof(ctx->error));
    }
#endif
	return exported_size;
}

/**
 * Import algorithm context from a memory region.
 *
 * @param ctx pointer to the algorithm context
 * @param in pointer to the data to import
 * @param size size of data to import
 * @return the size of the imported data on success, 0 on fail.
 */
size_t bt_import(torrent_ctx* ctx, const void* in, size_t size)
{
	const size_t head_size = sizeof(bt_export_header);
	const size_t ctx_head_size = offsetof(torrent_ctx, hash_blocks);
	size_t imported_size = head_size + ctx_head_size;
	const char* in_ptr = (const char*)in;
	size_t padding_size;
	size_t hash_data_left;
	size_t length;
	size_t i;
	const bt_export_header* header = (const bt_export_header*)in_ptr;
	if (size < imported_size)
		return 0;
	if (header->torrent_ctx_size != sizeof(torrent_ctx))
		return 0;
	in_ptr += sizeof(bt_export_header);

	memset(ctx, 0, sizeof(torrent_ctx));
	memcpy(ctx, in_ptr, ctx_head_size);
	in_ptr += ctx_head_size;

	hash_data_left = ctx->piece_count * BT_HASH_SIZE;
	imported_size += hash_data_left;
	padding_size = GET_EXPORT_PADDING(imported_size);
	imported_size += padding_size;
	assert(IS_EXPORT_ALIGNED(imported_size));
	if (size < imported_size)
		return 0;

	while (hash_data_left) {
		size_t left = (hash_data_left < BT_BLOCK_SIZE_IN_BYTES ? hash_data_left : BT_BLOCK_SIZE_IN_BYTES);
		unsigned char* block = (unsigned char*)malloc(BT_BLOCK_SIZE_IN_BYTES);
		if (!block)
			return 0;
		if (!bt_vector_add_ptr(&ctx->hash_blocks, block)) {
			free(block);
			return 0;
		}
		memcpy(block, in_ptr, left);
		in_ptr += left;
		hash_data_left -= left;
	}
	in_ptr += padding_size;
	assert((size_t)(in_ptr - (char*)in) == imported_size);
	assert(IS_EXPORT_ALIGNED(imported_size));

	for (i = 0; i < header->files_size; i++) {
		uint64_t filesize;
		imported_size += sizeof(uint64_t);
		if (size < (imported_size + sizeof(size_t)))
			return 0;
		filesize = *(uint64_t*)in_ptr;
		in_ptr += sizeof(uint64_t);
		length = *(size_t*)in_ptr;
		imported_size += GET_EXPORT_SIZED_STR_LEN(length);
		if (!length || size < imported_size)
			return 0;
		if (!bt_add_file(ctx, in_ptr + sizeof(size_t), filesize))
			return 0;
		in_ptr += GET_EXPORT_SIZED_STR_LEN(length);
	}
	assert((size_t)(in_ptr - (char*)in) == imported_size);
	assert(IS_EXPORT_ALIGNED(imported_size));

	for (i = 0; i < header->announce_size; i++) {
		if (size < (imported_size + sizeof(size_t)))
			return 0;
		length = *(size_t*)in_ptr;
		imported_size += GET_EXPORT_SIZED_STR_LEN(length);
		if (!length || size < imported_size)
			return 0;
		if (!bt_add_announce(ctx, in_ptr + sizeof(size_t)))
			return 0;
		in_ptr += GET_EXPORT_SIZED_STR_LEN(length);
	}
	assert((size_t)(in_ptr - (char*)in) == imported_size);
	assert(IS_EXPORT_ALIGNED(imported_size));

	length = header->program_name_length;
	if (length) {
		imported_size += GET_EXPORT_STR_LEN(length);
		if (size < imported_size)
			return 0;
		if (!bt_set_program_name(ctx, in_ptr))
			return 0;
		in_ptr += GET_EXPORT_STR_LEN(length);
		assert((size_t)(in_ptr - (char*)in) == imported_size);
		assert(IS_EXPORT_ALIGNED(imported_size));
	}

#if defined(USE_OPENSSL)
	/* must restore ctx->error flag before calling bt_str_ensure_length() */
	if ((ctx->error & BT_CTX_OSSL_FLAG) != 0) {
		ctx->error &= ~BT_CTX_OSSL_FLAG;
		rhash_load_sha1_methods(&ctx->sha1_methods, METHODS_OPENSSL);
	} else {
		rhash_load_sha1_methods(&ctx->sha1_methods, METHODS_RHASH);
	}
#endif

	length = header->content_length;
	if (length) {
		imported_size += GET_EXPORT_STR_LEN(length);
		if (size < imported_size)
			return 0;
		if (!bt_str_ensure_length(ctx, length))
			return 0;
		memcpy(ctx->content.str, in_ptr, length);
		in_ptr += GET_EXPORT_STR_LEN(length);
		assert((size_t)(in_ptr - (char*)in) == imported_size);
		assert(IS_EXPORT_ALIGNED(imported_size));
	}
	return imported_size;
}
#endif /* !defined(NO_IMPORT_EXPORT) */
