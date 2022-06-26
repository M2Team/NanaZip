#ifndef TTH_H
#define TTH_H
#include "ustd.h"
#include "tiger.h"

#ifdef __cplusplus
extern "C" {
#endif

/* algorithm context */
typedef struct tth_ctx
{
	tiger_ctx tiger;       /* context used to hash tree leaves */
	uint64_t block_count;  /* number of processed blocks */
	uint64_t stack[64 * 3];
} tth_ctx;

/* hash functions */

void rhash_tth_init(tth_ctx* ctx);
void rhash_tth_update(tth_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_tth_final(tth_ctx* ctx, unsigned char result[24]);

#if !defined(NO_IMPORT_EXPORT)
size_t rhash_tth_export(const tth_ctx* ctx, void* out, size_t size);
size_t rhash_tth_import(tth_ctx* ctx, const void* in, size_t size);
#endif /* !defined(NO_IMPORT_EXPORT) */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* TTH_H */
