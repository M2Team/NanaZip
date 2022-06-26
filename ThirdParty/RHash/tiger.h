/* tiger.h */
#ifndef TIGER_H
#define TIGER_H
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define tiger_block_size 64
#define tiger_hash_length 24

/* algorithm context */
typedef struct tiger_ctx
{
	/* the order of the fields slightly influence the algorithm speed */
	uint64_t hash[3]; /* algorithm 192-bit state */
	unsigned char message[tiger_block_size]; /* 512-bit buffer for leftovers */
	uint64_t length;  /* processed message length */
} tiger_ctx;

/* hash functions */

#ifndef NO_TIGER2
void rhash_tiger2_init(tiger_ctx* ctx);
#endif /* NO_TIGER2 */

void rhash_tiger_init(tiger_ctx* ctx);
void rhash_tiger_update(tiger_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_tiger_final(tiger_ctx* ctx, unsigned char result[24]);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* TIGER_H */
