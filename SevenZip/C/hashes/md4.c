
/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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
#include "md4.h"

#define A m->counter[0]
#define B m->counter[1]
#define C m->counter[2]
#define D m->counter[3]
#define X data

/* Vector Crays doesn't have a good 32-bit type, or more precisely,
   int32_t as defined by <bind/bitypes.h> isn't 32 bits, and we don't
   want to depend in being able to redefine this type.  To cope with
   this we have to clamp the result in some places to [0,2^32); no
   need to do this on other machines.  Did I say this was a mess?
   */

#ifdef _CRAY
#define CRAYFIX(X) ((X) & 0xffffffff)
#else
#define CRAYFIX(X) (X)
#endif

static uint32_t cshift (uint32_t x, unsigned int n)
{
    x = CRAYFIX(x);
    return CRAYFIX((x << n) | (x >> (32 - n)));
}

void
MD4_Init (struct md4 *m)
{
  m->sz[0] = 0;
  m->sz[1] = 0;
  D = 0x10325476;
  C = 0x98badcfe;
  B = 0xefcdab89;
  A = 0x67452301;
}

#define F(x,y,z) CRAYFIX((x & y) | (~x & z))
#define G(x,y,z) ((x & y) | (x & z) | (y & z))
#define H(x,y,z) (x ^ y ^ z)

#define DOIT(a,b,c,d,k,s,i,OP) \
a = cshift(a + OP(b,c,d) + X[k] + i, s)

#define DO1(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,F)
#define DO2(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,G)
#define DO3(a,b,c,d,k,s,i) DOIT(a,b,c,d,k,s,i,H)

static void
calc (struct md4 *m, uint32_t *data)
{
  uint32_t AA, BB, CC, DD;

  AA = A;
  BB = B;
  CC = C;
  DD = D;

  /* Round 1 */

  DO1(A,B,C,D,0,3,0);
  DO1(D,A,B,C,1,7,0);
  DO1(C,D,A,B,2,11,0);
  DO1(B,C,D,A,3,19,0);

  DO1(A,B,C,D,4,3,0);
  DO1(D,A,B,C,5,7,0);
  DO1(C,D,A,B,6,11,0);
  DO1(B,C,D,A,7,19,0);

  DO1(A,B,C,D,8,3,0);
  DO1(D,A,B,C,9,7,0);
  DO1(C,D,A,B,10,11,0);
  DO1(B,C,D,A,11,19,0);

  DO1(A,B,C,D,12,3,0);
  DO1(D,A,B,C,13,7,0);
  DO1(C,D,A,B,14,11,0);
  DO1(B,C,D,A,15,19,0);

  /* Round 2 */

  DO2(A,B,C,D,0,3,0x5A827999);
  DO2(D,A,B,C,4,5,0x5A827999);
  DO2(C,D,A,B,8,9,0x5A827999);
  DO2(B,C,D,A,12,13,0x5A827999);

  DO2(A,B,C,D,1,3,0x5A827999);
  DO2(D,A,B,C,5,5,0x5A827999);
  DO2(C,D,A,B,9,9,0x5A827999);
  DO2(B,C,D,A,13,13,0x5A827999);

  DO2(A,B,C,D,2,3,0x5A827999);
  DO2(D,A,B,C,6,5,0x5A827999);
  DO2(C,D,A,B,10,9,0x5A827999);
  DO2(B,C,D,A,14,13,0x5A827999);

  DO2(A,B,C,D,3,3,0x5A827999);
  DO2(D,A,B,C,7,5,0x5A827999);
  DO2(C,D,A,B,11,9,0x5A827999);
  DO2(B,C,D,A,15,13,0x5A827999);

  /* Round 3 */

  DO3(A,B,C,D,0,3,0x6ED9EBA1);
  DO3(D,A,B,C,8,9,0x6ED9EBA1);
  DO3(C,D,A,B,4,11,0x6ED9EBA1);
  DO3(B,C,D,A,12,15,0x6ED9EBA1);

  DO3(A,B,C,D,2,3,0x6ED9EBA1);
  DO3(D,A,B,C,10,9,0x6ED9EBA1);
  DO3(C,D,A,B,6,11,0x6ED9EBA1);
  DO3(B,C,D,A,14,15,0x6ED9EBA1);

  DO3(A,B,C,D,1,3,0x6ED9EBA1);
  DO3(D,A,B,C,9,9,0x6ED9EBA1);
  DO3(C,D,A,B,5,11,0x6ED9EBA1);
  DO3(B,C,D,A,13,15,0x6ED9EBA1);

  DO3(A,B,C,D,3,3,0x6ED9EBA1);
  DO3(D,A,B,C,11,9,0x6ED9EBA1);
  DO3(C,D,A,B,7,11,0x6ED9EBA1);
  DO3(B,C,D,A,15,15,0x6ED9EBA1);

  A += AA;
  B += BB;
  C += CC;
  D += DD;
}

