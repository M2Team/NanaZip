/* lzma2_enc.c -- LZMA2 Encoder
Based on LzmaEnc.c and Lzma2Enc.c : Igor Pavlov
Modified for FL2 by Conor McCarthy
Public domain
*/

#include <stdlib.h>
#include <math.h>

#include "fl2_errors.h"
#include "fl2_internal.h"
#include "lzma2_enc.h"
#include "fl2_compress_internal.h"
#include "mem.h"
#include "count.h"
#include "radix_mf.h"
#include "range_enc.h"

#ifdef FL2_XZ_BUILD
#  include "tuklib_integer.h"
#  define MEM_readLE32(a) unaligned_read32le(a)

#  ifdef TUKLIB_FAST_UNALIGNED_ACCESS
#    define MEM_read16(a) (*(const U16*)(a))
#  endif

#endif

#define kNumReps 4U
#define kNumStates 12U

#define kNumLiterals 0x100U
#define kNumLitTables 3U

#define kNumLenToPosStates 4U
#define kNumPosSlotBits 6U
#define kDicLogSizeMin 18U
#define kDicLogSizeMax 31U
#define kDistTableSizeMax (kDicLogSizeMax * 2U)

#define kNumAlignBits 4U
#define kAlignTableSize (1U << kNumAlignBits)
#define kAlignMask (kAlignTableSize - 1U)
#define kMatchRepriceFrequency 64U
#define kRepLenRepriceFrequency 64U

#define kStartPosModelIndex 4U
#define kEndPosModelIndex 14U
#define kNumPosModels (kEndPosModelIndex - kStartPosModelIndex)

#define kNumFullDistancesBits (kEndPosModelIndex >> 1U)
#define kNumFullDistances (1U << kNumFullDistancesBits)

#define kNumPositionBitsMax 4U
#define kNumPositionStatesMax (1U << kNumPositionBitsMax)
#define kNumLiteralContextBitsMax 4U
#define kNumLiteralPosBitsMax 4U
#define kLcLpMax 4U


#define kLenNumLowBits 3U
#define kLenNumLowSymbols (1U << kLenNumLowBits)
#define kLenNumHighBits 8U
#define kLenNumHighSymbols (1U << kLenNumHighBits)

#define kLenNumSymbolsTotal (kLenNumLowSymbols * 2 + kLenNumHighSymbols)

#define kMatchLenMin 2U
#define kMatchLenMax (kMatchLenMin + kLenNumSymbolsTotal - 1U)

#define kMatchesMax 65U /* Doesn't need to be larger than FL2_HYBRIDCYCLES_MAX + 1 */

#define kOptimizerEndSize 32U
#define kOptimizerBufferSize (kMatchLenMax * 2U + kOptimizerEndSize)
#define kOptimizerSkipSize 16U
#define kInfinityPrice (1UL << 30U)
#define kNullDist (U32)-1

#define kMaxMatchEncodeSize 20

#define kMaxChunkCompressedSize (1UL << 16U)
/* Need to leave sufficient space for expanded output from a full opt buffer with bad starting probs */
#define kChunkSize (kMaxChunkCompressedSize - 2048U)
#define kSqrtChunkSize 252U

/* Hard to define where the match table read pos definitely catches up with the output size, but
 * 64 bytes of input expanding beyond 256 bytes right after an encoder reset is most likely impossible.
 * The encoder will error out if this happens. */
#define kTempMinOutput 256U
#define kTempBufferSize (kTempMinOutput + kOptimizerBufferSize + kOptimizerBufferSize / 4U)

#define kMaxChunkUncompressedSize (1UL << 21U)

#define kChunkHeaderSize 5U
#define kChunkResetShift 5U
#define kChunkUncompressedDictReset 1U
#define kChunkUncompressed 2U
#define kChunkCompressedFlag 0x80U
#define kChunkNothingReset 0U
#define kChunkStateReset (1U << kChunkResetShift)
#define kChunkStatePropertiesReset (2U << kChunkResetShift)
#define kChunkAllReset (3U << kChunkResetShift)

#define kMaxHashDictBits 14U
#define kHash3Bits 14U
#define kNullLink -1

#define kMinTestChunkSize 0x4000U
#define kRandomFilterMarginBits 8U

#define kState_LitAfterMatch 4
#define kState_LitAfterRep   5
#define kState_MatchAfterLit 7
#define kState_RepAfterLit   8

static const BYTE kLiteralNextStates[kNumStates] = { 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5 };
#define LIT_NEXT_STATE(s) kLiteralNextStates[s]
static const BYTE kMatchNextStates[kNumStates] = { 7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10 };
#define MATCH_NEXT_STATE(s) kMatchNextStates[s]
static const BYTE kRepNextStates[kNumStates] = { 8, 8, 8, 8, 8, 8, 8, 11, 11, 11, 11, 11 };
#define REP_NEXT_STATE(s) kRepNextStates[s]
static const BYTE kShortRepNextStates[kNumStates] = { 9, 9, 9, 9, 9, 9, 9, 11, 11, 11, 11, 11 };
#define SHORT_REP_NEXT_STATE(s) kShortRepNextStates[s]

#include "fastpos_table.h"
#include "radix_get.h"

/* Probabilities and prices for encoding match lengths.
 * Two objects of this type are needed, one for normal matches
 * and another for rep matches.
 */
typedef struct 
{
    size_t table_size;
    unsigned prices[kNumPositionStatesMax][kLenNumSymbolsTotal];
    LZMA2_prob choice; /* low[0] is choice_2. Must be consecutive for speed */
    LZMA2_prob low[kNumPositionStatesMax << (kLenNumLowBits + 1)];
    LZMA2_prob high[kLenNumHighSymbols];
} LZMA2_lenStates;

/* All probabilities for the encoder. This is a separate from the encoder object
 * so the state can be saved and restored in case a chunk is not compressible.
 */
typedef struct
{
    /* Fields are ordered for speed */
    LZMA2_lenStates rep_len_states;
    LZMA2_prob is_rep0_long[kNumStates][kNumPositionStatesMax];

    size_t state;
    U32 reps[kNumReps];

    LZMA2_prob is_match[kNumStates][kNumPositionStatesMax];
    LZMA2_prob is_rep[kNumStates];
    LZMA2_prob is_rep_G0[kNumStates];
    LZMA2_prob is_rep_G1[kNumStates];
    LZMA2_prob is_rep_G2[kNumStates];

    LZMA2_lenStates len_states;

    LZMA2_prob dist_slot_encoders[kNumLenToPosStates][1 << kNumPosSlotBits];
    LZMA2_prob dist_align_encoders[1 << kNumAlignBits];
    LZMA2_prob dist_encoders[kNumFullDistances - kEndPosModelIndex];

    LZMA2_prob literal_probs[(kNumLiterals * kNumLitTables) << kLcLpMax];
} LZMA2_encStates;

/* 
 * Linked list item for optimal parsing
 */
typedef struct
{
    size_t state;
    U32 price;
    unsigned extra; /*  0   : normal
                     *  1   : LIT : MATCH
                     *  > 1 : MATCH (extra-1) : LIT : REP0 (len) */
    unsigned len;
    U32 dist;
    U32 reps[kNumReps];
} LZMA2_node;

#define MARK_LITERAL(node) (node).dist = kNullDist; (node).extra = 0;
#define MARK_SHORT_REP(node) (node).dist = 0; (node).extra = 0;

/*
 * Table and chain for 3-byte hash. Extra elements in hash_chain_3 are malloced.
 */
typedef struct {
    S32 table_3[1 << kHash3Bits];
    S32 hash_chain_3[1];
} LZMA2_hc3;

/*
 * LZMA2 encoder.
 */
struct LZMA2_ECtx_s
{
    unsigned lc;
    unsigned lp;
    unsigned pb;
    unsigned fast_length;
    size_t len_end_max;
    size_t lit_pos_mask;
    size_t pos_mask;
    unsigned match_cycles;
    FL2_strategy strategy;

    RC_encoder rc;
    /* Finish writing the chunk at this size */
    size_t chunk_size;
    /* Don't encode a symbol beyond this limit (used by fast mode) */
    size_t chunk_limit;

    LZMA2_encStates states;

    unsigned match_price_count;
    unsigned rep_len_price_count;
    size_t dist_price_table_size;
    unsigned align_prices[kAlignTableSize];
    unsigned dist_slot_prices[kNumLenToPosStates][kDistTableSizeMax];
    unsigned distance_prices[kNumLenToPosStates][kNumFullDistances];

    RMF_match base_match; /* Allows access to matches[-1] in LZMA_optimalParse */
    RMF_match matches[kMatchesMax];
    size_t match_count;

    LZMA2_node opt_buf[kOptimizerBufferSize];

    LZMA2_hc3* hash_buf;
    ptrdiff_t chain_mask_2;
    ptrdiff_t chain_mask_3;
    ptrdiff_t hash_dict_3;
    ptrdiff_t hash_prev_index;
    ptrdiff_t hash_alloc_3;

    /* Temp output buffer before space frees up in the match table */
    BYTE out_buf[kTempBufferSize];
};

LZMA2_ECtx* LZMA2_createECtx(void)
{
    LZMA2_ECtx *const enc = malloc(sizeof(LZMA2_ECtx));
    DEBUGLOG(3, "LZMA2_createECtx");
    if (enc == NULL)
        return NULL;

    enc->lc = 3;
    enc->lp = 0;
    enc->pb = 2;
    enc->fast_length = 48;
    enc->len_end_max = kOptimizerBufferSize - 1;
    enc->lit_pos_mask = (1 << enc->lp) - 1;
    enc->pos_mask = (1 << enc->pb) - 1;
    enc->match_cycles = 1;
    enc->strategy = FL2_ultra;
    enc->match_price_count = 0;
    enc->rep_len_price_count = 0;
    enc->dist_price_table_size = kDistTableSizeMax;
    enc->hash_buf = NULL;
    enc->hash_dict_3 = 0;
    enc->chain_mask_3 = 0;
    enc->hash_alloc_3 = 0;
    return enc;
}

void LZMA2_freeECtx(LZMA2_ECtx *const enc)
{
    if (enc == NULL)
        return;
    free(enc->hash_buf);
    free(enc);
}

#define LITERAL_PROBS(enc, pos, prev_symbol) (enc->states.literal_probs + ((((pos) & enc->lit_pos_mask) << enc->lc) + ((prev_symbol) >> (8 - enc->lc))) * kNumLiterals * kNumLitTables)

#define LEN_TO_DIST_STATE(len) (((len) < kNumLenToPosStates + 1) ? (len) - 2 : kNumLenToPosStates - 1)

#define IS_LIT_STATE(state) ((state) < 7)

HINT_INLINE
unsigned LZMA_getRepLen1Price(LZMA2_ECtx* const enc, size_t const state, size_t const pos_state)
{
    unsigned const rep_G0_prob = enc->states.is_rep_G0[state];
    unsigned const rep0_long_prob = enc->states.is_rep0_long[state][pos_state];
    return GET_PRICE_0(rep_G0_prob) + GET_PRICE_0(rep0_long_prob);
}

