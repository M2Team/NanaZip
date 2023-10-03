/* sha.h sha256 and sha224 hash functions */
#ifndef SHA256_H
#define SHA256_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define sha256_block_size 64
#define sha256_hash_size  32
#define sha224_hash_size  28

/* algorithm context */
typedef struct sha256_ctx
{
	unsigned message[16];   /* 512-bit buffer for leftovers */
	uint64_t length;        /* number of processed bytes */
	unsigned hash[8];       /* 256-bit algorithm internal hashing state */
	unsigned digest_length; /* length of the algorithm digest in bytes */
} sha256_ctx;

void rhash_sha224_init(sha256_ctx* ctx);
void rhash_sha256_init(sha256_ctx* ctx);
void rhash_sha256_update(sha256_ctx* ctx, const unsigned char* data, size_t length);
void rhash_sha256_final(sha256_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* SHA256_H */
