#ifndef LZ5HC_COMMON_H
#define LZ5HC_COMMON_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "mem.h"  // MEM_read32


#define LZ5HC_DEBUG(fmt, ...) //printf(fmt, __VA_ARGS__)
#define LZ5_LOG_PARSER(fmt, ...) //printf(fmt, __VA_ARGS__)
#define LZ5_LOG_PRICE(fmt, ...) //printf(fmt, __VA_ARGS__)
#define LZ5_LOG_ENCODE(fmt, ...) //printf(fmt, __VA_ARGS__)

#define MAX(a,b) ((a)>(b))?(a):(b)
#define LZ5_OPT_NUM   (1<<12)


#if MINMATCH == 3
    #define MEM_read24(ptr) (U32)(MEM_read32(ptr)<<8) 
#else
    #define MEM_read24(ptr) (U32)(MEM_read32(ptr)) 
#endif 


#define LZ5_SHORT_LITERALS          ((1<<RUN_BITS2)-1)
#define LZ5_LITERALS                ((1<<RUN_BITS)-1)

#define LZ5_SHORT_LITLEN_COST(len)  (len<LZ5_SHORT_LITERALS ? 0 : (len-LZ5_SHORT_LITERALS < 255 ? 1 : (len-LZ5_SHORT_LITERALS-255 < (1<<7) ? 2 : 3)))
#define LZ5_LEN_COST(len)           (len<LZ5_LITERALS ? 0 : (len-LZ5_LITERALS < 255 ? 1 : (len-LZ5_LITERALS-255 < (1<<7) ? 2 : 3)))

static size_t LZ5_LIT_COST(size_t len, size_t offset){ return (len)+(((offset > LZ5_MID_OFFSET_DISTANCE) || (offset<LZ5_SHORT_OFFSET_DISTANCE)) ? LZ5_SHORT_LITLEN_COST(len) : LZ5_LEN_COST(len)); }
static size_t LZ5_MATCH_COST(size_t mlen, size_t offset) { return LZ5_LEN_COST(mlen) + ((offset == 0) ? 1 : (offset<LZ5_SHORT_OFFSET_DISTANCE ? 2 : (offset<LZ5_MID_OFFSET_DISTANCE ? 3 : 4))); }

#define LZ5_CODEWORD_COST(litlen,offset,mlen)   (LZ5_MATCH_COST(mlen,offset) + LZ5_LIT_COST(litlen,offset))
#define LZ5_LIT_ONLY_COST(len)                  ((len)+(LZ5_LEN_COST(len))+1)

#define LZ5_NORMAL_MATCH_COST(mlen,offset)  (LZ5_MATCH_COST(mlen,offset))
#define LZ5_NORMAL_LIT_COST(len)            (len)



/* *************************************
*  HC Inline functions and Macros
***************************************/

static const U64 prime6bytes = 227718039650203ULL;
static const U64 prime7bytes = 58295818150454627ULL;

#if MINMATCH == 3
static const U32 prime3bytes = 506832829U;
static U32 LZ5HC_hash3(U32 u, U32 h) { return (u * prime3bytes) << (32-24) >> (32-h) ; }
static size_t LZ5HC_hash3Ptr(const void* ptr, U32 h) { return LZ5HC_hash3(MEM_read32(ptr), h); }
#endif

static U32 LZ5HC_hash4(U32 u, U32 h) { return (u * prime4bytes) >> (32-h) ; }
static size_t LZ5HC_hash4Ptr(const void* ptr, U32 h) { return LZ5HC_hash4(MEM_read32(ptr), h); }

static size_t LZ5HC_hash5(U64 u, U32 h) { return (size_t)((u * prime5bytes) << (64-40) >> (64-h)) ; }
static size_t LZ5HC_hash5Ptr(const void* p, U32 h) { return LZ5HC_hash5(MEM_read64(p), h); }

static size_t LZ5HC_hash6(U64 u, U32 h) { return (size_t)((u * prime6bytes) << (64-48) >> (64-h)) ; }
static size_t LZ5HC_hash6Ptr(const void* p, U32 h) { return LZ5HC_hash6(MEM_read64(p), h); }

static size_t LZ5HC_hash7(U64 u, U32 h) { return (size_t)((u * prime7bytes) << (64-56) >> (64-h)) ; }
static size_t LZ5HC_hash7Ptr(const void* p, U32 h) { return LZ5HC_hash7(MEM_read64(p), h); }

static size_t LZ5HC_hashPtr(const void* p, U32 hBits, U32 mls)
{
    switch(mls)
    {
    default:
    case 4: return LZ5HC_hash4Ptr(p, hBits);
    case 5: return LZ5HC_hash5Ptr(p, hBits);
    case 6: return LZ5HC_hash6Ptr(p, hBits);
    case 7: return LZ5HC_hash7Ptr(p, hBits);
    }
}



