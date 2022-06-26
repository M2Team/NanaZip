/* ripemd-160.h */
#ifndef  RMD160_H
#define  RMD160_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ripemd160_block_size 64
#define ripemd160_hash_size  20

/* algorithm context */
typedef struct ripemd160_ctx
{
	unsigned message[ripemd160_block_size / 4]; /* 512-bit buffer for leftovers */
	uint64_t length;  /* number of processed bytes */
	unsigned hash[5]; /* 160-bit algorithm internal hashing state */
} ripemd160_ctx;

/* hash functions */

void rhash_ripemd160_init(ripemd160_ctx* ctx);
void rhash_ripemd160_update(ripemd160_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_ripemd160_final(ripemd160_ctx* ctx, unsigned char result[20]);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* RMD160_H */
