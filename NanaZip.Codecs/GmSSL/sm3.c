/*
 *  Copyright 2014-2023 The GmSSL Project. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the License); you may
 *  not use this file except in compliance with the License.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 */


#include <string.h>
#include "sm3.h"
#include "endian.h"


#ifdef SM3_SSE3
# include <x86intrin.h>
# include <immintrin.h>

# define _mm_rotl_epi32(X,i) \
	_mm_xor_si128(_mm_slli_epi32((X),(i)), _mm_srli_epi32((X),32-(i)))
#endif


#define ROTL(x,n)  (((x)<<(n)) | ((x)>>(32-(n))))
#define P0(x) ((x) ^ ROL32((x), 9) ^ ROL32((x),17))
#define P1(x) ((x) ^ ROL32((x),15) ^ ROL32((x),23))

#define FF00(x,y,z)  ((x) ^ (y) ^ (z))
#define FF16(x,y,z)  (((x)&(y)) | ((x)&(z)) | ((y)&(z)))
#define GG00(x,y,z)  ((x) ^ (y) ^ (z))
#define GG16(x,y,z)  ((((y)^(z)) & (x)) ^ (z))

#define R(A, B, C, D, E, F, G, H, xx)				\
	SS1 = ROL32((ROL32(A, 12) + E + K[j]), 7);		\
	SS2 = SS1 ^ ROL32(A, 12);				\
	TT1 = FF##xx(A, B, C) + D + SS2 + (W[j] ^ W[j + 4]);	\
	TT2 = GG##xx(E, F, G) + H + SS1 + W[j];			\
	B = ROL32(B, 9);					\
	H = TT1;						\
	F = ROL32(F, 19);					\
	D = P0(TT2);						\
	j++

#define R8(A, B, C, D, E, F, G, H, xx)				\
	R(A, B, C, D, E, F, G, H, xx);				\
	R(H, A, B, C, D, E, F, G, xx);				\
	R(G, H, A, B, C, D, E, F, xx);				\
	R(F, G, H, A, B, C, D, E, xx);				\
	R(E, F, G, H, A, B, C, D, xx);				\
	R(D, E, F, G, H, A, B, C, xx);				\
	R(C, D, E, F, G, H, A, B, xx);				\
	R(B, C, D, E, F, G, H, A, xx)



#define T00 0x79cc4519U
#define T16 0x7a879d8aU

#define K0	0x79cc4519U
#define K1	0xf3988a32U
#define K2	0xe7311465U
#define K3	0xce6228cbU
#define K4	0x9cc45197U
#define K5	0x3988a32fU
#define K6	0x7311465eU
#define K7	0xe6228cbcU
#define K8	0xcc451979U
#define K9	0x988a32f3U
#define K10	0x311465e7U
#define K11	0x6228cbceU
#define K12	0xc451979cU
#define K13	0x88a32f39U
#define K14	0x11465e73U
#define K15	0x228cbce6U
#define K16	0x9d8a7a87U
#define K17	0x3b14f50fU
#define K18	0x7629ea1eU
#define K19	0xec53d43cU
#define K20	0xd8a7a879U
#define K21	0xb14f50f3U
#define K22	0x629ea1e7U
#define K23	0xc53d43ceU
#define K24	0x8a7a879dU
#define K25	0x14f50f3bU
#define K26	0x29ea1e76U
#define K27	0x53d43cecU
#define K28	0xa7a879d8U
#define K29	0x4f50f3b1U
#define K30	0x9ea1e762U
#define K31	0x3d43cec5U
#define K32	0x7a879d8aU
#define K33	0xf50f3b14U
#define K34	0xea1e7629U
#define K35	0xd43cec53U
#define K36	0xa879d8a7U
#define K37	0x50f3b14fU
#define K38	0xa1e7629eU
#define K39	0x43cec53dU
#define K40	0x879d8a7aU
#define K41	0x0f3b14f5U
#define K42	0x1e7629eaU
#define K43	0x3cec53d4U
#define K44	0x79d8a7a8U
#define K45	0xf3b14f50U
#define K46	0xe7629ea1U
#define K47	0xcec53d43U
#define K48	0x9d8a7a87U
#define K49	0x3b14f50fU
#define K50	0x7629ea1eU
#define K51	0xec53d43cU
#define K52	0xd8a7a879U
#define K53	0xb14f50f3U
#define K54	0x629ea1e7U
#define K55	0xc53d43ceU
#define K56	0x8a7a879dU
#define K57	0x14f50f3bU
#define K58	0x29ea1e76U
#define K59	0x53d43cecU
#define K60	0xa7a879d8U
#define K61	0x4f50f3b1U
#define K62	0x9ea1e762U
#define K63	0x3d43cec5U

