/*
 * LZ4 auto-framing library
 * Copyright (c) Yann Collet. All rights reserved.
 *
 * BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at :
 * - LZ4 homepage : http://www.lz4.org
 * - LZ4 source repository : https://github.com/lz4/lz4
 */

/* LZ4F is a stand-alone API to create LZ4-compressed Frames
 * in full conformance with specification v1.6.1 .
 * This library rely upon memory management capabilities (malloc, free)
 * provided either by <stdlib.h>,
 * or redirected towards another library of user's choice
 * (see Memory Routines below).
 */


/*-************************************
*  Compiler Options
**************************************/
#include <limits.h>
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)   /* disable: C4127: conditional expression is constant */
#endif


/*-************************************
*  Tuning parameters
**************************************/
/*
 * LZ4F_HEAPMODE :
 * Control how LZ4F_compressFrame allocates the Compression State,
 * either on stack (0:default, fastest), or in memory heap (1:requires malloc()).
 */
#ifndef LZ4F_HEAPMODE
#  define LZ4F_HEAPMODE 0
#endif

/*
 * LZ4F_MULTITHREAD :
 * Enables multi-threaded compression, requested with LZ4F_preferences_t.nbWorkers.
 * Requires <pthread.h> on POSIX systems (compile and link with -pthread), or Windows threads.
 * Default is 0 : no threading code is compiled,
 * and multi-threaded requests are served serially by the calling thread, producing the same output.
 */
#ifndef LZ4F_MULTITHREAD
#  define LZ4F_MULTITHREAD 0
#endif
#if defined(LZ4_FREESTANDING) && (LZ4_FREESTANDING == 1)
#  undef  LZ4F_MULTITHREAD
#  define LZ4F_MULTITHREAD 0
#endif
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)   /* MinGW.org, unlike mingw-w64, lacks Windows Vista API (condition variables) */
#  undef  LZ4F_MULTITHREAD
#  define LZ4F_MULTITHREAD 0
#endif


/*-************************************
*  Library declarations
**************************************/
#define LZ4F_STATIC_LINKING_ONLY
#include "lz4frame.h"
#define LZ4_STATIC_LINKING_ONLY
#include "lz4.h"
#define LZ4_HC_STATIC_LINKING_ONLY
#include "lz4hc.h"
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"


/*-************************************
*  Memory routines
**************************************/
/*
 * Default allocation functions, employed when LZ4F_CustomMem doesn't define them :
 * malloc(), calloc() and free(), or LZ4_malloc(), LZ4_calloc() and LZ4_free()
 * when lz4 is compiled with LZ4_USER_MEMORY_FUNCTIONS (see lz4.c).
**/

#include <string.h>   /* memset, memcpy, memmove */
#ifndef LZ4_SRC_INCLUDED  /* avoid redefinition when sources are coalesced */
#  define MEM_INIT(p,v,s)   memset((p),(v),(s))
#endif

#ifndef LZ4_SRC_INCLUDED   /* avoid redefinition when sources are coalesced */
#  if defined(LZ4_USER_MEMORY_FUNCTIONS)
     void* LZ4_malloc(size_t s);
     void* LZ4_calloc(size_t n, size_t s);
     void  LZ4_free(void* p);
#    define ALLOC(s)          LZ4_malloc(s)
#    define ALLOC_AND_ZERO(s) LZ4_calloc(1,(s))
#    define FREEMEM(p)        LZ4_free(p)
#  else
#    include <stdlib.h>   /* malloc, calloc, free */
#    define ALLOC(s)          malloc(s)
#    define ALLOC_AND_ZERO(s) calloc(1,(s))
#    define FREEMEM(p)        free(p)
#  endif
#endif

static void* LZ4F_calloc(size_t s, LZ4F_CustomMem cmem)
{
    /* custom calloc defined : use it */
    if (cmem.customCalloc != NULL) {
        return cmem.customCalloc(cmem.opaqueState, s);
    }
    /* nothing defined : use default <stdlib.h>'s calloc() */
    if (cmem.customAlloc == NULL) {
        return ALLOC_AND_ZERO(s);
    }
    /* only custom alloc defined : use it, and combine it with memset() */
    {   void* const p = cmem.customAlloc(cmem.opaqueState, s);
        if (p != NULL) MEM_INIT(p, 0, s);
        return p;
}   }

static void* LZ4F_malloc(size_t s, LZ4F_CustomMem cmem)
{
    /* custom malloc defined : use it */
    if (cmem.customAlloc != NULL) {
        return cmem.customAlloc(cmem.opaqueState, s);
    }
    /* nothing defined : use default <stdlib.h>'s malloc() */
    return ALLOC(s);
}

static void LZ4F_free(void* p, LZ4F_CustomMem cmem)
{
    if (p == NULL) return;
    if (cmem.customFree != NULL) {
        /* custom allocation defined : use it */
        cmem.customFree(cmem.opaqueState, p);
        return;
    }
    /* nothing defined : use default <stdlib.h>'s free() */
    FREEMEM(p);
}


/*-************************************
*  Debug
**************************************/
#if defined(LZ4_DEBUG) && (LZ4_DEBUG>=1)
#  include <assert.h>
#else
#  ifndef assert
#    define assert(condition) ((void)0)
#  endif
#endif

#define LZ4F_STATIC_ASSERT(c)    { enum { LZ4F_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */

#if defined(LZ4_DEBUG) && (LZ4_DEBUG>=2) && !defined(DEBUGLOG)
#  include <stdio.h>
static int g_debuglog_enable = 1;
#  define DEBUGLOG(l, ...) {                                  \
                if ((g_debuglog_enable) && (l<=LZ4_DEBUG)) {  \
                    fprintf(stderr, __FILE__ " %i: ", __LINE__ );  \
                    fprintf(stderr, __VA_ARGS__);             \
                    fprintf(stderr, " \n");                   \
            }   }
#endif
#ifndef DEBUGLOG   /* may already be defined, e.g. by lz4.c in an amalgamated build */
#  define DEBUGLOG(l, ...)      {}    /* disabled */
#endif


/*-************************************
*  Basic Types
**************************************/
#ifndef LZ4_SRC_INCLUDED
# if !defined (__VMS) && (defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#   include <stdint.h>
    typedef  uint8_t BYTE;
    typedef uint16_t U16;
    typedef uint32_t U32;
    typedef  int32_t S32;
    typedef uint64_t U64;
# else
    typedef unsigned char      BYTE;
    typedef unsigned short     U16;
    typedef unsigned int       U32;
    typedef   signed int       S32;
    typedef unsigned long long U64;
# endif
#endif


/* unoptimized version; solves endianness & alignment issues */
static U32 LZ4F_readLE32 (const void* src)
{
    const BYTE* const srcPtr = (const BYTE*)src;
    U32 value32 = srcPtr[0];
    value32 |= ((U32)srcPtr[1])<< 8;
    value32 |= ((U32)srcPtr[2])<<16;
    value32 |= ((U32)srcPtr[3])<<24;
    return value32;
}

static void LZ4F_writeLE32 (void* dst, U32 value32)
{
    BYTE* const dstPtr = (BYTE*)dst;
    dstPtr[0] = (BYTE)value32;
    dstPtr[1] = (BYTE)(value32 >> 8);
    dstPtr[2] = (BYTE)(value32 >> 16);
    dstPtr[3] = (BYTE)(value32 >> 24);
}

static U64 LZ4F_readLE64 (const void* src)
{
    const BYTE* const srcPtr = (const BYTE*)src;
    U64 value64 = srcPtr[0];
    value64 |= ((U64)srcPtr[1]<<8);
    value64 |= ((U64)srcPtr[2]<<16);
    value64 |= ((U64)srcPtr[3]<<24);
    value64 |= ((U64)srcPtr[4]<<32);
    value64 |= ((U64)srcPtr[5]<<40);
    value64 |= ((U64)srcPtr[6]<<48);
    value64 |= ((U64)srcPtr[7]<<56);
    return value64;
}

static void LZ4F_writeLE64 (void* dst, U64 value64)
{
    BYTE* const dstPtr = (BYTE*)dst;
    dstPtr[0] = (BYTE)value64;
    dstPtr[1] = (BYTE)(value64 >> 8);
    dstPtr[2] = (BYTE)(value64 >> 16);
    dstPtr[3] = (BYTE)(value64 >> 24);
    dstPtr[4] = (BYTE)(value64 >> 32);
    dstPtr[5] = (BYTE)(value64 >> 40);
    dstPtr[6] = (BYTE)(value64 >> 48);
    dstPtr[7] = (BYTE)(value64 >> 56);
}


/*-************************************
*  Constants
**************************************/
#ifndef LZ4_SRC_INCLUDED   /* avoid double definition */
#  define KB *(1<<10)
#  define MB *(1<<20)
#  define GB *(1<<30)
#endif

#define _1BIT  0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0x0F
#define _8BITS 0xFF

#define LZ4F_BLOCKUNCOMPRESSED_FLAG 0x80000000U
#define LZ4F_BLOCKSIZEID_DEFAULT LZ4F_max64KB

static const size_t minFHSize = LZ4F_HEADER_SIZE_MIN;   /*  7 */
static const size_t maxFHSize = LZ4F_HEADER_SIZE_MAX;   /* 19 */
static const size_t BHSize = LZ4F_BLOCK_HEADER_SIZE;  /* block header : size, and compress flag */
static const size_t BFSize = LZ4F_BLOCK_CHECKSUM_SIZE;  /* block footer : checksum (optional) */


/*-************************************
*  Structures and local types
**************************************/

typedef enum { LZ4B_COMPRESSED, LZ4B_UNCOMPRESSED} LZ4F_BlockCompressMode_e;
typedef enum { ctxNone, ctxFast, ctxHC } LZ4F_CtxType_e;

struct LZ4F_CDict_s {
    LZ4F_CustomMem cmem;
    void* dictContent;
    LZ4_stream_t* fastCtx;
    LZ4_streamHC_t* HCCtx;
}; /* typedef'd to LZ4F_CDict within lz4frame_static.h */

typedef struct LZ4F_cctx_s
{
    LZ4F_CustomMem cmem;
    LZ4F_preferences_t prefs;
    U32    version;
    U32    cStage;     /* 0 : compression uninitialized ; 1 : initialized, can compress ; 2 : frame ending (LZ4F_compressEnd() in progress) */
    const LZ4F_CDict* cdict;
    size_t maxBlockSize;
    size_t tmpBuffCapacity;
    BYTE*  tmpBuff;    /* internal buffer, for streaming */
    BYTE*  tmpIn;      /* starting position of data compress within internal buffer (>= tmpBuff) */
    size_t tmpInSize;  /* amount of data to compress after tmpIn */
    U64    totalInSize;
    XXH32_state_t xxh;
    void*  lz4CtxPtr;
    U16    lz4CtxAlloc; /* sized for: 0 = none, 1 = lz4 ctx, 2 = lz4hc ctx */
    U16    lz4CtxType;  /* in use as: 0 = none, 1 = lz4 ctx, 2 = lz4hc ctx */
    LZ4F_BlockCompressMode_e  blockCompressMode;
    struct LZ4F_mtCtx_s* mt;   /* multi-threading resources, kept across frames; NULL when never needed */
    U32    mtActive;   /* current frame is cut into jobs (prefs.nbWorkers >= 1) */
    /* LZ4F_flush() and LZ4F_compressEnd() : output not fitting into dstBuffer yet, see LZ4F_emitPending() */
    U32    flushStage; /* 0 : none ; 1 : flush point set, emitting ; LZ4F_compressEnd() only : 2 : footer ; 3 : completed */
    const BYTE* pending;   /* remaining output of a block (stageBuff), or of the frame footer */
    size_t pendingSize;
    BYTE*  stageBuff;  /* a block not fitting into dstBuffer : allocated on first need */
    size_t stageCapacity;
    BYTE   footer[8];  /* endMark + optional content checksum */
    /* raw dictionary (LZ4F_compressBegin_usingDict()) digested for current frame, then employed as a CDict */
    struct LZ4F_CDict_s rawDict;   /* fastCtx or HCCtx, the other one NULL ; no dictContent : not owned */
    LZ4_stream_t*   rawDictFast;   /* allocated on first need */
    LZ4_streamHC_t* rawDictHC;     /* allocated on first need */
} LZ4F_cctx_t;

/* multi-threaded compression, see LZ4F_preferences_t.nbWorkers */
typedef struct LZ4F_mtCtx_s LZ4F_mtCtx_t;
static void   LZ4F_mt_abort(LZ4F_mtCtx_t* mt);
static size_t LZ4F_mt_begin(LZ4F_cctx_t* cctx);
static void   LZ4F_mt_free(LZ4F_mtCtx_t* mt, LZ4F_CustomMem cmem);
static size_t LZ4F_mt_sizeof(const LZ4F_mtCtx_t* mt);


/*-************************************
*  Error management
**************************************/
#define LZ4F_GENERATE_STRING(STRING) #STRING,
static const char* LZ4F_errorStrings[] = { LZ4F_LIST_ERRORS(LZ4F_GENERATE_STRING) };


unsigned LZ4F_isError(LZ4F_errorCode_t code)
{
    return (code > (LZ4F_errorCode_t)(-LZ4F_ERROR_maxCode));
}

const char* LZ4F_getErrorName(LZ4F_errorCode_t code)
{
    static const char* codeError = "Unspecified error code";
    if (LZ4F_isError(code)) return LZ4F_errorStrings[-(int)(code)];
    return codeError;
}

LZ4F_errorCodes LZ4F_getErrorCode(size_t functionResult)
{
    if (!LZ4F_isError(functionResult)) return LZ4F_OK_NoError;
    return (LZ4F_errorCodes)(-(ptrdiff_t)functionResult);
}

static LZ4F_errorCode_t LZ4F_returnErrorCode(LZ4F_errorCodes code)
{
    /* A compilation error here means sizeof(ptrdiff_t) is not large enough */
    LZ4F_STATIC_ASSERT(sizeof(ptrdiff_t) >= sizeof(size_t));
    return (LZ4F_errorCode_t)-(ptrdiff_t)code;
}

#define RETURN_ERROR(e) return LZ4F_returnErrorCode(LZ4F_ERROR_ ## e)

#define RETURN_ERROR_IF(c,e) do {  \
        if (c) {                   \
            DEBUGLOG(3, "Error: " #c); \
            RETURN_ERROR(e);       \
        }                          \
    } while (0)

/* note : @r is evaluated only once, so it can be an expression with side effects */
#define FORWARD_IF_ERROR(r) do {                     \
        size_t const err_code = (r);                 \
        if (LZ4F_isError(err_code)) return err_code; \
    } while (0)

/* a failed operation invalidates the frame, and discards its jobs (multi-threaded mode) :
 * next operations fail with compressionState_uninitialized, until LZ4F_compressBegin().
 * Idempotent : also employed to forward errors of functions which already invalidated the frame */
static void LZ4F_invalidateFrame(LZ4F_cctx_t* cctx)
{
    if (cctx->mtActive) LZ4F_mt_abort(cctx->mt);
    cctx->cStage = 0;
}

#define FORWARD_IF_ERROR_INVALIDATE(r, cctx) do { \
        size_t const err_code = (r);               \
        if (LZ4F_isError(err_code)) {              \
            LZ4F_invalidateFrame(cctx);            \
            return err_code;                       \
        }                                          \
    } while (0)

unsigned LZ4F_getVersion(void) { return LZ4F_VERSION; }

int LZ4F_compressionLevel_max(void) { return LZ4HC_CLEVEL_MAX; }

size_t LZ4F_getBlockSize(LZ4F_blockSizeID_t blockSizeID)
{
    static const size_t blockSizes[4] = { 64 KB, 256 KB, 1 MB, 4 MB };

    if (blockSizeID == 0) blockSizeID = LZ4F_BLOCKSIZEID_DEFAULT;
    if (blockSizeID < LZ4F_max64KB || blockSizeID > LZ4F_max4MB)
        RETURN_ERROR(maxBlockSize_invalid);
    {   int const blockSizeIdx = (int)blockSizeID - (int)LZ4F_max64KB;
        return blockSizes[blockSizeIdx];
}   }

/*-************************************
*  Private functions
**************************************/
#undef MIN
#define MIN(a,b)   ( (a) < (b) ? (a) : (b) )

static BYTE LZ4F_headerChecksum (const void* header, size_t length)
{
    U32 const xxh = XXH32(header, length, 0);
    return (BYTE)(xxh >> 8);
}


/*-************************************
*  Simple-pass compression functions
**************************************/
static LZ4F_blockSizeID_t LZ4F_optimalBSID(const LZ4F_blockSizeID_t requestedBSID,
                                           const size_t srcSize)
{
    LZ4F_blockSizeID_t proposedBSID = LZ4F_max64KB;
    size_t maxBlockSize = 64 KB;
    while (requestedBSID > proposedBSID) {
        if (srcSize <= maxBlockSize)
            return proposedBSID;
        proposedBSID = (LZ4F_blockSizeID_t)((int)proposedBSID + 1);
        maxBlockSize <<= 2;
    }
    return requestedBSID;
}

/*! LZ4F_compressBound_autoFlush() :
 *  worst case : @srcSize as blocks stored uncompressed, the last one partial, then the frame footer.
 *  Also bounds one-shot compression (LZ4F_compressFrameBound()), which compresses all its input at once.
 *  When srcSize==0, it bounds LZ4F_flush() and LZ4F_compressEnd() operations.
 */
static size_t LZ4F_compressBound_autoFlush(size_t srcSize, const LZ4F_preferences_t* prefsPtr)
{
    size_t const blockSize = LZ4F_getBlockSize(prefsPtr->frameInfo.blockSizeID);
    size_t const nbFullBlocks = srcSize / blockSize;
    size_t const lastBlockSize = srcSize & (blockSize-1);
    size_t const nbBlocks = nbFullBlocks + (lastBlockSize>0);
    size_t const blockCRCSize = BFSize * prefsPtr->frameInfo.blockChecksumFlag;
    size_t const frameEnd = BHSize + (prefsPtr->frameInfo.contentChecksumFlag*BFSize);
    assert(prefsPtr->autoFlush);
    return ((BHSize + blockCRCSize) * nbBlocks) +
           (blockSize * nbFullBlocks) + lastBlockSize + frameEnd;
}

size_t LZ4F_compressFrameBound(size_t srcSize, const LZ4F_preferences_t* preferencesPtr)
{
    LZ4F_preferences_t prefs;
    size_t const headerSize = maxFHSize;      /* max header size, including optional fields */

    if (preferencesPtr!=NULL) prefs = *preferencesPtr;
    else MEM_INIT(&prefs, 0, sizeof(prefs));
    prefs.autoFlush = 1;

    return headerSize + LZ4F_compressBound_autoFlush(srcSize, &prefs);
}


/*-************************************
*  Multi-threading parameters
**************************************/
/* see LZ4F_preferences_t.nbWorkers */
/* sanity limit (ex: uninitialized preferences) ; on 32-bit systems, keeps LZ4F_compressBound() within address space */
#ifndef LZ4F_NBWORKERS_MAX
#  define LZ4F_NBWORKERS_MAX ((sizeof(size_t) >= 8) ? 1024u : 128u)
#endif
#define LZ4F_MT_PREFIX_SIZE (64 KB)   /* history employed by linked blocks */

/* Job size :
 * Input is cut into jobs at fixed positions of the frame's content (restarting after each flush) :
 * a whole nb of blocks, up to LZ4F_MT_JOBSIZE, or a single block when larger.
 * With linked blocks, each job restarts the stream, using previous 64 KB as dictionary.
 * Job boundaries are therefore part of the output definition : this size must remain frozen.
 * With independent blocks, output only depends on block boundaries, not on job boundaries.
 * Small jobs balance load among threads, and engage multi-threading on smaller inputs. */
#ifndef LZ4F_MT_JOBSIZE   /* override for testing only : changes output of linked blocks */
#  define LZ4F_MT_JOBSIZE (1 MB)
#endif

static size_t LZ4F_mtJobSize(size_t blockSize)
{
    if (blockSize >= LZ4F_MT_JOBSIZE) return blockSize;
    return (LZ4F_MT_JOBSIZE / blockSize) * blockSize;   /* whole nb of blocks */
}

unsigned LZ4F_getMaxNbWorkers(void)
{
#if LZ4F_MULTITHREAD
    return LZ4F_NBWORKERS_MAX;
#else
    return 0;
#endif
}

/* frames are cut into jobs only with valid block modes, see LZ4F_mt_begin() */
static int LZ4F_mtEligible(const LZ4F_preferences_t* prefs)
{
    if (prefs->nbWorkers == 0) return 0;
    /* linked blocks : jobs define the output, so frames are cut into jobs even without threads */
    if (prefs->frameInfo.blockMode == LZ4F_blockLinked) return 1;
#if LZ4F_MULTITHREAD
    return (prefs->frameInfo.blockMode == LZ4F_blockIndependent);
#else
    /* independent blocks : the output is identical to single-threaded mode, which is faster without threads */
    return 0;
#endif
}

/* nb of job slots for @nbThreads worker threads :
 * one being filled, one per worker, plus room for jobs completed out of order,
 * since their outputs are emitted in order */
#ifndef LZ4F_MT_SLOTS_PER_THREAD
#  define LZ4F_MT_SLOTS_PER_THREAD 2
#endif
#define LZ4F_MT_NBSLOTS(nbThreads) ((size_t)(nbThreads) * LZ4F_MT_SLOTS_PER_THREAD + 1)
#ifndef LZ4F_MT_EXTRA_INBUFFS
#  define LZ4F_MT_EXTRA_INBUFFS 1
#endif


/*! LZ4F_blocksBound() :
 * @return worst case size of LZ4F_compressBlocks_internal() output */
static size_t LZ4F_blocksBound(size_t srcSize, size_t blockSize, LZ4F_blockChecksum_t crcFlag)
{
    size_t const nbBlocks = (srcSize + blockSize - 1) / blockSize;
    return srcSize + nbBlocks * (BHSize + (size_t)crcFlag * BFSize);
}

/*! LZ4F_mtCompressBound() :
 *  LZ4F_compressBound() for frames cut into jobs, without autoFlush : the job is the unit, instead of the block.
 *  LZ4F_compressUpdate() defers completed jobs not fitting into dstBuffer :
 *  output is only required to start a job while all slots are busy (the oldest one),
 *  and an invocation starts at most ceil(srcSize / jobSize) jobs, since the job being filled holds < jobSize bytes.
 *  @srcSize == 0 : LZ4F_flush() and LZ4F_compressEnd() emit at least one whole job per invocation, then the frame footer.
 *  The frame footer is always included, so that the bound grows with @srcSize. */