static unsigned LZMA_getRepPrice(LZMA2_ECtx* const enc, size_t const rep_index, size_t const state, size_t const pos_state)
{
    unsigned price;
    unsigned const rep_G0_prob = enc->states.is_rep_G0[state];
    if (rep_index == 0) {
        unsigned const rep0_long_prob = enc->states.is_rep0_long[state][pos_state];
        price = GET_PRICE_0(rep_G0_prob);
        price += GET_PRICE_1(rep0_long_prob);
    }
    else {
        unsigned const rep_G1_prob = enc->states.is_rep_G1[state];
        price = GET_PRICE_1(rep_G0_prob);
        if (rep_index == 1) {
            price += GET_PRICE_0(rep_G1_prob);
        }
        else {
            unsigned const rep_G2_prob = enc->states.is_rep_G2[state];
            price += GET_PRICE_1(rep_G1_prob);
            price += GET_PRICE(rep_G2_prob, rep_index - 2);
        }
    }
    return price;
}

static unsigned LZMA_getRepMatch0Price(LZMA2_ECtx *const enc, size_t const len, size_t const state, size_t const pos_state)
{
    unsigned const rep_G0_prob = enc->states.is_rep_G0[state];
    unsigned const rep0_long_prob = enc->states.is_rep0_long[state][pos_state];
    return enc->states.rep_len_states.prices[pos_state][len - kMatchLenMin]
        + GET_PRICE_0(rep_G0_prob)
        + GET_PRICE_1(rep0_long_prob);
}

static unsigned LZMA_getLiteralPriceMatched(const LZMA2_prob *const prob_table, U32 symbol, unsigned match_byte)
{
    unsigned price = 0;
    unsigned offs = 0x100;
    symbol |= 0x100;
    do {
        match_byte <<= 1;
        price += GET_PRICE(prob_table[offs + (match_byte & offs) + (symbol >> 8)], (symbol >> 7) & 1);
        symbol <<= 1;
        offs &= ~(match_byte ^ symbol);
    } while (symbol < 0x10000);
    return price;
}

HINT_INLINE
void LZMA_encodeLiteral(LZMA2_ECtx *const enc, size_t const pos, U32 symbol, unsigned const prev_symbol)
{
    RC_encodeBit0(&enc->rc, &enc->states.is_match[enc->states.state][pos & enc->pos_mask]);
    enc->states.state = LIT_NEXT_STATE(enc->states.state);

    LZMA2_prob* const prob_table = LITERAL_PROBS(enc, pos, prev_symbol);
    symbol |= 0x100;
    do {
        RC_encodeBit(&enc->rc, prob_table + (symbol >> 8), symbol & (1 << 7));
        symbol <<= 1;
    } while (symbol < 0x10000);
}

HINT_INLINE
void LZMA_encodeLiteralMatched(LZMA2_ECtx *const enc, const BYTE* const data_block, size_t const pos, U32 symbol)
{
    RC_encodeBit0(&enc->rc, &enc->states.is_match[enc->states.state][pos & enc->pos_mask]);
    enc->states.state = LIT_NEXT_STATE(enc->states.state);

    unsigned match_symbol = data_block[pos - enc->states.reps[0] - 1];
    LZMA2_prob* const prob_table = LITERAL_PROBS(enc, pos, data_block[pos - 1]);
    unsigned offset = 0x100;
    symbol |= 0x100;
    do {
        match_symbol <<= 1;
        size_t prob_index = offset + (match_symbol & offset) + (symbol >> 8);
        RC_encodeBit(&enc->rc, prob_table + prob_index, symbol & (1 << 7));
        symbol <<= 1;
        offset &= ~(match_symbol ^ symbol);
    } while (symbol < 0x10000);
}

HINT_INLINE
void LZMA_encodeLiteralBuf(LZMA2_ECtx *const enc, const BYTE* const data_block, size_t const pos)
{
    U32 const symbol = data_block[pos];
    if (IS_LIT_STATE(enc->states.state)) {
        unsigned const prev_symbol = data_block[pos - 1];
        LZMA_encodeLiteral(enc, pos, symbol, prev_symbol);
    }
    else {
        LZMA_encodeLiteralMatched(enc, data_block, pos, symbol);
    }
}

static void LZMA_lengthStates_SetPrices(const LZMA2_prob *probs, U32 start_price, unsigned *prices)
{
    for (size_t i = 0; i < 8; i += 2) {
        U32 prob = probs[4 + (i >> 1)];
        U32 price = start_price + GET_PRICE(probs[1], (i >> 2))
            + GET_PRICE(probs[2 + (i >> 2)], (i >> 1) & 1);
        prices[i] = price + GET_PRICE_0(prob);
        prices[i + 1] = price + GET_PRICE_1(prob);
    }
}

FORCE_NOINLINE
static void LZMA_lengthStates_updatePrices(LZMA2_ECtx *const enc, LZMA2_lenStates* const ls)
{
    U32 b;

    {
        unsigned const prob = ls->choice;
        U32 a, c;
        b = GET_PRICE_1(prob);
        a = GET_PRICE_0(prob);
        c = b + GET_PRICE_0(ls->low[0]);
        for (size_t pos_state = 0; pos_state <= enc->pos_mask; pos_state++) {
            unsigned *const prices = ls->prices[pos_state];
            const LZMA2_prob *const probs = ls->low + (pos_state << (1 + kLenNumLowBits));
            LZMA_lengthStates_SetPrices(probs, a, prices);
            LZMA_lengthStates_SetPrices(probs + kLenNumLowSymbols, c, prices + kLenNumLowSymbols);
        }
    }

    size_t i = ls->table_size;

    if (i > kLenNumLowSymbols * 2) {
        const LZMA2_prob *const probs = ls->high;
        unsigned *const prices = ls->prices[0] + kLenNumLowSymbols * 2;
        i = (i - (kLenNumLowSymbols * 2 - 1)) >> 1;
        b += GET_PRICE_1(ls->low[0]);
        do {
            --i;
            size_t sym = i + (1 << (kLenNumHighBits - 1));
            U32 price = b;
            do {
                size_t bit = sym & 1;
                sym >>= 1;
                price += GET_PRICE(probs[sym], bit);
            } while (sym >= 2);

            unsigned const prob = probs[i + (1 << (kLenNumHighBits - 1))];
            prices[i * 2] = price + GET_PRICE_0(prob);
            prices[i * 2 + 1] = price + GET_PRICE_1(prob);
        } while (i);

        size_t const size = (ls->table_size - kLenNumLowSymbols * 2) * sizeof(ls->prices[0][0]);
        for (size_t pos_state = 1; pos_state <= enc->pos_mask; pos_state++)
            memcpy(ls->prices[pos_state] + kLenNumLowSymbols * 2, ls->prices[0] + kLenNumLowSymbols * 2, size);
    }
}

/* Rare enough that not inlining is faster overall */
FORCE_NOINLINE
static void LZMA_encodeLength_MidHigh(LZMA2_ECtx *const enc, LZMA2_lenStates* const len_prob_table, unsigned const len, size_t const pos_state)
{
    RC_encodeBit1(&enc->rc, &len_prob_table->choice);
    if (len < kLenNumLowSymbols * 2) {
        RC_encodeBit0(&enc->rc, &len_prob_table->low[0]);
        RC_encodeBitTree(&enc->rc, len_prob_table->low + kLenNumLowSymbols + (pos_state << (1 + kLenNumLowBits)), kLenNumLowBits, len - kLenNumLowSymbols);
    }
    else {
        RC_encodeBit1(&enc->rc, &len_prob_table->low[0]);
        RC_encodeBitTree(&enc->rc, len_prob_table->high, kLenNumHighBits, len - kLenNumLowSymbols * 2);
    }
}

HINT_INLINE
void LZMA_encodeLength(LZMA2_ECtx *const enc, LZMA2_lenStates* const len_prob_table, unsigned len, size_t const pos_state)
{
    len -= kMatchLenMin;
    if (len < kLenNumLowSymbols) {
        RC_encodeBit0(&enc->rc, &len_prob_table->choice);
        RC_encodeBitTree(&enc->rc, len_prob_table->low + (pos_state << (1 + kLenNumLowBits)), kLenNumLowBits, len);
    }
    else {
        LZMA_encodeLength_MidHigh(enc, len_prob_table, len, pos_state);
    }
}

FORCE_NOINLINE
static void LZMA_encodeRepMatchShort(LZMA2_ECtx *const enc, size_t const pos_state)
{
    DEBUGLOG(7, "LZMA_encodeRepMatchShort");
    RC_encodeBit1(&enc->rc, &enc->states.is_match[enc->states.state][pos_state]);
    RC_encodeBit1(&enc->rc, &enc->states.is_rep[enc->states.state]);
    RC_encodeBit0(&enc->rc, &enc->states.is_rep_G0[enc->states.state]);
    RC_encodeBit0(&enc->rc, &enc->states.is_rep0_long[enc->states.state][pos_state]);
    enc->states.state = SHORT_REP_NEXT_STATE(enc->states.state);
}

FORCE_NOINLINE
static void LZMA_encodeRepMatchLong(LZMA2_ECtx *const enc, unsigned const len, unsigned const rep, size_t const pos_state)
{
    DEBUGLOG(7, "LZMA_encodeRepMatchLong : length %u, rep %u", len, rep);
    RC_encodeBit1(&enc->rc, &enc->states.is_match[enc->states.state][pos_state]);
    RC_encodeBit1(&enc->rc, &enc->states.is_rep[enc->states.state]);
    if (rep == 0) {
        RC_encodeBit0(&enc->rc, &enc->states.is_rep_G0[enc->states.state]);
        RC_encodeBit1(&enc->rc, &enc->states.is_rep0_long[enc->states.state][pos_state]);
    }
    else {
        U32 const distance = enc->states.reps[rep];
        RC_encodeBit1(&enc->rc, &enc->states.is_rep_G0[enc->states.state]);
        if (rep == 1) {
            RC_encodeBit0(&enc->rc, &enc->states.is_rep_G1[enc->states.state]);
        }
        else {
            RC_encodeBit1(&enc->rc, &enc->states.is_rep_G1[enc->states.state]);
            RC_encodeBit(&enc->rc, &enc->states.is_rep_G2[enc->states.state], rep - 2);
            if (rep == 3)
                enc->states.reps[3] = enc->states.reps[2];
            enc->states.reps[2] = enc->states.reps[1];
        }
        enc->states.reps[1] = enc->states.reps[0];
        enc->states.reps[0] = distance;
    }
    LZMA_encodeLength(enc, &enc->states.rep_len_states, len, pos_state);
    enc->states.state = REP_NEXT_STATE(enc->states.state);
    ++enc->rep_len_price_count;
}


/* 
 * Distance slot functions based on fastpos.h from XZ
 */

HINT_INLINE
unsigned LZMA_fastDistShift(unsigned const n)
{
    return n * (kFastDistBits - 1);
}

HINT_INLINE
unsigned LZMA_fastDistResult(U32 const dist, unsigned const n)
{
    return distance_table[dist >> LZMA_fastDistShift(n)]
        + 2 * LZMA_fastDistShift(n);
}

