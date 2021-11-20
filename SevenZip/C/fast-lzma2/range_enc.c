/*
* Bitwise range encoder by Igor Pavlov
* Modified by Conor McCarthy
*
* Public domain
*/

#include "fl2_internal.h"
#include "mem.h"
#include "platform.h"
#include "range_enc.h"

/* The first and last elements of these tables are never used */
BYTE price_table[2][kPriceTableSize] = { {
   0, 193, 182, 166, 154, 145, 137, 131,
 125, 120, 115, 111, 107, 103, 100,  97,
  94,  91,  89,  86,  84,  82,  80,  78,
  76,  74,  72,  71,  69,  67,  66,  64,
  63,  61,  60,  59,  57,  56,  55,  54,
  53,  52,  50,  49,  48,  47,  46,  45,
  44,  43,  42,  42,  41,  40,  39,  38,
  37,  36,  36,  35,  34,  33,  33,  32,
  31,  30,  30,  29,  28,  28,  27,  26,
  26,  25,  25,  24,  23,  23,  22,  21,
  21,  20,  20,  19,  19,  18,  18,  17,
  17,  16,  16,  15,  15,  14,  14,  13,
  13,  12,  12,  11,  11,  10,  10,   9,
   9,   8,   8,   8,   7,   7,   6,   6,
   5,   5,   5,   4,   4,   3,   3,   3,
   2,   2,   2,   1,   1,   0,   0,   0
}, {
   0,   0,   0,   1,   1,   2,   2,   2,
   3,   3,   3,   4,   4,   5,   5,   5,
   6,   6,   7,   7,   8,   8,   8,   9,
   9,  10,  10,  11,  11,  12,  12,  13,
  13,  13,  14,  14,  15,  15,  16,  17,
  17,  18,  18,  19,  19,  20,  20,  21,
  21,  22,  23,  23,  24,  24,  25,  26,
  26,  27,  28,  28,  29,  30,  30,  31,
  32,  33,  33,  34,  35,  36,  36,  37,
  38,  39,  40,  41,  41,  42,  43,  44,
  45,  46,  47,  48,  49,  50,  51,  53,
  54,  55,  56,  57,  59,  60,  61,  63,
  64,  66,  67,  69,  70,  72,  74,  76,
  78,  80,  82,  84,  86,  89,  91,  94,
  97, 100, 103, 107, 111, 115, 119, 125,
 130, 137, 145, 154, 165, 181, 192,   0
} };

#if 0

#include <stdio.h>

/* Generates price_table */
void RC_printPriceTable()
{
    static const unsigned test_size = 0x4000;
    const unsigned test_div = test_size >> 8;
    BYTE buf[0x3062];
    unsigned table0[kPriceTableSize];
    unsigned table1[kPriceTableSize];
    unsigned count[kPriceTableSize];
    memset(table0, 0, sizeof(table0));
    memset(table1, 0, sizeof(table1));
    memset(count, 0, sizeof(count));
    for (LZMA2_prob i = 31; i <= kBitModelTotal - 31; ++i) {
        RC_encoder rc;
        RC_reset(&rc);
        RC_setOutputBuffer(&rc, buf);
        for (unsigned j = 0; j < test_size; ++j) {
            LZMA2_prob prob = i;
            RC_encodeBit0(&rc, &prob);
        }
        RC_flush(&rc);
        table0[i >> kNumMoveReducingBits] += (unsigned)rc.out_index - 5;
        RC_reset(&rc);
        RC_setOutputBuffer(&rc, buf);
        for (unsigned j = 0; j < test_size; ++j) {
            LZMA2_prob prob = i;
            RC_encodeBit1(&rc, &prob);
        }
        RC_flush(&rc);
        table1[i >> kNumMoveReducingBits] += (unsigned)rc.out_index - 5;
        ++count[i >> kNumMoveReducingBits];
    }
    for (int i = 0; i < kPriceTableSize; ++i) if (count[i]) {
        table0[i] = (table0[i] / count[i]) / test_div;
        table1[i] = (table1[i] / count[i]) / test_div;
    }
    fputs("const BYTE price_table[2][kPriceTableSize] = {\r\n", stdout);
    for (int i = 0; i < kPriceTableSize;) {
        for (int j = 0; j < 8; ++j, ++i)
            printf("%4d,", table0[i]);
        fputs("\r\n", stdout);
    }
    fputs("}, {\r\n", stdout);
    for (int i = 0; i < kPriceTableSize;) {
        for (int j = 0; j < 8; ++j, ++i)
            printf("%4d,", table1[i]);
        fputs("\r\n", stdout);
    }
    fputs("} };\r\n", stdout);
}

