/*
 *  Copyright 2014-2022 The GmSSL Project. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the License); you may
 *  not use this file except in compliance with the License.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 */


#ifndef GMSSL_SM3_H
#define GMSSL_SM3_H

#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
SM3 Public API

	SM3_DIGEST_SIZE
	SM3_HMAC_SIZE

	SM3_CTX
	sm3_init
	sm3_update
	sm3_finish

	SM3_HMAC_CTX
	sm3_hmac_init
	sm3_hmac_update
	sm3_hmac_finish

	sm3_digest
	sm3_hmac
*/

#define SM3_IS_BIG_ENDIAN	1

#define SM3_DIGEST_SIZE		32
#define SM3_BLOCK_SIZE		64
#define SM3_STATE_WORDS		8
#define SM3_HMAC_SIZE		(SM3_DIGEST_SIZE)


typedef struct {
	uint32_t digest[SM3_STATE_WORDS];
	uint64_t nblocks;
	uint8_t block[SM3_BLOCK_SIZE];
	size_t num;
} SM3_CTX;

void sm3_init(SM3_CTX *ctx);
void sm3_update(SM3_CTX *ctx, const uint8_t *data, size_t datalen);
void sm3_finish(SM3_CTX *ctx, uint8_t dgst[SM3_DIGEST_SIZE]);
void sm3_digest(const uint8_t *data, size_t datalen, uint8_t dgst[SM3_DIGEST_SIZE]);

void sm3_compress_blocks(uint32_t digest[8], const uint8_t *data, size_t blocks);

typedef struct {
	SM3_CTX sm3_ctx;
	uint8_t key[SM3_BLOCK_SIZE];
} SM3_HMAC_CTX;

void sm3_hmac_init(SM3_HMAC_CTX *ctx, const uint8_t *key, size_t keylen);
void sm3_hmac_update(SM3_HMAC_CTX *ctx, const uint8_t *data, size_t datalen);
void sm3_hmac_finish(SM3_HMAC_CTX *ctx, uint8_t mac[SM3_HMAC_SIZE]);
void sm3_hmac(const uint8_t *key, size_t keylen,
	const uint8_t *data, size_t datalen,
	uint8_t mac[SM3_HMAC_SIZE]);


typedef struct {
	SM3_CTX sm3_ctx;
	size_t outlen;
} SM3_KDF_CTX;

void sm3_kdf_init(SM3_KDF_CTX *ctx, size_t outlen);
void sm3_kdf_update(SM3_KDF_CTX *ctx, const uint8_t *data, size_t datalen);
void sm3_kdf_finish(SM3_KDF_CTX *ctx, uint8_t *out);


#ifdef __cplusplus
}
#endif
#endif