static size_t LZ4F_mtCompressBound(size_t srcSize, const LZ4F_preferences_t* prefs)
{
    size_t const bs = LZ4F_getBlockSize(prefs->frameInfo.blockSizeID);
    size_t const blockSize = LZ4F_isError(bs) ? 4 MB : bs;
    size_t const jobSize = LZ4F_mtJobSize(blockSize);
    size_t const nbJobs = srcSize ? (srcSize / jobSize) + ((srcSize % jobSize) != 0) : 1;
    size_t const frameEnd = BHSize + (prefs->frameInfo.contentChecksumFlag*BFSize);
    return nbJobs * LZ4F_blocksBound(jobSize, blockSize, prefs->frameInfo.blockChecksumFlag) + frameEnd;
}

/*! LZ4F_compressFrame_usingCDict() :
 *  Compress srcBuffer using a dictionary, in a single step.
 *  cdict can be NULL, in which case, no dictionary is used.
 *  Success is guaranteed when dstCapacity >= LZ4F_compressFrameBound(srcSize, preferencesPtr).
 *  Otherwise, compression fails with dstCapacity_tooSmall if the frame doesn't fit.
 *  The LZ4F_preferences_t structure is optional : you may provide NULL as argument,
 *  however, it's the only way to provide a dictID, so it's not recommended.
 * @return : number of bytes written into dstBuffer,
 *           or an error code if it fails (can be tested using LZ4F_isError())
 */
size_t LZ4F_compressFrame_usingCDict(LZ4F_cctx* cctx,
                                     void* dstBuffer, size_t dstCapacity,
                               const void* srcBuffer, size_t srcSize,
                               const LZ4F_CDict* cdict,
                               const LZ4F_preferences_t* preferencesPtr)
{
    LZ4F_preferences_t prefs;
    LZ4F_compressOptions_t options;
    BYTE* const dstStart = (BYTE*) dstBuffer;
    BYTE* dstPtr = dstStart;
    BYTE* const dstEnd = dstStart + dstCapacity;

    DEBUGLOG(4, "LZ4F_compressFrame_usingCDict (srcSize=%u)", (unsigned)srcSize);
    if (preferencesPtr!=NULL)
        prefs = *preferencesPtr;
    else
        MEM_INIT(&prefs, 0, sizeof(prefs));
    if (prefs.frameInfo.contentSize != 0)
        prefs.frameInfo.contentSize = (U64)srcSize;   /* auto-correct content size if selected (!=0) */

    prefs.frameInfo.blockSizeID = LZ4F_optimalBSID(prefs.frameInfo.blockSizeID, srcSize);
    prefs.autoFlush = 1;
    if (srcSize <= LZ4F_getBlockSize(prefs.frameInfo.blockSizeID))
        prefs.frameInfo.blockMode = LZ4F_blockIndependent;   /* only one block => no need for inter-block link */

    if (LZ4F_mtEligible(&prefs)) {
        /* a single job produces the same output as single-threaded compression, without the setup cost */
        size_t const blockSize = LZ4F_getBlockSize(prefs.frameInfo.blockSizeID);
        if (LZ4F_isError(blockSize) || srcSize <= LZ4F_mtJobSize(blockSize))
            prefs.nbWorkers = 0;
    }

    MEM_INIT(&options, 0, sizeof(options));
    options.stableSrc = LZ4F_srcStableUntilFlush;   /* src remains until LZ4F_compressEnd() below */

    { size_t const headerSize = LZ4F_compressBegin_usingCDict(cctx, dstBuffer, dstCapacity, cdict, &prefs);  /* write header */
      FORWARD_IF_ERROR(headerSize);
      dstPtr += headerSize;   /* header size */ }

    assert(dstEnd >= dstPtr);
    { size_t const cSize = LZ4F_compressUpdate(cctx, dstPtr, (size_t)(dstEnd-dstPtr), srcBuffer, srcSize, &options);
      FORWARD_IF_ERROR(cSize);
      dstPtr += cSize; }

    assert(dstEnd >= dstPtr);
    { size_t const tailSize = LZ4F_compressEnd(cctx, dstPtr, (size_t)(dstEnd-dstPtr), &options);   /* flush last block, and generate suffix */
      int const completed = !LZ4F_isError(tailSize) && ((cctx->cStage == 0) || (cctx->flushStage == 3));
      if (!completed) {
          /* the frame doesn't fit */
          LZ4F_invalidateFrame(cctx);
          return LZ4F_isError(tailSize) ? tailSize : LZ4F_returnErrorCode(LZ4F_ERROR_dstCapacity_tooSmall);
      }
      cctx->cStage = 0;
      cctx->flushStage = 0;
      dstPtr += tailSize; }

    assert(dstEnd >= dstStart);
    return (size_t)(dstPtr - dstStart);
}


/*! LZ4F_compressFrame() :
 *  Compress an entire srcBuffer into a valid LZ4 frame, in a single step.
 *  Success is guaranteed when dstCapacity >= LZ4F_compressFrameBound(srcSize, preferencesPtr).
 *  Otherwise, compression fails with dstCapacity_tooSmall if the frame doesn't fit.
 *  The LZ4F_preferences_t structure is optional : you can provide NULL as argument. All preferences will be set to default.
 * @return : number of bytes written into dstBuffer.
 *           or an error code if it fails (can be tested using LZ4F_isError())
 */
size_t LZ4F_compressFrame(void* dstBuffer, size_t dstCapacity,
                    const void* srcBuffer, size_t srcSize,
                    const LZ4F_preferences_t* preferencesPtr)
{
    size_t result;
#if (LZ4F_HEAPMODE)
    LZ4F_cctx_t* cctxPtr;
    result = LZ4F_createCompressionContext(&cctxPtr, LZ4F_VERSION);
    FORWARD_IF_ERROR(result);
#else
    LZ4F_cctx_t cctx;
    LZ4_stream_t lz4ctx;
    LZ4F_cctx_t* const cctxPtr = &cctx;

    MEM_INIT(&cctx, 0, sizeof(cctx));
    cctx.version = LZ4F_VERSION;
    cctx.tmpBuffCapacity = 5 MB;   /* mess with real buffer size to prevent dynamic allocation; works only because autoflush==1 & stableSrc */
    if ( preferencesPtr == NULL
      || preferencesPtr->compressionLevel < LZ4HC_CLEVEL_MIN ) {
        LZ4_initStream(&lz4ctx, sizeof(lz4ctx));
        cctxPtr->lz4CtxPtr = &lz4ctx;
        cctxPtr->lz4CtxAlloc = ctxFast;
        cctxPtr->lz4CtxType = ctxFast;
    }
#endif
    DEBUGLOG(4, "LZ4F_compressFrame");

    result = LZ4F_compressFrame_usingCDict(cctxPtr, dstBuffer, dstCapacity,
                                           srcBuffer, srcSize,
                                           NULL, preferencesPtr);

#if (LZ4F_HEAPMODE)
    LZ4F_freeCompressionContext(cctxPtr);
#else
    if ( preferencesPtr != NULL
      && preferencesPtr->compressionLevel >= LZ4HC_CLEVEL_MIN ) {
        LZ4F_free(cctxPtr->lz4CtxPtr, cctxPtr->cmem);
    }
    LZ4F_mt_free(cctxPtr->mt, cctxPtr->cmem);
    LZ4F_free(cctxPtr->stageBuff, cctxPtr->cmem);
#endif
    return result;
}


/*-***************************************************
*   Dictionary compression
*****************************************************/

LZ4F_CDict*
LZ4F_createCDict_advanced(LZ4F_CustomMem cmem, const void* dictBuffer, size_t dictSize)
{
    const char* dictStart = (const char*)dictBuffer;
    LZ4F_CDict* cdict = NULL;

    DEBUGLOG(4, "LZ4F_createCDict_advanced");

    if (!dictStart)
        return NULL;
    cdict = (LZ4F_CDict*)LZ4F_malloc(sizeof(*cdict), cmem);
    if (!cdict)
        return NULL;

    cdict->cmem = cmem;
    if (dictSize > 64 KB) {
        dictStart += dictSize - 64 KB;
        dictSize = 64 KB;
    }
    cdict->dictContent = LZ4F_malloc(dictSize, cmem);
    /* note: using @cmem to allocate => can't use default create */
    cdict->fastCtx = (LZ4_stream_t*)LZ4F_malloc(sizeof(LZ4_stream_t), cmem);
    cdict->HCCtx = (LZ4_streamHC_t*)LZ4F_malloc(sizeof(LZ4_streamHC_t), cmem);
    if (!cdict->dictContent || !cdict->fastCtx || !cdict->HCCtx) {
        LZ4F_freeCDict(cdict);
        return NULL;
    }
    memcpy(cdict->dictContent, dictStart, dictSize);
    LZ4_initStream(cdict->fastCtx, sizeof(LZ4_stream_t));
    LZ4_loadDictSlow(cdict->fastCtx, (const char*)cdict->dictContent, (int)dictSize);
    LZ4_initStreamHC(cdict->HCCtx, sizeof(LZ4_streamHC_t));
    /* note: we don't know at this point which compression level is going to be used
     * as a consequence, HCCtx is created for the more common HC mode */
    LZ4_setCompressionLevel(cdict->HCCtx, LZ4HC_CLEVEL_DEFAULT);
    LZ4_loadDictHC(cdict->HCCtx, (const char*)cdict->dictContent, (int)dictSize);
    return cdict;
}

/*! LZ4F_createCDict() :
 *  When compressing multiple messages / blocks with the same dictionary, it's recommended to load it just once.
 *  LZ4F_createCDict() will create a digested dictionary, ready to start future compression operations without startup delay.
 *  LZ4F_CDict can be created once and shared by multiple threads concurrently, since its usage is read-only.
 * @dictBuffer can be released after LZ4F_CDict creation, since its content is copied within CDict
 * @return : digested dictionary for compression, or NULL if failed */
LZ4F_CDict* LZ4F_createCDict(const void* dictBuffer, size_t dictSize)
{
    DEBUGLOG(4, "LZ4F_createCDict");
    return LZ4F_createCDict_advanced(LZ4F_defaultCMem, dictBuffer, dictSize);
}

void LZ4F_freeCDict(LZ4F_CDict* cdict)
{
    if (cdict==NULL) return;  /* support free on NULL */
    LZ4F_free(cdict->dictContent, cdict->cmem);
    LZ4F_free(cdict->fastCtx, cdict->cmem);
    LZ4F_free(cdict->HCCtx, cdict->cmem);
    LZ4F_free(cdict, cdict->cmem);
}


/*-*********************************
*  Advanced compression functions
***********************************/

LZ4F_cctx*
LZ4F_createCompressionContext_advanced(LZ4F_CustomMem customMem, unsigned version)
{
    LZ4F_cctx* const cctxPtr =
        (LZ4F_cctx*)LZ4F_calloc(sizeof(LZ4F_cctx), customMem);
    if (cctxPtr==NULL) return NULL;

    cctxPtr->cmem = customMem;
    cctxPtr->version = version;
    cctxPtr->cStage = 0;   /* Uninitialized. Next stage : init cctx */

    return cctxPtr;
}

/*! LZ4F_createCompressionContext() :
 *  The first thing to do is to create a compressionContext object, which will be used in all compression operations.
 *  This is achieved using LZ4F_createCompressionContext(), which takes as argument a version and an LZ4F_preferences_t structure.
 *  The version provided MUST be LZ4F_VERSION. It is intended to track potential incompatible differences between different binaries.
 *  The function will provide a pointer to an allocated LZ4F_compressionContext_t object.
 *  If the result LZ4F_errorCode_t is not OK_NoError, there was an error during context creation.
 *  Object can release its memory using LZ4F_freeCompressionContext();
**/
LZ4F_errorCode_t
LZ4F_createCompressionContext(LZ4F_cctx** LZ4F_compressionContextPtr, unsigned version)
{
    assert(LZ4F_compressionContextPtr != NULL); /* considered a violation of narrow contract */
    /* in case it nonetheless happen in production */
    RETURN_ERROR_IF(LZ4F_compressionContextPtr == NULL, parameter_null);

    *LZ4F_compressionContextPtr = LZ4F_createCompressionContext_advanced(LZ4F_defaultCMem, version);
    RETURN_ERROR_IF(*LZ4F_compressionContextPtr==NULL, allocation_failed);
    return LZ4F_OK_NoError;
}

LZ4F_errorCode_t LZ4F_freeCompressionContext(LZ4F_cctx* cctxPtr)
{
    if (cctxPtr != NULL) {  /* support free on NULL */
       LZ4F_mt_free(cctxPtr->mt, cctxPtr->cmem);   /* first : waits for jobs still in flight */
       LZ4F_free(cctxPtr->lz4CtxPtr, cctxPtr->cmem);  /* note: LZ4_streamHC_t and LZ4_stream_t are simple POD types */
       LZ4F_free(cctxPtr->tmpBuff, cctxPtr->cmem);
       LZ4F_free(cctxPtr->stageBuff, cctxPtr->cmem);
       LZ4F_free(cctxPtr->rawDictFast, cctxPtr->cmem);
       LZ4F_free(cctxPtr->rawDictHC, cctxPtr->cmem);
       LZ4F_free(cctxPtr, cctxPtr->cmem);
    }
    return LZ4F_OK_NoError;
}


/**
 * This function prepares the internal LZ4(HC) stream for a new compression,
 * resetting the context and attaching the dictionary, if there is one.
 *
 * It needs to be called at the beginning of each independent compression
 * stream (i.e., at the beginning of a frame in blockLinked mode, or at the
 * beginning of each block in blockIndependent mode).
 */
static void LZ4F_initStream(void* ctx,
                            const LZ4F_CDict* cdict,
                            int level,
                            LZ4F_blockMode_t blockMode) {
    if (level < LZ4HC_CLEVEL_MIN) {
        /* In these cases, we will call LZ4_compress_fast_continue(),
         * which needs an already reset context. Otherwise, we'll call a
         * one-shot API. The non-continued APIs internally perform their own
         * resets at the beginning of their calls, where they know what
         * tableType they need the context to be in. So in that case this
         * would be misguided / wasted work. */
        if (cdict) {
            LZ4_resetStream_fast((LZ4_stream_t*)ctx);
            LZ4_attach_dictionary((LZ4_stream_t*)ctx, cdict->fastCtx);
        } else if (blockMode == LZ4F_blockLinked) {
            /* Clear the table, rather than LZ4_resetStream_fast() :
             * output doesn't depend on the context's history,
             * and it's faster, unless the frame is tiny. */
            LZ4_initStream(ctx, sizeof(LZ4_stream_t));
        }
        /* In these cases, we'll call a one-shot API.
         * The non-continued APIs internally perform their own resets
         * at the beginning of their calls, where they know
         * which tableType they need the context to be in.
         * Therefore, a reset here would be wasted work. */
    } else {
        LZ4_resetStreamHC_fast((LZ4_streamHC_t*)ctx, level);
        if (cdict)
            LZ4_attach_HC_dictionary((LZ4_streamHC_t*)ctx, cdict->HCCtx);
    }
}

/* LZ4F_startStream() :
 * starts the LZ4 stream @ctx at the compression level of @prefs : for a frame, a job, or after uncompressed blocks.
 * With linked blocks, the stream starts from dictionary @cdict, or from history @dict (@dictSize bytes) ;
 * independent blocks start their own stream (see LZ4F_compressBlock()). */
static void LZ4F_startStream(void* ctx, const LZ4F_preferences_t* prefs,
                             const LZ4F_CDict* cdict, const void* dict, size_t dictSize)
{
    int const level = prefs->compressionLevel;
    if (prefs->frameInfo.blockMode == LZ4F_blockLinked)
        LZ4F_initStream(ctx, cdict, level, LZ4F_blockLinked);
    if (level >= LZ4HC_CLEVEL_MIN)
        LZ4_favorDecompressionSpeed((LZ4_streamHC_t*)ctx, (int)prefs->favorDecSpeed);
    if (dict) {
        assert(cdict == NULL);
        assert(prefs->frameInfo.blockMode == LZ4F_blockLinked);
        assert(dictSize <= INT_MAX);
        if (level < LZ4HC_CLEVEL_MIN) {
            LZ4_loadDict((LZ4_stream_t*)ctx, (const char*)dict, (int)dictSize);
        } else {
            LZ4_loadDictHC((LZ4_streamHC_t*)ctx, (const char*)dict, (int)dictSize);
        }
    }
}

/* LZ4F_sizeofLZ4State() :
 * @return the size of the inner LZ4 state (pointed by lz4CtxPtr) for @ctxType :
 *  LZ4_stream_t for fast levels, LZ4_streamHC_t for HC levels.
 *  ctxNone means no state allocated, hence a size of 0.
 *  Note : the larger of fast and HC states depends on LZ4_MEMORY_USAGE,
 *         so reusing an allocated state requires comparing sizes. */
static size_t LZ4F_sizeofLZ4State(LZ4F_CtxType_e ctxType)
{
    switch (ctxType) {
    case ctxNone: return 0;
    case ctxFast: return sizeof(LZ4_stream_t);
    case ctxHC:   return LZ4_STREAMHC_EXTSIZE;   /* extended : levels >= 10 allocate nothing */
    }
    assert(0);   /* invalid ctxType */
    return 0;
}

/* LZ4F_initLZ4State() :
 * initializes the inner LZ4 state @state for @ctxType (fast or HC), at compression @level */
static void LZ4F_initLZ4State(void* state, LZ4F_CtxType_e ctxType, int level)
{
    if (ctxType == ctxFast) {
        LZ4_initStream(state, sizeof(LZ4_stream_t));
    } else {
        assert(ctxType == ctxHC);
        LZ4_initStreamHC(state, LZ4_STREAMHC_EXTSIZE);
        LZ4_setCompressionLevel((LZ4_streamHC_t*)state, level);
    }
}

size_t LZ4F_cctx_size(const LZ4F_cctx* cctx) {
    if (cctx == NULL) {
        return 0;
    }
    return sizeof(*cctx) + cctx->tmpBuffCapacity + cctx->stageCapacity + LZ4F_sizeofLZ4State((LZ4F_CtxType_e)cctx->lz4CtxAlloc)
         + (cctx->rawDictFast ? sizeof(LZ4_stream_t) : 0) + (cctx->rawDictHC ? sizeof(LZ4_streamHC_t) : 0)
         + LZ4F_mt_sizeof(cctx->mt);
}

/* LZ4F_digestRawDict() :
 * digests @dictBuffer within states owned by @cctx, without copying it (it must outlive the compression session),
 * for the compression level of current frame, which then employs it as a CDict.
 * Since it's done for each frame, fast levels employ LZ4_loadDict(), faster than LZ4F_createCDict()'s LZ4_loadDictSlow().
 * HC levels digest it for the frame's level : level 2 (lz4mid) has its own format,
 * which a CDict (digested at default level) lacks.
 * @return : CDict, or NULL if allocation failed */
static const LZ4F_CDict* LZ4F_digestRawDict(LZ4F_cctx_t* cctx, const void* dictBuffer, size_t dictSize)
{
    const char* dictStart = (const char*)dictBuffer;
    if (dictSize > 64 KB) {
        dictStart += dictSize - 64 KB;
        dictSize = 64 KB;
    }
    cctx->rawDict.cmem = cctx->cmem;
    cctx->rawDict.dictContent = NULL;
    cctx->rawDict.fastCtx = NULL;
    cctx->rawDict.HCCtx = NULL;
    if (cctx->prefs.compressionLevel < LZ4HC_CLEVEL_MIN) {
        if (cctx->rawDictFast == NULL) cctx->rawDictFast = (LZ4_stream_t*)LZ4F_malloc(sizeof(LZ4_stream_t), cctx->cmem);
        if (cctx->rawDictFast == NULL) return NULL;
        LZ4_initStream(cctx->rawDictFast, sizeof(LZ4_stream_t));
        LZ4_loadDict(cctx->rawDictFast, dictStart, (int)dictSize);
        cctx->rawDict.fastCtx = cctx->rawDictFast;
    } else {
        if (cctx->rawDictHC == NULL) cctx->rawDictHC = (LZ4_streamHC_t*)LZ4F_malloc(sizeof(LZ4_streamHC_t), cctx->cmem);
        if (cctx->rawDictHC == NULL) return NULL;
        LZ4_initStreamHC(cctx->rawDictHC, sizeof(LZ4_streamHC_t));
        LZ4_setCompressionLevel(cctx->rawDictHC, cctx->prefs.compressionLevel);
        LZ4_loadDictHC(cctx->rawDictHC, dictStart, (int)dictSize);
        cctx->rawDict.HCCtx = cctx->rawDictHC;
    }
    return &cctx->rawDict;
}

/* LZ4F_compressBegin_internal()
 * Note: only accepts @cdict _or_ @dictBuffer as non NULL.
 */