static uint32_t K[64] = {
	K0,  K1,  K2,  K3,  K4,  K5,  K6,  K7,
	K8,  K9,  K10, K11, K12, K13, K14, K15,
	K16, K17, K18, K19, K20, K21, K22, K23,
	K24, K25, K26, K27, K28, K29, K30, K31,
	K32, K33, K34, K35, K36, K37, K38, K39,
	K40, K41, K42, K43, K44, K45, K46, K47,
	K48, K49, K50, K51, K52, K53, K54, K55,
	K56, K57, K58, K59, K60, K61, K62, K63,
	/*
	0x79cc4519U, 0xf3988a32U, 0xe7311465U, 0xce6228cbU,
	0x9cc45197U, 0x3988a32fU, 0x7311465eU, 0xe6228cbcU,
	0xcc451979U, 0x988a32f3U, 0x311465e7U, 0x6228cbceU,
	0xc451979cU, 0x88a32f39U, 0x11465e73U, 0x228cbce6U,
	0x9d8a7a87U, 0x3b14f50fU, 0x7629ea1eU, 0xec53d43cU,
	0xd8a7a879U, 0xb14f50f3U, 0x629ea1e7U, 0xc53d43ceU,
	0x8a7a879dU, 0x14f50f3bU, 0x29ea1e76U, 0x53d43cecU,
	0xa7a879d8U, 0x4f50f3b1U, 0x9ea1e762U, 0x3d43cec5U,
	0x7a879d8aU, 0xf50f3b14U, 0xea1e7629U, 0xd43cec53U,
	0xa879d8a7U, 0x50f3b14fU, 0xa1e7629eU, 0x43cec53dU,
	0x879d8a7aU, 0x0f3b14f5U, 0x1e7629eaU, 0x3cec53d4U,
	0x79d8a7a8U, 0xf3b14f50U, 0xe7629ea1U, 0xcec53d43U,
	0x9d8a7a87U, 0x3b14f50fU, 0x7629ea1eU, 0xec53d43cU,
	0xd8a7a879U, 0xb14f50f3U, 0x629ea1e7U, 0xc53d43ceU,
	0x8a7a879dU, 0x14f50f3bU, 0x29ea1e76U, 0x53d43cecU,
	0xa7a879d8U, 0x4f50f3b1U, 0x9ea1e762U, 0x3d43cec5U,
	*/
};

#ifndef SM3_AVX_BMI2
void sm3_compress_blocks(uint32_t digest[8], const uint8_t *data, size_t blocks)
{
	uint32_t A;
	uint32_t B;
	uint32_t C;
	uint32_t D;
	uint32_t E;
	uint32_t F;
	uint32_t G;
	uint32_t H;
	uint32_t W[68];
	uint32_t SS1, SS2, TT1, TT2;
	int j;

#ifdef SM3_SSE3
	__m128i X, T, R;
	__m128i M = _mm_setr_epi32(0, 0, 0, 0xffffffff);
	__m128i V = _mm_setr_epi8(3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12);
#endif

	while (blocks--) {

		A = digest[0];
		B = digest[1];
		C = digest[2];
		D = digest[3];
		E = digest[4];
		F = digest[5];
		G = digest[6];
		H = digest[7];


#ifdef SM3_SSE3

		for (j = 0; j < 16; j += 4) {
			X = _mm_loadu_si128((__m128i *)(data + j * 4));
			X = _mm_shuffle_epi8(X, V);
			_mm_storeu_si128((__m128i *)(W + j), X);
		}

		for (j = 16; j < 68; j += 4) {
			/* X = (W[j - 3], W[j - 2], W[j - 1], 0) */
			X = _mm_loadu_si128((__m128i *)(W + j - 3));
			X = _mm_andnot_si128(M, X);

			X = _mm_rotl_epi32(X, 15);
			T = _mm_loadu_si128((__m128i *)(W + j - 9));
			X = _mm_xor_si128(X, T);
			T = _mm_loadu_si128((__m128i *)(W + j - 16));
			X = _mm_xor_si128(X, T);

			/* P1() */
			T = _mm_rotl_epi32(X, (23 - 15));
			T = _mm_xor_si128(T, X);
			T = _mm_rotl_epi32(T, 15);
			X = _mm_xor_si128(X, T);

			T = _mm_loadu_si128((__m128i *)(W + j - 13));
			T = _mm_rotl_epi32(T, 7);
			X = _mm_xor_si128(X, T);
			T = _mm_loadu_si128((__m128i *)(W + j - 6));
			X = _mm_xor_si128(X, T);

			/* W[j + 3] ^= P1(ROL32(W[j + 1], 15)) */
			R = _mm_shuffle_epi32(X, 0);
			R = _mm_and_si128(R, M);
			T = _mm_rotl_epi32(R, 15);
			T = _mm_xor_si128(T, R);
			T = _mm_rotl_epi32(T, 9);
			R = _mm_xor_si128(R, T);
			R = _mm_rotl_epi32(R, 6);
			X = _mm_xor_si128(X, R);

			_mm_storeu_si128((__m128i *)(W + j), X);
		}
#else
		for (j = 0; j < 16; j++)
			W[j] = GETU32(data + j*4);

		for (; j < 68; j++)
			W[j] = P1(W[j - 16] ^ W[j - 9] ^ ROL32(W[j - 3], 15))
				^ ROL32(W[j - 13], 7) ^ W[j - 6];
#endif


		j = 0;

#define FULL_UNROLL
#ifdef FULL_UNROLL
		R8(A, B, C, D, E, F, G, H, 00);
		R8(A, B, C, D, E, F, G, H, 00);
		R8(A, B, C, D, E, F, G, H, 16);
		R8(A, B, C, D, E, F, G, H, 16);
		R8(A, B, C, D, E, F, G, H, 16);
		R8(A, B, C, D, E, F, G, H, 16);
		R8(A, B, C, D, E, F, G, H, 16);
		R8(A, B, C, D, E, F, G, H, 16);
#else
		for (; j < 16; j++) {
			SS1 = ROL32((ROL32(A, 12) + E + K(j)), 7);
			SS2 = SS1 ^ ROL32(A, 12);
			TT1 = FF00(A, B, C) + D + SS2 + (W[j] ^ W[j + 4]);
			TT2 = GG00(E, F, G) + H + SS1 + W[j];
			D = C;
			C = ROL32(B, 9);
			B = A;
			A = TT1;
			H = G;
			G = ROL32(F, 19);
			F = E;
			E = P0(TT2);
		}

		for (; j < 64; j++) {
			SS1 = ROL32((ROL32(A, 12) + E + K(j)), 7);
			SS2 = SS1 ^ ROL32(A, 12);
			TT1 = FF16(A, B, C) + D + SS2 + (W[j] ^ W[j + 4]);
			TT2 = GG16(E, F, G) + H + SS1 + W[j];
			D = C;
			C = ROL32(B, 9);
			B = A;
			A = TT1;
			H = G;
			G = ROL32(F, 19);
			F = E;
			E = P0(TT2);
		}
#endif

		digest[0] ^= A;
		digest[1] ^= B;
		digest[2] ^= C;
		digest[3] ^= D;
		digest[4] ^= E;
		digest[5] ^= F;
		digest[6] ^= G;
		digest[7] ^= H;

		data += 64;
	}
}
#endif

