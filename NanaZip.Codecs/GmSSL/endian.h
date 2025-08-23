/*
 *  Copyright 2014-2022 The GmSSL Project. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the License); you may
 *  not use this file except in compliance with the License.
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 */


#ifndef GMSSL_ENDIAN_H
#define GMSSL_ENDIAN_H


/* Big Endian R/W */

#define GETU16(p) \
	((uint16_t)(p)[0] <<  8 | \
	 (uint16_t)(p)[1])

#define GETU32(p) \
	((uint32_t)(p)[0] << 24 | \
	 (uint32_t)(p)[1] << 16 | \
	 (uint32_t)(p)[2] <<  8 | \
	 (uint32_t)(p)[3])

#define GETU64(p) \
	((uint64_t)(p)[0] << 56 | \
	 (uint64_t)(p)[1] << 48 | \
	 (uint64_t)(p)[2] << 40 | \
	 (uint64_t)(p)[3] << 32 | \
	 (uint64_t)(p)[4] << 24 | \
	 (uint64_t)(p)[5] << 16 | \
	 (uint64_t)(p)[6] <<  8 | \
	 (uint64_t)(p)[7])


// 注意：PUTU32(buf, val++) 会出错！
#define PUTU16(p,V) \
	((p)[0] = (uint8_t)((V) >> 8), \
	 (p)[1] = (uint8_t)(V))

#define PUTU32(p,V) \
	((p)[0] = (uint8_t)((V) >> 24), \
	 (p)[1] = (uint8_t)((V) >> 16), \
	 (p)[2] = (uint8_t)((V) >>  8), \
	 (p)[3] = (uint8_t)(V))

#define PUTU64(p,V) \
	((p)[0] = (uint8_t)((V) >> 56), \
	 (p)[1] = (uint8_t)((V) >> 48), \
	 (p)[2] = (uint8_t)((V) >> 40), \
	 (p)[3] = (uint8_t)((V) >> 32), \
	 (p)[4] = (uint8_t)((V) >> 24), \
	 (p)[5] = (uint8_t)((V) >> 16), \
	 (p)[6] = (uint8_t)((V) >>  8), \
	 (p)[7] = (uint8_t)(V))

/* Little Endian R/W */

#define GETU16_LE(p)	(*(const uint16_t *)(p))
#define GETU32_LE(p)	(*(const uint32_t *)(p))
#define GETU64_LE(p)	(*(const uint64_t *)(p))

#define PUTU16_LE(p,V)	*(uint16_t *)(p) = (V)
#define PUTU32_LE(p,V)	*(uint32_t *)(p) = (V)
#define PUTU64_LE(p,V)	*(uint64_t *)(p) = (V)

/* Rotate */

#define ROL32(a,n)     (((a)<<(n))|(((a)&0xffffffff)>>(32-(n))))
#define ROL64(a,n)	(((a)<<(n))|((a)>>(64-(n))))

#define ROR32(a,n)	ROL32((a),32-(n))
#define ROR64(a,n)	ROL64(a,64-n)


#endif
