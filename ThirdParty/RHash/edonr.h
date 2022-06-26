/* edonr.h EDON-R 224/256/384/512 hash functions */
#ifndef EDONR_H
#define EDONR_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define edonr224_hash_size  28
#define edonr256_hash_size  32
#define edonr256_block_size 64

#define edonr384_hash_size  48
#define edonr512_hash_size  64
#define edonr512_block_size 128

struct edonr256_data {
	unsigned message[16];  /* 512-bit buffer for leftovers */
	unsigned hash[16];     /* 512-bit algorithm internal hashing state */
};

struct edonr512_data {
	uint64_t message[16];  /* 1024-bit buffer for leftovers */
	uint64_t hash[16];     /* 1024-bit algorithm internal hashing state */
};

/* algorithm context */
typedef struct edonr_ctx
{
	union {
		struct edonr256_data data256;
		struct edonr512_data data512;
	} u;
	uint64_t length;        /* number of processed bytes */
	unsigned digest_length; /* length of the algorithm digest in bytes */
} edonr_ctx;

void rhash_edonr224_init(edonr_ctx* ctx);
void rhash_edonr256_init(edonr_ctx* ctx);
void rhash_edonr256_update(edonr_ctx* ctx, const unsigned char* data, size_t length);
void rhash_edonr256_final(edonr_ctx* ctx, unsigned char* result);

void rhash_edonr384_init(edonr_ctx* ctx);
void rhash_edonr512_init(edonr_ctx* ctx);
void rhash_edonr512_update(edonr_ctx* ctx, const unsigned char* data, size_t length);
void rhash_edonr512_final(edonr_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* EDONR_H */