static size_t LZMA_getDistSlot(U32 const distance)
{
    U32 limit = 1UL << kFastDistBits;
    /* If it is small enough, we can pick the result directly from */
    /* the precalculated table. */
    if (distance < limit) {
        return distance_table[distance];
    }
    limit <<= LZMA_fastDistShift(1);
    if (distance < limit) {
        return LZMA_fastDistResult(distance, 1);
    }
    return LZMA_fastDistResult(distance, 2);
}

/* * */


HINT_INLINE
void LZMA_encodeNormalMatch(LZMA2_ECtx *const enc, unsigned const len, U32 const dist, size_t const pos_state)
{
    DEBUGLOG(7, "LZMA_encodeNormalMatch : length %u, dist %u", len, dist);
    RC_encodeBit1(&enc->rc, &enc->states.is_match[enc->states.state][pos_state]);
    RC_encodeBit0(&enc->rc, &enc->states.is_rep[enc->states.state]);
    enc->states.state = MATCH_NEXT_STATE(enc->states.state);

    LZMA_encodeLength(enc, &enc->states.len_states, len, pos_state);

    size_t const dist_slot = LZMA_getDistSlot(dist);
    RC_encodeBitTree(&enc->rc, enc->states.dist_slot_encoders[LEN_TO_DIST_STATE(len)], kNumPosSlotBits, (unsigned)dist_slot);
    if (dist_slot >= kStartPosModelIndex) {
        unsigned const footer_bits = ((unsigned)(dist_slot >> 1) - 1);
        size_t const base = ((2 | (dist_slot & 1)) << footer_bits);
        unsigned const dist_reduced = (unsigned)(dist - base);
        if (dist_slot < kEndPosModelIndex) {
            RC_encodeBitTreeReverse(&enc->rc, enc->states.dist_encoders + base - dist_slot - 1, footer_bits, dist_reduced);
        }
        else {
            RC_encodeDirect(&enc->rc, dist_reduced >> kNumAlignBits, footer_bits - kNumAlignBits);
            RC_encodeBitTreeReverse(&enc->rc, enc->states.dist_align_encoders, kNumAlignBits, dist_reduced & kAlignMask);
        }
    }
    enc->states.reps[3] = enc->states.reps[2];
    enc->states.reps[2] = enc->states.reps[1];
    enc->states.reps[1] = enc->states.reps[0];
    enc->states.reps[0] = dist;

    ++enc->match_price_count;
}

FORCE_INLINE_TEMPLATE
size_t LZMA_encodeChunkFast(LZMA2_ECtx *const enc,
    FL2_dataBlock const block,
    FL2_matchTable* const tbl,
    int const struct_tbl,
    size_t pos,
    size_t const uncompressed_end)
{
    size_t const pos_mask = enc->pos_mask;
    size_t prev = pos;
    unsigned const search_depth = tbl->params.depth;

    while (pos < uncompressed_end && enc->rc.out_index < enc->chunk_size) {
        size_t max_len;
        const BYTE* data;
        /* Table of distance restrictions for short matches */
        static const U32 max_dist_table[] = { 0, 0, 0, 1 << 6, 1 << 14 };
        /* Get a match from the table, extended to its full length */
        RMF_match best_match = RMF_getMatch(block, tbl, search_depth, struct_tbl, pos);
        if (best_match.length < kMatchLenMin) {
            ++pos;
            continue;
        }
        /* Use if near enough */
        if (best_match.length >= 5 || best_match.dist < max_dist_table[best_match.length])
            best_match.dist += kNumReps;
        else
            best_match.length = 0;

        max_len = MIN(kMatchLenMax, block.end - pos);
        data = block.data + pos;

        RMF_match best_rep = { 0, 0 };
        RMF_match rep_match;
        /* Search all of the rep distances */
        for (rep_match.dist = 0; rep_match.dist < kNumReps; ++rep_match.dist) {
            const BYTE *data_2 = data - enc->states.reps[rep_match.dist] - 1;
            if (MEM_read16(data) != MEM_read16(data_2))
                continue;

            rep_match.length = (U32)(ZSTD_count(data + 2, data_2 + 2, data + max_len) + 2);
            if (rep_match.length >= max_len) {
                best_match = rep_match;
                goto _encode;
            }
            if (rep_match.length > best_rep.length)
                best_rep = rep_match;
        }
        /* Encode if it is kMatchLenMax or completes the block */
        if (best_match.length >= max_len)
            goto _encode;

        if (best_rep.length >= 2) {
            if (best_rep.length > best_match.length) {
                best_match = best_rep;
            }
            else {
                /* Modified ZSTD scheme for estimating cost */
                int const gain2 = (int)(best_rep.length * 3 - best_rep.dist);
                int const gain1 = (int)(best_match.length * 3 - ZSTD_highbit32(best_match.dist + 1) + 1);
                if (gain2 > gain1) {
                    DEBUGLOG(7, "Replace match (%u, %u) with rep (%u, %u)", best_match.length, best_match.dist, best_rep.length, best_rep.dist);
                    best_match = best_rep;
                }
            }
        }

        if (best_match.length < kMatchLenMin) {
            ++pos;
            continue;
        }

        for (size_t next = pos + 1; best_match.length < kMatchLenMax && next < uncompressed_end; ++next) {
            /* lazy matching scheme from ZSTD */
            RMF_match next_match = RMF_getNextMatch(block, tbl, search_depth, struct_tbl, next);
            if (next_match.length >= kMatchLenMin) {
                best_rep.length = 0;
                data = block.data + next;
                max_len = MIN(kMatchLenMax, block.end - next);
                for (rep_match.dist = 0; rep_match.dist < kNumReps; ++rep_match.dist) {
                    const BYTE *data_2 = data - enc->states.reps[rep_match.dist] - 1;
                    if (MEM_read16(data) != MEM_read16(data_2))
                        continue;

                    rep_match.length = (U32)(ZSTD_count(data + 2, data_2 + 2, data + max_len) + 2);
                    if (rep_match.length > best_rep.length)
                        best_rep = rep_match;
                }
                if (best_rep.length >= 3) {
                    int const gain2 = (int)(best_rep.length * 3 - best_rep.dist);
                    int const gain1 = (int)(best_match.length * 3 - ZSTD_highbit32((U32)best_match.dist + 1) + 1);
                    if (gain2 > gain1) {
                        DEBUGLOG(7, "Replace match (%u, %u) with rep (%u, %u)", best_match.length, best_match.dist, best_rep.length, best_rep.dist);
                        best_match = best_rep;
                        pos = next;
                    }
                }
                if (next_match.length >= 3 && next_match.dist != best_match.dist) {
                    int const gain2 = (int)(next_match.length * 4 - ZSTD_highbit32((U32)next_match.dist + 1));   /* raw approx */
                    int const gain1 = (int)(best_match.length * 4 - ZSTD_highbit32((U32)best_match.dist + 1) + 4);
                    if (gain2 > gain1) {
                        DEBUGLOG(7, "Replace match (%u, %u) with match (%u, %u)", best_match.length, best_match.dist, next_match.length, next_match.dist + kNumReps);
                        best_match = next_match;
                        best_match.dist += kNumReps;
                        pos = next;
                        continue;
                    }
                }
            }
            ++next;
            /* Recheck next < uncompressed_end. uncompressed_end could be block.end so decrementing the max chunk size won't obviate the need. */
            if (next >= uncompressed_end)
                break;

            next_match = RMF_getNextMatch(block, tbl, search_depth, struct_tbl, next);
            if (next_match.length < 4)
                break;

            data = block.data + next;
            max_len = MIN(kMatchLenMax, block.end - next);
            best_rep.length = 0;

            for (rep_match.dist = 0; rep_match.dist < kNumReps; ++rep_match.dist) {
                const BYTE *data_2 = data - enc->states.reps[rep_match.dist] - 1;
                if (MEM_read16(data) != MEM_read16(data_2))
                    continue;

                rep_match.length = (U32)(ZSTD_count(data + 2, data_2 + 2, data + max_len) + 2);
                if (rep_match.length > best_rep.length)
                    best_rep = rep_match;
            }
            if (best_rep.length >= 4) {
                int const gain2 = (int)(best_rep.length * 4 - (best_rep.dist >> 1));
                int const gain1 = (int)(best_match.length * 4 - ZSTD_highbit32((U32)best_match.dist + 1) + 1);
                if (gain2 > gain1) {
                    DEBUGLOG(7, "Replace match (%u, %u) with rep (%u, %u)", best_match.length, best_match.dist, best_rep.length, best_rep.dist);
                    best_match = best_rep;
                    pos = next;
                }
            }
            if (next_match.dist != best_match.dist) {
                int const gain2 = (int)(next_match.length * 4 - ZSTD_highbit32((U32)next_match.dist + 1));
                int const gain1 = (int)(best_match.length * 4 - ZSTD_highbit32((U32)best_match.dist + 1) + 7);
                if (gain2 > gain1) {
                    DEBUGLOG(7, "Replace match (%u, %u) with match (%u, %u)", best_match.length, best_match.dist, next_match.length, next_match.dist + kNumReps);
                    best_match = next_match;
                    best_match.dist += kNumReps;
                    pos = next;
                    continue;
                }
            }

            break;
        }
_encode:
        assert(pos + best_match.length <= block.end);

        while (prev < pos) {
            if (enc->rc.out_index >= enc->chunk_limit)
                return prev;

            if (block.data[prev] != block.data[prev - enc->states.reps[0] - 1]) {
                LZMA_encodeLiteralBuf(enc, block.data, prev);
                ++prev;
            }
            else {
                LZMA_encodeRepMatchShort(enc, prev & pos_mask);
                ++prev;
            }
        }

        if(best_match.length >= kMatchLenMin) {
            if (best_match.dist >= kNumReps) {
                LZMA_encodeNormalMatch(enc, best_match.length, best_match.dist - kNumReps, pos & pos_mask);
                pos += best_match.length;
                prev = pos;
            }
            else {
                LZMA_encodeRepMatchLong(enc, best_match.length, best_match.dist, pos & pos_mask);
                pos += best_match.length;
                prev = pos;
            }
        }
    }
    while (prev < pos && enc->rc.out_index < enc->chunk_limit) {
        if (block.data[prev] != block.data[prev - enc->states.reps[0] - 1])
            LZMA_encodeLiteralBuf(enc, block.data, prev);
        else
            LZMA_encodeRepMatchShort(enc, prev & pos_mask);
        ++prev;
    }
    return prev;
}

/*
 * Reverse the direction of the linked list generated by the optimal parser
 */