/*
 * From `Performance analysis of MD5' by Joseph D. Touch <touch@isi.edu>
 */

#if defined(WORDS_BIGENDIAN)
static uint32_t
swap_uint32_t (uint32_t t)
{
  uint32_t temp1, temp2;

  temp1   = cshift(t, 16);
  temp2   = temp1 >> 8;
  temp1  &= 0x00ff00ff;
  temp2  &= 0x00ff00ff;
  temp1 <<= 8;
  return temp1 | temp2;
}
#endif

struct x32{
  unsigned int a:32;
  unsigned int b:32;
};

void
MD4_Update (struct md4 *m, const void *v, size_t len)
{
    const unsigned char *p = v;
    size_t old_sz = m->sz[0];
    size_t offset;

    m->sz[0] += (unsigned int)len * 8;
    if (m->sz[0] < old_sz)
	++m->sz[1];
    offset = (old_sz / 8)  % 64;
    while(len > 0) {
	size_t l = min(len, 64 - offset);
	memcpy(m->save + offset, p, l);
	offset += l;
	p += l;
	len -= l;
	if(offset == 64) {
#if defined(WORDS_BIGENDIAN)
	    int i;
	    uint32_t current[16];
	    struct x32 *us = (struct x32*)m->save;
	    for(i = 0; i < 8; i++){
		current[2*i+0] = swap_uint32_t(us[i].a);
		current[2*i+1] = swap_uint32_t(us[i].b);
	    }
	    calc(m, current);
#else
	    calc(m, (uint32_t*)m->save);
#endif
	    offset = 0;
	}
    }
}

void
MD4_Final (void *res, struct md4 *m)
{
  unsigned char zeros[72];
  unsigned offset = (m->sz[0] / 8) % 64;
  unsigned int dstart = (120 - offset - 1) % 64 + 1;

  *zeros = 0x80;
  memset (zeros + 1, 0, sizeof(zeros) - 1);
  zeros[dstart+0] = (m->sz[0] >> 0) & 0xff;
  zeros[dstart+1] = (m->sz[0] >> 8) & 0xff;
  zeros[dstart+2] = (m->sz[0] >> 16) & 0xff;
  zeros[dstart+3] = (m->sz[0] >> 24) & 0xff;
  zeros[dstart+4] = (m->sz[1] >> 0) & 0xff;
  zeros[dstart+5] = (m->sz[1] >> 8) & 0xff;
  zeros[dstart+6] = (m->sz[1] >> 16) & 0xff;
  zeros[dstart+7] = (m->sz[1] >> 24) & 0xff;
  MD4_Update (m, zeros, dstart + 8);
  {
      int i;
      unsigned char *r = (unsigned char *)res;

      for (i = 0; i < 4; ++i) {
	  r[4*i]   = m->counter[i] & 0xFF;
	  r[4*i+1] = (m->counter[i] >> 8) & 0xFF;
	  r[4*i+2] = (m->counter[i] >> 16) & 0xFF;
	  r[4*i+3] = (m->counter[i] >> 24) & 0xFF;
      }
  }
#if 0
  {
    int i;
    uint32_t *r = (uint32_t *)res;

    for (i = 0; i < 4; ++i)
      r[i] = swap_uint32_t (m->counter[i]);
  }
#endif
}
