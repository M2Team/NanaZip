/* md5.h */
#ifndef MD5_HIDER
#define MD5_HIDER
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define md5_block_size 64
#define md5_hash_size  16

/* algorithm context */
typedef struct md5_ctx
{
	unsigned message[md5_block_size / 4]; /* 512-bit buffer for leftovers */
	uint64_t length;   /* number of processed bytes */
	unsigned hash[4];  /* 128-bit algorithm internal hashing state */
} md5_ctx;

/* hash functions */

void rhash_md5_init(md5_ctx* ctx);
void rhash_md5_update(md5_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_md5_final(md5_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* MD5_HIDER */
