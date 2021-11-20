
/*
 * Copyright (c) 2006, 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hash.h"
#include "sha.h"

#define Ch(x,y,z) (((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define ROTR(x,n)   (((x)>>(n)) | ((x) << (64 - (n))))

#define Sigma0(x)	(ROTR(x,28) ^ ROTR(x,34) ^ ROTR(x,39))
#define Sigma1(x)	(ROTR(x,14) ^ ROTR(x,18) ^ ROTR(x,41))
#define sigma0(x)	(ROTR(x,1)  ^ ROTR(x,8)  ^ ((x)>>7))
#define sigma1(x)	(ROTR(x,19) ^ ROTR(x,61) ^ ((x)>>6))

#define A m->counter[0]
#define B m->counter[1]
#define C m->counter[2]
#define D m->counter[3]
#define E m->counter[4]
#define F m->counter[5]
#define G m->counter[6]
#define H m->counter[7]

static uint64_t cshift64 (uint64_t x, unsigned int n)
{
  return ((uint64_t)x << (uint64_t)n) | ((uint64_t)x >> ((uint64_t)64 - (uint64_t)n));
}

static const uint64_t constant_512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
    0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
    0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
    0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
    0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
    0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
    0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
    0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
    0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
    0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
    0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

void
SHA512_Init (SHA512_CTX *m)
{
    m->sz[0] = 0;
    m->sz[1] = 0;
    A = 0x6a09e667f3bcc908ULL;
    B = 0xbb67ae8584caa73bULL;
    C = 0x3c6ef372fe94f82bULL;
    D = 0xa54ff53a5f1d36f1ULL;
    E = 0x510e527fade682d1ULL;
    F = 0x9b05688c2b3e6c1fULL;
    G = 0x1f83d9abfb41bd6bULL;
    H = 0x5be0cd19137e2179ULL;
}

static void
calc (SHA512_CTX *m, uint64_t *in)
{
    uint64_t AA, BB, CC, DD, EE, FF, GG, HH;
    uint64_t data[80];
    int i;

    AA = A;
    BB = B;
    CC = C;
    DD = D;
    EE = E;
    FF = F;
    GG = G;
    HH = H;

    for (i = 0; i < 16; ++i)
	data[i] = in[i];
    for (i = 16; i < 80; ++i)
	data[i] = sigma1(data[i-2]) + data[i-7] +
	    sigma0(data[i-15]) + data[i - 16];

    for (i = 0; i < 80; i++) {
	uint64_t T1, T2;

	T1 = HH + Sigma1(EE) + Ch(EE, FF, GG) + constant_512[i] + data[i];
	T2 = Sigma0(AA) + Maj(AA,BB,CC);

	HH = GG;
	GG = FF;
	FF = EE;
	EE = DD + T1;
	DD = CC;
	CC = BB;
	BB = AA;
	AA = T1 + T2;
    }

    A += AA;
    B += BB;
    C += CC;
    D += DD;
    E += EE;
    F += FF;
    G += GG;
    H += HH;
}

/*
 * From `Performance analysis of MD5' by Joseph D. Touch <touch@isi.edu>
 */

#if !defined(WORDS_BIGENDIAN) || defined(_CRAY)
static uint64_t
swap_uint64_t (uint64_t t)
{
    uint64_t temp;

    temp   = cshift64(t, 32);
    temp = ((temp & 0xff00ff00ff00ff00ULL) >> 8) |
           ((temp & 0x00ff00ff00ff00ffULL) << 8);
    return ((temp & 0xffff0000ffff0000ULL) >> 16) |
           ((temp & 0x0000ffff0000ffffULL) << 16);
}

struct x64{
    uint64_t a;
    uint64_t b;
};
#endif