static size_t LZ4F_compressBegin_internal(LZ4F_cctx* cctx,
                          void* dstBuffer, size_t dstCapacity,
                          const void* dictBuffer, size_t dictSize,
                          const LZ4F_CDict* cdict,
                          const LZ4F_preferences_t* preferencesPtr)
{
    LZ4F_preferences_t const prefNull = LZ4F_INIT_PREFERENCES;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* dstPtr = dstStart;
    int mtFrame;   /* frame cut into jobs (multi-threaded compression) */

    if (preferencesPtr == NULL) preferencesPtr = &prefNull;
    /* validate arguments before modifying @cctx : when they are rejected, a frame in progress continues */
    {   size_t const headerSize = minFHSize
                                + (preferencesPtr->frameInfo.contentSize ? 8 : 0)
                                + (preferencesPtr->frameInfo.dictID ? 4 : 0);
        RETURN_ERROR_IF(dstCapacity < headerSize, dstCapacity_tooSmall);
    }
    FORWARD_IF_ERROR( LZ4F_getBlockSize(preferencesPtr->frameInfo.blockSizeID) );
    RETURN_ERROR_IF(dictSize > INT_MAX, parameter_invalid);
    /* from now on, a failure (allocation) leaves no frame in progress */
    LZ4F_invalidateFrame(cctx);   /* jobs of an abandoned frame : must complete before @cctx is modified */
    cctx->mtActive = 0;
    cctx->flushStage = 0;
    cctx->pendingSize = 0;
    cctx->prefs = *preferencesPtr;
    mtFrame = LZ4F_mtEligible(&cctx->prefs);
    DEBUGLOG(5, "LZ4F_compressBegin_internal: Independent_blocks=%u", cctx->prefs.frameInfo.blockMode);

    /* cctx Management */
    {   LZ4F_CtxType_e const ctxType = (cctx->prefs.compressionLevel < LZ4HC_CLEVEL_MIN) ? ctxFast : ctxHC;
        size_t const requiredSize = LZ4F_sizeofLZ4State(ctxType);
        size_t const allocatedSize = LZ4F_sizeofLZ4State((LZ4F_CtxType_e)cctx->lz4CtxAlloc);
        if (allocatedSize < requiredSize) {
            /* not enough space allocated : must take ownership of memory allocation,
             * in order to respect custom allocator contract */
            LZ4F_free(cctx->lz4CtxPtr, cctx->cmem);
            cctx->lz4CtxAlloc = (U16)ctxNone;   /* in case allocation fails */
            cctx->lz4CtxPtr = LZ4F_malloc(requiredSize, cctx->cmem);
            RETURN_ERROR_IF(cctx->lz4CtxPtr == NULL, allocation_failed);
            cctx->lz4CtxAlloc = (U16)ctxType;
            cctx->lz4CtxType = (U16)ctxNone;    /* not initialized yet */
        }
        if (cctx->lz4CtxType != ctxType) {
            /* initialize the state for the correct context type */
            LZ4F_initLZ4State(cctx->lz4CtxPtr, ctxType, cctx->prefs.compressionLevel);
            cctx->lz4CtxType = (U16)ctxType;
    }   }

    /* raw dictionary : employed by each block as a CDict (independent blocks, or jobs of multi-threaded frames) ;
     * single-threaded linked blocks keep the historical behavior : dictionary loaded as history, preceding the first block,
     * from a copy within @tmpBuff (see below) */
    if ( dictBuffer
      && ((cctx->prefs.frameInfo.blockMode == LZ4F_blockIndependent) || mtFrame) ) {
        assert(cdict == NULL);
        cdict = LZ4F_digestRawDict(cctx, dictBuffer, dictSize);
        RETURN_ERROR_IF(cdict == NULL, allocation_failed);
        dictBuffer = NULL;
        dictSize = 0;
    }

    /* Buffer Management */
    if (cctx->prefs.frameInfo.blockSizeID == 0)
        cctx->prefs.frameInfo.blockSizeID = LZ4F_BLOCKSIZEID_DEFAULT;
    cctx->maxBlockSize = LZ4F_getBlockSize(cctx->prefs.frameInfo.blockSizeID);

    {   size_t const requiredBuffSize = mtFrame ? 0 :   /* jobs employ their own buffers */
                preferencesPtr->autoFlush ?
                ((cctx->prefs.frameInfo.blockMode == LZ4F_blockLinked) ? 128 KB : 0) :  /* history, then inputs < 64 KB (see LZ4F_st_update()) */
                cctx->maxBlockSize + ((cctx->prefs.frameInfo.blockMode == LZ4F_blockLinked) ? 128 KB : 0);

        if (cctx->tmpBuffCapacity < requiredBuffSize) {
            cctx->tmpBuffCapacity = 0;
            LZ4F_free(cctx->tmpBuff, cctx->cmem);
            cctx->tmpBuff = (BYTE*)LZ4F_malloc(requiredBuffSize, cctx->cmem);
            RETURN_ERROR_IF(cctx->tmpBuff == NULL, allocation_failed);
            cctx->tmpBuffCapacity = requiredBuffSize;
    }   }
    cctx->tmpIn = cctx->tmpBuff;
    cctx->tmpInSize = 0;
    if (dictBuffer) {
        /* single-threaded linked blocks : the dictionary is copied into @tmpBuff, as saved history :
         * buffered input follows it, and references it in prefix mode
         * (faster than an external dictionary, which fast levels also lose after the first block) */
        if (dictSize > 64 KB) {
            dictBuffer = (const BYTE*)dictBuffer + dictSize - 64 KB;
            dictSize = 64 KB;
        }
        assert(cctx->tmpBuffCapacity >= 128 KB);
        memcpy(cctx->tmpBuff, dictBuffer, dictSize);
        dictBuffer = cctx->tmpBuff;
        cctx->tmpIn = cctx->tmpBuff + dictSize;
    }
    (void)XXH32_reset(&(cctx->xxh), 0);

    /* context init */
    cctx->cdict = cdict;
    cctx->blockCompressMode = LZ4B_COMPRESSED;   /* a frame starts with the LZ4 state just initialized (dictionary included) */
    LZ4F_startStream(cctx->lz4CtxPtr, &cctx->prefs, cdict, dictBuffer, dictSize);

    /* Stage 2 : Write Frame Header */

    /* Magic Number */
    LZ4F_writeLE32(dstPtr, LZ4F_MAGICNUMBER);
    dstPtr += 4;
    {   BYTE* const headerStart = dstPtr;

        /* FLG Byte */
        *dstPtr++ = (BYTE)(((1 & _2BITS) << 6)    /* Version('01') */
            + ((cctx->prefs.frameInfo.blockMode & _1BIT ) << 5)
            + ((cctx->prefs.frameInfo.blockChecksumFlag & _1BIT ) << 4)
            + ((unsigned)(cctx->prefs.frameInfo.contentSize > 0) << 3)
            + ((cctx->prefs.frameInfo.contentChecksumFlag & _1BIT ) << 2)
            +  (cctx->prefs.frameInfo.dictID > 0) );
        /* BD Byte */
        *dstPtr++ = (BYTE)((cctx->prefs.frameInfo.blockSizeID & _3BITS) << 4);
        /* Optional Frame content size field */
        if (cctx->prefs.frameInfo.contentSize) {
            LZ4F_writeLE64(dstPtr, cctx->prefs.frameInfo.contentSize);
            dstPtr += 8;
            cctx->totalInSize = 0;
        }
        /* Optional dictionary ID field */
        if (cctx->prefs.frameInfo.dictID) {
            LZ4F_writeLE32(dstPtr, cctx->prefs.frameInfo.dictID);
            dstPtr += 4;
        }
        /* Header CRC Byte */
        *dstPtr = LZ4F_headerChecksum(headerStart, (size_t)(dstPtr - headerStart));
        dstPtr++;
    }

    if (mtFrame) {
        assert(dictBuffer == NULL);
        FORWARD_IF_ERROR( LZ4F_mt_begin(cctx) );
    }

    cctx->cStage = 1;   /* header written, now request input data block */
    return (size_t)(dstPtr - dstStart);
}

size_t LZ4F_compressBegin(LZ4F_cctx* cctx,
                          void* dstBuffer, size_t dstCapacity,
                          const LZ4F_preferences_t* preferencesPtr)
{
    return LZ4F_compressBegin_internal(cctx, dstBuffer, dstCapacity,
                                        NULL, 0,
                                        NULL, preferencesPtr);
}

size_t LZ4F_compressBegin_usingDict(LZ4F_cctx* cctx,
                          void* dstBuffer, size_t dstCapacity,
                          const void* dict, size_t dictSize,
                          const LZ4F_preferences_t* preferencesPtr)
{
    return LZ4F_compressBegin_internal(cctx, dstBuffer, dstCapacity,
                                        dict, dictSize,
                                        NULL, preferencesPtr);
}

size_t LZ4F_compressBegin_usingCDict(LZ4F_cctx* cctx,
                          void* dstBuffer, size_t dstCapacity,
                          const LZ4F_CDict* cdict,
                          const LZ4F_preferences_t* preferencesPtr)
{
    return LZ4F_compressBegin_internal(cctx, dstBuffer, dstCapacity,
                                        NULL, 0,
                                       cdict, preferencesPtr);
}


/*  LZ4F_compressBound() :
 * @return minimum capacity of dstBuffer for a given srcSize to handle worst case scenario.
 *  LZ4F_preferences_t structure is optional : if NULL, preferences will be set to cover worst case scenario.
 *  This function cannot fail.
 */
size_t LZ4F_compressBound(size_t srcSize, const LZ4F_preferences_t* preferencesPtr)
{
    size_t bound;
    size_t const blockCRCSize = preferencesPtr
                             ? BFSize * preferencesPtr->frameInfo.blockChecksumFlag
                             : BFSize;
    if (preferencesPtr && !preferencesPtr->autoFlush && LZ4F_mtEligible(preferencesPtr)) {
        /* frame cut into jobs : output can lag behind input */
        return LZ4F_mtCompressBound(srcSize, preferencesPtr);
    }
    if (preferencesPtr && preferencesPtr->autoFlush) {
        /* + 1 : output of LZ4F_flush() and LZ4F_compressEnd() remains strictly below LZ4F_compressBound(0),
         * since a filled dstBuffer means more output may remain */
        return LZ4F_compressBound_autoFlush(srcSize, preferencesPtr) + (srcSize == 0);
    }
    /* single-threaded : full blocks only, each one stored uncompressed in worst case.
     * Up to (blockSize-1) bytes can already be buffered : srcSize completes at most srcSize/blockSize + 1 blocks.
     * The buffered block can also be emitted separately (mode switch), or remain from an incomplete LZ4F_flush() :
     * then, srcSize completes at most srcSize/blockSize blocks.
     * @srcSize == 0 : LZ4F_flush() and LZ4F_compressEnd() emit the buffered block (< blockSize bytes : output
     * remains strictly below the bound, so that a single invocation completes them), then the frame footer.
     * The frame footer is always included, so that the bound grows with @srcSize. */
    {   size_t const bs = LZ4F_getBlockSize(preferencesPtr ? preferencesPtr->frameInfo.blockSizeID : LZ4F_default);
        size_t const blockSize = LZ4F_isError(bs) ? 4 MB : bs;
        size_t const contentCRCSize = preferencesPtr
                                    ? BFSize * preferencesPtr->frameInfo.contentChecksumFlag
                                    : BFSize;
        size_t const nbBlocks = srcSize / blockSize + 1;
        bound = nbBlocks * (blockSize + BHSize + blockCRCSize) + BHSize + contentCRCSize;
    }
    return bound;
}


typedef int (*compressFunc_t)(void* ctx, const char* src, char* dst, int srcSize, int dstSize, int level, const LZ4F_CDict* cdict);


/*! LZ4F_makeBlock():
 *  compress a single block, add header and optional checksum.
 *  The block is stored uncompressed when compression doesn't save space.
 * @return : size of the block written into @dst,
 *           or an error code if it doesn't fit into @dstCapacity.
 *  Note : success is guaranteed when dstCapacity >= BHSize + srcSize + crcSize
 */
static size_t LZ4F_makeBlock(void* dst, size_t dstCapacity,
                       const void* src, size_t srcSize,
                             compressFunc_t compress, void* lz4ctx, int level,
                       const LZ4F_CDict* cdict,
                             LZ4F_blockChecksum_t crcFlag)
{
    BYTE* const cSizePtr = (BYTE*)dst;
    size_t const overhead = BHSize + (size_t)crcFlag * BFSize;
    int cCapacity = (srcSize > 1) ? (int)srcSize - 1 : 1;
    U32 cSize;
    assert(compress != NULL);
    assert(srcSize > 0);
    RETURN_ERROR_IF(dstCapacity <= overhead, dstCapacity_tooSmall);
    if (dstCapacity - overhead < (size_t)cCapacity)
        cCapacity = (int)(dstCapacity - overhead);
    cSize = (U32)compress(lz4ctx, (const char*)src, (char*)(cSizePtr+BHSize),
                          (int)srcSize, cCapacity,
                          level, cdict);

    if (cSize == 0 || cSize >= srcSize) {
        RETURN_ERROR_IF(dstCapacity - overhead < srcSize, dstCapacity_tooSmall);
        cSize = (U32)srcSize;
        LZ4F_writeLE32(cSizePtr, cSize | LZ4F_BLOCKUNCOMPRESSED_FLAG);
        memcpy(cSizePtr+BHSize, src, srcSize);
    } else {
        LZ4F_writeLE32(cSizePtr, cSize);
    }
    if (crcFlag) {
        U32 const crc32 = XXH32(cSizePtr+BHSize, cSize, 0);  /* checksum of compressed data */
        LZ4F_writeLE32(cSizePtr+BHSize+cSize, crc32);
    }
    return BHSize + cSize + ((U32)crcFlag)*BFSize;
}

/*! LZ4F_compressBlocks_internal():
 *  compress @src as a sequence of blocks of @blockSize bytes (the last one can be smaller),
 *  each with its header and optional checksum, but without frame header, endMark nor content checksum.
 *  @lz4ctx must be prepared for @compress :
 *  linked blocks continue the stream, so they need a state initialized at frame start (or with a prefix).
 * @return : nb of bytes written into @dst,
 *           or an error code if it doesn't fit into @dstCapacity.
 *  Note : success is guaranteed when dstCapacity >= srcSize + nbBlocks * (BHSize + crcFlag * BFSize)
 */
static size_t LZ4F_compressBlocks_internal(void* dst, size_t dstCapacity,
                                     const void* src, size_t srcSize, size_t blockSize,
                                           compressFunc_t compress, void* lz4ctx, int level,
                                     const LZ4F_CDict* cdict,
                                           LZ4F_blockChecksum_t crcFlag)
{
    const BYTE* ip = (const BYTE*)src;
    const BYTE* const iend = srcSize ? ip + srcSize : ip;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;
    BYTE* const oend = dstCapacity ? ostart + dstCapacity : ostart;

    while (ip < iend) {
        size_t const bSize = MIN(blockSize, (size_t)(iend - ip));
        size_t const cSize = LZ4F_makeBlock(op, (size_t)(oend - op),
                                            ip, bSize,
                                            compress, lz4ctx, level,
                                            cdict, crcFlag);
        FORWARD_IF_ERROR(cSize);
        ip += bSize;
        op += cSize;
    }
    return (size_t)(op - ostart);
}


/* Negative compression levels select LZ4 "fast" mode, with acceleration = -level + 1.
 * Acceleration is capped at LZ4_ACCELERATION_MAX before this computation,
 * which would overflow for INT_MIN and INT_MIN+1. */
static int LZ4F_levelToAcceleration(int level)
{
    if (level >= 0) return 1;
    if (level < 1 - LZ4_ACCELERATION_MAX) return LZ4_ACCELERATION_MAX;
    return -level + 1;
}

static int LZ4F_compressBlock(void* ctx, const char* src, char* dst, int srcSize, int dstCapacity, int level, const LZ4F_CDict* cdict)
{
    int const acceleration = LZ4F_levelToAcceleration(level);
    DEBUGLOG(5, "LZ4F_compressBlock (srcSize=%i)", srcSize);
    LZ4F_initStream(ctx, cdict, level, LZ4F_blockIndependent);
    if (cdict) {
        return LZ4_compress_fast_continue((LZ4_stream_t*)ctx, src, dst, srcSize, dstCapacity, acceleration);
    } else {
        return LZ4_compress_fast_extState_fastReset(ctx, src, dst, srcSize, dstCapacity, acceleration);
    }
}

static int LZ4F_compressBlock_continue(void* ctx, const char* src, char* dst, int srcSize, int dstCapacity, int level, const LZ4F_CDict* cdict)
{
    int const acceleration = LZ4F_levelToAcceleration(level);
    (void)cdict; /* init once at beginning of frame */
    DEBUGLOG(5, "LZ4F_compressBlock_continue (srcSize=%i)", srcSize);
    return LZ4_compress_fast_continue((LZ4_stream_t*)ctx, src, dst, srcSize, dstCapacity, acceleration);
}

static int LZ4F_compressBlockHC(void* ctx, const char* src, char* dst, int srcSize, int dstCapacity, int level, const LZ4F_CDict* cdict)
{
    LZ4F_initStream(ctx, cdict, level, LZ4F_blockIndependent);
    if (cdict) {
        return LZ4_compress_HC_continue((LZ4_streamHC_t*)ctx, src, dst, srcSize, dstCapacity);
    }
    return LZ4_compress_HC_extStateHC_fastReset(ctx, src, dst, srcSize, dstCapacity, level);
}

static int LZ4F_compressBlockHC_continue(void* ctx, const char* src, char* dst, int srcSize, int dstCapacity, int level, const LZ4F_CDict* cdict)
{
    (void)level; (void)cdict; /* init once at beginning of frame */
    return LZ4_compress_HC_continue((LZ4_streamHC_t*)ctx, src, dst, srcSize, dstCapacity);
}

static int LZ4F_doNotCompressBlock(void* ctx, const char* src, char* dst, int srcSize, int dstCapacity, int level, const LZ4F_CDict* cdict)
{
    (void)ctx; (void)src; (void)dst; (void)srcSize; (void)dstCapacity; (void)level; (void)cdict;
    return 0;
}

static compressFunc_t LZ4F_selectCompression(LZ4F_blockMode_t blockMode, int level, LZ4F_BlockCompressMode_e  compressMode)
{
    if (compressMode == LZ4B_UNCOMPRESSED)
        return LZ4F_doNotCompressBlock;
    if (level < LZ4HC_CLEVEL_MIN) {
        if (blockMode == LZ4F_blockIndependent) return LZ4F_compressBlock;
        return LZ4F_compressBlock_continue;
    }
    if (blockMode == LZ4F_blockIndependent) return LZ4F_compressBlockHC;
    return LZ4F_compressBlockHC_continue;
}

/* Save or shorten history (up to 64KB) into @tmpBuff */
static void LZ4F_localSaveDict(LZ4F_cctx_t* cctxPtr)
{
    int const dictSize = (cctxPtr->prefs.compressionLevel < LZ4HC_CLEVEL_MIN) ?
                    LZ4_saveDict ((LZ4_stream_t*)(cctxPtr->lz4CtxPtr), (char*)(cctxPtr->tmpBuff), 64 KB) :
                    LZ4_saveDictHC ((LZ4_streamHC_t*)(cctxPtr->lz4CtxPtr), (char*)(cctxPtr->tmpBuff), 64 KB);
    cctxPtr->tmpIn = cctxPtr->tmpBuff + dictSize;
}

typedef enum { notDone, fromTmpBuffer, fromSrcBuffer } LZ4F_lastBlockStatus;

static const LZ4F_compressOptions_t k_cOptionsNull = { 0, 0, 0, 0 };

/*-***************************************************
*   Output not fitting into dstBuffer yet (LZ4F_flush(), LZ4F_compressEnd())
*****************************************************/

/* stageBuff holds one block, stored uncompressed in worst case.
 * @return : 0, or an error code (allocation_failed) */
static size_t LZ4F_reserveStage(LZ4F_cctx_t* cctx)
{
    size_t const needed = cctx->maxBlockSize + BHSize + BFSize;
    if (cctx->stageCapacity >= needed) return 0;
    LZ4F_free(cctx->stageBuff, cctx->cmem);
    cctx->stageCapacity = 0;
    cctx->stageBuff = (BYTE*)LZ4F_malloc(needed, cctx->cmem);
    RETURN_ERROR_IF(cctx->stageBuff == NULL, allocation_failed);
    cctx->stageCapacity = needed;
    return 0;
}

/* copies pending output into @dst + @pos, as much as fits.
 * @return : nb of bytes copied */
static size_t LZ4F_copyPending(LZ4F_cctx_t* cctx, BYTE* dst, size_t dstCapacity, size_t pos)
{
    size_t const n = MIN(cctx->pendingSize, dstCapacity - pos);
    assert(pos <= dstCapacity);
    if (n) {
        memcpy(dst + pos, cctx->pending, n);
        cctx->pending += n;
    }
    cctx->pendingSize -= n;
    return n;
}

/* compression operations first emit output left pending by an incomplete LZ4F_flush(),
 * entirely, since they must succeed : the flush ends there.
 * @return : nb of bytes written into @dst, or an error code (callers invalidate the frame) */
static size_t LZ4F_emitPendingWhole(LZ4F_cctx_t* cctx, BYTE* dst, size_t dstCapacity)
{
    cctx->flushStage = 0;
    RETURN_ERROR_IF(cctx->pendingSize > dstCapacity, dstCapacity_tooSmall);
    return LZ4F_copyPending(cctx, dst, dstCapacity, 0);
}

/*! LZ4F_flushBlock() :
 *  compresses input buffered within cctx (single-threaded mode), if any, into @dst.
 * @return : nb of bytes written into @dst, or an error code (the frame is then invalidated) */
static size_t LZ4F_flushBlock(LZ4F_cctx_t* cctxPtr, void* dstBuffer, size_t dstCapacity)
{
    compressFunc_t compress;
    size_t cSize;

    DEBUGLOG(5, "LZ4F_flushBlock: %zu buffered bytes (saved dict size = %i) (dstCapacity=%u)",
            cctxPtr->tmpInSize, (int)(cctxPtr->tmpIn - cctxPtr->tmpBuff), (unsigned)dstCapacity);
    if (cctxPtr->tmpInSize == 0) return 0;   /* nothing to flush */

    /* select compression function */
    compress = LZ4F_selectCompression(cctxPtr->prefs.frameInfo.blockMode, cctxPtr->prefs.compressionLevel, cctxPtr->blockCompressMode);

    /* compress tmp buffer */
    cSize = LZ4F_makeBlock(dstBuffer, dstCapacity,
                           cctxPtr->tmpIn, cctxPtr->tmpInSize,
                           compress, cctxPtr->lz4CtxPtr, cctxPtr->prefs.compressionLevel,
                           cctxPtr->cdict,
                           cctxPtr->prefs.frameInfo.blockChecksumFlag);
    FORWARD_IF_ERROR_INVALIDATE(cSize, cctxPtr);
    assert(((void)"flush overflows dstBuffer!", cSize <= dstCapacity));

    if (cctxPtr->prefs.frameInfo.blockMode == LZ4F_blockLinked)
        cctxPtr->tmpIn += cctxPtr->tmpInSize;
    cctxPtr->tmpInSize = 0;

    /* keep tmpIn within limits */
    if ((cctxPtr->tmpIn + cctxPtr->maxBlockSize) > (cctxPtr->tmpBuff + cctxPtr->tmpBuffCapacity)) {
        assert(cctxPtr->prefs.frameInfo.blockMode == LZ4F_blockLinked);
        LZ4F_localSaveDict(cctxPtr);
    }

    return cSize;
}

/* flush point, single-threaded mode : the buffered block is compressed into @dst when it fits in any case,
 * otherwise into stageBuff, as pending output. Same output : compression capacity is the same.
 * @return : nb of bytes written into @dst, or an error code (the frame is then invalidated) */
static size_t LZ4F_flushBlockPartial(LZ4F_cctx_t* cctx, BYTE* dst, size_t dstCapacity)
{
    size_t const worstCase = cctx->tmpInSize + BHSize + (size_t)cctx->prefs.frameInfo.blockChecksumFlag * BFSize;
    size_t cSize;
    if ((cctx->tmpInSize == 0) || (dstCapacity >= worstCase))
        return LZ4F_flushBlock(cctx, dst, dstCapacity);
    FORWARD_IF_ERROR_INVALIDATE(LZ4F_reserveStage(cctx), cctx);
    cSize = LZ4F_flushBlock(cctx, cctx->stageBuff, cctx->stageCapacity);
    FORWARD_IF_ERROR(cSize);   /* already invalidated */
    cctx->pending = cctx->stageBuff;
    cctx->pendingSize = cSize;
    return 0;
}