FORCE_NOINLINE
static void LZMA_reverseOptimalChain(LZMA2_node* const opt_buf, size_t cur)
{
    unsigned len = (unsigned)opt_buf[cur].len;
    U32 dist = opt_buf[cur].dist;

    for(;;) {
        unsigned const extra = (unsigned)opt_buf[cur].extra;
        cur -= len;

        if (extra) {
            opt_buf[cur].len = (U32)len;
            len = extra;
            if (extra == 1) {
                opt_buf[cur].dist = dist;
                dist = kNullDist;
                --cur;
            }
            else {
                opt_buf[cur].dist = 0;
                --cur;
                --len;
                opt_buf[cur].dist = kNullDist;
                opt_buf[cur].len = 1;
                cur -= len;
            }
        }

        unsigned const next_len = opt_buf[cur].len;
        U32 const next_dist = opt_buf[cur].dist;

        opt_buf[cur].dist = dist;
        opt_buf[cur].len = (U32)len;

        if (cur == 0)
            break;

        len = next_len;
        dist = next_dist;
    }
}

static unsigned LZMA_getLiteralPrice(LZMA2_ECtx *const enc, size_t const pos, size_t const state, unsigned const prev_symbol, U32 symbol, unsigned const match_byte)
{
    const LZMA2_prob* const prob_table = LITERAL_PROBS(enc, pos, prev_symbol);
    if (IS_LIT_STATE(state)) {
        unsigned price = 0;
        symbol |= 0x100;
        do {
            price += GET_PRICE(prob_table[symbol >> 8], (symbol >> 7) & 1);
            symbol <<= 1;
        } while (symbol < 0x10000);
        return price;
    }
    return LZMA_getLiteralPriceMatched(prob_table, symbol, match_byte);
}

/* 
 * Reset the hash object for encoding a new slice of a block
 */
static void LZMA_hashReset(LZMA2_ECtx *const enc, unsigned const dictionary_bits_3)
{
    enc->hash_dict_3 = (ptrdiff_t)1 << dictionary_bits_3;
    enc->chain_mask_3 = enc->hash_dict_3 - 1;
    memset(enc->hash_buf->table_3, 0xFF, sizeof(enc->hash_buf->table_3));
}

/*
 * Create hash table and chain with dict size dictionary_bits_3. Frees any existing object.
 */
static int LZMA_hashCreate(LZMA2_ECtx *const enc, unsigned const dictionary_bits_3)
{
    DEBUGLOG(3, "Create hash chain : dict bits %u", dictionary_bits_3);

    if (enc->hash_buf)
        free(enc->hash_buf);

    enc->hash_alloc_3 = (ptrdiff_t)1 << dictionary_bits_3;
    enc->hash_buf = malloc(sizeof(LZMA2_hc3) + (enc->hash_alloc_3 - 1) * sizeof(S32));

    if (enc->hash_buf == NULL)
        return 1;

    LZMA_hashReset(enc, dictionary_bits_3);

    return 0;
}

/* Create a hash chain for hybrid mode if options require one.
 * Used for allocating before compression begins. Any existing table will be reused if
 * it is at least as large as required.
 */
int LZMA2_hashAlloc(LZMA2_ECtx *const enc, const FL2_lzma2Parameters* const options)
{
    if (enc->strategy == FL2_ultra && enc->hash_alloc_3 < ((ptrdiff_t)1 << options->second_dict_bits))
        return LZMA_hashCreate(enc, options->second_dict_bits);

    return 0;
}

#define GET_HASH_3(data) ((((MEM_readLE32(data)) << 8) * 506832829U) >> (32 - kHash3Bits))

/* Find matches nearer than the match from the RMF. If none is at least as long as
 * the RMF match (most likely), insert that match at the end of the list.
 */
HINT_INLINE
size_t LZMA_hashGetMatches(LZMA2_ECtx *const enc, FL2_dataBlock const block,
    ptrdiff_t const pos,
    size_t const length_limit,
    RMF_match const match)
{
    ptrdiff_t const hash_dict_3 = enc->hash_dict_3;
    const BYTE* data = block.data;
    LZMA2_hc3* const tbl = enc->hash_buf;
    ptrdiff_t const chain_mask_3 = enc->chain_mask_3;

    enc->match_count = 0;
    enc->hash_prev_index = MAX(enc->hash_prev_index, pos - hash_dict_3);
    /* Update hash tables and chains for any positions that were skipped */
    while (++enc->hash_prev_index < pos) {
        size_t hash = GET_HASH_3(data + enc->hash_prev_index);
        tbl->hash_chain_3[enc->hash_prev_index & chain_mask_3] = tbl->table_3[hash];
        tbl->table_3[hash] = (S32)enc->hash_prev_index;
    }
    data += pos;

    size_t const hash = GET_HASH_3(data);
    ptrdiff_t const first_3 = tbl->table_3[hash];
    tbl->table_3[hash] = (S32)pos;

    size_t max_len = 2;

    if (first_3 >= 0) {
        int cycles = enc->match_cycles;
        ptrdiff_t const end_index = pos - (((ptrdiff_t)match.dist < hash_dict_3) ? match.dist : hash_dict_3);
        ptrdiff_t match_3 = first_3;
        if (match_3 >= end_index) {
            do {
                --cycles;
                const BYTE* data_2 = block.data + match_3;
                size_t len_test = ZSTD_count(data + 1, data_2 + 1, data + length_limit) + 1;
                if (len_test > max_len) {
                    enc->matches[enc->match_count].length = (U32)len_test;
                    enc->matches[enc->match_count].dist = (U32)(pos - match_3 - 1);
                    ++enc->match_count;
                    max_len = len_test;
                    if (len_test >= length_limit)
                        break;
                }
                if (cycles <= 0)
                    break;
                match_3 = tbl->hash_chain_3[match_3 & chain_mask_3];
            } while (match_3 >= end_index);
        }
    }
    tbl->hash_chain_3[pos & chain_mask_3] = (S32)first_3;
    if ((unsigned)max_len < match.length) {
        /* Insert the match from the RMF */
        enc->matches[enc->match_count] = match;
        ++enc->match_count;
        return match.length;
    }
    return max_len;
}