#endif

void RC_setOutputBuffer(RC_encoder* const rc, BYTE *const out_buffer)
{
    rc->out_buffer = out_buffer;
    rc->out_index = 0;
}

void RC_reset(RC_encoder* const rc)
{
    rc->low = 0;
    rc->range = (U32)-1;
    rc->cache_size = 0;
    rc->cache = 0;
}

#ifdef __64BIT__

void FORCE_NOINLINE RC_shiftLow(RC_encoder* const rc)
{
    U64 low = rc->low;
    rc->low = (U32)(low << 8);
    /* VC15 compiles 'if (low < 0xFF000000 || low > 0xFFFFFFFF)' to this single-branch conditional */
    if (low + 0xFFFFFFFF01000000 > 0xFFFFFF) {
        BYTE high = (BYTE)(low >> 32);
        rc->out_buffer[rc->out_index++] = rc->cache + high;
        rc->cache = (BYTE)(low >> 24);
        if (rc->cache_size != 0) {
            high += 0xFF;
            do {
                rc->out_buffer[rc->out_index++] = high;
            } while (--rc->cache_size != 0);
        }
    }
    else {
        rc->cache_size++;
    }
}

#else

void FORCE_NOINLINE RC_shiftLow(RC_encoder* const rc)
{
    U32 low = (U32)rc->low;
    unsigned high = (unsigned)(rc->low >> 32);
    rc->low = low << 8;
    if (low < (U32)0xFF000000 || high != 0) {
        rc->out_buffer[rc->out_index++] = rc->cache + (BYTE)high;
        rc->cache = (BYTE)(low >> 24);
        if (rc->cache_size != 0) {
            high += 0xFF;
            do {
                rc->out_buffer[rc->out_index++] = (BYTE)high;
            } while (--rc->cache_size != 0);
        }
    }
    else {
        rc->cache_size++;
    }
}

#endif

void RC_encodeBitTree(RC_encoder* const rc, LZMA2_prob *const probs, unsigned bit_count, unsigned symbol)
{
    assert(bit_count > 1);
    --bit_count;
    unsigned bit = symbol >> bit_count;
    RC_encodeBit(rc, &probs[1], bit);
    size_t tree_index = 1;
    do {
        --bit_count;
        tree_index = (tree_index << 1) | bit;
        bit = (symbol >> bit_count) & 1;
        RC_encodeBit(rc, &probs[tree_index], bit);
    } while (bit_count != 0);
}

void RC_encodeBitTreeReverse(RC_encoder* const rc, LZMA2_prob *const probs, unsigned bit_count, unsigned symbol)
{
    assert(bit_count != 0);
    unsigned bit = symbol & 1;
    RC_encodeBit(rc, &probs[1], bit);
    unsigned tree_index = 1;
    while (--bit_count != 0) {
        tree_index = (tree_index << 1) + bit;
        symbol >>= 1;
        bit = symbol & 1;
		RC_encodeBit(rc, &probs[tree_index], bit);
	}
}

void FORCE_NOINLINE RC_encodeDirect(RC_encoder* const rc, unsigned value, unsigned bit_count)
{
	assert(bit_count > 0);
	do {
        rc->range >>= 1;
		--bit_count;
        rc->low += rc->range & -((int)(value >> bit_count) & 1);
		if (rc->range < kTopValue) {
            rc->range <<= 8;
			RC_shiftLow(rc);
		}
	} while (bit_count != 0);
}