/*-***************************************************
*   Multi-threaded compression
*****************************************************/
/* When prefs.nbWorkers >= 1, input is cut into jobs (see LZ4F_mtJobSize()).
 * Jobs are compressed by a pool of worker threads, kept within cctx,
 * and their outputs are emitted in order by the calling thread.
 * Output only depends on content and flush points :
 * never on nb of workers, scheduling, input slicing, nor stableSrc.
 * nbWorkers is the nb of worker threads compressing jobs : the calling thread fills and emits them.
 * At a flush point (LZ4F_flush(), LZ4F_compressEnd(), autoFlush), it waits for all jobs : when no other job is running,
 * it compresses the flushed job itself. So at most nbWorkers threads compress at any time.
 * Without multi-threading support (LZ4F_MULTITHREAD), the calling thread processes jobs alone, producing the same output.
 * If no worker thread can be created, frame compression fails (allocation_failed) ;
 * with fewer threads than requested (ex: system limits), it proceeds with them. */

/*! LZ4F_compressJob() :
 *  compress one job, as a sequence of blocks (stored uncompressed when @mode == LZ4B_UNCOMPRESSED).
 *  With linked blocks, each job restarts the stream, using as dictionary
 *  the @prefixSize bytes of history right before @src,
 *  or @cdict for the first job of the frame (@prefixSize == 0).
 *  Output doesn't depend on @lz4ctx history.
 * @return : nb of bytes written into @dst, or an error code */
static size_t LZ4F_compressJob(void* lz4ctx, const LZ4F_preferences_t* prefs,
                         const LZ4F_CDict* cdict, size_t blockSize,
                               void* dst, size_t dstCapacity,
                         const BYTE* src, size_t srcSize, size_t prefixSize,
                               LZ4F_BlockCompressMode_e mode)
{
    int const level = prefs->compressionLevel;
    LZ4F_blockMode_t const blockMode = prefs->frameInfo.blockMode;

    if (mode == LZ4B_UNCOMPRESSED) {   /* LZ4F_compressOptions_t.uncompressed */
        return LZ4F_compressBlocks_internal(dst, dstCapacity, src, srcSize, blockSize,
                                            LZ4F_doNotCompressBlock, NULL, 0, NULL,
                                            prefs->frameInfo.blockChecksumFlag);
    }

    assert(prefixSize <= LZ4F_MT_PREFIX_SIZE);
    if (prefixSize) {
        LZ4F_startStream(lz4ctx, prefs, NULL, src - prefixSize, prefixSize);
    } else {
        LZ4F_startStream(lz4ctx, prefs, cdict, NULL, 0);
    }

    return LZ4F_compressBlocks_internal(dst, dstCapacity,
                                        src, srcSize, blockSize,
                                        LZ4F_selectCompression(blockMode, level, LZ4B_COMPRESSED),
                                        lz4ctx, level, cdict,
                                        prefs->frameInfo.blockChecksumFlag);
}

typedef void (*LZ4F_job_f)(void* arg, unsigned workerID);
typedef struct LZ4F_pool_s LZ4F_pool_t;

#if LZ4F_MULTITHREAD

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
   /* condition variables require Windows Vista+ ; _WIN32_WINNT may already be defined lower,
    * ex: by <_mingw.h> (included by standard headers) on older mingw-w64 (0x502) */
#  if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0600)
#    if defined(_WINDOWS_)   /* <windows.h> already included (ex: amalgamated sources), without them */
#      error "LZ4F_MULTITHREAD requires Windows Vista API : define _WIN32_WINNT >= 0x0600 before including <windows.h>, or disable LZ4F_MULTITHREAD"
#    endif
#    undef  _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
#  include <windows.h>
   typedef HANDLE             LZ4F_thread_t;
   typedef CRITICAL_SECTION   LZ4F_mutex_t;
   typedef CONDITION_VARIABLE LZ4F_cond_t;
#  define LZ4F_mutex_init(m)     (InitializeCriticalSection(m), 0)
#  define LZ4F_mutex_destroy(m)  DeleteCriticalSection(m)
#  define LZ4F_mutex_lock(m)     EnterCriticalSection(m)
#  define LZ4F_mutex_unlock(m)   LeaveCriticalSection(m)
#  define LZ4F_cond_init(c)      (InitializeConditionVariable(c), 0)
#  define LZ4F_cond_destroy(c)   ((void)(c))
#  define LZ4F_cond_wait(c, m)   SleepConditionVariableCS((c), (m), INFINITE)
#  define LZ4F_cond_signal(c)    WakeConditionVariable(c)
#  define LZ4F_cond_broadcast(c) WakeAllConditionVariable(c)
#else
#  include <pthread.h>
#  include <signal.h>
   typedef pthread_t       LZ4F_thread_t;
   typedef pthread_mutex_t LZ4F_mutex_t;
   typedef pthread_cond_t  LZ4F_cond_t;
#  define LZ4F_mutex_init(m)     pthread_mutex_init((m), NULL)
#  define LZ4F_mutex_destroy(m)  pthread_mutex_destroy(m)
#  define LZ4F_mutex_lock(m)     pthread_mutex_lock(m)
#  define LZ4F_mutex_unlock(m)   pthread_mutex_unlock(m)
#  define LZ4F_cond_init(c)      pthread_cond_init((c), NULL)
#  define LZ4F_cond_destroy(c)   pthread_cond_destroy(c)
#  define LZ4F_cond_wait(c, m)   pthread_cond_wait((c), (m))
#  define LZ4F_cond_signal(c)    pthread_cond_signal(c)
#  define LZ4F_cond_broadcast(c) pthread_cond_broadcast(c)
   /* worker threads must not receive process-directed signals intended for the application,
    * but synchronous signals (faults) remain deliverable : when blocked, a fault kills the process,
    * bypassing the application's handlers (ex: SIGBUS from a memory-mapped input truncated during compression) */
#  if defined(SIG_SETMASK) && (!defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE >= 199506L))
#    define LZ4F_MT_BLOCK_SIGNALS 1
#  else
#    define LZ4F_MT_BLOCK_SIGNALS 0
#  endif
#endif

/* some compression functions employ large stack buffers when not in HEAPMODE,
 * while default thread stacks can be small (ex: 128 KB on musl) */
#define LZ4F_MT_STACKSIZE_MIN (1 MB)

/* Persistent pool of worker threads, started on demand (see LZ4F_pool_submit()), up to maxThreads.
 * Queued jobs are run by worker threads (workerID 1 to nbThreads) :
 * the calling thread only submits jobs and waits for them (without pool, it runs jobs itself : workerID 0).
 * Allocations happen in the calling thread, since @cmem might not be thread-safe :
 * HC states are extended (LZ4_STREAMHC_EXTSIZE), so lz4hc allocates nothing within worker threads.
 * Note : like any thread, workers don't survive fork() : a forked child must not employ this pool. */
typedef struct { LZ4F_job_f fn; void* arg; } LZ4F_poolJob_t;
typedef struct { LZ4F_pool_t* pool; unsigned workerID; } LZ4F_poolWorker_t;

struct LZ4F_pool_s {
    LZ4F_mutex_t lock;
    LZ4F_cond_t  jobQueued;    /* to worker threads */
    LZ4F_cond_t  jobDone;      /* to the calling thread */
    LZ4F_poolJob_t* queue;     /* circular */
    unsigned queueCapacity;
    unsigned queueHead;
    unsigned queueSize;
    int shutdown;
    int cancel;                /* jobs of an abandoned frame : not started yet, they end without running */
    unsigned nbDone;           /* incremented at each job completion */
    unsigned nbBusy;           /* worker threads running a job */
    LZ4F_thread_t* threads;
    LZ4F_poolWorker_t* workers;
    unsigned nbThreads;        /* started on demand, up to maxThreads */
    unsigned maxThreads;
};

/* requires lock */
static LZ4F_poolJob_t LZ4F_pool_pop(LZ4F_pool_t* pool)
{
    LZ4F_poolJob_t const job = pool->queue[pool->queueHead];
    assert(pool->queueSize > 0);
    pool->queueHead = (pool->queueHead + 1) % pool->queueCapacity;
    pool->queueSize--;
    return job;
}

static void LZ4F_pool_work(LZ4F_poolWorker_t* worker)
{
    LZ4F_pool_t* const pool = worker->pool;
    int busy = 0;
    for (;;) {
        LZ4F_poolJob_t job;
        LZ4F_mutex_lock(&pool->lock);
        if (busy) pool->nbBusy--;
        while ((pool->queueSize == 0) && !pool->shutdown)
            LZ4F_cond_wait(&pool->jobQueued, &pool->lock);
        if (pool->queueSize == 0) {   /* shutdown */
            LZ4F_mutex_unlock(&pool->lock);
            return;
        }
        job = LZ4F_pool_pop(pool);
        pool->nbBusy++;
        busy = 1;
        LZ4F_mutex_unlock(&pool->lock);
        job.fn(job.arg, worker->workerID);
    }
}

#if defined(_WIN32)
static DWORD WINAPI LZ4F_pool_thread(LPVOID arg)
{
    LZ4F_pool_work((LZ4F_poolWorker_t*)arg);
    return 0;
}
#else
static void* LZ4F_pool_thread(void* arg)
{
    LZ4F_pool_work((LZ4F_poolWorker_t*)arg);
    return NULL;
}
#endif

/* starts threads, up to @nbThreads in total
 * @return : nb of threads of @pool (fewer than @nbThreads when a creation fails) */