/* The speed of this function is critical. The sections have many variables
* in common, so breaking it up into shorter functions is not feasible.
* For each position cur, starting at 1, check some or all possible
* encoding choices - a literal, 1-byte rep 0 match, all rep match lengths, and
* all match lengths at available distances. It also checks the combined
* sequences literal+rep0, rep+lit+rep0 and match+lit+rep0.
* If is_hybrid != 0, this method works in hybrid mode, using the
* hash chain to find shorter matches at near distances. */
FORCE_INLINE_TEMPLATE
size_t LZMA_optimalParse(LZMA2_ECtx* const enc, FL2_dataBlock const block,
    RMF_match match,
    size_t const pos,
    size_t const cur,
    size_t len_end,
    int const is_hybrid,
    U32* const reps)
{
    LZMA2_node* const cur_opt = &enc->opt_buf[cur];
    size_t const pos_mask = enc->pos_mask;
    size_t const pos_state = (pos & pos_mask);
    const BYTE* const data = block.data + pos;
    size_t const fast_length = enc->fast_length;
    size_t prev_index = cur - cur_opt->len;
    size_t state;
    size_t bytes_avail;
    U32 match_price;
    U32 rep_match_price;

    /* Update the states according to how this location was reached */
    if (cur_opt->len == 1) {
        /* Literal or 1-byte rep */
        const BYTE *next_state = (cur_opt->dist == 0) ? kShortRepNextStates : kLiteralNextStates;
        state = next_state[enc->opt_buf[prev_index].state];
    }
    else {
        /* Match or rep match */
        size_t const dist = cur_opt->dist;

        if (cur_opt->extra) {
            prev_index -= cur_opt->extra;
            state = kState_RepAfterLit - ((dist >= kNumReps) & (cur_opt->extra == 1));
        }
        else {
            state = enc->opt_buf[prev_index].state;
            state = MATCH_NEXT_STATE(state) + (dist < kNumReps);
        }
        const LZMA2_node *const prev_opt = &enc->opt_buf[prev_index];
        if (dist < kNumReps) {
            /* Move the chosen rep to the front.
             * The table is hideous but faster than branching :D */
            reps[0] = prev_opt->reps[dist];
            size_t table = 1 | (2 << 2) | (3 << 4)
                | (0 << 8) | (2 << 10) | (3 << 12)
                | (0L << 16) | (1L << 18) | (3L << 20)
                | (0L << 24) | (1L << 26) | (2L << 28);
            table >>= (dist << 3);
            reps[1] = prev_opt->reps[table & 3];
            table >>= 2;
            reps[2] = prev_opt->reps[table & 3];
            table >>= 2;
            reps[3] = prev_opt->reps[table & 3];
        }
        else {
            reps[0] = (U32)(dist - kNumReps);
            reps[1] = prev_opt->reps[0];
            reps[2] = prev_opt->reps[1];
            reps[3] = prev_opt->reps[2];
        }
    }
    cur_opt->state = state;
    memcpy(cur_opt->reps, reps, sizeof(cur_opt->reps));
    LZMA2_prob const is_rep_prob = enc->states.is_rep[state];

    {   LZMA2_node *const next_opt = &enc->opt_buf[cur + 1];
        U32 const cur_price = cur_opt->price;
        U32 const next_price = next_opt->price;
        LZMA2_prob const is_match_prob = enc->states.is_match[state][pos_state];
        unsigned const cur_byte = *data;
        unsigned const match_byte = *(data - reps[0] - 1);
       
        U32 cur_and_lit_price = cur_price + GET_PRICE_0(is_match_prob);
        /* This is a compromise to try to filter out cases where literal + rep0 is unlikely to be cheaper */
        BYTE try_lit = cur_and_lit_price + kMinLitPrice / 2U <= next_price;
        if (try_lit) {
            /* cur_and_lit_price is used later for the literal + rep0 test */
            cur_and_lit_price += LZMA_getLiteralPrice(enc, pos, state, data[-1], cur_byte, match_byte);
            /* Try literal */
            if (cur_and_lit_price < next_price) {
                next_opt->price = cur_and_lit_price;
                next_opt->len = 1;
                MARK_LITERAL(*next_opt);
                if (is_hybrid) /* Evaluates as a constant expression due to inlining */
                    try_lit = 0;
            }
        }
        match_price = cur_price + GET_PRICE_1(is_match_prob);
        rep_match_price = match_price + GET_PRICE_1(is_rep_prob);
        if (match_byte == cur_byte) {
            /* Try 1-byte rep0 */
            U32 short_rep_price = rep_match_price + LZMA_getRepLen1Price(enc, state, pos_state);
            if (short_rep_price <= next_opt->price) {
                next_opt->price = short_rep_price;
                next_opt->len = 1;
                MARK_SHORT_REP(*next_opt);
            }
        }
        bytes_avail = MIN(block.end - pos, kOptimizerBufferSize - 1 - cur);
        if (bytes_avail < 2)
            return len_end;

        /* If match_byte == cur_byte a rep0 begins at the current position */
        if (is_hybrid && try_lit && match_byte != cur_byte) {
            /* Try literal + rep0 */
            const BYTE *const data_2 = data - reps[0];
            size_t limit = MIN(bytes_avail - 1, fast_length);
            size_t len_test_2 = ZSTD_count(data + 1, data_2, data + 1 + limit);
            if (len_test_2 >= 2) {
                size_t const state_2 = LIT_NEXT_STATE(state);
                size_t const pos_state_next = (pos + 1) & pos_mask;
                U32 const next_rep_match_price = cur_and_lit_price +
                    GET_PRICE_1(enc->states.is_match[state_2][pos_state_next]) +
                    GET_PRICE_1(enc->states.is_rep[state_2]);
                U32 const cur_and_len_price = next_rep_match_price + LZMA_getRepMatch0Price(enc, len_test_2, state_2, pos_state_next);
                size_t const offset = cur + 1 + len_test_2;
                if (cur_and_len_price < enc->opt_buf[offset].price) {
                    len_end = MAX(len_end, offset);
                    enc->opt_buf[offset].price = cur_and_len_price;
                    enc->opt_buf[offset].len = (unsigned)len_test_2;
                    enc->opt_buf[offset].dist = 0;
                    enc->opt_buf[offset].extra = 1;
                }
            }
        }
    }

    size_t const max_length = MIN(bytes_avail, fast_length);
    size_t start_len = 2;

    if (match.length > 0) {
        size_t len_test;
        size_t len;
        U32 cur_rep_price;
        for (size_t rep_index = 0; rep_index < kNumReps; ++rep_index) {
            const BYTE *const data_2 = data - reps[rep_index] - 1;
            if (MEM_read16(data) != MEM_read16(data_2))
                continue;
            /* Test is limited to fast_length, but it is rare for the RMF to miss the longest match,
             * therefore this function is rarely called when a rep len > fast_length exists */
            len_test = ZSTD_count(data + 2, data_2 + 2, data + max_length) + 2;
            len_end = MAX(len_end, cur + len_test);
            cur_rep_price = rep_match_price + LZMA_getRepPrice(enc, rep_index, state, pos_state);
            len = 2;
            /* Try rep match */
            do {
                U32 const cur_and_len_price = cur_rep_price + enc->states.rep_len_states.prices[pos_state][len - kMatchLenMin];
                LZMA2_node *const opt = &enc->opt_buf[cur + len];
                if (cur_and_len_price < opt->price) {
                    opt->price = cur_and_len_price;
                    opt->len = (unsigned)len;
                    opt->dist = (U32)rep_index;
                    opt->extra = 0;
                }
            } while (++len <= len_test);

            if (rep_index == 0) {
                /* Save time by exluding normal matches not longer than the rep */
                start_len = len_test + 1;
            }
            /* rep + literal + rep0 is not common so this test is skipped for faster, non-hybrid encoding */
            if (is_hybrid && len_test + 3 <= bytes_avail && MEM_read16(data + len_test + 1) == MEM_read16(data_2 + len_test + 1)) {
                /* Try rep + literal + rep0.
                 * The second rep may be > fast_length, but it is not worth the extra time to handle this case
                 * and the price table is not filled for it */
                size_t const len_test_2 = ZSTD_count(data + len_test + 3,
                    data_2 + len_test + 3,
                    data + MIN(len_test + 1 + fast_length, bytes_avail)) + 2;
                size_t state_2 = REP_NEXT_STATE(state);
                size_t pos_state_next = (pos + len_test) & pos_mask;
                U32 rep_lit_rep_total_price =
                    cur_rep_price + enc->states.rep_len_states.prices[pos_state][len_test - kMatchLenMin]
                    + GET_PRICE_0(enc->states.is_match[state_2][pos_state_next])
                    + LZMA_getLiteralPriceMatched(LITERAL_PROBS(enc, pos + len_test, data[len_test - 1]),
                        data[len_test], data_2[len_test]);

                state_2 = kState_LitAfterRep;
                pos_state_next = (pos + len_test + 1) & pos_mask;
                rep_lit_rep_total_price +=
                    GET_PRICE_1(enc->states.is_match[state_2][pos_state_next]) +
                    GET_PRICE_1(enc->states.is_rep[state_2]);
                size_t const offset = cur + len_test + 1 + len_test_2;
                rep_lit_rep_total_price += LZMA_getRepMatch0Price(enc, len_test_2, state_2, pos_state_next);
                if (rep_lit_rep_total_price < enc->opt_buf[offset].price) {
                    len_end = MAX(len_end, offset);
                    enc->opt_buf[offset].price = rep_lit_rep_total_price;
                    enc->opt_buf[offset].len = (unsigned)len_test_2;
                    enc->opt_buf[offset].dist = (U32)rep_index;
                    enc->opt_buf[offset].extra = (unsigned)(len_test + 1);
                }
            }
        }
    }
    if (match.length >= start_len && max_length >= start_len) {
        /* Try normal match */
        U32 const normal_match_price = match_price + GET_PRICE_0(is_rep_prob);
        if (!is_hybrid) {
            /* Normal mode - single match */
            size_t const length = MIN(match.length, max_length);
            size_t const cur_dist = match.dist;
            size_t const dist_slot = LZMA_getDistSlot(match.dist);
            size_t len_test = length;
            len_end = MAX(len_end, cur + length);
            for (; len_test >= start_len; --len_test) {
                U32 cur_and_len_price = normal_match_price + enc->states.len_states.prices[pos_state][len_test - kMatchLenMin];
                size_t const len_to_dist_state = LEN_TO_DIST_STATE(len_test);

                if (cur_dist < kNumFullDistances)
                    cur_and_len_price += enc->distance_prices[len_to_dist_state][cur_dist];
                else 
                    cur_and_len_price += enc->dist_slot_prices[len_to_dist_state][dist_slot] + enc->align_prices[cur_dist & kAlignMask];

                LZMA2_node *const opt = &enc->opt_buf[cur + len_test];
                if (cur_and_len_price < opt->price) {
                    opt->price = cur_and_len_price;
                    opt->len = (unsigned)len_test;
                    opt->dist = (U32)(cur_dist + kNumReps);
                    opt->extra = 0;
                }
                else break;
            }
        }
        else {
            /* Hybrid mode */
            size_t main_len;

            match.length = MIN(match.length, (U32)max_length);
            /* Need to test max_length < 4 because the hash fn reads a U32 */
            if (match.length < 3 || max_length < 4) {
                enc->matches[0] = match;
                enc->match_count = 1;
                main_len = match.length;
            }
            else {
                main_len = LZMA_hashGetMatches(enc, block, pos, max_length, match);
            }
            ptrdiff_t match_index = enc->match_count - 1;
            len_end = MAX(len_end, cur + main_len);

            /* Start with a match longer than the best rep if one exists */
            ptrdiff_t start_match = 0;
            while (start_len > enc->matches[start_match].length)
                ++start_match;

            enc->matches[start_match - 1].length = (U32)start_len - 1; /* Avoids an if..else branch in the loop. [-1] is ok */

            for (; match_index >= start_match; --match_index) {
                size_t len_test = enc->matches[match_index].length;
                size_t const cur_dist = enc->matches[match_index].dist;
                const BYTE *const data_2 = data - cur_dist - 1;
                size_t const rep_0_pos = len_test + 1;
                size_t dist_slot = LZMA_getDistSlot((U32)cur_dist);
                U32 cur_and_len_price;
                /* Test from the full length down to 1 more than the next shorter match */
                size_t base_len = enc->matches[match_index - 1].length + 1;
                for (; len_test >= base_len; --len_test) {
                    cur_and_len_price = normal_match_price + enc->states.len_states.prices[pos_state][len_test - kMatchLenMin];
                    size_t const len_to_dist_state = LEN_TO_DIST_STATE(len_test);
                    if (cur_dist < kNumFullDistances)
                        cur_and_len_price += enc->distance_prices[len_to_dist_state][cur_dist];
                    else
                        cur_and_len_price += enc->dist_slot_prices[len_to_dist_state][dist_slot] + enc->align_prices[cur_dist & kAlignMask];

                    BYTE const sub_len = len_test < enc->matches[match_index].length;

                    LZMA2_node *const opt = &enc->opt_buf[cur + len_test];
                    if (cur_and_len_price < opt->price) {
                        opt->price = cur_and_len_price;
                        opt->len = (unsigned)len_test;
                        opt->dist = (U32)(cur_dist + kNumReps);
                        opt->extra = 0;
                    }
                    else if(sub_len)
                        break; /* End the tests if prices for shorter lengths are not lower than those already recorded */

                    if (!sub_len && rep_0_pos + 2 <= bytes_avail && MEM_read16(data + rep_0_pos) == MEM_read16(data_2 + rep_0_pos)) {
                        /* Try match + literal + rep0 */
                        size_t const limit = MIN(rep_0_pos + fast_length, bytes_avail);
                        size_t const len_test_2 = ZSTD_count(data + rep_0_pos + 2, data_2 + rep_0_pos + 2, data + limit) + 2;
                        size_t state_2 = MATCH_NEXT_STATE(state);
                        size_t pos_state_next = (pos + len_test) & pos_mask;
                        U32 match_lit_rep_total_price = cur_and_len_price +
                            GET_PRICE_0(enc->states.is_match[state_2][pos_state_next]) +
                            LZMA_getLiteralPriceMatched(LITERAL_PROBS(enc, pos + len_test, data[len_test - 1]),
                                data[len_test], data_2[len_test]);

                        state_2 = kState_LitAfterMatch;
                        pos_state_next = (pos_state_next + 1) & pos_mask;
                        match_lit_rep_total_price +=
                            GET_PRICE_1(enc->states.is_match[state_2][pos_state_next]) +
                            GET_PRICE_1(enc->states.is_rep[state_2]);
                        size_t const offset = cur + rep_0_pos + len_test_2;
                        match_lit_rep_total_price += LZMA_getRepMatch0Price(enc, len_test_2, state_2, pos_state_next);
                        if (match_lit_rep_total_price < enc->opt_buf[offset].price) {
                            len_end = MAX(len_end, offset);
                            enc->opt_buf[offset].price = match_lit_rep_total_price;
                            enc->opt_buf[offset].len = (unsigned)len_test_2;
                            enc->opt_buf[offset].extra = (unsigned)rep_0_pos;
                            enc->opt_buf[offset].dist = (U32)(cur_dist + kNumReps);
                        }
                    }
                }
            }
        }
    }
    return len_end;
}

FORCE_NOINLINE
static void LZMA_initMatchesPos0(LZMA2_ECtx *const enc,
    RMF_match const match,
    size_t const pos_state,
    size_t len,
    unsigned const normal_match_price)
{
    if ((unsigned)len <= match.length) {
        size_t const distance = match.dist;
        size_t const slot = LZMA_getDistSlot(match.dist);
        /* Test every available length of the match */
        do {
            unsigned cur_and_len_price = normal_match_price + enc->states.len_states.prices[pos_state][len - kMatchLenMin];
            size_t const len_to_dist_state = LEN_TO_DIST_STATE(len);

            if (distance < kNumFullDistances)
                cur_and_len_price += enc->distance_prices[len_to_dist_state][distance];
            else
                cur_and_len_price += enc->align_prices[distance & kAlignMask] + enc->dist_slot_prices[len_to_dist_state][slot];

            if (cur_and_len_price < enc->opt_buf[len].price) {
                enc->opt_buf[len].price = cur_and_len_price;
                enc->opt_buf[len].len = (unsigned)len;
                enc->opt_buf[len].dist = (U32)(distance + kNumReps);
                enc->opt_buf[len].extra = 0;
            }
            ++len;
        } while ((U32)len <= match.length);
    }
}

