/* gost12.h */
#ifndef GOST12_H
#define GOST12_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define gost12_block_size 64
#define gost12_256_hash_size 32
#define gost12_512_hash_size 64

/* algorithm context */
typedef struct gost12_ctx {
	uint64_t h[8];
	uint64_t N[8];
	uint64_t S[8];
	uint64_t message[8];
	unsigned index;
	unsigned hash_size;
} gost12_ctx;

/* hash functions */

void rhash_gost12_256_init(gost12_ctx* ctx);
void rhash_gost12_512_init(gost12_ctx* ctx);
void rhash_gost12_update(gost12_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_gost12_final(gost12_ctx* ctx, unsigned char* result);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* GOST12_H */
