/* snefru.h */
#ifndef SNEFRU_H
#define SNEFRU_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Snefru-128 processses message by blocks of 48 bytes, */
/* and Snefru-256 uses blocks of 32 bytes */

/* here we declare the maximal block size */
#define snefru_block_size 48

#define snefru128_hash_length 16
#define snefru256_hash_length 32

/* algorithm context */
typedef struct snefru_ctx
{
	unsigned hash[8];         /* algorithm 512-bit hashing state */
	unsigned char buffer[48]; /* 384-bit message block */
	uint64_t length;          /* processed message length */
	unsigned index;           /* index in the buffer of the last byte stored */
	unsigned digest_length;   /* length of the algorithm digest in bytes */
} snefru_ctx;

/* hash functions */

void rhash_snefru128_init(snefru_ctx* ctx);
void rhash_snefru256_init(snefru_ctx* ctx);
void rhash_snefru_update(snefru_ctx* ctx, const unsigned char* data, size_t size);
void rhash_snefru_final(snefru_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* SNEFRU_H */