FORCE_NOINLINE
static size_t LZMA_initMatchesPos0Best(LZMA2_ECtx *const enc, FL2_dataBlock const block,
    RMF_match const match,
    size_t const pos,
    size_t start_len,
    unsigned const normal_match_price)
{
    if (start_len <= match.length) {
        size_t main_len;
        if (match.length < 3 || block.end - pos < 4) {
            enc->matches[0] = match;
            enc->match_count = 1;
            main_len = match.length;
        }
        else {
            main_len = LZMA_hashGetMatches(enc, block, pos, MIN(block.end - pos, enc->fast_length), match);
        }

        ptrdiff_t start_match = 0;
        while (start_len > enc->matches[start_match].length)
            ++start_match;

        enc->matches[start_match - 1].length = (U32)start_len - 1; /* Avoids an if..else branch in the loop. [-1] is ok */

        size_t pos_state = pos & enc->pos_mask;

        for (ptrdiff_t match_index = enc->match_count - 1; match_index >= start_match; --match_index) {
            size_t len_test = enc->matches[match_index].length;
            size_t const distance = enc->matches[match_index].dist;
            size_t const slot = LZMA_getDistSlot((U32)distance);
            size_t const base_len = enc->matches[match_index - 1].length + 1;
            /* Test every available match length at the shortest distance. The buffer is sorted */
            /* in order of increasing length, and therefore increasing distance too. */
            for (; len_test >= base_len; --len_test) {
                unsigned cur_and_len_price = normal_match_price
                    + enc->states.len_states.prices[pos_state][len_test - kMatchLenMin];
                size_t const len_to_dist_state = LEN_TO_DIST_STATE(len_test);

                if (distance < kNumFullDistances)
                    cur_and_len_price += enc->distance_prices[len_to_dist_state][distance];
                else
                    cur_and_len_price += enc->align_prices[distance & kAlignMask] + enc->dist_slot_prices[len_to_dist_state][slot];

                if (cur_and_len_price < enc->opt_buf[len_test].price) {
                    enc->opt_buf[len_test].price = cur_and_len_price;
                    enc->opt_buf[len_test].len = (unsigned)len_test;
                    enc->opt_buf[len_test].dist = (U32)(distance + kNumReps);
                    enc->opt_buf[len_test].extra = 0;
                }
                else break;
            }
        }
        return main_len;
    }
    return 0;
}

/* Test all available options at position 0 of the optimizer buffer.
* The prices at this point are all initialized to kInfinityPrice.
* This function must not be called at a position where no match is
* available. */
FORCE_INLINE_TEMPLATE
size_t LZMA_initOptimizerPos0(LZMA2_ECtx *const enc, FL2_dataBlock const block,
    RMF_match const match,
    size_t const pos,
    int const is_hybrid,
    U32* const reps)
{
    size_t const max_length = MIN(block.end - pos, kMatchLenMax);
    const BYTE *const data = block.data + pos;
    const BYTE *data_2;
    size_t rep_max_index = 0;
    size_t rep_lens[kNumReps];

    /* Find any rep matches */
    for (size_t i = 0; i < kNumReps; ++i) {
        reps[i] = enc->states.reps[i];
        data_2 = data - reps[i] - 1;
        if (MEM_read16(data) != MEM_read16(data_2)) {
            rep_lens[i] = 0;
            continue;
        }
        rep_lens[i] = ZSTD_count(data + 2, data_2 + 2, data + max_length) + 2;
        if (rep_lens[i] > rep_lens[rep_max_index])
            rep_max_index = i;
    }
    if (rep_lens[rep_max_index] >= enc->fast_length) {
        enc->opt_buf[0].len = (unsigned)(rep_lens[rep_max_index]);
        enc->opt_buf[0].dist = (U32)rep_max_index;
        return 0;
    }
    if (match.length >= enc->fast_length) {
        enc->opt_buf[0].len = match.length;
        enc->opt_buf[0].dist = match.dist + kNumReps;
        return 0;
    }

    unsigned const cur_byte = *data;
    unsigned const match_byte = *(data - reps[0] - 1);
    size_t const state = enc->states.state;
    size_t const pos_state = pos & enc->pos_mask;
    LZMA2_prob const is_match_prob = enc->states.is_match[state][pos_state];
    LZMA2_prob const is_rep_prob = enc->states.is_rep[state];

    enc->opt_buf[0].state = state;
    /* Set the price for literal */
    enc->opt_buf[1].price = GET_PRICE_0(is_match_prob) +
        LZMA_getLiteralPrice(enc, pos, state, data[-1], cur_byte, match_byte);
    MARK_LITERAL(enc->opt_buf[1]);

    unsigned const match_price = GET_PRICE_1(is_match_prob);
    unsigned const rep_match_price = match_price + GET_PRICE_1(is_rep_prob);
    if (match_byte == cur_byte) {
        /* Try 1-byte rep0 */
        unsigned const short_rep_price = rep_match_price + LZMA_getRepLen1Price(enc, state, pos_state);
        if (short_rep_price < enc->opt_buf[1].price) {
            enc->opt_buf[1].price = short_rep_price;
            MARK_SHORT_REP(enc->opt_buf[1]);
        }
    }
    memcpy(enc->opt_buf[0].reps, reps, sizeof(enc->opt_buf[0].reps));
    enc->opt_buf[1].len = 1;
    /* Test the rep match prices */
    for (size_t i = 0; i < kNumReps; ++i) {
        size_t rep_len = rep_lens[i];
        if (rep_len < 2)
            continue;

        unsigned const price = rep_match_price + LZMA_getRepPrice(enc, i, state, pos_state);
        /* Test every available length of the rep */
        do {
            unsigned const cur_and_len_price = price + enc->states.rep_len_states.prices[pos_state][rep_len - kMatchLenMin];
            if (cur_and_len_price < enc->opt_buf[rep_len].price) {
                enc->opt_buf[rep_len].price = cur_and_len_price;
                enc->opt_buf[rep_len].len = (unsigned)rep_len;
                enc->opt_buf[rep_len].dist = (U32)i;
                enc->opt_buf[rep_len].extra = 0;
            }
        } while (--rep_len >= kMatchLenMin);
    }
    unsigned const normal_match_price = match_price + GET_PRICE_0(is_rep_prob);
    size_t const len = (rep_lens[0] >= 2) ? rep_lens[0] + 1 : 2;
    /* Test the match prices */
    if (!is_hybrid) {
        /* Normal mode */
        LZMA_initMatchesPos0(enc, match, pos_state, len, normal_match_price);
        return MAX(match.length, rep_lens[rep_max_index]);
    }
    else {
        /* Hybrid mode */
        size_t main_len = LZMA_initMatchesPos0Best(enc, block, match, pos, len, normal_match_price);
        return MAX(main_len, rep_lens[rep_max_index]);
    }
}

FORCE_INLINE_TEMPLATE
size_t LZMA_encodeOptimumSequence(LZMA2_ECtx *const enc, FL2_dataBlock const block,
    FL2_matchTable* const tbl,
    int const struct_tbl,
    int const is_hybrid,
    size_t start_index,
    size_t const uncompressed_end,
    RMF_match match)
{
    size_t len_end = enc->len_end_max;
    unsigned const search_depth = tbl->params.depth;
    do {
        size_t const pos_mask = enc->pos_mask;

        /* Reset all prices that were set last time */
        for (; (len_end & 3) != 0; --len_end)
            enc->opt_buf[len_end].price = kInfinityPrice;
        for (; len_end >= 4; len_end -= 4) {
            enc->opt_buf[len_end].price = kInfinityPrice;
            enc->opt_buf[len_end - 1].price = kInfinityPrice;
            enc->opt_buf[len_end - 2].price = kInfinityPrice;
            enc->opt_buf[len_end - 3].price = kInfinityPrice;
        }

        /* Set everything up at position 0 */
        size_t pos = start_index;
        U32 reps[kNumReps];
        len_end = LZMA_initOptimizerPos0(enc, block, match, pos, is_hybrid, reps);
        match.length = 0;
        size_t cur = 1;

        /* len_end == 0 if a match of fast_length was found */
        if (len_end > 0) {
            ++pos;
            for (; cur < len_end; ++cur, ++pos) {
                /* Terminate if the farthest calculated price is too near the buffer end */
                if (len_end >= kOptimizerBufferSize - kOptimizerEndSize) {
                    U32 price = enc->opt_buf[cur].price;
                    /* This is a compromise to favor more distant end points
                     * even if the price is a bit higher */
                    U32 const delta = price / (U32)cur / 2U;
                    for (size_t j = cur + 1; j <= len_end; j++) {
                        U32 const price2 = enc->opt_buf[j].price;
                        if (price >= price2) {
                            price = price2;
                            cur = j;
                        }
                        price += delta;
                    }
                    break;
                }

                /* Skip ahead if a lower or equal price is available at greater distance */
                size_t const end = MIN(cur + kOptimizerSkipSize, len_end);
                U32 price = enc->opt_buf[cur].price;
                for (size_t j = cur + 1; j <= end; j++) {
                    U32 const price2 = enc->opt_buf[j].price;
                    if (price >= price2) {
                        price = price2;
                        pos += j - cur;
                        cur = j;
                        if (cur == len_end)
                            goto reverse;
                    }
                }

                match = RMF_getMatch(block, tbl, search_depth, struct_tbl, pos);
                if (match.length >= enc->fast_length)
                    break;

                len_end = LZMA_optimalParse(enc, block, match, pos, cur, len_end, is_hybrid, reps);
            }
reverse:
            DEBUGLOG(6, "End optimal parse at %u", (U32)cur);
            LZMA_reverseOptimalChain(enc->opt_buf, cur);
        }
        /* Encode the selections in the buffer */
        size_t i = 0;
        do {
            unsigned const len = enc->opt_buf[i].len;

            if (len == 1 && enc->opt_buf[i].dist == kNullDist) {
                LZMA_encodeLiteralBuf(enc, block.data, start_index + i);
                ++i;
            }
            else {
                size_t const pos_state = (start_index + i) & pos_mask;
                U32 const dist = enc->opt_buf[i].dist;
                /* Updating i separately for each case may allow a branch to be eliminated */
                if (dist >= kNumReps) {
                    LZMA_encodeNormalMatch(enc, len, dist - kNumReps, pos_state);
                    i += len;
                }
                else if(len == 1) {
                    LZMA_encodeRepMatchShort(enc, pos_state);
                    ++i;
                }
                else {
                    LZMA_encodeRepMatchLong(enc, len, dist, pos_state);
                    i += len;
                }
            }
        } while (i < cur);
        start_index += i;
        /* Do another round if there is a long match pending,
         * because the reps must be checked and the match encoded. */
    } while (match.length >= enc->fast_length && start_index < uncompressed_end && enc->rc.out_index < enc->chunk_size);

    enc->len_end_max = len_end;

    return start_index;
}

