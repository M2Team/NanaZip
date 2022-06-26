/* blake2b.h */
#ifndef BLAKE2B_H
#define BLAKE2B_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define blake2b_block_size 128
#define blake2b_hash_size  64

typedef struct blake2b_ctx
{
	uint64_t hash[8];
	uint64_t message[16];
	uint64_t length;
} blake2b_ctx;

void rhash_blake2b_init(blake2b_ctx* ctx);
void rhash_blake2b_update(blake2b_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_blake2b_final(blake2b_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* BLAKE2B_H */
