/* has160.h */
#ifndef HAS160_H
#define HAS160_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define has160_block_size 64
#define has160_hash_size 20

typedef struct has160_ctx
{
	unsigned message[has160_block_size / 4]; /* 512-bit buffer for leftovers */
	uint64_t length;     /* number of processed bytes */
	unsigned hash[5];   /* 160-bit algorithm internal hashing state */
} has160_ctx;

/* hash functions */

void rhash_has160_init(has160_ctx* ctx);
void rhash_has160_update(has160_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_has160_final(has160_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* HAS160_H */