static void FORCE_NOINLINE LZMA_fillAlignPrices(LZMA2_ECtx *const enc)
{
    unsigned i;
    const LZMA2_prob *const probs = enc->states.dist_align_encoders;
    for (i = 0; i < kAlignTableSize / 2; i++) {
        U32 price = 0;
        unsigned sym = i;
        unsigned m = 1;
        unsigned bit;
        bit = sym & 1; sym >>= 1; price += GET_PRICE(probs[m], bit); m = (m << 1) + bit;
        bit = sym & 1; sym >>= 1; price += GET_PRICE(probs[m], bit); m = (m << 1) + bit;
        bit = sym & 1; sym >>= 1; price += GET_PRICE(probs[m], bit); m = (m << 1) + bit;
        U32 const prob = probs[m];
        enc->align_prices[i] = price + GET_PRICE_0(prob);
        enc->align_prices[i + 8] = price + GET_PRICE_1(prob);
    }
}

static void FORCE_NOINLINE LZMA_fillDistancesPrices(LZMA2_ECtx *const enc)
{
    U32 * const temp_prices = enc->distance_prices[kNumLenToPosStates - 1];

    enc->match_price_count = 0;

    for (size_t i = kStartPosModelIndex / 2; i < kNumFullDistances / 2; i++) {
        unsigned const dist_slot = distance_table[i];
        unsigned footer_bits = (dist_slot >> 1) - 1;
        size_t base = ((2 | (dist_slot & 1)) << footer_bits);
        const LZMA2_prob *probs = enc->states.dist_encoders + base * 2U;
        base += i;
        probs = probs - distance_table[base] - 1;
        U32 price = 0;
        unsigned m = 1;
        unsigned sym = (unsigned)i;
        unsigned const offset = (unsigned)1 << footer_bits;

        for (; footer_bits != 0; --footer_bits) {
            unsigned bit = sym & 1;
            sym >>= 1;
            price += GET_PRICE(probs[m], bit);
            m = (m << 1) + bit;
        };

        unsigned const prob = probs[m];
        temp_prices[base] = price + GET_PRICE_0(prob);
        temp_prices[base + offset] = price + GET_PRICE_1(prob);
    }

    for (unsigned lps = 0; lps < kNumLenToPosStates; lps++) {
        size_t slot;
        size_t const dist_table_size2 = (enc->dist_price_table_size + 1) >> 1;
        U32 *const dist_slot_prices = enc->dist_slot_prices[lps];
        const LZMA2_prob *const probs = enc->states.dist_slot_encoders[lps];

        for (slot = 0; slot < dist_table_size2; slot++) {
            /* dist_slot_prices[slot] = RcTree_GetPrice(encoder, kNumPosSlotBits, slot, p->ProbPrices); */
            U32 price;
            unsigned bit;
            unsigned sym = (unsigned)slot + (1 << (kNumPosSlotBits - 1));
            bit = sym & 1; sym >>= 1; price = GET_PRICE(probs[sym], bit);
            bit = sym & 1; sym >>= 1; price += GET_PRICE(probs[sym], bit);
            bit = sym & 1; sym >>= 1; price += GET_PRICE(probs[sym], bit);
            bit = sym & 1; sym >>= 1; price += GET_PRICE(probs[sym], bit);
            bit = sym & 1; sym >>= 1; price += GET_PRICE(probs[sym], bit);
            unsigned const prob = probs[slot + (1 << (kNumPosSlotBits - 1))];
            dist_slot_prices[slot * 2] = price + GET_PRICE_0(prob);
            dist_slot_prices[slot * 2 + 1] = price + GET_PRICE_1(prob);
        }

        {
            U32 delta = ((U32)((kEndPosModelIndex / 2 - 1) - kNumAlignBits) << kNumBitPriceShiftBits);
            for (slot = kEndPosModelIndex / 2; slot < dist_table_size2; slot++) {
                dist_slot_prices[slot * 2] += delta;
                dist_slot_prices[slot * 2 + 1] += delta;
                delta += ((U32)1 << kNumBitPriceShiftBits);
            }
        }

        {
            U32 *const dp = enc->distance_prices[lps];

            dp[0] = dist_slot_prices[0];
            dp[1] = dist_slot_prices[1];
            dp[2] = dist_slot_prices[2];
            dp[3] = dist_slot_prices[3];

            for (size_t i = 4; i < kNumFullDistances; i += 2) {
                U32 slot_price = dist_slot_prices[distance_table[i]];
                dp[i] = slot_price + temp_prices[i];
                dp[i + 1] = slot_price + temp_prices[i + 1];
            }
        }
    }
}

FORCE_INLINE_TEMPLATE
size_t LZMA_encodeChunkBest(LZMA2_ECtx *const enc,
    FL2_dataBlock const block,
    FL2_matchTable* const tbl,
    int const struct_tbl,
    size_t pos,
    size_t const uncompressed_end)
{
    unsigned const search_depth = tbl->params.depth;
    LZMA_fillDistancesPrices(enc);
    LZMA_fillAlignPrices(enc);
    LZMA_lengthStates_updatePrices(enc, &enc->states.len_states);
    LZMA_lengthStates_updatePrices(enc, &enc->states.rep_len_states);

    while (pos < uncompressed_end && enc->rc.out_index < enc->chunk_size)
    {
        RMF_match const match = RMF_getMatch(block, tbl, search_depth, struct_tbl, pos);
        if (match.length > 1) {
            /* Template-like inline function */
            if (enc->strategy == FL2_ultra) {
                pos = LZMA_encodeOptimumSequence(enc, block, tbl, struct_tbl, 1, pos, uncompressed_end, match);
            }
            else {
                pos = LZMA_encodeOptimumSequence(enc, block, tbl, struct_tbl, 0, pos, uncompressed_end, match);
            }
            if (enc->match_price_count >= kMatchRepriceFrequency) {
                LZMA_fillAlignPrices(enc);
                LZMA_fillDistancesPrices(enc);
                LZMA_lengthStates_updatePrices(enc, &enc->states.len_states);
            }
            if (enc->rep_len_price_count >= kRepLenRepriceFrequency) {
                enc->rep_len_price_count = 0;
                LZMA_lengthStates_updatePrices(enc, &enc->states.rep_len_states);
            }
        }
        else {
            if (block.data[pos] != block.data[pos - enc->states.reps[0] - 1]) {
                LZMA_encodeLiteralBuf(enc, block.data, pos);
                ++pos;
            }
            else {
                LZMA_encodeRepMatchShort(enc, pos & enc->pos_mask);
                ++pos;
            }
        }
    }
    return pos;
}

static void LZMA_lengthStates_Reset(LZMA2_lenStates* const ls, unsigned const fast_length)
{
    ls->choice = kProbInitValue;

    for (size_t i = 0; i < (kNumPositionStatesMax << (kLenNumLowBits + 1)); ++i)
        ls->low[i] = kProbInitValue;

    for (size_t i = 0; i < kLenNumHighSymbols; ++i)
        ls->high[i] = kProbInitValue;

    ls->table_size = fast_length + 1 - kMatchLenMin;
}

static void LZMA_encoderStates_Reset(LZMA2_encStates* const es, unsigned const lc, unsigned const lp, unsigned fast_length)
{
    es->state = 0;

    for (size_t i = 0; i < kNumReps; ++i)
        es->reps[i] = 0;

    for (size_t i = 0; i < kNumStates; ++i) {
        for (size_t j = 0; j < kNumPositionStatesMax; ++j) {
            es->is_match[i][j] = kProbInitValue;
            es->is_rep0_long[i][j] = kProbInitValue;
        }
        es->is_rep[i] = kProbInitValue;
        es->is_rep_G0[i] = kProbInitValue;
        es->is_rep_G1[i] = kProbInitValue;
        es->is_rep_G2[i] = kProbInitValue;
    }
    size_t const num = (size_t)(kNumLiterals * kNumLitTables) << (lp + lc);
    for (size_t i = 0; i < num; ++i)
        es->literal_probs[i] = kProbInitValue;

    for (size_t i = 0; i < kNumLenToPosStates; ++i) {
        LZMA2_prob *probs = es->dist_slot_encoders[i];
        for (size_t j = 0; j < (1 << kNumPosSlotBits); ++j)
            probs[j] = kProbInitValue;
    }
    for (size_t i = 0; i < kNumFullDistances - kEndPosModelIndex; ++i)
        es->dist_encoders[i] = kProbInitValue;

    LZMA_lengthStates_Reset(&es->len_states, fast_length);
    LZMA_lengthStates_Reset(&es->rep_len_states, fast_length);

    for (size_t i = 0; i < (1 << kNumAlignBits); ++i)
        es->dist_align_encoders[i] = kProbInitValue;
}

BYTE LZMA2_getDictSizeProp(size_t const dictionary_size)
{
    BYTE dict_size_prop = 0;
    for (BYTE bit = 11; bit < 32; ++bit) {
        if (((size_t)2 << bit) >= dictionary_size) {
            dict_size_prop = (bit - 11) << 1;
            break;
        }
        if (((size_t)3 << bit) >= dictionary_size) {
            dict_size_prop = ((bit - 11) << 1) | 1;
            break;
        }
    }
    return dict_size_prop;
}

size_t LZMA2_compressBound(size_t src_size)
{
	/* Minimum average uncompressed size. An average size of half kChunkSize should be assumed
	 * to account for thread_count incomplete end chunks per block. LZMA expansion is < 2% so 1/16
	 * is a safe overestimate. */
	static const unsigned chunk_min_avg = (kChunkSize - (kChunkSize / 16U)) / 2U;
	/* Maximum size of data stored in a sequence of uncompressed chunks */
	return src_size + ((src_size + chunk_min_avg - 1) / chunk_min_avg) * 3 + 6;
}

size_t LZMA2_encMemoryUsage(unsigned const chain_log, FL2_strategy const strategy, unsigned const thread_count)
{
    size_t size = sizeof(LZMA2_ECtx);
    if(strategy == FL2_ultra)
        size += sizeof(LZMA2_hc3) + (sizeof(U32) << chain_log) - sizeof(U32);
    return size * thread_count;
}

static void LZMA2_reset(LZMA2_ECtx *const enc, size_t const max_distance)
{
    DEBUGLOG(5, "LZMA encoder reset : max_distance %u", (unsigned)max_distance);
    RC_reset(&enc->rc);
    LZMA_encoderStates_Reset(&enc->states, enc->lc, enc->lp, enc->fast_length);
    enc->pos_mask = (1 << enc->pb) - 1;
    enc->lit_pos_mask = (1 << enc->lp) - 1;
    U32 i = 0;
    for (; max_distance > (size_t)1 << i; ++i) {
    }
    enc->dist_price_table_size = i * 2;
    enc->rep_len_price_count = 0;
    enc->match_price_count = 0;
}

static BYTE LZMA_getLcLpPbCode(LZMA2_ECtx *const enc)
{
    return (BYTE)((enc->pb * 5 + enc->lp) * 9 + enc->lc);
}