void
SHA512_Update (SHA512_CTX *m, const void *v, size_t len)
{
    const unsigned char *p = v;
    size_t old_sz = (size_t)m->sz[0];
    size_t offset;

    m->sz[0] += len * 8;
    if (m->sz[0] < old_sz)
	++m->sz[1];
    offset = (old_sz / 8) % 128;
    while(len > 0){
	size_t l = min(len, 128 - offset);
	memcpy(m->save + offset, p, l);
	offset += l;
	p += l;
	len -= l;
	if(offset == 128){
#if !defined(WORDS_BIGENDIAN) || defined(_CRAY)
	    int i;
	    uint64_t current[16];
	    struct x64 *us = (struct x64*)m->save;
	    for(i = 0; i < 8; i++){
		current[2*i+0] = swap_uint64_t(us[i].a);
		current[2*i+1] = swap_uint64_t(us[i].b);
	    }
	    calc(m, current);
#else
	    calc(m, (uint64_t*)m->save);
#endif
	    offset = 0;
	}
    }
}

void
SHA512_Final (void *res, SHA512_CTX *m)
{
    unsigned char zeros[128 + 16];
    unsigned offset = (m->sz[0] / 8) % 128;
    unsigned int dstart = (240 - offset - 1) % 128 + 1;

    *zeros = 0x80;
    memset (zeros + 1, 0, sizeof(zeros) - 1);
    zeros[dstart+15] = (m->sz[0] >> 0) & 0xff;
    zeros[dstart+14] = (m->sz[0] >> 8) & 0xff;
    zeros[dstart+13] = (m->sz[0] >> 16) & 0xff;
    zeros[dstart+12] = (m->sz[0] >> 24) & 0xff;
    zeros[dstart+11] = (m->sz[0] >> 32) & 0xff;
    zeros[dstart+10] = (m->sz[0] >> 40) & 0xff;
    zeros[dstart+9]  = (m->sz[0] >> 48) & 0xff;
    zeros[dstart+8]  = (m->sz[0] >> 56) & 0xff;

    zeros[dstart+7] = (m->sz[1] >> 0) & 0xff;
    zeros[dstart+6] = (m->sz[1] >> 8) & 0xff;
    zeros[dstart+5] = (m->sz[1] >> 16) & 0xff;
    zeros[dstart+4] = (m->sz[1] >> 24) & 0xff;
    zeros[dstart+3] = (m->sz[1] >> 32) & 0xff;
    zeros[dstart+2] = (m->sz[1] >> 40) & 0xff;
    zeros[dstart+1] = (m->sz[1] >> 48) & 0xff;
    zeros[dstart+0] = (m->sz[1] >> 56) & 0xff;
    SHA512_Update (m, zeros, dstart + 16);
    {
	int i;
	unsigned char *r = (unsigned char*)res;

	for (i = 0; i < 8; ++i) {
	    r[8*i+7] = m->counter[i] & 0xFF;
	    r[8*i+6] = (m->counter[i] >> 8) & 0xFF;
	    r[8*i+5] = (m->counter[i] >> 16) & 0xFF;
	    r[8*i+4] = (m->counter[i] >> 24) & 0xFF;
	    r[8*i+3] = (m->counter[i] >> 32) & 0XFF;
	    r[8*i+2] = (m->counter[i] >> 40) & 0xFF;
	    r[8*i+1] = (m->counter[i] >> 48) & 0xFF;
	    r[8*i]   = (m->counter[i] >> 56) & 0xFF;
	}
    }
}

void
SHA384_Init(SHA384_CTX *m)
{
    m->sz[0] = 0;
    m->sz[1] = 0;
    A = 0xcbbb9d5dc1059ed8ULL;
    B = 0x629a292a367cd507ULL;
    C = 0x9159015a3070dd17ULL;
    D = 0x152fecd8f70e5939ULL;
    E = 0x67332667ffc00b31ULL;
    F = 0x8eb44a8768581511ULL;
    G = 0xdb0c2e0d64f98fa7ULL;
    H = 0x47b5481dbefa4fa4ULL;
}

void
SHA384_Update (SHA384_CTX *m, const void *v, size_t len)
{
    SHA512_Update(m, v, len);
}

void
SHA384_Final (void *res, SHA384_CTX *m)
{
    unsigned char data[SHA512_DIGEST_LENGTH];
    SHA512_Final(data, m);
    memcpy(res, data, SHA384_DIGEST_LENGTH);
}
