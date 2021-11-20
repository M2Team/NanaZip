/* lzma2_enc.h -- LZMA2 Encoder
Based on LzmaEnc.h and Lzma2Enc.h : Igor Pavlov
Modified for FL2 by Conor McCarthy
Public domain
*/

#ifndef RADYX_LZMA2_ENCODER_H
#define RADYX_LZMA2_ENCODER_H

#include "mem.h"
#include "data_block.h"
#include "radix_mf.h"
#include "atomic.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define kFastDistBits 12U

#define LZMA2_END_MARKER '\0'
#define ENC_MIN_BYTES_PER_THREAD 0x1C000 /* Enough for 8 threads, 1 Mb dict, 2/16 overlap */


typedef struct LZMA2_ECtx_s LZMA2_ECtx;

typedef struct
{
    unsigned lc;
    unsigned lp;
    unsigned pb;
    unsigned fast_length;
    unsigned match_cycles;
    FL2_strategy strategy;
    unsigned second_dict_bits;
    unsigned reset_interval;
} FL2_lzma2Parameters;


LZMA2_ECtx* LZMA2_createECtx(void);

void LZMA2_freeECtx(LZMA2_ECtx *const enc);

int LZMA2_hashAlloc(LZMA2_ECtx *const enc, const FL2_lzma2Parameters* const options);

size_t LZMA2_encode(LZMA2_ECtx *const enc,
    FL2_matchTable* const tbl,
    FL2_dataBlock const block,
    const FL2_lzma2Parameters* const options,
    int stream_prop,
    FL2_atomic *const progress_in,
    FL2_atomic *const progress_out,
    int *const canceled);

BYTE LZMA2_getDictSizeProp(size_t const dictionary_size);

size_t LZMA2_compressBound(size_t src_size);

size_t LZMA2_encMemoryUsage(unsigned const chain_log, FL2_strategy const strategy, unsigned const thread_count);

#if defined (__cplusplus)
}
#endif

#endif /* RADYX_LZMA2_ENCODER_H */