static unsigned LZ4F_pool_startThreads(LZ4F_pool_t* pool, unsigned nbThreads)
{
    unsigned n;
#if defined(_WIN32)
    assert(nbThreads <= pool->maxThreads);
    for (n = pool->nbThreads; n < nbThreads; n++) {
        /* stack size : reserved, and committed on demand (otherwise, CreateThread() commits it upfront) */
        pool->threads[n] = CreateThread(NULL, LZ4F_MT_STACKSIZE_MIN, LZ4F_pool_thread, &pool->workers[n],
                                        STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
        if (pool->threads[n] == NULL) break;
    }
#else
    pthread_attr_t attr;
    int const attrOK = !pthread_attr_init(&attr);
#  if LZ4F_MT_BLOCK_SIGNALS
    sigset_t allSignals, previousMask;
    sigfillset(&allSignals);
    sigdelset(&allSignals, SIGBUS);
    sigdelset(&allSignals, SIGSEGV);
    sigdelset(&allSignals, SIGFPE);
    sigdelset(&allSignals, SIGILL);
    pthread_sigmask(SIG_SETMASK, &allSignals, &previousMask);   /* new threads inherit this mask */
#  endif
    if (attrOK) {
        size_t stackSize;
        if (!pthread_attr_getstacksize(&attr, &stackSize) && (stackSize < LZ4F_MT_STACKSIZE_MIN))
            (void)pthread_attr_setstacksize(&attr, LZ4F_MT_STACKSIZE_MIN);
    }
    assert(nbThreads <= pool->maxThreads);
    for (n = pool->nbThreads; n < nbThreads; n++) {
        if (pthread_create(&pool->threads[n], attrOK ? &attr : NULL, LZ4F_pool_thread, &pool->workers[n])) break;
    }
#  if LZ4F_MT_BLOCK_SIGNALS
    pthread_sigmask(SIG_SETMASK, &previousMask, NULL);
#  endif
    if (attrOK) pthread_attr_destroy(&attr);
#endif
    pool->nbThreads = n;
    return n;
}

static void LZ4F_pool_freeMem(LZ4F_pool_t* pool, LZ4F_CustomMem cmem)
{
    LZ4F_free(pool->queue, cmem);
    LZ4F_free(pool->threads, cmem);
    LZ4F_free(pool->workers, cmem);
    LZ4F_free(pool, cmem);
}

static void LZ4F_pool_free(LZ4F_pool_t* pool, LZ4F_CustomMem cmem)
{
    unsigned n;
    if (pool == NULL) return;
    LZ4F_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    LZ4F_cond_broadcast(&pool->jobQueued);
    LZ4F_mutex_unlock(&pool->lock);
    for (n = 0; n < pool->nbThreads; n++) {
#if defined(_WIN32)
        WaitForSingleObject(pool->threads[n], INFINITE);   /* never ends under the loader lock (see lz4frame.h) */
        CloseHandle(pool->threads[n]);
#else
        pthread_join(pool->threads[n], NULL);
#endif
    }
    LZ4F_cond_destroy(&pool->jobDone);
    LZ4F_cond_destroy(&pool->jobQueued);
    LZ4F_mutex_destroy(&pool->lock);
    LZ4F_pool_freeMem(pool, cmem);
}

/*! LZ4F_pool_create() :
 *  threads are started on demand (see LZ4F_pool_submit()), up to @maxThreads.
 * @return : a pool with one thread started, or NULL */
static LZ4F_pool_t* LZ4F_pool_create(unsigned maxThreads, unsigned queueCapacity, LZ4F_CustomMem cmem)
{
    LZ4F_pool_t* const pool = (LZ4F_pool_t*)LZ4F_calloc(sizeof(*pool), cmem);
    if (pool == NULL) return NULL;
    pool->queue = (LZ4F_poolJob_t*)LZ4F_malloc(queueCapacity * sizeof(*pool->queue), cmem);
    pool->threads = (LZ4F_thread_t*)LZ4F_malloc(maxThreads * sizeof(*pool->threads), cmem);
    pool->workers = (LZ4F_poolWorker_t*)LZ4F_malloc(maxThreads * sizeof(*pool->workers), cmem);
    if (pool->queue == NULL || pool->threads == NULL || pool->workers == NULL) {
        LZ4F_pool_freeMem(pool, cmem);
        return NULL;
    }
    if (LZ4F_mutex_init(&pool->lock)) {
        LZ4F_pool_freeMem(pool, cmem);
        return NULL;
    }
    if (LZ4F_cond_init(&pool->jobQueued)) {
        LZ4F_mutex_destroy(&pool->lock);
        LZ4F_pool_freeMem(pool, cmem);
        return NULL;
    }
    if (LZ4F_cond_init(&pool->jobDone)) {
        LZ4F_cond_destroy(&pool->jobQueued);
        LZ4F_mutex_destroy(&pool->lock);
        LZ4F_pool_freeMem(pool, cmem);
        return NULL;
    }
    pool->queueCapacity = queueCapacity;
    pool->maxThreads = maxThreads;
    {   unsigned n;
        for (n = 0; n < maxThreads; n++) {
            pool->workers[n].pool = pool;
            pool->workers[n].workerID = n+1;
    }   }
    if (LZ4F_pool_startThreads(pool, 1) == 0) {
        LZ4F_pool_free(pool, cmem);
        return NULL;
    }
    return pool;
}

/* A frame cut into jobs has a pool : its creation failure fails the frame (see LZ4F_mt_begin()).
 * Only LZ4F_pool_free() and LZ4F_pool_setCancel() accept a NULL pool, left by such a failure (see LZ4F_mt_free()). */

static unsigned LZ4F_pool_nbThreads(const LZ4F_pool_t* pool)
{
    assert(pool != NULL);
    return pool->nbThreads;
}

/* memory allocated with the custom allocator (thread stacks excluded) */
static size_t LZ4F_pool_sizeof(const LZ4F_pool_t* pool)
{
    assert(pool != NULL);
    return sizeof(*pool) + pool->queueCapacity * sizeof(*pool->queue)
         + pool->maxThreads * (sizeof(*pool->threads) + sizeof(*pool->workers));
}

/* @return : 1 when queued jobs outnumber idle threads, and more threads can be started (see LZ4F_pool_startThreads()) */
static int LZ4F_pool_submit(LZ4F_pool_t* pool, LZ4F_job_f fn, void* arg)
{
    int moreThreads;
    assert(pool != NULL);
    LZ4F_mutex_lock(&pool->lock);
    assert(pool->queueSize < pool->queueCapacity);
    {   LZ4F_poolJob_t* const job = pool->queue + (pool->queueHead + pool->queueSize) % pool->queueCapacity;
        job->fn = fn;
        job->arg = arg;
    }
    pool->queueSize++;
    moreThreads = (pool->queueSize > pool->nbThreads - pool->nbBusy) && (pool->nbThreads < pool->maxThreads);
    LZ4F_cond_signal(&pool->jobQueued);
    LZ4F_mutex_unlock(&pool->lock);
    return moreThreads;
}

static void LZ4F_pool_setDone(LZ4F_pool_t* pool, int* doneFlag)
{
    assert(pool != NULL);
    LZ4F_mutex_lock(&pool->lock);
    *doneFlag = 1;
    pool->nbDone++;
    LZ4F_cond_broadcast(&pool->jobDone);
    LZ4F_mutex_unlock(&pool->lock);
}

static void LZ4F_pool_setCancel(LZ4F_pool_t* pool, int cancel)
{
    if (pool == NULL) return;
    LZ4F_mutex_lock(&pool->lock);
    pool->cancel = cancel;
    LZ4F_mutex_unlock(&pool->lock);
}

static int LZ4F_pool_isCancelled(LZ4F_pool_t* pool)
{
    int cancel;
    assert(pool != NULL);
    LZ4F_mutex_lock(&pool->lock);
    cancel = pool->cancel;
    LZ4F_mutex_unlock(&pool->lock);
    return cancel;
}

static int LZ4F_pool_isDone(LZ4F_pool_t* pool, const int* doneFlag)
{
    int done;
    assert(pool != NULL);
    LZ4F_mutex_lock(&pool->lock);
    done = *doneFlag;
    LZ4F_mutex_unlock(&pool->lock);
    return done;
}

/* waits for *doneFlag */
static void LZ4F_pool_waitDone(LZ4F_pool_t* pool, const int* doneFlag)
{
    assert(pool != NULL);
    LZ4F_mutex_lock(&pool->lock);
    while (!*doneFlag) LZ4F_cond_wait(&pool->jobDone, &pool->lock);
    LZ4F_mutex_unlock(&pool->lock);
}

static unsigned LZ4F_pool_nbDone(LZ4F_pool_t* pool)
{
    unsigned nbDone;
    assert(pool != NULL);
    LZ4F_mutex_lock(&pool->lock);
    nbDone = pool->nbDone;
    LZ4F_mutex_unlock(&pool->lock);
    return nbDone;
}

/* waits for any job completion after @nbDone was observed */
static void LZ4F_pool_waitAny(LZ4F_pool_t* pool, unsigned nbDone)
{
    assert(pool != NULL);
    LZ4F_mutex_lock(&pool->lock);
    while (pool->nbDone == nbDone) LZ4F_cond_wait(&pool->jobDone, &pool->lock);
    LZ4F_mutex_unlock(&pool->lock);
}

#else  /* !LZ4F_MULTITHREAD : no pool is ever created, jobs run within the calling thread */

static unsigned LZ4F_pool_nbThreads(const LZ4F_pool_t* pool) { (void)pool; return 0; }
static size_t LZ4F_pool_sizeof(const LZ4F_pool_t* pool) { (void)pool; return 0; }
static void LZ4F_pool_free(LZ4F_pool_t* pool, LZ4F_CustomMem cmem) { (void)pool; (void)cmem; }
static int  LZ4F_pool_submit(LZ4F_pool_t* pool, LZ4F_job_f fn, void* arg) { (void)pool; fn(arg, 0); return 0; }
static unsigned LZ4F_pool_startThreads(LZ4F_pool_t* pool, unsigned nbThreads) { (void)pool; (void)nbThreads; return 0; }
static void LZ4F_pool_setDone(LZ4F_pool_t* pool, int* doneFlag) { (void)pool; *doneFlag = 1; }
static void LZ4F_pool_setCancel(LZ4F_pool_t* pool, int cancel) { (void)pool; (void)cancel; }
static int  LZ4F_pool_isCancelled(LZ4F_pool_t* pool) { (void)pool; return 0; }
static int  LZ4F_pool_isDone(LZ4F_pool_t* pool, const int* doneFlag) { (void)pool; return *doneFlag; }
static void LZ4F_pool_waitDone(LZ4F_pool_t* pool, const int* doneFlag) { (void)pool; (void)doneFlag; assert(*doneFlag); }
static unsigned LZ4F_pool_nbDone(LZ4F_pool_t* pool) { (void)pool; return 0; }
static void LZ4F_pool_waitAny(LZ4F_pool_t* pool, unsigned nbDone) { (void)pool; (void)nbDone; assert(0); }

#endif  /* LZ4F_MULTITHREAD */

typedef struct {
    LZ4F_mtCtx_t* mt;
    const BYTE* src;     /* job content, right after its prefixSize bytes of history */
    size_t srcSize;
    size_t prefixSize;
    BYTE* inBuff;        /* input buffer (LZ4F_MT_PREFIX_SIZE + jobSize), when content is copied : NULL when read in place */
    BYTE* outBuff;       /* compressed blocks */
    LZ4F_BlockCompressMode_e mode;   /* LZ4B_UNCOMPRESSED : uncompressed input (LZ4F_compressOptions_t.uncompressed) */
    size_t cSize;        /* result : compressed size, or error code */
    int done;            /* result available (protected by pool's lock) */
} LZ4F_mtJob_t;

struct LZ4F_mtCtx_s {
    LZ4F_CustomMem cmem;
    LZ4F_pool_t* pool;       /* NULL without multi-threading support (jobs run within the calling thread), or after a failed creation */
    unsigned poolRequested;  /* nbWorkers requested when pool was created : threads are started on demand, up to it */
    void** states;           /* one LZ4 state per worker (allocated when it starts) ; states[0] : cctx's own state, without pool */
    unsigned nbStates;       /* capacity of states[] */
    LZ4F_CtxType_e statesType;
    LZ4F_mtJob_t* jobs;      /* ring buffer of job slots, for the maximum nb of threads : outBuff allocated on first use */
    unsigned nbJobSlots;
    size_t inCapacity;       /* of each input buffer */
    size_t outCapacity;      /* of each outBuff */
    /* input buffers are only needed until their job is compressed : they are shared by job slots */
    BYTE** inBuffs;          /* all allocated input buffers */
    BYTE** inFree;           /* free input buffers (stack) */
    unsigned nbInBuffs;
    unsigned nbInFree;
    unsigned maxInBuffs;     /* capacity of inBuffs[] and inFree[] : one per thread, plus the one being filled */
    unsigned oldest;         /* oldest submitted job, not emitted yet */
    size_t emitted;          /* part of the oldest job already emitted, see LZ4F_mt_emitPartial() */
    unsigned nbSubmitted;
    int filling;             /* the slot following submitted jobs is being filled */
    BYTE* history;           /* last bytes of content, up to LZ4F_MT_PREFIX_SIZE (linked blocks) */
    size_t historySize;
    const BYTE* stableEnd;   /* end of previous input, when stable (LZ4F_srcStableUntilFlush) */
    size_t stableRun;        /* stable input contiguously preceding stableEnd, up to LZ4F_MT_PREFIX_SIZE */
    /* current frame */
    const LZ4F_preferences_t* prefs;
    const LZ4F_CDict* cdict;
    size_t blockSize;
    size_t jobSize;
};

static void LZ4F_mt_runJob(void* arg, unsigned workerID)
{
    LZ4F_mtJob_t* const job = (LZ4F_mtJob_t*)arg;
    LZ4F_mtCtx_t* const mt = job->mt;
    assert(mt->states[workerID] != NULL);
    if (LZ4F_pool_isCancelled(mt->pool)) {
        job->cSize = 0;   /* abandoned frame : result discarded */
    } else {
        job->cSize = LZ4F_compressJob(mt->states[workerID], mt->prefs, mt->cdict, mt->blockSize,
                                      job->outBuff, mt->outCapacity,
                                      job->src, job->srcSize, job->prefixSize, job->mode);
    }
    LZ4F_pool_setDone(mt->pool, &job->done);
}

/* jobs in flight : up to LZ4F_MT_NBSLOTS() of current nb of threads (within ring buffer of job slots) */
static unsigned LZ4F_mt_nbUsableSlots(const LZ4F_mtCtx_t* mt)
{
    unsigned const nbThreads = LZ4F_pool_nbThreads(mt->pool);
    return nbThreads ? (unsigned)LZ4F_MT_NBSLOTS(nbThreads) : 1;
}

/* allocates the LZ4 state of worker @workerID, unless already done
 * @return : 1 when the state is ready, 0 if allocation failed */
static int LZ4F_mt_allocState(LZ4F_mtCtx_t* mt, unsigned workerID, int level)
{
    if (mt->states[workerID] == NULL) {
        void* const state = LZ4F_malloc(LZ4F_sizeofLZ4State(mt->statesType), mt->cmem);
        if (state == NULL) return 0;
        LZ4F_initLZ4State(state, mt->statesType, level);
        mt->states[workerID] = state;
    }
    return 1;
}

/* starts one more worker thread, with its LZ4 state (allocated by the calling thread) :
 * on failure (allocation, thread creation), compression proceeds with current threads */
static void LZ4F_mt_addWorker(LZ4F_mtCtx_t* mt)
{
    unsigned const workerID = LZ4F_pool_nbThreads(mt->pool) + 1;
    assert(workerID < mt->nbStates);
    if (LZ4F_mt_allocState(mt, workerID, mt->prefs->compressionLevel))
        (void)LZ4F_pool_startThreads(mt->pool, workerID);
}

/* @n-th submitted job, from the oldest one : @n == nbSubmitted is the slot following them (see LZ4F_mt_fillingSlot()) */
static LZ4F_mtJob_t* LZ4F_mt_job(LZ4F_mtCtx_t* mt, unsigned n)
{
    return mt->jobs + (mt->oldest + n) % mt->nbJobSlots;
}

static LZ4F_mtJob_t* LZ4F_mt_fillingSlot(LZ4F_mtCtx_t* mt)
{
    return LZ4F_mt_job(mt, mt->nbSubmitted);
}

/* keeps the last LZ4F_MT_PREFIX_SIZE bytes of content */
static void LZ4F_mt_addHistory(LZ4F_mtCtx_t* mt, const BYTE* content, size_t size)
{
    if (size >= LZ4F_MT_PREFIX_SIZE) {
        memcpy(mt->history, content + size - LZ4F_MT_PREFIX_SIZE, LZ4F_MT_PREFIX_SIZE);
        mt->historySize = LZ4F_MT_PREFIX_SIZE;
    } else {
        size_t const kept = MIN(mt->historySize, LZ4F_MT_PREFIX_SIZE - size);
        memmove(mt->history, mt->history + mt->historySize - kept, kept);
        memcpy(mt->history + kept, content, size);
        mt->historySize = kept + size;
    }
}

/* submits the job being filled : to worker threads,
 * or, @inCallingThread, runs it right away with the cctx's own state (states[0], unused by workers) */
static void LZ4F_mt_submit_internal(LZ4F_mtCtx_t* mt, int inCallingThread)
{
    LZ4F_mtJob_t* const job = LZ4F_mt_fillingSlot(mt);
    assert(mt->filling);
    assert(job->srcSize > 0);
    job->done = 0;
    mt->filling = 0;
    mt->nbSubmitted++;
    if (inCallingThread)
        LZ4F_mt_runJob(job, 0);
    else if (LZ4F_pool_submit(mt->pool, LZ4F_mt_runJob, job))
        LZ4F_mt_addWorker(mt);   /* more jobs than idle threads */
    if (mt->prefs->frameInfo.blockMode == LZ4F_blockLinked)
        LZ4F_mt_addHistory(mt, job->src, job->srcSize);   /* job content is only read while job runs */
}

static void LZ4F_mt_submit(LZ4F_mtCtx_t* mt) { LZ4F_mt_submit_internal(mt, 0); }

/* the calling thread is about to wait for all jobs : when no other job is running, it compresses the last one itself,
 * which saves the synchronization with a worker (same output : jobs don't depend on their state) */
static void LZ4F_mt_submitLast(LZ4F_mtCtx_t* mt) { LZ4F_mt_submit_internal(mt, mt->nbSubmitted == 0); }

/* input buffer of a completed job returns to the free stack */
static void LZ4F_mt_releaseInBuff(LZ4F_mtCtx_t* mt, LZ4F_mtJob_t* job)
{
    if (job->inBuff == NULL) return;
    assert(mt->nbInFree < mt->nbInBuffs);
    mt->inFree[mt->nbInFree++] = job->inBuff;
    job->inBuff = NULL;
}

/* the oldest job, completed, is emitted or discarded : its slot is free */
static void LZ4F_mt_retireOldest(LZ4F_mtCtx_t* mt)
{
    LZ4F_mt_releaseInBuff(mt, mt->jobs + mt->oldest);
    mt->oldest = (mt->oldest + 1) % mt->nbJobSlots;
    mt->nbSubmitted--;
    mt->emitted = 0;
}

/*! LZ4F_mt_acquireInBuff() :
 *  When all input buffers are employed, one is reclaimed from a submitted job,
 *  the first one to complete (job durations vary).
 * @return : an input buffer, or NULL if allocation failed */
static BYTE* LZ4F_mt_acquireInBuff(LZ4F_mtCtx_t* mt, LZ4F_CustomMem cmem)
{
    if (mt->nbInFree) return mt->inFree[--mt->nbInFree];
    if (mt->nbInBuffs < LZ4F_pool_nbThreads(mt->pool) + LZ4F_MT_EXTRA_INBUFFS) {
        BYTE* const buff = (BYTE*)LZ4F_malloc(mt->inCapacity, cmem);
        assert(mt->nbInBuffs < mt->maxInBuffs);
        if (buff != NULL) mt->inBuffs[mt->nbInBuffs++] = buff;
        return buff;
    }
    for (;;) {
        unsigned const nbDone = LZ4F_pool_nbDone(mt->pool);
        unsigned n;
        for (n = 0; n < mt->nbSubmitted; n++) {
            LZ4F_mtJob_t* const job = LZ4F_mt_job(mt, n);
            if ((job->inBuff != NULL) && LZ4F_pool_isDone(mt->pool, &job->done)) {
                BYTE* const buff = job->inBuff;
                job->inBuff = NULL;
                return buff;
        }   }
        /* buffers are held by submitted jobs, still running : wait for one of them */
        LZ4F_pool_waitAny(mt->pool, nbDone);
    }
}

/* emits oldest submitted job into @dst + @pos, waiting for it if needed.
 * On error, the job remains submitted : callers invalidate the frame, discarding its jobs (FORWARD_IF_ERROR_INVALIDATE) */
static size_t LZ4F_mt_emitOldest(LZ4F_mtCtx_t* mt, BYTE* dst, size_t dstCapacity, size_t pos)
{
    LZ4F_mtJob_t* const job = mt->jobs + mt->oldest;
    size_t toEmit;
    assert(mt->nbSubmitted > 0);
    assert(pos <= dstCapacity);
    LZ4F_pool_waitDone(mt->pool, &job->done);
    FORWARD_IF_ERROR(job->cSize);
    toEmit = job->cSize - mt->emitted;   /* part already emitted by an incomplete LZ4F_flush() */
    RETURN_ERROR_IF(toEmit > dstCapacity - pos, dstCapacity_tooSmall);
    memcpy(dst + pos, job->outBuff + mt->emitted, toEmit);
    LZ4F_mt_retireOldest(mt);
    return toEmit;
}

/* emits submitted jobs in order : all of them (@all), or only the already completed ones fitting into @dst
 * (their emission isn't required yet : the others are emitted later).
 * On error, callers invalidate the frame (FORWARD_IF_ERROR_INVALIDATE) */
static size_t LZ4F_mt_emit(LZ4F_mtCtx_t* mt, BYTE* dst, size_t dstCapacity, size_t pos, int all)
{
    size_t written = 0;
    while (mt->nbSubmitted > 0) {
        const LZ4F_mtJob_t* const job = mt->jobs + mt->oldest;
        if (!all) {
            if (!LZ4F_pool_isDone(mt->pool, &job->done)) break;
            if (job->cSize - mt->emitted > dstCapacity - (pos + written)) break;   /* also defers an error code */
        }
        {   size_t const r = LZ4F_mt_emitOldest(mt, dst, dstCapacity, pos + written);
            FORWARD_IF_ERROR(r);
            written += r;
    }   }
    return written;
}

/* discards submitted jobs : those not started yet end without running, others are waited for */
static void LZ4F_mt_abort(LZ4F_mtCtx_t* mt)
{
    LZ4F_pool_setCancel(mt->pool, 1);
    while (mt->nbSubmitted > 0) {
        LZ4F_pool_waitDone(mt->pool, &mt->jobs[mt->oldest].done);
        LZ4F_mt_retireOldest(mt);
    }
    LZ4F_pool_setCancel(mt->pool, 0);
    if (mt->filling) LZ4F_mt_releaseInBuff(mt, LZ4F_mt_fillingSlot(mt));
    mt->filling = 0;
    mt->emitted = 0;
    mt->stableEnd = NULL;
}

static void LZ4F_mt_freeStates(LZ4F_mtCtx_t* mt, LZ4F_CustomMem cmem)
{
    unsigned n;
    for (n = 1; n < mt->nbStates; n++) LZ4F_free(mt->states[n], cmem);   /* states[0] belongs to cctx ; NULL : not allocated */
    LZ4F_free(mt->states, cmem);
    mt->states = NULL;
    mt->nbStates = 0;
}

static void LZ4F_mt_freeInBuffs(LZ4F_mtCtx_t* mt, LZ4F_CustomMem cmem)
{
    unsigned n;
    assert(mt->nbInFree == mt->nbInBuffs);   /* no job in flight */
    for (n = 0; n < mt->nbInBuffs; n++) LZ4F_free(mt->inBuffs[n], cmem);
    LZ4F_free(mt->inBuffs, cmem);
    LZ4F_free(mt->inFree, cmem);
    mt->inBuffs = mt->inFree = NULL;
    mt->nbInBuffs = mt->nbInFree = mt->maxInBuffs = 0;
}

static void LZ4F_mt_freeJobs(LZ4F_mtCtx_t* mt, LZ4F_CustomMem cmem)
{
    unsigned n;
    for (n = 0; n < mt->nbJobSlots; n++) {
        assert(mt->jobs[n].inBuff == NULL);
        LZ4F_free(mt->jobs[n].outBuff, cmem);
    }
    LZ4F_free(mt->jobs, cmem);
    mt->jobs = NULL;
    mt->nbJobSlots = 0;
}

static void LZ4F_mt_free(LZ4F_mtCtx_t* mt, LZ4F_CustomMem cmem)
{
    if (mt == NULL) return;
    LZ4F_mt_abort(mt);
    LZ4F_pool_free(mt->pool, cmem);
    LZ4F_mt_freeStates(mt, cmem);
    LZ4F_mt_freeJobs(mt, cmem);
    LZ4F_mt_freeInBuffs(mt, cmem);
    LZ4F_free(mt->history, cmem);
    LZ4F_free(mt, cmem);
}

static size_t LZ4F_mt_sizeof(const LZ4F_mtCtx_t* mt)
{
    size_t size;
    unsigned n;
    if (mt == NULL) return 0;
    size = sizeof(*mt) + LZ4F_MT_PREFIX_SIZE
         + mt->nbStates * sizeof(void*)
         + mt->nbJobSlots * sizeof(LZ4F_mtJob_t)
         + mt->maxInBuffs * 2 * sizeof(BYTE*) + mt->nbInBuffs * mt->inCapacity
         + (mt->pool ? LZ4F_pool_sizeof(mt->pool) : 0);
    for (n = 1; n < mt->nbStates; n++) if (mt->states[n] != NULL) size += LZ4F_sizeofLZ4State(mt->statesType);
    for (n = 0; n < mt->nbJobSlots; n++) if (mt->jobs[n].outBuff != NULL) size += mt->outCapacity;
    return size;
}

/*! LZ4F_mt_begin() :
 *  prepares multi-threading resources for a new frame, cut into jobs.
 *  Resources are kept within cctx, and reused by next frames.
 *  Requires a cctx prepared by LZ4F_compressBegin_internal(), with no job in flight.
 * @return : 0, or an error code */
static size_t LZ4F_mt_begin(LZ4F_cctx_t* cctx)
{
    LZ4F_CustomMem const cmem = cctx->cmem;
    unsigned const nbWorkers = MIN(cctx->prefs.nbWorkers, LZ4F_NBWORKERS_MAX);
    LZ4F_CtxType_e const ctxType = (LZ4F_CtxType_e)cctx->lz4CtxType;
    size_t const blockSize = cctx->maxBlockSize;
    size_t const jobSize = LZ4F_mtJobSize(blockSize);
    size_t const inCapacity = LZ4F_MT_PREFIX_SIZE + jobSize;
    size_t const outCapacity = LZ4F_blocksBound(jobSize, blockSize, LZ4F_blockChecksumEnabled);
    LZ4F_mtCtx_t* mt = cctx->mt;
    unsigned nbThreads, maxThreads, nbJobSlots;

    assert(nbWorkers >= 1);
    (void)nbWorkers;   /* only employed by multi-threading support */
    if (mt == NULL) {
        mt = (LZ4F_mtCtx_t*)LZ4F_calloc(sizeof(*mt), cmem);
        RETURN_ERROR_IF(mt == NULL, allocation_failed);
        mt->history = (BYTE*)LZ4F_malloc(LZ4F_MT_PREFIX_SIZE, cmem);
        if (mt->history == NULL) {
            LZ4F_free(mt, cmem);
            RETURN_ERROR(allocation_failed);
        }
        mt->cmem = cmem;
        cctx->mt = mt;
    }
    assert(mt->nbSubmitted == 0 && !mt->filling);

    /* worker threads */
#if LZ4F_MULTITHREAD
    /* an incomplete pool (fewer threads than requested) is kept ;
     * without any worker thread, frame compression fails, and next frames try again */
    if ((mt->pool == NULL) || (mt->poolRequested != nbWorkers)) {
        LZ4F_pool_free(mt->pool, cmem);
        mt->pool = LZ4F_pool_create(nbWorkers, (unsigned)LZ4F_MT_NBSLOTS(nbWorkers), cmem);   /* queue : all job slots */
        mt->poolRequested = nbWorkers;
        RETURN_ERROR_IF(mt->pool == NULL, allocation_failed);
    }
#endif
    /* resources are sized for the maximum nb of threads, but only allocated when needed :
     * threads with their LZ4 state, job slots' outputs and input buffers */
    nbThreads = LZ4F_pool_nbThreads(mt->pool);   /* already started */
    maxThreads = mt->pool ? mt->poolRequested : 0;
    nbJobSlots = maxThreads ? (unsigned)LZ4F_MT_NBSLOTS(maxThreads) : 1;

    /* one LZ4 state per worker */
    if ((mt->nbStates != maxThreads + 1) || (mt->statesType != ctxType)) {
        LZ4F_mt_freeStates(mt, cmem);
        mt->states = (void**)LZ4F_calloc((maxThreads + 1) * sizeof(void*), cmem);
        RETURN_ERROR_IF(mt->states == NULL, allocation_failed);
        mt->nbStates = maxThreads + 1;
        mt->statesType = ctxType;
    }
    {   unsigned n;
        for (n = 1; n <= nbThreads; n++)   /* threads already started (previous frames) */
            RETURN_ERROR_IF(!LZ4F_mt_allocState(mt, n, cctx->prefs.compressionLevel), allocation_failed);
    }
    mt->states[0] = cctx->lz4CtxPtr;

    /* job slots (outputs allocated on first use) */
    if (mt->nbJobSlots != nbJobSlots) {
        LZ4F_mt_freeJobs(mt, cmem);
        mt->jobs = (LZ4F_mtJob_t*)LZ4F_calloc(nbJobSlots * sizeof(LZ4F_mtJob_t), cmem);
        RETURN_ERROR_IF(mt->jobs == NULL, allocation_failed);
        mt->nbJobSlots = nbJobSlots;
    }
    if (mt->outCapacity < outCapacity) {
        unsigned n;
        for (n = 0; n < mt->nbJobSlots; n++) {
            LZ4F_free(mt->jobs[n].outBuff, cmem);
            mt->jobs[n].outBuff = NULL;
        }
        mt->outCapacity = outCapacity;
    }

    /* input buffers : one per thread, plus the one being filled (allocated on first need) */
    if ((mt->maxInBuffs != maxThreads + LZ4F_MT_EXTRA_INBUFFS) || (mt->inCapacity < inCapacity)) {
        LZ4F_mt_freeInBuffs(mt, cmem);
        mt->inBuffs = (BYTE**)LZ4F_malloc((maxThreads + LZ4F_MT_EXTRA_INBUFFS) * sizeof(BYTE*), cmem);
        mt->inFree = (BYTE**)LZ4F_malloc((maxThreads + LZ4F_MT_EXTRA_INBUFFS) * sizeof(BYTE*), cmem);
        if ((mt->inBuffs == NULL) || (mt->inFree == NULL)) {
            LZ4F_mt_freeInBuffs(mt, cmem);
            RETURN_ERROR(allocation_failed);
        }
        mt->maxInBuffs = maxThreads + LZ4F_MT_EXTRA_INBUFFS;
        mt->inCapacity = inCapacity;
    }

    /* current frame */
    {   unsigned n;
        for (n = 0; n < nbJobSlots; n++) mt->jobs[n].mt = mt;
    }
    mt->prefs = &cctx->prefs;
    mt->cdict = cctx->cdict;
    mt->blockSize = blockSize;
    mt->jobSize = jobSize;
    mt->oldest = 0;
    mt->emitted = 0;
    mt->nbSubmitted = 0;
    mt->filling = 0;
    mt->historySize = 0;
    mt->stableEnd = NULL;
    mt->stableRun = 0;
    cctx->mtActive = 1;
    return 0;
}

/*! LZ4F_mt_flushPoint() :
 *  LZ4F_flush() and LZ4F_compressEnd(), multi-threaded mode : the job being filled is submitted (see LZ4F_mt_submitLast()),
 *  and all jobs are waited for : input is no longer read, and their output is emitted by LZ4F_mt_emitPartial(). */
static void LZ4F_mt_flushPoint(LZ4F_mtCtx_t* mt)
{
    unsigned n;
    if (mt->filling) LZ4F_mt_submitLast(mt);
    for (n = 0; n < mt->nbSubmitted; n++)
        LZ4F_pool_waitDone(mt->pool, &LZ4F_mt_job(mt, n)->done);
}

/*! LZ4F_mt_emitPartial() :
 *  emits completed jobs in order into @dst + @pos, as much as fits : the oldest one can be emitted partially.
 *  Requires all submitted jobs completed, see LZ4F_mt_flushPoint().
 * @return : nb of bytes written, or an error code */
static size_t LZ4F_mt_emitPartial(LZ4F_mtCtx_t* mt, BYTE* dst, size_t dstCapacity, size_t pos)
{
    size_t const start = pos;
    while (mt->nbSubmitted > 0) {
        LZ4F_mtJob_t* const job = mt->jobs + mt->oldest;
        size_t n;
        assert(LZ4F_pool_isDone(mt->pool, &job->done));
        FORWARD_IF_ERROR(job->cSize);
        n = MIN(job->cSize - mt->emitted, dstCapacity - pos);
        if (n) memcpy(dst + pos, job->outBuff + mt->emitted, n);
        pos += n;
        mt->emitted += n;
        if (mt->emitted < job->cSize) break;   /* dst is full */
        LZ4F_mt_retireOldest(mt);
    }
    return pos - start;
}

/*! LZ4F_mt_update() :
 *  LZ4F_compressUpdate(), multi-threaded mode (@mode : compressed or uncompressed input ; @cOptPtr : not NULL).
 *  Input is copied into jobs, submitted once full.
 *  Job content is read in place instead, when it remains available until the job completes :
 *  with stableSrc == LZ4F_srcStableUntilFlush, or with autoFlush, since all jobs complete within the invocation.
 *  LZ4F_srcStable (historical value 1) only guarantees the last 64 KB : content is copied.
 *  Uncompressed segments are jobs too : a mode switch submits the job being filled,
 *  so that blocks are cut as in single-threaded mode (the buffered block is flushed on a mode switch).
 *  Completed jobs are emitted in order, as long as they fit. With autoFlush, all input is compressed and emitted.
 *  Success is guaranteed when @dstCapacity >= LZ4F_compressBound(srcSize, prefs) :
 *  without autoFlush, output is only required to start a job while all slots are busy (oldest job). */
static size_t LZ4F_mt_update(LZ4F_cctx_t* cctx,
                             void* dstBuffer, size_t dstCapacity,
                       const void* srcBuffer, size_t srcSize,
                       const LZ4F_compressOptions_t* cOptPtr,
                             LZ4F_BlockCompressMode_e mode)
{
    LZ4F_mtCtx_t* const mt = cctx->mt;
    const BYTE* const istart = (const BYTE*)srcBuffer;
    const BYTE* ip = istart;
    const BYTE* const iend = srcSize ? istart + srcSize : istart;
    BYTE* const dst = (BYTE*)dstBuffer;
    size_t pos = 0;
    int const checksum = (cctx->prefs.frameInfo.contentChecksumFlag == LZ4F_contentChecksumEnabled);
    int const linked = (cctx->prefs.frameInfo.blockMode == LZ4F_blockLinked);
    int const stableUntilFlush = (cOptPtr->stableSrc == LZ4F_srcStableUntilFlush);
    int const inPlace = cctx->prefs.autoFlush || stableUntilFlush;
    /* stable input remains available : when contiguous with previous input, history can be read in place */
    size_t const prevRun = (stableUntilFlush && srcSize && (istart == mt->stableEnd)) ? mt->stableRun : 0;

    DEBUGLOG(4, "LZ4F_mt_update (srcSize=%u, mode=%u)", (unsigned)srcSize, (unsigned)mode);
    RETURN_ERROR_IF(cctx->cStage != 1, compressionState_uninitialized);
    assert(cctx->pendingSize == 0);   /* multi-threaded mode : pending output is within jobs */
    cctx->flushStage = 0;   /* an incomplete LZ4F_flush() ends here : remaining jobs are emitted as usual */
    if (mt->filling && (LZ4F_mt_fillingSlot(mt)->mode != mode))
        LZ4F_mt_submit(mt);   /* mode switch */

    while (ip < iend) {
        LZ4F_mtJob_t* const job = LZ4F_mt_fillingSlot(mt);
        if (!mt->filling) {
            /* new job */
            size_t const remaining = (size_t)(iend - ip);
            size_t const prefixSize = linked ? mt->historySize : 0;
            if (mt->nbSubmitted >= LZ4F_mt_nbUsableSlots(mt)) {
                /* all slots busy : emit oldest job, to free a slot */
                size_t const r = LZ4F_mt_emitOldest(mt, dst, dstCapacity, pos);
                FORWARD_IF_ERROR_INVALIDATE(r, cctx);
                pos += r;
                assert(job == LZ4F_mt_fillingSlot(mt));
            }
            if (job->outBuff == NULL) {   /* first use of this slot */
                job->outBuff = (BYTE*)LZ4F_malloc(mt->outCapacity, cctx->cmem);
                if (job->outBuff == NULL) {
                    LZ4F_invalidateFrame(cctx);
                    RETURN_ERROR(allocation_failed);
                }
            }
            mt->filling = 1;
            job->mode = mode;
            job->prefixSize = prefixSize;
            if ( inPlace
              && ((remaining >= mt->jobSize) || cctx->prefs.autoFlush)
              && (prevRun + (size_t)(ip - istart) >= prefixSize) ) {
                /* read in place : whole job is within @srcBuffer, and its history right before */
                size_t const jobSize = MIN(remaining, mt->jobSize);
                job->src = ip;
                job->srcSize = jobSize;
                if (cctx->prefs.autoFlush && (jobSize == remaining))
                    LZ4F_mt_submitLast(mt);   /* all jobs are waited for before returning */
                else
                    LZ4F_mt_submit(mt);
                if (checksum) (void)XXH32_update(&(cctx->xxh), ip, jobSize);
                ip += jobSize;
                continue;
            }
            /* copy : history first, right before content */
            assert(job->inBuff == NULL);
            job->inBuff = LZ4F_mt_acquireInBuff(mt, cctx->cmem);
            if (job->inBuff == NULL) {
                mt->filling = 0;
                LZ4F_invalidateFrame(cctx);
                RETURN_ERROR(allocation_failed);
            }
            job->src = job->inBuff + LZ4F_MT_PREFIX_SIZE;
            job->srcSize = 0;
            memcpy(job->inBuff + LZ4F_MT_PREFIX_SIZE - prefixSize, mt->history + mt->historySize - prefixSize, prefixSize);
        }
        {   size_t const toCopy = MIN(mt->jobSize - job->srcSize, (size_t)(iend - ip));
            assert(job->src == job->inBuff + LZ4F_MT_PREFIX_SIZE);
            memcpy(job->inBuff + LZ4F_MT_PREFIX_SIZE + job->srcSize, ip, toCopy);
            job->srcSize += toCopy;
            if (job->srcSize == mt->jobSize) LZ4F_mt_submit(mt);   /* before checksum : workers start sooner */
            if (checksum) (void)XXH32_update(&(cctx->xxh), ip, toCopy);
            ip += toCopy;
        }
    }
    cctx->totalInSize += srcSize;
    if (srcSize) {
        mt->stableEnd = stableUntilFlush ? iend : NULL;
        mt->stableRun = MIN(prevRun + srcSize, LZ4F_MT_PREFIX_SIZE);
    }

    /* autoFlush : the last job is submitted too (see LZ4F_mt_submitLast()), and all jobs are emitted */
    if (cctx->prefs.autoFlush && mt->filling) LZ4F_mt_submitLast(mt);
    {   size_t const r = LZ4F_mt_emit(mt, dst, dstCapacity, pos, cctx->prefs.autoFlush != 0);
        FORWARD_IF_ERROR_INVALIDATE(r, cctx);
        return pos + r;
    }
}


/*! LZ4F_st_update() :
 *  LZ4F_compressUpdate(), single-threaded mode (@blockCompression : compressed or uncompressed input ;
 *  @compressOptionsPtr : not NULL).
 *  Input first completes the block buffered within cctx, then full blocks are compressed directly from @srcBuffer
 *  (all remaining input with autoFlush), and the remainder is buffered.
 *  A change of @blockCompression first flushes the buffered block. */
static size_t LZ4F_st_update(LZ4F_cctx* cctxPtr,
                     void* dstBuffer, size_t dstCapacity,
                     const void* srcBuffer, size_t srcSize,
                     const LZ4F_compressOptions_t* compressOptionsPtr,
                     LZ4F_BlockCompressMode_e blockCompression)
  {
    size_t const blockSize = cctxPtr->maxBlockSize;
    const BYTE* srcPtr = (const BYTE*)srcBuffer;
    const BYTE* const srcEnd = srcSize ? (assert(srcPtr!=NULL), srcPtr + srcSize) : srcPtr;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* const dstEnd = dstStart + dstCapacity;
    BYTE* dstPtr = dstStart;
    LZ4F_lastBlockStatus lastBlockCompressed = notDone;
    compressFunc_t const compress = LZ4F_selectCompression(cctxPtr->prefs.frameInfo.blockMode, cctxPtr->prefs.compressionLevel, blockCompression);
    size_t bytesWritten;
    DEBUGLOG(4, "LZ4F_compressUpdate (srcSize=%zu)", srcSize);

    RETURN_ERROR_IF(cctxPtr->cStage != 1, compressionState_uninitialized);   /* state must be initialized and waiting for next block */

    /* output left pending by an incomplete LZ4F_flush() */
    bytesWritten = LZ4F_emitPendingWhole(cctxPtr, dstPtr, dstCapacity);
    FORWARD_IF_ERROR_INVALIDATE(bytesWritten, cctxPtr);
    dstPtr += bytesWritten;

    /* flush currently written block, to continue with new block compression */
    if (cctxPtr->blockCompressMode != blockCompression) {
        bytesWritten = LZ4F_flushBlock(cctxPtr, dstPtr, (size_t)(dstEnd - dstPtr));
        FORWARD_IF_ERROR(bytesWritten);   /* note : a failed flush invalidates the frame */
        dstPtr += bytesWritten;
        cctxPtr->blockCompressMode = blockCompression;
        if ((blockCompression == LZ4B_UNCOMPRESSED) && (cctxPtr->prefs.frameInfo.blockMode == LZ4F_blockLinked)) {
            /* linked blocks : the decoder's history will contain uncompressed data, unknown to the LZ4 state :
             * history is dropped, blocks compressed next don't reference anything before them */
            LZ4F_startStream(cctxPtr->lz4CtxPtr, &cctxPtr->prefs, NULL, NULL, 0);
        }
    }

    /* complete tmp buffer */
    if (cctxPtr->tmpInSize > 0) {   /* some data already within tmp buffer */
        size_t const sizeToCopy = blockSize - cctxPtr->tmpInSize;
        assert(blockSize > cctxPtr->tmpInSize);
        if (sizeToCopy > srcSize) {
            /* add src to tmpIn buffer */
            memcpy(cctxPtr->tmpIn + cctxPtr->tmpInSize, srcBuffer, srcSize);
            srcPtr = srcEnd;
            cctxPtr->tmpInSize += srcSize;
            /* still needs some CRC */
        } else {
            /* complete tmpIn block and then compress it */
            lastBlockCompressed = fromTmpBuffer;
            memcpy(cctxPtr->tmpIn + cctxPtr->tmpInSize, srcBuffer, sizeToCopy);
            srcPtr += sizeToCopy;

            bytesWritten = LZ4F_makeBlock(dstPtr, (size_t)(dstEnd - dstPtr),
                                          cctxPtr->tmpIn, blockSize,
                                          compress, cctxPtr->lz4CtxPtr, cctxPtr->prefs.compressionLevel,
                                          cctxPtr->cdict,
                                          cctxPtr->prefs.frameInfo.blockChecksumFlag);
            FORWARD_IF_ERROR_INVALIDATE(bytesWritten, cctxPtr);
            dstPtr += bytesWritten;
            if (cctxPtr->prefs.frameInfo.blockMode==LZ4F_blockLinked) cctxPtr->tmpIn += blockSize;
            cctxPtr->tmpInSize = 0;
    }   }

    /* compress full blocks; with autoFlush, remaining input (< blockSize) is compressed too */
    {   size_t const remaining = (size_t)(srcEnd - srcPtr);
        size_t const toCompress = cctxPtr->prefs.autoFlush ? remaining : remaining - (remaining % blockSize);
        /* autoFlush, linked blocks, unstable input smaller than the history window (64 KB) :
         * compressed from @srcBuffer, it would become the only history of next blocks.
         * It's compressed from tmpBuff instead, right after the history, which remains whole. */
        int const viaTmp = cctxPtr->prefs.autoFlush
                        && (cctxPtr->prefs.frameInfo.blockMode == LZ4F_blockLinked)
                        && !compressOptionsPtr->stableSrc
                        && (blockCompression == LZ4B_COMPRESSED)
                        && (toCompress > 0) && (toCompress < 64 KB);
        if (viaTmp) {
            if (cctxPtr->tmpIn + toCompress > cctxPtr->tmpBuff + cctxPtr->tmpBuffCapacity)
                LZ4F_localSaveDict(cctxPtr);   /* tmpBuff >= 128 KB : room after the history */
            memcpy(cctxPtr->tmpIn, srcPtr, toCompress);
            lastBlockCompressed = fromTmpBuffer;
            bytesWritten = LZ4F_makeBlock(dstPtr, (size_t)(dstEnd - dstPtr),
                                          cctxPtr->tmpIn, toCompress,
                                          compress, cctxPtr->lz4CtxPtr, cctxPtr->prefs.compressionLevel,
                                          cctxPtr->cdict,
                                          cctxPtr->prefs.frameInfo.blockChecksumFlag);
            FORWARD_IF_ERROR_INVALIDATE(bytesWritten, cctxPtr);
            dstPtr += bytesWritten;
            srcPtr += toCompress;
            cctxPtr->tmpIn += toCompress;
        } else if (toCompress) {
            lastBlockCompressed = fromSrcBuffer;
            bytesWritten = LZ4F_compressBlocks_internal(dstPtr, (size_t)(dstEnd - dstPtr),
                                          srcPtr, toCompress, blockSize,
                                          compress, cctxPtr->lz4CtxPtr, cctxPtr->prefs.compressionLevel,
                                          cctxPtr->cdict,
                                          cctxPtr->prefs.frameInfo.blockChecksumFlag);
            FORWARD_IF_ERROR_INVALIDATE(bytesWritten, cctxPtr);
            dstPtr += bytesWritten;
            srcPtr += toCompress;
    }   }

    /* preserve dictionary within @tmpBuff whenever necessary */
    if ((cctxPtr->prefs.frameInfo.blockMode==LZ4F_blockLinked) && (lastBlockCompressed==fromSrcBuffer)) {
        /* note : after uncompressed blocks, the LZ4 state is empty (history dropped), so is the saved dictionary */
        if (compressOptionsPtr->stableSrc) {
            cctxPtr->tmpIn = cctxPtr->tmpBuff;  /* src is stable : dictionary remains in src across invocations */
        } else {
            LZ4F_localSaveDict(cctxPtr);
        }
    }

    /* keep tmpIn within limits */
    if (!(cctxPtr->prefs.autoFlush)  /* no autoflush : there may be some data left within internal buffer */
      && (cctxPtr->tmpIn + blockSize) > (cctxPtr->tmpBuff + cctxPtr->tmpBuffCapacity) )  /* not enough room to store next block */
    {
        /* only preserve 64KB within internal buffer. Ensures there is enough room for next block.
         * note: this situation necessarily implies lastBlockCompressed==fromTmpBuffer */
        LZ4F_localSaveDict(cctxPtr);
        assert((cctxPtr->tmpIn + blockSize) <= (cctxPtr->tmpBuff + cctxPtr->tmpBuffCapacity));
    }

    /* some input data left, necessarily < blockSize */
    if (srcPtr < srcEnd) {
        /* fill tmp buffer */
        size_t const sizeToCopy = (size_t)(srcEnd - srcPtr);
        memcpy(cctxPtr->tmpIn, srcPtr, sizeToCopy);
        cctxPtr->tmpInSize = sizeToCopy;
    }

    if (cctxPtr->prefs.frameInfo.contentChecksumFlag == LZ4F_contentChecksumEnabled)
        (void)XXH32_update(&(cctxPtr->xxh), srcBuffer, srcSize);

    cctxPtr->totalInSize += srcSize;
    return (size_t)(dstPtr - dstStart);
}

/*! LZ4F_compressUpdate() :
 *  LZ4F_compressUpdate() can be called repetitively to compress as much data as necessary.
 *  When successful, the function always entirely consumes @srcBuffer.
 *  src data is either buffered or compressed into @dstBuffer.
 *  With @compressOptionsPtr->uncompressed == 1, src data is stored as uncompressed blocks instead.
 *  With linked blocks, blocks compressed next don't reference data preceding them (single-threaded mode),
 *  or reference the uncompressed data (multi-threaded mode, see LZ4F_compressJob()).
 *  A switch between compressed and uncompressed input first flushes buffered data.
 *  Success is guaranteed when @dstCapacity >= LZ4F_compressBound(srcSize, preferencesPtr).
 *  Otherwise, the operation fails with dstCapacity_tooSmall if its output doesn't fit.
 * @compressOptionsPtr is optional : provide NULL to mean "default".
 * @return : the number of bytes written into dstBuffer. It can be zero, meaning input data was just buffered.
 *           or an error code if it fails (which can be tested using LZ4F_isError())
 *  After an error, the frame is invalidated, and must be restarted with LZ4F_compressBegin().
 */
size_t LZ4F_compressUpdate(LZ4F_cctx* cctxPtr,
                           void* dstBuffer, size_t dstCapacity,
                     const void* srcBuffer, size_t srcSize,
                     const LZ4F_compressOptions_t* compressOptionsPtr)
{
    LZ4F_BlockCompressMode_e mode;
    if (compressOptionsPtr == NULL) compressOptionsPtr = &k_cOptionsNull;
    mode = (compressOptionsPtr->uncompressed == 1) ? LZ4B_UNCOMPRESSED : LZ4B_COMPRESSED;
    if (cctxPtr->mtActive)
        return LZ4F_mt_update(cctxPtr, dstBuffer, dstCapacity, srcBuffer, srcSize, compressOptionsPtr, mode);
    return LZ4F_st_update(cctxPtr, dstBuffer, dstCapacity, srcBuffer, srcSize, compressOptionsPtr, mode);
}

/*! LZ4F_uncompressedUpdate() : deprecated
 *  LZ4F_compressUpdate() with option uncompressed.
 */
size_t LZ4F_uncompressedUpdate(LZ4F_cctx* cctxPtr,
                               void* dstBuffer, size_t dstCapacity,
                         const void* srcBuffer, size_t srcSize,
                         const LZ4F_compressOptions_t* compressOptionsPtr)
{
    LZ4F_compressOptions_t cOpts = k_cOptionsNull;
    if (compressOptionsPtr != NULL) cOpts = *compressOptionsPtr;
    cOpts.uncompressed = 1;
    return LZ4F_compressUpdate(cctxPtr, dstBuffer, dstCapacity, srcBuffer, srcSize, &cOpts);
}


/* emits pending output into @dst + @pos, as much as fits : block or footer (pending), then jobs (multi-threaded mode).
 * @return : new position, or an error code (the frame is then invalidated) */
static size_t LZ4F_emitPending(LZ4F_cctx_t* cctx, BYTE* dst, size_t dstCapacity, size_t pos)
{
    pos += LZ4F_copyPending(cctx, dst, dstCapacity, pos);
    if (cctx->mtActive && (cctx->pendingSize == 0)) {
        size_t const r = LZ4F_mt_emitPartial(cctx->mt, dst, dstCapacity, pos);
        FORWARD_IF_ERROR_INVALIDATE(r, cctx);
        pos += r;
    }
    return pos;
}

static int LZ4F_outputPending(const LZ4F_cctx_t* cctx)
{
    return (cctx->pendingSize > 0) || (cctx->mtActive && (cctx->mt->nbSubmitted > 0));
}

/* @return : 1 if LZ4F_flush() has output to produce */
static int LZ4F_somethingToFlush(const LZ4F_cctx_t* cctx)
{
    if (LZ4F_outputPending(cctx)) return 1;
    if (cctx->mtActive) return cctx->mt->filling;
    return cctx->tmpInSize > 0;
}

/* flush point : input retained within cctx is compressed, and no longer read.
 * @return : nb of bytes written into @dst, or an error code (the frame is then invalidated) */
static size_t LZ4F_flushPoint(LZ4F_cctx_t* cctx, BYTE* dst, size_t dstCapacity)
{
    size_t r = 0;
    if (cctx->mtActive) {
        LZ4F_mt_flushPoint(cctx->mt);
    } else {
        r = LZ4F_flushBlockPartial(cctx, dst, dstCapacity);
    }
    if (!LZ4F_isError(r)) cctx->flushStage = 1;
    return r;
}

/*! LZ4F_flush() :
 *  When compressed data must be sent immediately, without waiting for a block to be filled,
 *  invoke LZ4F_flush(), which will immediately compress any remaining data stored within LZ4F_cctx.
 *  The invocation fills @dstBuffer, or completes the flush :
 *  as long as it returns @dstCapacity, output may remain : invoke LZ4F_flush() again.
 *  The first invocation sets the flush point : input retained within cctx is compressed (all jobs completed).
 *  The result of the function is the number of bytes written into dstBuffer.
 *  It can be zero, this means there was no data left within LZ4F_cctx.
 *  The function outputs an error code if it fails (can be tested using LZ4F_isError())
 *  A single invocation completes the flush when dstCapacity >= LZ4F_compressBound(0, preferencesPtr).
 *  dstCapacity == 0 is rejected (dstCapacity_tooSmall) when there is something to flush, without modifying the frame.
 *  LZ4F_compressOptions_t* is optional : NULL is a valid argument.
 */
size_t LZ4F_flush(LZ4F_cctx* cctxPtr,
                  void* dstBuffer, size_t dstCapacity,
            const LZ4F_compressOptions_t* compressOptionsPtr)
{
    BYTE* const dst = (BYTE*)dstBuffer;
    size_t pos = 0;
    DEBUGLOG(5, "LZ4F_flush (dstCapacity=%u)", (unsigned)dstCapacity);
    (void)compressOptionsPtr;   /* not useful (yet) */
    RETURN_ERROR_IF(cctxPtr->cStage != 1, compressionState_uninitialized);
    if (dstCapacity == 0) {
        /* no progress possible : rejected, the frame continues */
        RETURN_ERROR_IF(LZ4F_somethingToFlush(cctxPtr), dstCapacity_tooSmall);
        cctxPtr->flushStage = 0;
        return 0;
    }
    if (cctxPtr->flushStage == 0) {
        pos = LZ4F_flushPoint(cctxPtr, dst, dstCapacity);
        FORWARD_IF_ERROR(pos);   /* already invalidated */
    }
    pos = LZ4F_emitPending(cctxPtr, dst, dstCapacity, pos);
    FORWARD_IF_ERROR(pos);   /* already invalidated */
    if (pos < dstCapacity) cctxPtr->flushStage = 0;   /* completed */
    return pos;
}


/*! LZ4F_compressEnd() :
 *  When you want to properly finish the compressed frame, just call LZ4F_compressEnd().
 *  It will flush whatever data remained within compressionContext (like LZ4_flush())
 *  but also properly finalize the frame, with an endMark and an (optional) checksum.
 *  The invocation fills @dstBuffer, or completes the frame :
 *  as long as it returns @dstCapacity, output may remain : invoke LZ4F_compressEnd() again.
 *  LZ4F_compressOptions_t structure is optional : you can provide NULL as argument.
 * @return: the number of bytes written into dstBuffer (the last invocation writes the endMark, unless already written)
 *       or an error code if it fails (can be tested using LZ4F_isError())
 *  Once completed, the context can be used again to compress a new frame, starting with LZ4F_compressBegin().
 */
size_t LZ4F_compressEnd(LZ4F_cctx* cctxPtr,
                        void* dstBuffer, size_t dstCapacity,
                  const LZ4F_compressOptions_t* compressOptionsPtr)
{
    BYTE* const dst = (BYTE*)dstBuffer;
    size_t pos = 0;
    DEBUGLOG(5,"LZ4F_compressEnd: dstCapacity=%u", (unsigned)dstCapacity);
    (void)compressOptionsPtr;   /* not useful (yet) */
    RETURN_ERROR_IF((cctxPtr->cStage != 1) && (cctxPtr->cStage != 2), compressionState_uninitialized);
    RETURN_ERROR_IF(dstCapacity == 0, dstCapacity_tooSmall);   /* no progress possible : rejected, the frame continues */

    if (cctxPtr->cStage == 1) {
        /* end of frame : no more input */
        if (cctxPtr->prefs.frameInfo.contentSize && (cctxPtr->prefs.frameInfo.contentSize != cctxPtr->totalInSize)) {
            LZ4F_invalidateFrame(cctxPtr);
            RETURN_ERROR(frameSize_wrong);
        }
        cctxPtr->cStage = 2;
        if (cctxPtr->flushStage == 0) {
            pos = LZ4F_flushPoint(cctxPtr, dst, dstCapacity);
            FORWARD_IF_ERROR(pos);   /* already invalidated */
    }   }

    if (cctxPtr->flushStage == 1) {
        pos = LZ4F_emitPending(cctxPtr, dst, dstCapacity, pos);
        FORWARD_IF_ERROR(pos);   /* already invalidated */
        if (LZ4F_outputPending(cctxPtr)) return pos;   /* dstBuffer is full */
        /* footer : endMark, then optional content checksum */
        LZ4F_writeLE32(cctxPtr->footer, 0);
        cctxPtr->pendingSize = 4;
        if (cctxPtr->prefs.frameInfo.contentChecksumFlag == LZ4F_contentChecksumEnabled) {
            U32 const xxh = XXH32_digest(&(cctxPtr->xxh));
            DEBUGLOG(5,"Writing 32-bit content checksum (0x%0X)", xxh);
            LZ4F_writeLE32(cctxPtr->footer + 4, xxh);
            cctxPtr->pendingSize = 8;
        }
        cctxPtr->pending = cctxPtr->footer;
        cctxPtr->flushStage = 2;
    }

    if (cctxPtr->flushStage == 2) {
        pos += LZ4F_copyPending(cctxPtr, dst, dstCapacity, pos);
        if (cctxPtr->pendingSize) return pos;   /* dstBuffer is full */
        cctxPtr->flushStage = 3;
    }

    assert(cctxPtr->flushStage == 3);
    if (pos < dstCapacity) {
        /* completed : the context is available for a new frame */
        cctxPtr->cStage = 0;
        cctxPtr->flushStage = 0;
    }   /* otherwise, completed, but dstBuffer is full : next invocation returns 0 */
    return pos;
}


/*-***************************************************
*   Frame Decompression
*****************************************************/

typedef enum {
    dstage_getFrameHeader=0, dstage_storeFrameHeader,
    dstage_init,
    dstage_getBlockHeader,
    dstage_copyDirect, dstage_getBlockChecksum,
    dstage_getCBlock,
    dstage_flushOut,
    dstage_getSuffix,
    dstage_getSFrameSize,
    dstage_skipSkippable
} dStage_t;

struct LZ4F_dctx_s {
    LZ4F_CustomMem cmem;
    LZ4F_frameInfo_t frameInfo;
    U32    version;
    dStage_t dStage;
    U64    frameRemainingSize;
    size_t maxBlockSize;
    size_t tmpOutBufferCapacity;
    BYTE*  tmpIn;
    size_t tmpInCapacity;
    size_t tmpInSize;
    size_t tmpInTarget;
    BYTE*  tmpOutBuffer;
    const BYTE* dict;
    size_t dictSize;
    BYTE*  tmpOut;
    size_t tmpOutSize;
    size_t tmpOutStart;
    XXH32_state_t xxh;
    XXH32_state_t blockChecksum;
    int    skipBlockChecksum;
    int    skipContentChecksum;
    BYTE   header[LZ4F_HEADER_SIZE_MAX];
};  /* typedef'd to LZ4F_dctx in lz4frame.h */


LZ4F_dctx* LZ4F_createDecompressionContext_advanced(LZ4F_CustomMem customMem, unsigned version)
{
    LZ4F_dctx* const dctx = (LZ4F_dctx*)LZ4F_calloc(sizeof(LZ4F_dctx), customMem);
    if (dctx == NULL) return NULL;

    dctx->cmem = customMem;
    dctx->version = version;
    return dctx;
}

/*! LZ4F_createDecompressionContext() :
 *  Create a decompressionContext object, which will track all decompression operations.
 *  Provides a pointer to a fully allocated and initialized LZ4F_decompressionContext object.
 *  Object can later be released using LZ4F_freeDecompressionContext().
 * @return : if != 0, there was an error during context creation.
 */
LZ4F_errorCode_t
LZ4F_createDecompressionContext(LZ4F_dctx** LZ4F_decompressionContextPtr, unsigned versionNumber)
{
    assert(LZ4F_decompressionContextPtr != NULL);  /* violation of narrow contract */
    RETURN_ERROR_IF(LZ4F_decompressionContextPtr == NULL, parameter_null);  /* in case it nonetheless happen in production */

    *LZ4F_decompressionContextPtr = LZ4F_createDecompressionContext_advanced(LZ4F_defaultCMem, versionNumber);
    if (*LZ4F_decompressionContextPtr == NULL) {  /* failed allocation */
        RETURN_ERROR(allocation_failed);
    }
    return LZ4F_OK_NoError;
}

LZ4F_errorCode_t LZ4F_freeDecompressionContext(LZ4F_dctx* dctx)
{
    LZ4F_errorCode_t result = LZ4F_OK_NoError;
    if (dctx != NULL) {   /* can accept NULL input, like free() */
      result = (LZ4F_errorCode_t)dctx->dStage;
      LZ4F_free(dctx->tmpIn, dctx->cmem);
      LZ4F_free(dctx->tmpOutBuffer, dctx->cmem);
      LZ4F_free(dctx, dctx->cmem);
    }
    return result;
}

size_t LZ4F_dctx_size(const LZ4F_dctx* dctx) {
    if (dctx == NULL) {
        return 0;
    }
    return sizeof(*dctx) + dctx->tmpInCapacity + dctx->tmpOutBufferCapacity;
}


/*==---   Streaming Decompression operations   ---==*/
void LZ4F_resetDecompressionContext(LZ4F_dctx* dctx)
{
    DEBUGLOG(5, "LZ4F_resetDecompressionContext");
    dctx->dStage = dstage_getFrameHeader;
    dctx->dict = NULL;
    dctx->dictSize = 0;
    dctx->skipBlockChecksum = 0;
    dctx->skipContentChecksum = 0;
    dctx->frameRemainingSize = 0;
}


/*! LZ4F_decodeHeader() :
 *  input   : `src` points at the **beginning of the frame**, srcSize >= minFHSize
 *  output  : set internal values of dctx, such as
 *            dctx->frameInfo and dctx->dStage.
 *  @return : nb Bytes read from src (necessarily <= srcSize)
 *            or an error code (testable with LZ4F_isError())
 */
static size_t LZ4F_decodeHeader(LZ4F_dctx* dctx, const void* src, size_t srcSize)
{
    unsigned blockMode, blockChecksumFlag, contentSizeFlag, contentChecksumFlag, dictIDFlag, blockSizeID;
    size_t frameHeaderSize;
    const BYTE* srcPtr = (const BYTE*)src;

    DEBUGLOG(5, "LZ4F_decodeHeader");
    assert(srcSize >= minFHSize);   /* minimal frame header size */
    MEM_INIT(&(dctx->frameInfo), 0, sizeof(dctx->frameInfo));

    /* special case : skippable frames */
    if ((LZ4F_readLE32(srcPtr) & 0xFFFFFFF0U) == LZ4F_MAGIC_SKIPPABLE_START) {
        dctx->frameInfo.frameType = LZ4F_skippableFrame;
        dctx->dStage = dstage_getSFrameSize;
        if (src == (void*)(dctx->header)) {
            /* the beginning of the frame size field is already gathered in header+4 */
            assert(srcSize <= 8);
            dctx->tmpInSize = srcSize - 4;
            return srcSize;
        } else {
            dctx->tmpInSize = 0;
            return 4;
    }   }

    /* control magic number */
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (LZ4F_readLE32(srcPtr) != LZ4F_MAGICNUMBER) {
        DEBUGLOG(4, "frame header error : unknown magic number");
        RETURN_ERROR(frameType_unknown);
    }
#endif
    dctx->frameInfo.frameType = LZ4F_frame;

    /* Flags */
    {   U32 const FLG = srcPtr[4];
        U32 const version = (FLG>>6) & _2BITS;
        blockChecksumFlag = (FLG>>4) & _1BIT;
        blockMode = (FLG>>5) & _1BIT;
        contentSizeFlag = (FLG>>3) & _1BIT;
        contentChecksumFlag = (FLG>>2) & _1BIT;
        dictIDFlag = FLG & _1BIT;
        /* validate */
        if (((FLG>>1)&_1BIT) != 0) RETURN_ERROR(reservedFlag_set); /* Reserved bit */
        if (version != 1) RETURN_ERROR(headerVersion_wrong);       /* Version Number, only supported value */
    }
    DEBUGLOG(6, "contentSizeFlag: %u", contentSizeFlag);

    /* Frame Header Size */
    frameHeaderSize = minFHSize + (contentSizeFlag?8:0) + (dictIDFlag?4:0);

    if (srcSize < frameHeaderSize) {
        /* only the minimal header is gathered into dctx->header : gather the rest */
        assert(srcPtr == dctx->header);
        assert(dctx->dStage == dstage_storeFrameHeader);
        assert(dctx->tmpInSize == srcSize);
        dctx->tmpInTarget = frameHeaderSize;
        return srcSize;
    }

    {   U32 const BD = srcPtr[5];
        blockSizeID = (BD>>4) & _3BITS;
        /* validate */
        if (((BD>>7)&_1BIT) != 0) RETURN_ERROR(reservedFlag_set);   /* Reserved bit */
        if (blockSizeID < 4) RETURN_ERROR(maxBlockSize_invalid);    /* 4-7 only supported values for the time being */
        if (((BD>>0)&_4BITS) != 0) RETURN_ERROR(reservedFlag_set);  /* Reserved bits */
    }

    /* check header */
    assert(frameHeaderSize > 5);
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    {   BYTE const HC = LZ4F_headerChecksum(srcPtr+4, frameHeaderSize-5);
        RETURN_ERROR_IF(HC != srcPtr[frameHeaderSize-1], headerChecksum_invalid);
    }
#endif

    /* save */
    dctx->frameInfo.blockMode = (LZ4F_blockMode_t)blockMode;
    dctx->frameInfo.blockChecksumFlag = (LZ4F_blockChecksum_t)blockChecksumFlag;
    dctx->frameInfo.contentChecksumFlag = (LZ4F_contentChecksum_t)contentChecksumFlag;
    dctx->frameInfo.blockSizeID = (LZ4F_blockSizeID_t)blockSizeID;
    dctx->maxBlockSize = LZ4F_getBlockSize((LZ4F_blockSizeID_t)blockSizeID);
    if (contentSizeFlag) {
        dctx->frameRemainingSize = dctx->frameInfo.contentSize = LZ4F_readLE64(srcPtr+6);
    }
    if (dictIDFlag)
        dctx->frameInfo.dictID = LZ4F_readLE32(srcPtr + frameHeaderSize - 5);

    dctx->dStage = dstage_init;

    return frameHeaderSize;
}


/*! LZ4F_headerSize() :
 * @return : size of frame header
 *           or an error code, which can be tested using LZ4F_isError()
 */
size_t LZ4F_headerSize(const void* src, size_t srcSize)
{
    RETURN_ERROR_IF(src == NULL, srcPtr_wrong);

    /* minimal srcSize to determine header size */
    if (srcSize < LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH)
        RETURN_ERROR(frameHeader_incomplete);

    /* special case : skippable frames */
    if ((LZ4F_readLE32(src) & 0xFFFFFFF0U) == LZ4F_MAGIC_SKIPPABLE_START)
        return 8;

    /* control magic number */
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (LZ4F_readLE32(src) != LZ4F_MAGICNUMBER)
        RETURN_ERROR(frameType_unknown);
#endif

    /* Frame Header Size */
    {   BYTE const FLG = ((const BYTE*)src)[4];
        U32 const contentSizeFlag = (FLG>>3) & _1BIT;
        U32 const dictIDFlag = FLG & _1BIT;
        return minFHSize + (contentSizeFlag?8:0) + (dictIDFlag?4:0);
    }
}

/*! LZ4F_getFrameInfo() :
 *  This function extracts frame parameters (max blockSize, frame checksum, etc.).
 *  Usage is optional. Objective is to provide relevant information for allocation purposes.
 *  This function works in 2 situations :
 *   - At the beginning of a new frame, in which case it will decode this information from `srcBuffer`, and start the decoding process.
 *     Amount of input data provided must be large enough to successfully decode the frame header.
 *     A header size is variable, but is guaranteed to be <= LZ4F_HEADER_SIZE_MAX bytes. It's possible to provide more input data than this minimum.
 *   - After decoding has been started. In which case, no input is read, frame parameters are extracted from dctx.
 *  The number of bytes consumed from srcBuffer will be updated within *srcSizePtr (necessarily <= original value).
 *  Decompression must resume from (srcBuffer + *srcSizePtr).
 * @return : an hint about how many srcSize bytes LZ4F_decompress() expects for next call,
 *           or an error code which can be tested using LZ4F_isError()
 *  note 1 : in case of error, dctx is not modified. Decoding operations can resume from where they stopped.
 *  note 2 : frame parameters are *copied into* an already allocated LZ4F_frameInfo_t structure.
 */
LZ4F_errorCode_t LZ4F_getFrameInfo(LZ4F_dctx* dctx,
                                   LZ4F_frameInfo_t* frameInfoPtr,
                             const void* srcBuffer, size_t* srcSizePtr)
{
    assert(dctx != NULL);
    RETURN_ERROR_IF(frameInfoPtr == NULL, parameter_null);
    RETURN_ERROR_IF(srcSizePtr == NULL, parameter_null);

    LZ4F_STATIC_ASSERT(dstage_getFrameHeader < dstage_storeFrameHeader);
    if (dctx->dStage > dstage_storeFrameHeader) {
        /* frameInfo already decoded */
        size_t o=0, i=0;
        *srcSizePtr = 0;
        *frameInfoPtr = dctx->frameInfo;
        /* returns : recommended nb of bytes for LZ4F_decompress() */
        return LZ4F_decompress(dctx, NULL, &o, NULL, &i, NULL);
    } else {
        if (dctx->dStage == dstage_storeFrameHeader) {
            /* frame decoding already started, in the middle of header => automatic fail */
            *srcSizePtr = 0;
            RETURN_ERROR(frameDecoding_alreadyStarted);
        } else {
            size_t const hSize = LZ4F_headerSize(srcBuffer, *srcSizePtr);
            if (LZ4F_isError(hSize)) { *srcSizePtr=0; return hSize; }
            if (*srcSizePtr < hSize) {
                *srcSizePtr=0;
                RETURN_ERROR(frameHeader_incomplete);
            }

            {   size_t decodeResult = LZ4F_decodeHeader(dctx, srcBuffer, hSize);
                if (LZ4F_isError(decodeResult)) {
                    *srcSizePtr = 0;
                } else {
                    *srcSizePtr = decodeResult;
                    decodeResult = BHSize;   /* block header size */
                }
                *frameInfoPtr = dctx->frameInfo;
                return decodeResult;
    }   }   }
}


/* LZ4F_moveDictBeforeTmpOut() :
 * The block decoded in tmpOut is being flushed, while the history preceding it is elsewhere.
 * Copy the end of this history in front of tmpOut (up to 64 KB with the block), so that the dictionary
 * becomes contiguous within tmpOutBuffer, ending with the flushed part of tmpOut.
 * @flushSize : bytes being flushed, not yet counted in tmpOutStart
 */
static void LZ4F_moveDictBeforeTmpOut(LZ4F_dctx* dctx, size_t flushSize)
{
    size_t const preserveSize = (size_t)(dctx->tmpOut - dctx->tmpOutBuffer);
    size_t copySize = 64 KB - dctx->tmpOutSize;
    const BYTE* const oldDictEnd = dctx->dict + dctx->dictSize - dctx->tmpOutStart;
    if (dctx->tmpOutSize > 64 KB) copySize = 0;
    if (copySize > preserveSize) copySize = preserveSize;
    assert(dctx->tmpOutBuffer != NULL);

    memcpy(dctx->tmpOutBuffer + preserveSize - copySize, oldDictEnd - copySize, copySize);

    dctx->dict = dctx->tmpOutBuffer;
    dctx->dictSize = preserveSize + dctx->tmpOutStart + flushSize;
}

/* LZ4F_updateDict() :
 * only used for LZ4F_blockLinked mode
 * Condition : @dstPtr != NULL
 */
static void LZ4F_updateDict(LZ4F_dctx* dctx,
                      const BYTE* dstPtr, size_t dstSize, const BYTE* dstBufferStart,
                      unsigned withinTmp)
{
    assert(dstPtr != NULL);
    if (dctx->dictSize==0) dctx->dict = (const BYTE*)dstPtr;  /* will lead to prefix mode */
    assert(dctx->dict != NULL);

    if (dctx->dict + dctx->dictSize == dstPtr) {  /* prefix mode, everything within dstBuffer */
        dctx->dictSize += dstSize;
        return;
    }

    assert(dstPtr >= dstBufferStart);
    if ((size_t)(dstPtr - dstBufferStart) + dstSize >= 64 KB) {  /* history in dstBuffer becomes large enough to become dictionary */
        dctx->dict = (const BYTE*)dstBufferStart;
        dctx->dictSize = (size_t)(dstPtr - dstBufferStart) + dstSize;
        return;
    }

    assert(dstSize < 64 KB);   /* if dstSize >= 64 KB, dictionary would be set into dstBuffer directly */

    /* dstBuffer does not contain whole useful history (64 KB), so it must be saved within tmpOutBuffer */
    assert(dctx->tmpOutBuffer != NULL);

    if (withinTmp && (dctx->dict == dctx->tmpOutBuffer)) {   /* continue history within tmpOutBuffer */
        /* withinTmp expectation : content of [dstPtr,dstSize] is same as [dict+dictSize,dstSize], so we just extend it */
        assert(dctx->dict + dctx->dictSize == dctx->tmpOut + dctx->tmpOutStart);
        dctx->dictSize += dstSize;
        return;
    }

    if (withinTmp) { /* copy relevant dict portion in front of tmpOut within tmpOutBuffer */
        LZ4F_moveDictBeforeTmpOut(dctx, dstSize);
        return;
    }

    if (dctx->dict == dctx->tmpOutBuffer) {    /* copy dst into tmp to complete dict */
        if (dctx->dictSize + dstSize > dctx->tmpOutBufferCapacity) {  /* tmp buffer not large enough */
            size_t const preserveSize = 64 KB - dstSize;
            memcpy(dctx->tmpOutBuffer, dctx->dict + dctx->dictSize - preserveSize, preserveSize);
            dctx->dictSize = preserveSize;
        }
        memcpy(dctx->tmpOutBuffer + dctx->dictSize, dstPtr, dstSize);
        dctx->dictSize += dstSize;
        return;
    }

    /* join dict & dest into tmp */
    {   size_t preserveSize = 64 KB - dstSize;
        if (preserveSize > dctx->dictSize) preserveSize = dctx->dictSize;
        memcpy(dctx->tmpOutBuffer, dctx->dict + dctx->dictSize - preserveSize, preserveSize);
        memcpy(dctx->tmpOutBuffer + preserveSize, dstPtr, dstSize);
        dctx->dict = dctx->tmpOutBuffer;
        dctx->dictSize = preserveSize + dstSize;
    }
}


/* LZ4F_endMarkFollows() :
 * the frame's endMark is within input, right after the block just output (after @skip bytes : its block checksum, if not consumed yet) :
 * no block follows, so the history of linked blocks is no longer needed (saving it can cost as much as decoding a small block).
 * Only valid once the block is entirely output : the same invocation then reads the endMark, which ends the frame. */
static int LZ4F_endMarkFollows(const BYTE* srcPtr, const BYTE* srcEnd, size_t skip)
{
    return ((size_t)(srcEnd - srcPtr) >= skip + BHSize) && (LZ4F_readLE32(srcPtr + skip) == 0);
}

/* LZ4F_decodeBlock() :
 * Decode compressed block @src (block checksum excluded) into @dst (capacity >= maxBlockSize),
 * using current dictionary, and update content checksum and remaining frame size.
 * @return : decoded size, or an error code (testable with LZ4F_isError())
 */
static size_t LZ4F_decodeBlock(LZ4F_dctx* dctx, const BYTE* src, size_t srcSize, BYTE* dst)
{
    const char* dict = (const char*)dctx->dict;
    size_t dictSize = dctx->dictSize;
    int decodedSize;
    assert(dst != NULL);
    if (dict && dictSize > 1 GB) {
        /* the dictSize param is an int, avoid truncation / sign issues */
        dict += dictSize - 64 KB;
        dictSize = 64 KB;
    }
    decodedSize = LZ4_decompress_safe_usingDict(
            (const char*)src, (char*)dst,
            (int)srcSize, (int)dctx->maxBlockSize,
            dict, (int)dictSize);
    RETURN_ERROR_IF(decodedSize < 0, decompressionFailed);
    if (dctx->frameInfo.contentChecksumFlag && !dctx->skipContentChecksum)
        XXH32_update(&(dctx->xxh), dst, (size_t)decodedSize);
    if (dctx->frameInfo.contentSize)
        dctx->frameRemainingSize -= (size_t)decodedSize;
    return (size_t)decodedSize;
}


/* LZ4F_gatherInput() :
 * Provides @wanted contiguous input bytes : directly from src if they are all available and none is buffered yet,
 * otherwise accumulated into @buffer, over as many calls as needed (dctx->tmpInSize bytes buffered so far).
 * @return : pointer to the @wanted bytes (*srcPtrPtr is moved past consumed input),
 *           or NULL if input is exhausted before (all of it is then buffered).
 */
static const BYTE*
LZ4F_gatherInput(LZ4F_dctx* dctx, BYTE* buffer, size_t wanted,
                 const BYTE** srcPtrPtr, const BYTE* srcEnd)
{
    const BYTE* const srcPtr = *srcPtrPtr;
    size_t const available = (size_t)(srcEnd - srcPtr);
    if ((dctx->tmpInSize == 0) && (available >= wanted)) {
        *srcPtrPtr = srcPtr + wanted;
        return srcPtr;
    }
    {   size_t const sizeToCopy = MIN(wanted - dctx->tmpInSize, available);
        memcpy(buffer + dctx->tmpInSize, srcPtr, sizeToCopy);
        dctx->tmpInSize += sizeToCopy;
        *srcPtrPtr = srcPtr + sizeToCopy;
    }
    if (dctx->tmpInSize < wanted) return NULL;
    dctx->tmpInSize = 0;   /* ready for next gathering */
    return buffer;
}


/*! LZ4F_decompress() :
 *  Call this function repetitively to regenerate compressed data in srcBuffer.
 *  The function will attempt to decode up to *srcSizePtr bytes from srcBuffer
 *  into dstBuffer of capacity *dstSizePtr.
 *
 *  The number of bytes regenerated into dstBuffer will be provided within *dstSizePtr (necessarily <= original value).
 *
 *  The number of bytes effectively read from srcBuffer will be provided within *srcSizePtr (necessarily <= original value).
 *  If number of bytes read is < number of bytes provided, then decompression operation is not complete.
 *  Remaining data will have to be presented again in a subsequent invocation.
 *
 *  The function result is an hint of the better srcSize to use for next call to LZ4F_decompress.
 *  Schematically, it's the size of the current (or remaining) compressed block + header of next block.
 *  Respecting the hint provides a small boost to performance, since it allows less buffer shuffling.
 *  Note that this is just a hint, and it's always possible to any srcSize value.
 *  When a frame is fully decoded, @return will be 0.
 *  If decompression failed, @return is an error code which can be tested using LZ4F_isError().
 */
size_t LZ4F_decompress(LZ4F_dctx* dctx,
                       void* dstBuffer, size_t* dstSizePtr,
                       const void* srcBuffer, size_t* srcSizePtr,
                       const LZ4F_decompressOptions_t* decompressOptionsPtr)
{
    static const BYTE emptySrc = 0;   /* NULL srcBuffer (no input, used by LZ4F_getFrameInfo()) points here instead :
                                       * NULL+0 and memcpy(dst, NULL, 0) are UB */
    LZ4F_decompressOptions_t optionsNull;
    const BYTE* const srcStart = srcBuffer ? (const BYTE*)srcBuffer : &emptySrc;
    const BYTE* const srcEnd = srcBuffer ? srcStart + *srcSizePtr : srcStart;
    const BYTE* srcPtr = srcStart;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* const dstEnd = dstStart ? dstStart + *dstSizePtr : NULL;
    BYTE* dstPtr = dstStart;
    const BYTE* selectedIn = NULL;
    unsigned doAnotherStage = 1;
    size_t nextSrcSizeHint = 1;


    DEBUGLOG(5, "LZ4F_decompress: src[%p](%u) => dst[%p](%u)",
            srcBuffer, (unsigned)*srcSizePtr, dstBuffer, (unsigned)*dstSizePtr);
    if (dstBuffer == NULL) assert(*dstSizePtr == 0);
    if (srcBuffer == NULL) assert(*srcSizePtr == 0);
    MEM_INIT(&optionsNull, 0, sizeof(optionsNull));
    if (decompressOptionsPtr==NULL) decompressOptionsPtr = &optionsNull;
    *srcSizePtr = 0;
    *dstSizePtr = 0;
    assert(dctx != NULL);
    if (decompressOptionsPtr->skipChecksums) {   /* once set, disable for the remainder of the frame */
        dctx->skipBlockChecksum = 1;
        dctx->skipContentChecksum = 1;
    }

    /* behaves as a state machine */

    while (doAnotherStage) {

        switch(dctx->dStage)
        {

        case dstage_getFrameHeader:
            DEBUGLOG(6, "dstage_getFrameHeader");
            if ((size_t)(srcEnd-srcPtr) >= maxFHSize) {  /* enough to decode - shortcut */
                size_t const hSize = LZ4F_decodeHeader(dctx, srcPtr, (size_t)(srcEnd-srcPtr));  /* will update dStage appropriately */
                if (LZ4F_isError(hSize)) { nextSrcSizeHint = hSize; goto _output; }
                srcPtr += hSize;
                break;
            }
            dctx->tmpInSize = 0;
            if (srcEnd-srcPtr == 0) return minFHSize;   /* 0-size input */
            dctx->tmpInTarget = minFHSize;   /* minimum size to decode header */
            dctx->dStage = dstage_storeFrameHeader;
            /* fall-through */

        case dstage_storeFrameHeader:
            DEBUGLOG(6, "dstage_storeFrameHeader");
            {   size_t const sizeToCopy = MIN(dctx->tmpInTarget - dctx->tmpInSize, (size_t)(srcEnd - srcPtr));
                memcpy(dctx->header + dctx->tmpInSize, srcPtr, sizeToCopy);
                dctx->tmpInSize += sizeToCopy;
                srcPtr += sizeToCopy;
            }
            if (dctx->tmpInSize < dctx->tmpInTarget) {
                nextSrcSizeHint = (dctx->tmpInTarget - dctx->tmpInSize) + BHSize;   /* rest of header + nextBlockHeader */
                doAnotherStage = 0;   /* not enough src data, ask for some more */
                break;
            }
            {   size_t const hSize = LZ4F_decodeHeader(dctx, dctx->header, dctx->tmpInTarget); /* will update dStage appropriately */
                if (LZ4F_isError(hSize)) { nextSrcSizeHint = hSize; goto _output; }
            }
            break;

        case dstage_init:
            DEBUGLOG(6, "dstage_init");
            if (dctx->frameInfo.contentChecksumFlag) (void)XXH32_reset(&(dctx->xxh), 0);
            /* internal buffers allocation */
            {   size_t const tmpInNeeded = dctx->maxBlockSize + BFSize /* block checksum */;
                size_t const tmpOutNeeded = dctx->maxBlockSize
                    + ((dctx->frameInfo.blockMode==LZ4F_blockLinked) ? 128 KB : 0);
                if ( (tmpInNeeded > dctx->tmpInCapacity)
                  || (tmpOutNeeded > dctx->tmpOutBufferCapacity) ) {   /* tmp buffers too small */
                    /* capacities remain 0 until allocation succeeds,
                     * so that allocation will be re-attempted on next entry */
                    LZ4F_free(dctx->tmpIn, dctx->cmem);
                    dctx->tmpInCapacity = 0;
                    dctx->tmpIn = (BYTE*)LZ4F_malloc(tmpInNeeded, dctx->cmem);
                    if (dctx->tmpIn == NULL) {
                        nextSrcSizeHint = LZ4F_returnErrorCode(LZ4F_ERROR_allocation_failed);
                        goto _output;
                    }
                    dctx->tmpInCapacity = tmpInNeeded;
                    LZ4F_free(dctx->tmpOutBuffer, dctx->cmem);
                    dctx->tmpOutBufferCapacity = 0;
                    dctx->tmpOutBuffer= (BYTE*)LZ4F_malloc(tmpOutNeeded, dctx->cmem);
                    if (dctx->tmpOutBuffer== NULL) {
                        nextSrcSizeHint = LZ4F_returnErrorCode(LZ4F_ERROR_allocation_failed);
                        goto _output;
                    }
                    dctx->tmpOutBufferCapacity = tmpOutNeeded;
            }   }
            dctx->tmpInSize = 0;
            dctx->tmpInTarget = 0;
            dctx->tmpOut = dctx->tmpOutBuffer;
            dctx->tmpOutStart = 0;
            dctx->tmpOutSize = 0;

            dctx->dStage = dstage_getBlockHeader;
            /* fall-through */

        case dstage_getBlockHeader:
            selectedIn = LZ4F_gatherInput(dctx, dctx->tmpIn, BHSize, &srcPtr, srcEnd);
            if (selectedIn == NULL) {   /* not enough input for cBlockSize */
                nextSrcSizeHint = BHSize - dctx->tmpInSize;
                doAnotherStage = 0;
                break;
            }

            /* decode block header */
            {   U32 const blockHeader = LZ4F_readLE32(selectedIn);
                size_t const nextCBlockSize = blockHeader & 0x7FFFFFFFU;
                size_t const crcSize = dctx->frameInfo.blockChecksumFlag * BFSize;
                if (blockHeader==0) {  /* frameEnd signal, no more block */
                    DEBUGLOG(5, "end of frame");
                    dctx->dStage = dstage_getSuffix;
                    break;
                }
                if (nextCBlockSize > dctx->maxBlockSize) {
                    nextSrcSizeHint = LZ4F_returnErrorCode(LZ4F_ERROR_maxBlockSize_invalid);
                    goto _output;
                }
                if (blockHeader & LZ4F_BLOCKUNCOMPRESSED_FLAG) {
                    /* next block is uncompressed */
                    dctx->tmpInTarget = nextCBlockSize;
                    DEBUGLOG(5, "next block is uncompressed (size %u)", (U32)nextCBlockSize);
                    if (dctx->frameInfo.blockChecksumFlag) {
                        (void)XXH32_reset(&dctx->blockChecksum, 0);
                    }
                    dctx->dStage = dstage_copyDirect;
                    break;
                }
                /* next block is a compressed block */
                dctx->tmpInTarget = nextCBlockSize + crcSize;
                dctx->dStage = dstage_getCBlock;
                if (dstPtr==dstEnd || srcPtr==srcEnd) {
                    nextSrcSizeHint = BHSize + nextCBlockSize + crcSize;
                    doAnotherStage = 0;
                }
                break;
            }

        case dstage_copyDirect:   /* uncompressed block */
            DEBUGLOG(6, "dstage_copyDirect");
            {   size_t sizeToCopy;
                if (dstPtr == NULL) {
                    sizeToCopy = 0;
                } else {
                    size_t const minBuffSize = MIN((size_t)(srcEnd-srcPtr), (size_t)(dstEnd-dstPtr));
                    sizeToCopy = MIN(dctx->tmpInTarget, minBuffSize);
                    memcpy(dstPtr, srcPtr, sizeToCopy);
                    if (dctx->frameInfo.blockChecksumFlag && !dctx->skipBlockChecksum)
                        (void)XXH32_update(&dctx->blockChecksum, srcPtr, sizeToCopy);
                    if (dctx->frameInfo.contentChecksumFlag && !dctx->skipContentChecksum)
                        (void)XXH32_update(&dctx->xxh, srcPtr, sizeToCopy);
                    if (dctx->frameInfo.contentSize)
                        dctx->frameRemainingSize -= sizeToCopy;

                    /* history management (linked blocks only), unless the frame ends with this block */
                    if ( (dctx->frameInfo.blockMode == LZ4F_blockLinked)
                      && !( (sizeToCopy == dctx->tmpInTarget)
                         && LZ4F_endMarkFollows(srcPtr + sizeToCopy, srcEnd, dctx->frameInfo.blockChecksumFlag * BFSize) ) ) {
                        LZ4F_updateDict(dctx, dstPtr, sizeToCopy, dstStart, 0);
                    }
                    srcPtr += sizeToCopy;
                    dstPtr += sizeToCopy;
                }
                if (sizeToCopy == dctx->tmpInTarget) {   /* all done */
                    if (dctx->frameInfo.blockChecksumFlag)
                        dctx->dStage = dstage_getBlockChecksum;
                    else
                        dctx->dStage = dstage_getBlockHeader;  /* new block */
                    break;
                }
                dctx->tmpInTarget -= sizeToCopy;  /* need to copy more */
            }
            nextSrcSizeHint = dctx->tmpInTarget
                            + (dctx->frameInfo.blockChecksumFlag ? BFSize : 0)
                            + BHSize /* next header size */;
            doAnotherStage = 0;
            break;

        /* check block checksum for recently transferred uncompressed block */
        case dstage_getBlockChecksum:
            DEBUGLOG(6, "dstage_getBlockChecksum");
            selectedIn = LZ4F_gatherInput(dctx, dctx->tmpIn, BFSize, &srcPtr, srcEnd);
            if (selectedIn == NULL) {   /* all input consumed */
                nextSrcSizeHint = (BFSize - dctx->tmpInSize) + BHSize /* next header size */;
                doAnotherStage = 0;
                break;
            }
            if (!dctx->skipBlockChecksum) {
                U32 const readCRC = LZ4F_readLE32(selectedIn);
                U32 const calcCRC = XXH32_digest(&dctx->blockChecksum);
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
                DEBUGLOG(6, "compare block checksum");
                if (readCRC != calcCRC) {
                    DEBUGLOG(4, "incorrect block checksum: %08X != %08X",
                            readCRC, calcCRC);
                    nextSrcSizeHint = LZ4F_returnErrorCode(LZ4F_ERROR_blockChecksum_invalid);
                    goto _output;
                }
#else
                (void)readCRC;
                (void)calcCRC;
#endif
            }
            dctx->dStage = dstage_getBlockHeader;  /* new block */
            break;

        case dstage_getCBlock:
            DEBUGLOG(6, "dstage_getCBlock");
            selectedIn = LZ4F_gatherInput(dctx, dctx->tmpIn, dctx->tmpInTarget, &srcPtr, srcEnd);
            if (selectedIn == NULL) {   /* need more input */
                nextSrcSizeHint = (dctx->tmpInTarget - dctx->tmpInSize)   /* block checksum included */
                                + BHSize /* next header size */;
                doAnotherStage = 0;
                break;
            }

            /* At this stage, input is large enough to decode a block */

            /* First, decode and control block checksum if it exists */
            if (dctx->frameInfo.blockChecksumFlag) {
                assert(dctx->tmpInTarget >= 4);
                dctx->tmpInTarget -= 4;
                if (!dctx->skipBlockChecksum) {
                    U32 const readBlockCrc = LZ4F_readLE32(selectedIn + dctx->tmpInTarget);
                    U32 const calcBlockCrc = XXH32(selectedIn, dctx->tmpInTarget, 0);
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
                    if (readBlockCrc != calcBlockCrc) {
                        nextSrcSizeHint = LZ4F_returnErrorCode(LZ4F_ERROR_blockChecksum_invalid);
                        goto _output;
                    }
#else
                    (void)readBlockCrc;
                    (void)calcBlockCrc;
#endif
            }   }

            /* decode directly into destination buffer if there is enough room */
            if ( ((size_t)(dstEnd-dstPtr) >= dctx->maxBlockSize)
                 /* unless the dictionary is stored in tmpOut:
                  * in which case it's faster to decode within tmpOut
                  * to benefit from prefix speedup */
              && !(dctx->dict!= NULL && (const BYTE*)dctx->dict + dctx->dictSize == dctx->tmpOut) )
            {   size_t const decodedSize = LZ4F_decodeBlock(dctx, selectedIn, dctx->tmpInTarget, dstPtr);
                if (LZ4F_isError(decodedSize)) { nextSrcSizeHint = decodedSize; goto _output; }

                /* dictionary management, unless the frame ends with this block */
                if ( (dctx->frameInfo.blockMode==LZ4F_blockLinked)
                  && !LZ4F_endMarkFollows(srcPtr, srcEnd, 0) ) {
                    LZ4F_updateDict(dctx, dstPtr, decodedSize, dstStart, 0);
                }

                dstPtr += decodedSize;
                dctx->dStage = dstage_getBlockHeader;  /* end of block, let's get another one */
                break;
            }

            /* not enough place into dst : decode into tmpOut */

            /* manage dictionary */
            if (dctx->frameInfo.blockMode == LZ4F_blockLinked) {
                if (dctx->dict == dctx->tmpOutBuffer) {
                    /* truncate dictionary to 64 KB if too big */
                    if (dctx->dictSize > 128 KB) {
                        memcpy(dctx->tmpOutBuffer, dctx->dict + dctx->dictSize - 64 KB, 64 KB);
                        dctx->dictSize = 64 KB;
                    }
                    dctx->tmpOut = dctx->tmpOutBuffer + dctx->dictSize;
                } else {  /* dict not within tmpOut */
                    size_t const reservedDictSpace = MIN(dctx->dictSize, 64 KB);
                    dctx->tmpOut = dctx->tmpOutBuffer + reservedDictSpace;
            }   }

            /* Decode block into tmpOut */
            {   size_t const decodedSize = LZ4F_decodeBlock(dctx, selectedIn, dctx->tmpInTarget, dctx->tmpOut);
                if (LZ4F_isError(decodedSize)) { nextSrcSizeHint = decodedSize; goto _output; }
                dctx->tmpOutSize = decodedSize;
                dctx->tmpOutStart = 0;
                dctx->dStage = dstage_flushOut;
            }
            /* fall-through */

        case dstage_flushOut:  /* flush decoded data from tmpOut to dstBuffer */
            DEBUGLOG(6, "dstage_flushOut");
            if (dstPtr != NULL) {
                size_t const sizeToCopy = MIN(dctx->tmpOutSize - dctx->tmpOutStart, (size_t)(dstEnd-dstPtr));
                memcpy(dstPtr, dctx->tmpOut + dctx->tmpOutStart, sizeToCopy);

                /* dictionary management, unless the frame ends with this block */
                if ( (dctx->frameInfo.blockMode == LZ4F_blockLinked)
                  && !( (dctx->tmpOutStart + sizeToCopy == dctx->tmpOutSize)
                     && LZ4F_endMarkFollows(srcPtr, srcEnd, 0) ) )
                    LZ4F_updateDict(dctx, dstPtr, sizeToCopy, dstStart, 1 /*withinTmp*/);

                dctx->tmpOutStart += sizeToCopy;
                dstPtr += sizeToCopy;
            }
            if (dctx->tmpOutStart == dctx->tmpOutSize) { /* all flushed */
                dctx->dStage = dstage_getBlockHeader;  /* get next block */
                break;
            }
            /* could not flush everything : stop there, just request a block header */
            doAnotherStage = 0;
            nextSrcSizeHint = BHSize;
            break;

        case dstage_getSuffix:
            if (dctx->frameRemainingSize) {   /* incorrect frame size decoded */
                nextSrcSizeHint = LZ4F_returnErrorCode(LZ4F_ERROR_frameSize_wrong);
                goto _output;
            }
            if (!dctx->frameInfo.contentChecksumFlag) {  /* no checksum, frame is completed */
                nextSrcSizeHint = 0;
                LZ4F_resetDecompressionContext(dctx);
                doAnotherStage = 0;
                break;
            }
            selectedIn = LZ4F_gatherInput(dctx, dctx->tmpIn, 4, &srcPtr, srcEnd);
            if (selectedIn == NULL) {   /* not enough input to read complete suffix */
                nextSrcSizeHint = 4 - dctx->tmpInSize;
                doAnotherStage = 0;
                break;
            }
            if (!dctx->skipContentChecksum) {
                U32 const readCRC = LZ4F_readLE32(selectedIn);
                U32 const resultCRC = XXH32_digest(&(dctx->xxh));
                DEBUGLOG(4, "frame checksum: stored 0x%0X vs 0x%0X processed", readCRC, resultCRC);
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
                if (readCRC != resultCRC) {
                    nextSrcSizeHint = LZ4F_returnErrorCode(LZ4F_ERROR_contentChecksum_invalid);
                    goto _output;
                }
#else
                (void)readCRC;
                (void)resultCRC;
#endif
            }
            nextSrcSizeHint = 0;
            LZ4F_resetDecompressionContext(dctx);
            doAnotherStage = 0;
            break;

        case dstage_getSFrameSize:
            selectedIn = LZ4F_gatherInput(dctx, dctx->header + 4, 4, &srcPtr, srcEnd);
            if (selectedIn == NULL) {   /* not enough input to read the skippable frame size */
                nextSrcSizeHint = 4 - dctx->tmpInSize;
                doAnotherStage = 0;
                break;
            }
            {   size_t const SFrameSize = LZ4F_readLE32(selectedIn);
                dctx->frameInfo.contentSize = SFrameSize;
                dctx->tmpInTarget = SFrameSize;
                dctx->dStage = dstage_skipSkippable;
                break;
            }

        case dstage_skipSkippable:
            {   size_t const skipSize = MIN(dctx->tmpInTarget, (size_t)(srcEnd-srcPtr));
                srcPtr += skipSize;
                dctx->tmpInTarget -= skipSize;
                doAnotherStage = 0;
                nextSrcSizeHint = dctx->tmpInTarget;
                if (nextSrcSizeHint) break;  /* still more to skip */
                /* frame fully skipped : prepare context for a new frame */
                LZ4F_resetDecompressionContext(dctx);
                break;
            }
        }   /* switch (dctx->dStage) */
    }   /* while (doAnotherStage) */

    /* preserve history within tmpOut whenever necessary */
    LZ4F_STATIC_ASSERT((unsigned)dstage_init == 2);
    if ( (dctx->frameInfo.blockMode==LZ4F_blockLinked)  /* next block will use up to 64KB from previous ones */
      && (dctx->dict != dctx->tmpOutBuffer)             /* dictionary is not already within tmp */
      && (dctx->dict != NULL)                           /* dictionary exists */
      && (!decompressOptionsPtr->stableDst)             /* cannot rely on dst data to remain there for next call */
      && ((unsigned)(dctx->dStage)-2 < (unsigned)(dstage_getSuffix)-2) )  /* valid stages : [init ... getSuffix[ */
    {
        if (dctx->dStage == dstage_flushOut) {
            LZ4F_moveDictBeforeTmpOut(dctx, 0);
        } else {
            const BYTE* const oldDictEnd = dctx->dict + dctx->dictSize;
            size_t const newDictSize = MIN(dctx->dictSize, 64 KB);

            memcpy(dctx->tmpOutBuffer, oldDictEnd - newDictSize, newDictSize);

            dctx->dict = dctx->tmpOutBuffer;
            dctx->dictSize = newDictSize;
            dctx->tmpOut = dctx->tmpOutBuffer + newDictSize;
        }
    }

_output:   /* also on error : report progress, but do not preserve history for reuse */
    *srcSizePtr = (size_t)(srcPtr - srcStart);
    *dstSizePtr = dstStart ? (size_t)(dstPtr - dstStart) : 0;
    return nextSrcSizeHint;
}

/*! LZ4F_decompress_usingDict() :
 *  Same as LZ4F_decompress(), using a predefined dictionary.
 *  Dictionary is used "in place", without any preprocessing.
 *  It must remain accessible throughout the entire frame decoding.
 */
size_t LZ4F_decompress_usingDict(LZ4F_dctx* dctx,
                       void* dstBuffer, size_t* dstSizePtr,
                       const void* srcBuffer, size_t* srcSizePtr,
                       const void* dict, size_t dictSize,
                       const LZ4F_decompressOptions_t* decompressOptionsPtr)
{
    if (dctx->dStage <= dstage_init) {
        dctx->dict = (const BYTE*)dict;
        dctx->dictSize = dictSize;
    }
    return LZ4F_decompress(dctx, dstBuffer, dstSizePtr,
                           srcBuffer, srcSizePtr,
                           decompressOptionsPtr);
}

/*! LZ4F_resumeFrame() :
 *  see lz4frame.h (static section) */
size_t LZ4F_resumeFrame(LZ4F_dctx* dctx, const LZ4F_frameInfo_t* frameInfo,
                        unsigned long long decodedPos,
                        const void* dict, size_t dictSize)
{
    assert(dctx != NULL);
    RETURN_ERROR_IF(frameInfo == NULL, parameter_null);
    RETURN_ERROR_IF(dict == NULL && dictSize > 0, parameter_null);
    RETURN_ERROR_IF(frameInfo->frameType != LZ4F_frame, frameType_unknown);
    RETURN_ERROR_IF(frameInfo->blockSizeID < LZ4F_max64KB || frameInfo->blockSizeID > LZ4F_max4MB, maxBlockSize_invalid);
    RETURN_ERROR_IF((unsigned)frameInfo->blockMode > 1, blockMode_invalid);
    RETURN_ERROR_IF((unsigned)frameInfo->blockChecksumFlag > 1, parameter_invalid);
    RETURN_ERROR_IF((unsigned)frameInfo->contentChecksumFlag > 1, parameter_invalid);
    RETURN_ERROR_IF(frameInfo->contentSize && decodedPos > frameInfo->contentSize, frameSize_wrong);

    LZ4F_resetDecompressionContext(dctx);
    dctx->frameInfo = *frameInfo;
    dctx->maxBlockSize = LZ4F_getBlockSize(frameInfo->blockSizeID);
    if (frameInfo->contentSize)
        dctx->frameRemainingSize = frameInfo->contentSize - decodedPos;
    dctx->dict = (const BYTE*)dict;
    dctx->dictSize = dictSize;
    dctx->skipContentChecksum = 1;   /* content checksum covers the whole frame : can't be verified */
    dctx->dStage = dstage_init;
    return 0;
}