/* Integer square root from https://stackoverflow.com/a/1101217 */
static U32 LZMA2_isqrt(U32 op)
{
    U32 res = 0;
    /* "one" starts at the highest power of four <= than the argument. */
    U32 one = (U32)1 << (ZSTD_highbit32(op) & ~1);

    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = res + 2U * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

static BYTE LZMA2_isChunkIncompressible(const FL2_matchTable* const tbl,
    FL2_dataBlock const block, size_t const start,
	unsigned const strategy)
{
	if (block.end - start >= kMinTestChunkSize) {
		static const size_t max_dist_table[][5] = {
			{ 0, 0, 0, 1U << 6, 1U << 14 }, /* fast */
			{ 0, 0, 1U << 6, 1U << 14, 1U << 22 }, /* opt */
			{ 0, 0, 1U << 6, 1U << 14, 1U << 22 } }; /* ultra */
		static const size_t margin_divisor[3] = { 60U, 45U, 120U };
		static const U32 dev_table[3] = { 24, 24, 20};

		size_t const end = MIN(start + kChunkSize, block.end);
		size_t const chunk_size = end - start;
		size_t count = 0;
		size_t const margin = chunk_size / margin_divisor[strategy];
		size_t const terminator = start + margin;

		if (tbl->is_struct) {
			size_t prev_dist = 0;
			for (size_t pos = start; pos < end; ) {
				U32 const link = GetMatchLink(tbl->table, pos);
				if (link == RADIX_NULL_LINK) {
					++pos;
					++count;
					prev_dist = 0;
				}
				else {
					size_t const length = GetMatchLength(tbl->table, pos);
					size_t const dist = pos - GetMatchLink(tbl->table, pos);
                    if (length > 4) {
                        /* Increase the cost if it's not the same match */
                        count += dist != prev_dist;
                    }
                    else {
                        /* Increment the cost for a short match. The cost is the entire length if it's too far */
                        count += (dist < max_dist_table[strategy][length]) ? 1 : length;
                    }
					pos += length;
					prev_dist = dist;
				}
				if (count + terminator <= pos)
					return 0;
			}
		}
		else {
			size_t prev_dist = 0;
			for (size_t pos = start; pos < end; ) {
				U32 const link = tbl->table[pos];
				if (link == RADIX_NULL_LINK) {
					++pos;
					++count;
					prev_dist = 0;
				}
				else {
					size_t const length = link >> RADIX_LINK_BITS;
					size_t const dist = pos - (link & RADIX_LINK_MASK);
					if (length > 4)
						count += dist != prev_dist;
					else
						count += (dist < max_dist_table[strategy][length]) ? 1 : length;
					pos += length;
					prev_dist = dist;
				}
				if (count + terminator <= pos)
					return 0;
			}
		}

        U32 char_count[256];
        U32 char_total = 0;
        /* Expected normal character count * 4 */
        U32 const avg = (U32)(chunk_size / 64U);

        memset(char_count, 0, sizeof(char_count));
        for (size_t pos = start; pos < end; ++pos)
            char_count[block.data[pos]] += 4;
        /* Sum the deviations */
        for (size_t i = 0; i < 256; ++i) {
            S32 delta = char_count[i] - avg;
            char_total += delta * delta;
        }
        U32 sqrt_chunk = (chunk_size == kChunkSize) ? kSqrtChunkSize : LZMA2_isqrt((U32)chunk_size);
        /* Result base on character count std dev */
        return LZMA2_isqrt(char_total) / sqrt_chunk <= dev_table[strategy];
	}
	return 0;
}

static size_t LZMA2_encodeChunk(LZMA2_ECtx *const enc,
    FL2_matchTable* const tbl,
    FL2_dataBlock const block,
    size_t const pos, size_t const uncompressed_end)
{
    /* Template-like inline functions */
    if (enc->strategy == FL2_fast) {
        if (tbl->is_struct) {
            return LZMA_encodeChunkFast(enc, block, tbl, 1,
                pos, uncompressed_end);
        }
        else {
            return LZMA_encodeChunkFast(enc, block, tbl, 0,
                pos, uncompressed_end);
        }
    }
    else {
        if (tbl->is_struct) {
            return LZMA_encodeChunkBest(enc, block, tbl, 1,
                pos, uncompressed_end);
        }
        else {
            return LZMA_encodeChunkBest(enc, block, tbl, 0,
                pos, uncompressed_end);
        }
    }
}

size_t LZMA2_encode(LZMA2_ECtx *const enc,
    FL2_matchTable* const tbl,
    FL2_dataBlock const block,
    const FL2_lzma2Parameters* const options,
    int stream_prop,
    FL2_atomic *const progress_in,
    FL2_atomic *const progress_out,
    int *const canceled)
{
    size_t const start = block.start;

    /* Output starts in the temp buffer */
    BYTE* out_dest = enc->out_buf;
    enc->chunk_size = kTempMinOutput;
    enc->chunk_limit = kTempBufferSize - kMaxMatchEncodeSize * 2;

    /* Each encoder writes a properties byte because the upstream encoder(s) could */
	/* write only uncompressed chunks with no properties. */
	BYTE encode_properties = 1;
    BYTE incompressible = 0;

    if (block.end <= block.start)
        return 0;

    enc->lc = options->lc;
    enc->lp = MIN(options->lp, kNumLiteralPosBitsMax);

    if (enc->lc + enc->lp > kLcLpMax)
        enc->lc = kLcLpMax - enc->lp;

    enc->pb = MIN(options->pb, kNumPositionBitsMax);
    enc->strategy = options->strategy;
    enc->fast_length = MIN(options->fast_length, kMatchLenMax);
    enc->match_cycles = MIN(options->match_cycles, kMatchesMax - 1);

    LZMA2_reset(enc, block.end);

    if (enc->strategy == FL2_ultra) {
        /* Create a hash chain to put the encoder into hybrid mode */
        if (enc->hash_alloc_3 < ((ptrdiff_t)1 << options->second_dict_bits)) {
            if(LZMA_hashCreate(enc, options->second_dict_bits) != 0)
                return FL2_ERROR(memory_allocation);
        }
        else {
            LZMA_hashReset(enc, options->second_dict_bits);
        }
        enc->hash_prev_index = (start >= (size_t)enc->hash_dict_3) ? (ptrdiff_t)(start - enc->hash_dict_3) : (ptrdiff_t)-1;
    }
    enc->len_end_max = kOptimizerBufferSize - 1;

    /* Limit the matches near the end of this slice to not exceed block.end */
    RMF_limitLengths(tbl, block.end);

    for (size_t pos = start; pos < block.end;) {
        size_t header_size = (stream_prop >= 0) + (encode_properties ? kChunkHeaderSize + 1 : kChunkHeaderSize);
        LZMA2_encStates saved_states;
        size_t next_index;

        RC_reset(&enc->rc);
        RC_setOutputBuffer(&enc->rc, out_dest + header_size);

        if (!incompressible) {
            size_t cur = pos;
            size_t const end = (enc->strategy == FL2_fast) ? MIN(block.end, pos + kMaxChunkUncompressedSize - kMatchLenMax + 1)
                : MIN(block.end, pos + kMaxChunkUncompressedSize - kOptimizerBufferSize + 2); /* last byte of opt_buf unused */

            /* Copy states in case chunk is incompressible */
            saved_states = enc->states;

            if (pos == 0) {
                /* First byte of the dictionary */
                LZMA_encodeLiteral(enc, 0, block.data[0], 0);
                ++cur;
            }
            if (pos == start) {
                /* After kTempMinOutput bytes we can write data to the match table because the */
                /* compressed data will never catch up with the table position being read. */
                cur = LZMA2_encodeChunk(enc, tbl, block, cur, end);

				if (header_size + enc->rc.out_index > kTempBufferSize)
					return FL2_ERROR(internal);

                /* Switch to the match table as output buffer */
                out_dest = RMF_getTableAsOutputBuffer(tbl, start);
                memcpy(out_dest, enc->out_buf, header_size + enc->rc.out_index);
                enc->rc.out_buffer = out_dest + header_size;

                /* Now encode up to the full chunk size */
                enc->chunk_size = kChunkSize;
                enc->chunk_limit = kMaxChunkCompressedSize - kMaxMatchEncodeSize * 2;
            }
            next_index = LZMA2_encodeChunk(enc, tbl, block, cur, end);
            RC_flush(&enc->rc);
        }
        else {
            next_index = MIN(pos + kChunkSize, block.end);
        }
        size_t compressed_size = enc->rc.out_index;
        size_t uncompressed_size = next_index - pos;

        if (compressed_size > kMaxChunkCompressedSize || uncompressed_size > kMaxChunkUncompressedSize)
            return FL2_ERROR(internal);

        BYTE* header = out_dest;

        if (stream_prop >= 0) {
            *header++ = (BYTE)stream_prop;
            stream_prop = -1;
        }

        header[1] = (BYTE)((uncompressed_size - 1) >> 8);
        header[2] = (BYTE)(uncompressed_size - 1);
        /* Output an uncompressed chunk if necessary */
        if (incompressible || uncompressed_size + 3 <= compressed_size + header_size) {
            DEBUGLOG(6, "Storing chunk : was %u => %u", (unsigned)uncompressed_size, (unsigned)compressed_size);

            header[0] = (pos == 0) ? kChunkUncompressedDictReset : kChunkUncompressed;

            /* Copy uncompressed data into the output */
            memcpy(header + 3, block.data + pos, uncompressed_size);

            compressed_size = uncompressed_size;
            header_size = 3 + (header - out_dest);

            /* Restore states if compression was attempted */
            if (!incompressible)
                enc->states = saved_states;
        }
        else {
            DEBUGLOG(6, "Compressed chunk : %u => %u", (unsigned)uncompressed_size, (unsigned)compressed_size);

            if (pos == 0)
                header[0] = kChunkCompressedFlag | kChunkAllReset;
            else if (encode_properties)
                header[0] = kChunkCompressedFlag | kChunkStatePropertiesReset;
            else
                header[0] = kChunkCompressedFlag | kChunkNothingReset;

            header[0] |= (BYTE)((uncompressed_size - 1) >> 16);
            header[3] = (BYTE)((compressed_size - 1) >> 8);
            header[4] = (BYTE)(compressed_size - 1);
            if (encode_properties) {
                header[5] = LZMA_getLcLpPbCode(enc);
                encode_properties = 0;
            }
        }
        if (incompressible || uncompressed_size + 3 <= compressed_size + (compressed_size >> kRandomFilterMarginBits) + header_size) {
            /* Test the next chunk for compressibility */
            incompressible = LZMA2_isChunkIncompressible(tbl, block, next_index, enc->strategy);
        }
        out_dest += compressed_size + header_size;

        /* Update progress concurrently with other encoder threads */
        FL2_atomic_add(*progress_in, (long)(next_index - pos));
        FL2_atomic_add(*progress_out, (long)(compressed_size + header_size));

        pos = next_index;

        if (*canceled)
            return FL2_ERROR(canceled);
    }
    return out_dest - RMF_getTableAsOutputBuffer(tbl, start);
}
