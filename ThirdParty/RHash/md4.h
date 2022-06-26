/* md4.h */
#ifndef MD4_HIDER
#define MD4_HIDER
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define md4_block_size 64
#define md4_hash_size  16

/* algorithm context */
typedef struct md4_ctx
{
	unsigned hash[4];  /* 128-bit algorithm internal hashing state */
	unsigned message[md4_block_size / 4]; /* 512-bit buffer for leftovers */
	uint64_t length;   /* number of processed bytes */
} md4_ctx;

/* hash functions */

void rhash_md4_init(md4_ctx* ctx);
void rhash_md4_update(md4_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_md4_final(md4_ctx* ctx, unsigned char result[16]);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* MD4_HIDER */
