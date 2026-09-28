/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Hash.Torrent.cpp
 * PURPOSE:    Implementation for BitTorrent InfoHash (BTIH) hash algorithm
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

// **************** Inherited RHash BTIH Implementation Start ****************
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

#include <K7Base.h>
#include "RHash/byte_order.h"
#include "RHash/hex.h"
#include "RHash/util.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>  /* time() */

#define btih_hash_size  20

/* vector structure */
typedef struct torrent_vect
{
    void** array;     /* array of elements of the vector */
    size_t size;      /* vector size */
    size_t allocated; /* number of allocated elements */
} torrent_vect;

/* a binary string */
typedef struct torrent_str
{
    char* str;
    size_t length;
    size_t allocated;
} torrent_str;

/* BitTorrent algorithm context */
typedef struct torrent_ctx
{
    unsigned char btih[20]; /* resulting BTIH hash sum */
    unsigned options;       /* algorithm options */
    K7_BASE_HASH_HANDLE sha1_context; /* context for hashing current file piece */
    size_t index;             /* byte index in the current piece */
    size_t piece_length;      /* length of a torrent file piece */
    size_t piece_count;       /* the number of pieces processed */
    size_t error;             /* non-zero if error occurred, zero otherwise */
    torrent_vect hash_blocks; /* array of blocks storing SHA1 hashes */
    torrent_vect files;       /* names of files in a torrent batch */
    torrent_vect announce;    /* announce URLs */
    char* program_name;       /* the name of the program */

    torrent_str content;      /* the content of generated torrent file */
} torrent_ctx;

/* possible options */
#define BT_OPT_PRIVATE 1
#define BT_OPT_INFOHASH_ONLY 2
#define BT_OPT_TRANSMISSION 4

size_t bt_default_piece_length(uint64_t total_size, int transmission);

static void SHA1_INIT(torrent_ctx* ctx)
{
    if (ctx->error)
        return;

    if (ctx->sha1_context)
    {
        ::K7BaseHashDestroy(ctx->sha1_context);
        ctx->sha1_context = nullptr;
    }

    if (::K7BaseHashCreate(
        &ctx->sha1_context,
        K7_BASE_HASH_ALGORITHM_SHA1,
        nullptr,
        0) != MO_RESULT_SUCCESS_OK)
    {
        ctx->error = 1;
    }
}

static void SHA1_UPDATE(
    torrent_ctx* ctx,
    const unsigned char* msg,
    size_t size)
{
    if (ctx->error)
        return;

    while (size > 0)
    {
        const MO_UINT32 input_size = static_cast<MO_UINT32>(
            size > UINT32_MAX ? UINT32_MAX : size);

        if (::K7BaseHashUpdate(
            ctx->sha1_context,
            const_cast<unsigned char*>(msg),
            input_size) != MO_RESULT_SUCCESS_OK)
        {
            ctx->error = 1;
            return;
        }

        msg += input_size;
        size -= input_size;
    }
}

static void SHA1_FINAL(torrent_ctx* ctx, unsigned char* result)
{
    if (ctx->error ||
        ::K7BaseHashFinal(
            ctx->sha1_context,
            result,
            btih_hash_size) != MO_RESULT_SUCCESS_OK)
    {
        ctx->error = 1;
        memset(result, 0, btih_hash_size);
    }
}

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
    vect->array = NULL;
    vect->size = 0;
    vect->allocated = 0;
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

    if (ctx->sha1_context)
    {
        ::K7BaseHashDestroy(ctx->sha1_context);
        ctx->sha1_context = nullptr;
    }
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
    if (ctx->error)
        return 0;
    ctx->piece_count++;
    return 1;
}

#pragma warning(push)
#pragma warning(disable: 4200)
/**
 * A filepath and filesize information.
 */
typedef struct bt_file_info
{
    uint64_t size;
    char path[];
} bt_file_info;
#pragma warning(pop)

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
    if (ctx->error)
        return;

    const unsigned char* pmsg = (const unsigned char*)msg;
    size_t rest = (size_t)(ctx->piece_length - ctx->index);
    assert(ctx->index < ctx->piece_length);

    while (size > 0) {
        size_t left = (size < rest ? size : rest);
        SHA1_UPDATE(ctx, pmsg, left);
        if (ctx->error)
            return;
        if (size < rest) {
            ctx->index += left;
            break;
        }
        if (!bt_store_piece_sha1(ctx)) {
            ctx->error = 1;
            return;
        }
        SHA1_INIT(ctx);
        if (ctx->error)
            return;
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
    if (ctx->error)
        goto finish;

    if (ctx->index > 0) {
        if (!bt_store_piece_sha1(ctx)) { /* flush buffered data */
            ctx->error = 1;
            goto finish;
        }
    }

    bt_generate_torrent(ctx);
finish:
    if (ctx->error)
        memset(ctx->btih, 0, btih_hash_size);
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
        ctx->piece_length = bt_default_piece_length(
            total_size,
            ctx->options & BT_OPT_TRANSMISSION);
    }

    if ((ctx->options & BT_OPT_INFOHASH_ONLY) == 0) {
        /* write the torrent header */
        bt_str_append(ctx, "d");
        if (ctx->announce.array && ctx->announce.size > 0) {
            bt_bencode_str(
                ctx,
                "8:announce",
                (const char*)ctx->announce.array[0]);

            /* if more than one announce url */
            if (ctx->announce.size > 1) {
                /* add the announce-list key-value pair */
                size_t i;
                bt_str_append(ctx, "13:announce-listll");

                for (i = 0; i < ctx->announce.size; i++) {
                    if (i > 0) {
                        bt_str_append(ctx, "el");
                    }
                    bt_bencode_str(ctx, 0, (const char*)ctx->announce.array[i]);
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
    if (ctx->error)
        return;
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
// **************** Inherited RHash BTIH Implementation End ****************

namespace NanaZip::Codecs::Hash
{
    struct Torrent : public Mile::ComObject<Torrent, IHasher>
    {
    private:

        torrent_ctx Context = {};

    public:

        Torrent()
        {
            this->Init();
        }

        ~Torrent()
        {
            ::bt_cleanup(
                &this->Context);
        }

        void STDMETHODCALLTYPE Init()
        {
            ::bt_cleanup(
                &this->Context);
            ::bt_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::bt_update(
                &this->Context,
                reinterpret_cast<const void*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::bt_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return btih_hash_size;
        }
    };

    IHasher* CreateTorrent()
    {
        return new Torrent();
    }
}
