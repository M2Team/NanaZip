/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Hash.Aich.cpp
 * PURPOSE:    Implementation for EMule AICH hash algorithm
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

// **************** Inherited RHash AICH Implementation Start ****************
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

#include <K7Base.h>
#include "RHash/byte_order.h"
#include "RHash/util.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static constexpr size_t sha1_hash_size = 20;

/* algorithm context */
typedef struct aich_ctx
{
    K7_BASE_HASH_HANDLE sha1_context; /* SHA-1 handle */
    unsigned index;        /* algorithm position in the current ed2k chunk */
    int error;             /* non-zero on hashing or resource failure */
    size_t chunks_count;   /* the number of ed2k chunks hashed */
    size_t allocated;      /* allocated size of the chunk_table */
    unsigned char (*block_hashes)[sha1_hash_size];
    void** chunk_table;    /* table of chunk hashes */
} aich_ctx;

static void SHA1_INIT(aich_ctx* ctx)
{
    if (ctx->error)
        return;

    if (ctx->sha1_context)
    {
        ::K7BaseHashDestroy(ctx->sha1_context);
        ctx->sha1_context = nullptr;
    }

    if (MO_RESULT_SUCCESS_OK != ::K7BaseHashCreate(
        &ctx->sha1_context,
        K7_BASE_HASH_ALGORITHM_SHA1,
        nullptr,
        0))
    {
        ctx->error = 1;
    }
}

static void SHA1_UPDATE(
    aich_ctx* ctx,
    const unsigned char* msg,
    size_t size)
{
    if (ctx->error)
        return;

    const MO_UINT32 input_size = static_cast<MO_UINT32>(size);
    if (size != input_size || MO_RESULT_SUCCESS_OK != ::K7BaseHashUpdate(
        ctx->sha1_context,
        const_cast<unsigned char*>(msg),
        input_size))
    {
        ctx->error = 1;
    }
}

static void SHA1_FINAL(aich_ctx* ctx, unsigned char* result)
{
    if (ctx->error || MO_RESULT_SUCCESS_OK != ::K7BaseHashFinal(
        ctx->sha1_context,
        result,
        static_cast<MO_UINT32>(sha1_hash_size)))
    {
        ctx->error = 1;
        memset(result, 0, sha1_hash_size);
    }
}

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
 * The function is called automatically by rhash_aich_final.
 * Shall be called when aborting hash calculations.
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

    if (ctx->sha1_context)
    {
        ::K7BaseHashDestroy(ctx->sha1_context);
        ctx->sha1_context = nullptr;
    }
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
    blocks_stack[0] = blocks = (unsigned)(type == AICH_HASH_FULL_TREE
        ? ctx->chunks_count
        : (ctx->index + FULL_BLOCK_SIZE - 1) / FULL_BLOCK_SIZE);

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
        if (ctx->error)
            return;
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
        unsigned block_left = left_in_chunk <= LAST_BLOCK_SIZE
            ? left_in_chunk
            : FULL_BLOCK_SIZE - ctx->index % FULL_BLOCK_SIZE;
        assert(block_left > 0);

        if (size >= block_left) {
            SHA1_UPDATE(ctx, msg, block_left);
            msg  += block_left;
            size -= block_left;
            ctx->index += block_left;

            /* process a 180KiB-blok */
            rhash_aich_process_block(ctx, AICH_PROCESS_FLUSH_BLOCK);
            if (ctx->error)
                return;

            SHA1_INIT(ctx);
            if (ctx->error)
                return;
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
    unsigned char hash[sha1_hash_size] = {};

    if (ctx->error)
        goto cleanup;

    if (ctx->chunks_count == 0 && ctx->block_hashes == NULL) {
        assert(ctx->index < FULL_BLOCK_SIZE);
        SHA1_FINAL(ctx, hash); /* return just sha1 hash */
        rhash_aich_cleanup(ctx);
        if (result) memcpy(result, hash, sha1_hash_size);
        return;
    }

    /* if there is unprocessed data left in the last 180K block */
    if ((ctx->index % FULL_BLOCK_SIZE) > 0) {
        /* then process the last block */
        rhash_aich_process_block(
            ctx,
            ctx->block_hashes
            ? AICH_PROCESS_FINAL_BLOCK | AICH_PROCESS_FLUSH_BLOCK
            : AICH_PROCESS_FLUSH_BLOCK);
        if (ctx->error)
            goto cleanup;
    }

    /* if processed message was shorter than a ed2k chunk */
    if (ctx->chunks_count == 0) {
        /* then return the aich hash for the first chunk */
        rhash_aich_hash_tree(ctx, hash, AICH_HASH_LEFT_BRANCH);
    } else {
        if (ctx->index > 0) {
            /* process the last block of the message */
            rhash_aich_process_block(ctx, AICH_PROCESS_FINAL_BLOCK);
            if (ctx->error)
                goto cleanup;
        }
        assert(ctx->chunks_count > 0);
        assert(ctx->block_hashes != NULL);

        rhash_aich_hash_tree(ctx, hash, AICH_HASH_FULL_TREE);
    }

cleanup:
    if (ctx->error)
        memset(hash, 0, sha1_hash_size);
    rhash_aich_cleanup(ctx);
    if (result) memcpy(result, hash, sha1_hash_size);
}
// **************** Inherited RHash AICH Implementation End ****************

namespace NanaZip::Codecs::Hash
{
    struct Aich : public Mile::ComObject<Aich, IHasher>
    {
    private:

        aich_ctx Context = {};

    public:

        Aich()
        {
            this->Init();
        }

        ~Aich()
        {
            ::rhash_aich_cleanup(
                &this->Context);
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_aich_cleanup(&this->Context);
            ::rhash_aich_init(&this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_aich_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_aich_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 20;
        }
    };

    IHasher* CreateAich()
    {
        return new Aich();
    }
}
