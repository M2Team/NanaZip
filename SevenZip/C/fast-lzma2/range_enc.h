/*
* Bitwise range encoder by Igor Pavlov
* Modified by Conor McCarthy
*
* Public domain
*/

#ifndef RANGE_ENCODER_H
#define RANGE_ENCODER_H

#include "mem.h"
#include "compiler.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef LZMA_ENC_PROB32
typedef U32 LZMA2_prob;
#else
typedef U16 LZMA2_prob;
#endif

#define kNumTopBits 24U
#define kTopValue (1UL << kNumTopBits)
#define kNumBitModelTotalBits 11U
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5U
#define kProbInitValue (kBitModelTotal >> 1U)
#define kNumMoveReducingBits 4U
#define kNumBitPriceShiftBits 5U
#define kPriceTableSize (kBitModelTotal >> kNumMoveReducingBits)

extern BYTE price_table[2][kPriceTableSize];
#if 0
void RC_printPriceTable();
#endif

typedef struct
{
	BYTE *out_buffer;
	size_t out_index;
	U64 cache_size;
	U64 low;
	U32 range;
	BYTE cache;
} RC_encoder;

void RC_reset(RC_encoder* const rc);

void RC_setOutputBuffer(RC_encoder* const rc, BYTE *const out_buffer);

void FORCE_NOINLINE RC_shiftLow(RC_encoder* const rc);

void RC_encodeBitTree(RC_encoder* const rc, LZMA2_prob *const probs, unsigned bit_count, unsigned symbol);

void RC_encodeBitTreeReverse(RC_encoder* const rc, LZMA2_prob *const probs, unsigned bit_count, unsigned symbol);

void FORCE_NOINLINE RC_encodeDirect(RC_encoder* const rc, unsigned value, unsigned bit_count);

HINT_INLINE
void RC_encodeBit0(RC_encoder* const rc, LZMA2_prob *const rprob)
{
	unsigned prob = *rprob;
    rc->range = (rc->range >> kNumBitModelTotalBits) * prob;
	prob += (kBitModelTotal - prob) >> kNumMoveBits;
	*rprob = (LZMA2_prob)prob;
	if (rc->range < kTopValue) {
        rc->range <<= 8;
		RC_shiftLow(rc);
	}
}

HINT_INLINE
void RC_encodeBit1(RC_encoder* const rc, LZMA2_prob *const rprob)
{
	unsigned prob = *rprob;
	U32 new_bound = (rc->range >> kNumBitModelTotalBits) * prob;
    rc->low += new_bound;
    rc->range -= new_bound;
	prob -= prob >> kNumMoveBits;
	*rprob = (LZMA2_prob)prob;
	if (rc->range < kTopValue) {
        rc->range <<= 8;
		RC_shiftLow(rc);
	}
}

HINT_INLINE
void RC_encodeBit(RC_encoder* const rc, LZMA2_prob *const rprob, unsigned const bit)
{
	unsigned prob = *rprob;
	if (bit != 0) {
		U32 const new_bound = (rc->range >> kNumBitModelTotalBits) * prob;
        rc->low += new_bound;
        rc->range -= new_bound;
		prob -= prob >> kNumMoveBits;
	}
	else {
        rc->range = (rc->range >> kNumBitModelTotalBits) * prob;
		prob += (kBitModelTotal - prob) >> kNumMoveBits;
	}
	*rprob = (LZMA2_prob)prob;
	if (rc->range < kTopValue) {
        rc->range <<= 8;
		RC_shiftLow(rc);
	}
}

#define GET_PRICE(prob, symbol) \
  price_table[symbol][(prob) >> kNumMoveReducingBits]

#define GET_PRICE_0(prob) price_table[0][(prob) >> kNumMoveReducingBits]

#define GET_PRICE_1(prob) price_table[1][(prob) >> kNumMoveReducingBits]

#define kMinLitPrice 8U

HINT_INLINE
unsigned RC_getTreePrice(const LZMA2_prob* const prob_table, unsigned bit_count, size_t symbol)
{
	unsigned price = 0;
    symbol |= ((size_t)1 << bit_count);
    do {
		size_t const next_symbol = symbol >> 1;
		unsigned prob = prob_table[next_symbol];
        size_t bit = symbol & 1;
		price += GET_PRICE(prob, bit);
		symbol = next_symbol;
    } while (symbol != 1);
	return price;
}

HINT_INLINE
unsigned RC_getReverseTreePrice(const LZMA2_prob* const prob_table, unsigned bit_count, size_t symbol)
{
    unsigned prob = prob_table[1];
    size_t bit = symbol & 1;
    unsigned price = GET_PRICE(prob, bit);
    size_t m = 1;
    while (--bit_count != 0) {
        m = (m << 1) | bit;
        symbol >>= 1;
        prob = prob_table[m];
        bit = symbol & 1;
        price += GET_PRICE(prob, bit);
    }
    return price;
}

HINT_INLINE
void RC_flush(RC_encoder* const rc)
{
    for (int i = 0; i < 5; ++i)
        RC_shiftLow(rc);
}

#if defined (__cplusplus)
}
#endif

#endif /* RANGE_ENCODER_H */