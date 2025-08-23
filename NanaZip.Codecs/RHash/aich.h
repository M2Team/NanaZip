/* aich.h */
#ifndef AICH_H
#define AICH_H
#include "byte_order.h"
#include "sha1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* algorithm context */
typedef struct aich_ctx
{
	sha1_ctx sha1_context; /* context used to hash tree leaves */
#if defined(USE_OPENSSL) || defined(OPENSSL_RUNTIME)
	unsigned long reserved; /* need more space for openssl sha1 context */
#endif
	unsigned index;        /* algorithm position in the current ed2k chunk */
	int error;             /* non-zero if a memory error occurred, 0 otherwise */
	size_t chunks_count;   /* the number of ed2k chunks hashed */
	size_t allocated;      /* allocated size of the chunk_table */
	unsigned char (*block_hashes)[sha1_hash_size];
	void** chunk_table;    /* table of chunk hashes */
#if defined(USE_OPENSSL) || defined(OPENSSL_RUNTIME)
	rhash_hashing_methods sha1_methods;
#endif
} aich_ctx;

/* hash functions */

void rhash_aich_init(aich_ctx* ctx);
void rhash_aich_update(aich_ctx* ctx, const unsigned char* msg, size_t size);
void rhash_aich_final(aich_ctx* ctx, unsigned char result[20]);

#if !defined(NO_IMPORT_EXPORT)
size_t rhash_aich_export(const aich_ctx* ctx, void* out, size_t size);
size_t rhash_aich_import(aich_ctx* ctx, const void* in, size_t size);
#endif /* !defined(NO_IMPORT_EXPORT) */

/* Clean up context by freeing allocated memory.
 * The function is called automatically by rhash_aich_final.
 * Shall be called when aborting hash calculations. */
void rhash_aich_cleanup(aich_ctx* ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* AICH_H */
