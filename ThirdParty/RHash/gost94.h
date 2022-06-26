/* gost94.h */
#ifndef GOST94_H
#define GOST94_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define gost94_block_size 32
#define gost94_hash_length 32

/* algorithm context */
typedef struct gost94_ctx
{
	unsigned hash[8];  /* algorithm 256-bit state */
	unsigned sum[8];   /* sum of processed message blocks */
	unsigned char message[gost94_block_size]; /* 256-bit buffer for leftovers */
	uint64_t length;   /* number of processed bytes */
	unsigned cryptpro; /* boolean flag, the type of sbox to use */
} gost94_ctx;

/* hash functions */

void rhash_gost94_init(gost94_ctx* ctx);
void rhash_gost94_cryptopro_init(gost94_ctx* ctx);
void rhash_gost94_update(gost94_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_gost94_final(gost94_ctx* ctx, unsigned char result[32]);

#ifdef GENERATE_GOST94_LOOKUP_TABLE
void rhash_gost94_init_table(void); /* initialize algorithm static data */
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* GOST94_H */