/* *************************************
*  HC Types
***************************************/
/** from faster to stronger */
typedef enum { LZ5HC_fast, LZ5HC_price_fast, LZ5HC_lowest_price, LZ5HC_optimal_price, LZ5HC_optimal_price_bt } LZ5HC_strategy;

typedef struct
{
    U32 windowLog;     /* largest match distance : impact decompression buffer size */
    U32 contentLog;    /* full search segment : larger == more compression, slower, more memory (useless for fast) */
    U32 hashLog;       /* dispatch table : larger == more memory, faster*/
    U32 hashLog3;      /* dispatch table : larger == more memory, faster*/
    U32 searchNum;     /* nb of searches : larger == more compression, slower*/
    U32 searchLength;  /* size of matches : larger == faster decompression */
    U32 sufficientLength;  /* used only by optimal parser: size of matches which is acceptable: larger == more compression, slower */
    U32 fullSearch;    /* used only by optimal parser: perform full search of matches: 1 == more compression, slower */
    LZ5HC_strategy strategy;
} LZ5HC_parameters;


struct LZ5HC_Data_s
{
    U32*   hashTable;
    U32*   hashTable3;
    U32*   chainTable;
    const BYTE* end;        /* next block here to continue on current prefix */
    const BYTE* base;       /* All index relative to this position */
    const BYTE* dictBase;   /* alternate base for extDict */
    const BYTE* inputBuffer;      /* for debugging */
    const BYTE* outputBuffer;     /* for debugging */
    U32   dictLimit;        /* below that point, need extDict */
    U32   lowLimit;         /* below that point, no more dict */
    U32   nextToUpdate;     /* index from which to continue dictionary update */
    U32   compressionLevel;
    U32   last_off;
    LZ5HC_parameters params;
};

typedef struct
{
	int off;
	int len;
	int back;
} LZ5HC_match_t;

typedef struct
{
	int price;
	int off;
	int mlen;
	int litlen;
   	int rep;
} LZ5HC_optimal_t;



/* *************************************
*  HC Pre-defined compression levels
***************************************/

static const int g_maxCompressionLevel = LZ5HC_MAX_CLEVEL;
static const int LZ5HC_compressionLevel_default = 6;

static const LZ5HC_parameters LZ5HC_defaultParameters[LZ5HC_MAX_CLEVEL+1] =
{
    /* windLog, contentLog,  H, H3,  Snum, SL, SuffL, FS, Strategy */
    {        0,          0,  0,  0,     0,  0,     0,  0, LZ5HC_fast             }, // level 0 - never used
    { MAXD_LOG,   MAXD_LOG, 13,  0,     4,  6,     0,  0, LZ5HC_fast             }, // level 1
    { MAXD_LOG,   MAXD_LOG, 13,  0,     2,  6,     0,  0, LZ5HC_fast             }, // level 2
    { MAXD_LOG,   MAXD_LOG, 13,  0,     1,  5,     0,  0, LZ5HC_fast             }, // level 3
    { MAXD_LOG,   MAXD_LOG, 14, 13,     1,  4,     0,  0, LZ5HC_price_fast       }, // level 4
    { MAXD_LOG,   MAXD_LOG, 17, 13,     1,  4,     0,  0, LZ5HC_price_fast       }, // level 5
    { MAXD_LOG,   MAXD_LOG, 15, 13,     1,  4,     0,  0, LZ5HC_lowest_price     }, // level 6
    { MAXD_LOG,   MAXD_LOG, 17, 13,     1,  4,     0,  0, LZ5HC_lowest_price     }, // level 7
    { MAXD_LOG,   MAXD_LOG, 19, 16,     1,  4,     0,  0, LZ5HC_lowest_price     }, // level 8
    { MAXD_LOG,   MAXD_LOG, 23, 16,     3,  4,     0,  0, LZ5HC_lowest_price     }, // level 9
    { MAXD_LOG,   MAXD_LOG, 23, 16,     8,  4,     0,  0, LZ5HC_lowest_price     }, // level 10
    { MAXD_LOG,   MAXD_LOG, 23, 16,     8,  4,    12,  0, LZ5HC_optimal_price    }, // level 11
    { MAXD_LOG,   MAXD_LOG, 23, 16,     8,  4,    64,  0, LZ5HC_optimal_price    }, // level 12
    { MAXD_LOG, MAXD_LOG+1, 23, 16,     8,  4,    64,  1, LZ5HC_optimal_price_bt }, // level 13
    { MAXD_LOG, MAXD_LOG+1, 23, 16,   128,  4,    64,  1, LZ5HC_optimal_price_bt }, // level 14
    { MAXD_LOG, MAXD_LOG+1, 28, 24, 1<<10,  4, 1<<10,  1, LZ5HC_optimal_price_bt }, // level 15
//  {       10,         10, 10,  0,     0,  4,     0,  0, LZ5HC_fast          }, // min values
//  {       24,         24, 28, 24, 1<<24,  7, 1<<24,  2, LZ5HC_optimal_price }, // max values
};



#if defined (__cplusplus)
}
#endif

#endif /* LZ5HC_COMMON_H */