void sm3_init(SM3_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->digest[0] = 0x7380166F;
	ctx->digest[1] = 0x4914B2B9;
	ctx->digest[2] = 0x172442D7;
	ctx->digest[3] = 0xDA8A0600;
	ctx->digest[4] = 0xA96F30BC;
	ctx->digest[5] = 0x163138AA;
	ctx->digest[6] = 0xE38DEE4D;
	ctx->digest[7] = 0xB0FB0E4E;
}

void sm3_update(SM3_CTX *ctx, const uint8_t *data, size_t data_len)
{
	size_t blocks;

	ctx->num &= 0x3f;
	if (ctx->num) {
		size_t left = SM3_BLOCK_SIZE - ctx->num;
		if (data_len < left) {
			memcpy(ctx->block + ctx->num, data, data_len);
			ctx->num += data_len;
			return;
		} else {
			memcpy(ctx->block + ctx->num, data, left);
			sm3_compress_blocks(ctx->digest, ctx->block, 1);
			ctx->nblocks++;
			data += left;
			data_len -= left;
		}
	}

	blocks = data_len / SM3_BLOCK_SIZE;
	if (blocks) {
		sm3_compress_blocks(ctx->digest, data, blocks);
		ctx->nblocks += blocks;
		data += SM3_BLOCK_SIZE * blocks;
		data_len -= SM3_BLOCK_SIZE * blocks;
	}

	ctx->num = data_len;
	if (data_len) {
		memcpy(ctx->block, data, data_len);
	}
}

void sm3_finish(SM3_CTX *ctx, uint8_t *digest)
{
	int i;

	ctx->num &= 0x3f;
	ctx->block[ctx->num] = 0x80;

	if (ctx->num <= SM3_BLOCK_SIZE - 9) {
		memset(ctx->block + ctx->num + 1, 0, SM3_BLOCK_SIZE - ctx->num - 9);
	} else {
		memset(ctx->block + ctx->num + 1, 0, SM3_BLOCK_SIZE - ctx->num - 1);
		sm3_compress_blocks(ctx->digest, ctx->block, 1);
		memset(ctx->block, 0, SM3_BLOCK_SIZE - 8);
	}
	PUTU32(ctx->block + 56, ctx->nblocks >> 23);
	PUTU32(ctx->block + 60, (ctx->nblocks << 9) + (ctx->num << 3));

	sm3_compress_blocks(ctx->digest, ctx->block, 1);
	for (i = 0; i < 8; i++) {
		PUTU32(digest + i*4, ctx->digest[i]);
	}
}

void sm3_digest(const uint8_t *msg, size_t msglen,
	uint8_t dgst[SM3_DIGEST_SIZE])
{
	SM3_CTX ctx;
	sm3_init(&ctx);
	sm3_update(&ctx, msg, msglen);
	sm3_finish(&ctx, dgst);
	memset(&ctx, 0, sizeof(ctx));
}
