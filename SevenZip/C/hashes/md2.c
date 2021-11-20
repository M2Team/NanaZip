
/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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
#include "md2.h"

static const unsigned char subst[256] = {
  41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
  19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
  76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
  138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
  245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
  148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
  39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
  181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
  150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
  112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
  96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
  85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
  234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
  129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
  8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
  203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
  166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
  31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

void
MD2_Init (struct md2 *m)
{
    memset(m, 0, sizeof(*m));
}

static void
calc(struct md2 *m, const void *v)
{
    unsigned char x[48], L;
    const unsigned char *p = v;
    int i, j, t;

    L = m->checksum[15];
    for (i = 0; i < 16; i++)
	L = m->checksum[i] ^= subst[p[i] ^ L];

    for (i = 0; i < 16; i++) {
	x[i]      = m->state[i];
	x[i + 16] = p[i];
	x[i + 32] = x[i] ^ p[i];
    }

    t = 0;
    for (i = 0; i < 18; i++) {
	for (j = 0; j < 48; j++)
	    t = x[j] ^= subst[t];
	t = (t + i) & 0xff;
    }

    memcpy(m->state, x, 16);
    memset(x, 0, sizeof(x));
}

void
MD2_Update (struct md2 *m, const void *v, size_t len)
{
    size_t idx = m->len & 0xf;
    const unsigned char *p = v;

    m->len += len;
    if (len + idx >= 16) {
	if (idx) {
	    memcpy(m->data + idx, p, 16 - idx);
	    calc(m, m->data);
	    p += 16;
	    len -= 16 - idx;
	}
	while (len >= 16) {
	    calc(m, p);
	    p += 16;
	    len -= 16;
	}
	idx = 0;
    }

    memcpy(m->data + idx, p, len);
}

void
MD2_Final (void *res, struct md2 *m)
{
    unsigned char pad[16];
    size_t padlen;

    padlen = 16 - (m->len % 16);
    memset(pad, (int)padlen, padlen);

    MD2_Update(m, pad, padlen);
    memcpy(pad, m->checksum, 16);
    MD2_Update(m, pad, 16);

    memcpy(res, m->state, MD2_DIGEST_LENGTH);
    memset(m, 0, sizeof(*m));
}
