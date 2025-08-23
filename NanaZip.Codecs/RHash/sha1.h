/* sha1.h */
#ifndef SHA1_H
#define SHA1_H
#include "ustd.h"
#include <K7Pal.h>

#ifdef __cplusplus
extern "C" {
#endif

#define sha1_block_size 64
#define sha1_hash_size  20

/* algorithm context */
typedef struct sha1_ctx
{
    K7_PAL_HASH_HANDLE context; /* internal context */
	uint64_t length;   /* number of processed bytes */
	unsigned hash[5];  /* 160-bit algorithm internal hashing state */
} sha1_ctx;

/* hash functions */

void rhash_sha1_init(sha1_ctx* ctx);
void rhash_sha1_update(sha1_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_sha1_final(sha1_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* SHA1_H */
