/*
LZ5 auto-framing library
Copyright (C) 2011-2015, Yann Collet.

BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

You can contact the author at :
- LZ5 source repository : https://github.com/inikep/lz5
- LZ5 public forum : https://groups.google.com/forum/#!forum/lz5c
*/

/* LZ5F is a stand-alone API to create LZ5-compressed Frames
*  in full conformance with specification v1.5.0
*  All related operations, including memory management, are handled by the library.
* */


/**************************************
*  Compiler Options
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#endif


/**************************************
*  Memory routines
**************************************/
#include <stdlib.h>   /* malloc, calloc, free */
#define ALLOCATOR(s)   calloc(1,s)
#define FREEMEM        free
#include <string.h>   /* memset, memcpy, memmove */
#include <stdio.h>
#define MEM_INIT       memset


/**************************************
*  Includes
**************************************/
#include "lz5frame_static.h"
#include "lz5.h"
#include "lz5hc.h"
#define XXH_STATIC_LINKING_ONLY
#include "../zstd/xxhash.h"

/**************************************
*  Basic Types
**************************************/
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)   /* C99 */
# include <stdint.h>
typedef  uint8_t BYTE;
typedef uint16_t U16;
typedef uint32_t U32;
typedef  int32_t S32;
typedef uint64_t U64;
#else
typedef unsigned char       BYTE;
typedef unsigned short      U16;
typedef unsigned int        U32;
typedef   signed int        S32;
typedef unsigned long long  U64;
#endif


/**************************************
*  Constants
**************************************/
#define KB *(1<<10)
#define MB *(1<<20)
#define GB *(1<<30)

#define _1BIT  0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0x0F
#define _8BITS 0xFF

#define LZ5F_MAGIC_SKIPPABLE_START 0x184D2A50U
#define LZ5F_MAGICNUMBER 0x184D2205U
#define LZ5F_BLOCKUNCOMPRESSED_FLAG 0x80000000U
#define LZ5F_BLOCKSIZEID_DEFAULT LZ5F_max64KB
#define LZ5F_DICT_SIZE (1 << 22)

static const size_t minFHSize = 7;
static const size_t maxFHSize = 15;
static const size_t BHSize = 4;
static const int    minHClevel = 1;


/**************************************
*  Structures and local types
**************************************/
typedef struct LZ5F_cctx_s
{
    LZ5F_preferences_t prefs;
    U32    version;
    U32    cStage;
    size_t maxBlockSize;
    size_t maxBufferSize;
    BYTE*  tmpBuff;
    BYTE*  tmpIn;
    size_t tmpInSize;
    U64    totalInSize;
    XXH32_state_t xxh;
    void*  lz5CtxPtr;
    U32    lz5CtxLevel;     /* 0: unallocated;  1: LZ5_stream_t;  3: LZ5_streamHC_t */
} LZ5F_cctx_t;

typedef struct LZ5F_dctx_s
{
    LZ5F_frameInfo_t frameInfo;
    U32    version;
    U32    dStage;
    U64    frameRemainingSize;
    size_t maxBlockSize;
    size_t maxBufferSize;
    const BYTE* srcExpect;
    BYTE*  tmpIn;
    size_t tmpInSize;
    size_t tmpInTarget;
    BYTE*  tmpOutBuffer;
    const BYTE*  dict;
    size_t dictSize;
    BYTE*  tmpOut;
    size_t tmpOutSize;
    size_t tmpOutStart;
    XXH32_state_t xxh;
    BYTE   header[16];
} LZ5F_dctx_t;


/**************************************
*  Error management
**************************************/
#define LZ5F_GENERATE_STRING(STRING) #STRING,
static const char* LZ5F_errorStrings[] = { LZ5F_LIST_ERRORS(LZ5F_GENERATE_STRING) };


unsigned LZ5F_isError(LZ5F_errorCode_t code)
{
    return (code > (LZ5F_errorCode_t)(-LZ5F_ERROR_maxCode));
}

const char* LZ5F_getErrorName(LZ5F_errorCode_t code)
{
    static const char* codeError = "Unspecified error code";
    if (LZ5F_isError(code)) return LZ5F_errorStrings[-(int)(code)];
    return codeError;
}


/**************************************
*  Private functions
**************************************/
static size_t LZ5F_getBlockSize(unsigned blockSizeID)
{
    static const size_t blockSizes[7] = { 64 KB, 256 KB, 1 MB, 4 MB, 16 MB, 64 MB, 256 MB };

    if (blockSizeID == 0) blockSizeID = LZ5F_BLOCKSIZEID_DEFAULT;
    blockSizeID -= 1;
    if (blockSizeID >= 7) return (size_t)-LZ5F_ERROR_maxBlockSize_invalid;

  //  printf("LZ5F_getBlockSize %d %d\n", blockSizeID+1, (int)blockSizes[blockSizeID]);
    return blockSizes[blockSizeID];
}


/* unoptimized version; solves endianess & alignment issues */
static U32 LZ5F_readLE32 (const BYTE* srcPtr)
{
    U32 value32 = srcPtr[0];
    value32 += (srcPtr[1]<<8);
    value32 += (srcPtr[2]<<16);
    value32 += ((U32)srcPtr[3])<<24;
    return value32;
}

static void LZ5F_writeLE32 (BYTE* dstPtr, U32 value32)
{
    dstPtr[0] = (BYTE)value32;
    dstPtr[1] = (BYTE)(value32 >> 8);
    dstPtr[2] = (BYTE)(value32 >> 16);
    dstPtr[3] = (BYTE)(value32 >> 24);
}

static U64 LZ5F_readLE64 (const BYTE* srcPtr)
{
    U64 value64 = srcPtr[0];
    value64 += ((U64)srcPtr[1]<<8);
    value64 += ((U64)srcPtr[2]<<16);
    value64 += ((U64)srcPtr[3]<<24);
    value64 += ((U64)srcPtr[4]<<32);
    value64 += ((U64)srcPtr[5]<<40);
    value64 += ((U64)srcPtr[6]<<48);
    value64 += ((U64)srcPtr[7]<<56);
    return value64;
}

static void LZ5F_writeLE64 (BYTE* dstPtr, U64 value64)
{
    dstPtr[0] = (BYTE)value64;
    dstPtr[1] = (BYTE)(value64 >> 8);
    dstPtr[2] = (BYTE)(value64 >> 16);
    dstPtr[3] = (BYTE)(value64 >> 24);
    dstPtr[4] = (BYTE)(value64 >> 32);
    dstPtr[5] = (BYTE)(value64 >> 40);
    dstPtr[6] = (BYTE)(value64 >> 48);
    dstPtr[7] = (BYTE)(value64 >> 56);
}


static BYTE LZ5F_headerChecksum (const void* header, size_t length)
{
    U32 xxh = XXH32(header, length, 0);
    return (BYTE)(xxh >> 8);
}


/**************************************
*  Simple compression functions
**************************************/
static LZ5F_blockSizeID_t LZ5F_optimalBSID(const LZ5F_blockSizeID_t requestedBSID, const size_t srcSize)
{
    LZ5F_blockSizeID_t proposedBSID = LZ5F_max64KB;
    size_t maxBlockSize = 64 KB;
    while (requestedBSID > proposedBSID)
    {
        if (srcSize <= maxBlockSize)
            return proposedBSID;
        proposedBSID = (LZ5F_blockSizeID_t)((int)proposedBSID + 1);
        maxBlockSize <<= 2;
    }
    return requestedBSID;
}


void LZ5F_freeStream(LZ5F_cctx_t* cctxPtr)
{
    if (cctxPtr->lz5CtxLevel == 1)
        LZ5_freeStream((LZ5_stream_t*)cctxPtr->lz5CtxPtr);
    else if (cctxPtr->lz5CtxLevel == 2)
        LZ5_freeStreamHC((LZ5_streamHC_t*)cctxPtr->lz5CtxPtr);
    cctxPtr->lz5CtxLevel = 0;
}


size_t LZ5F_compressFrameBound(size_t srcSize, const LZ5F_preferences_t* preferencesPtr)
{
    LZ5F_preferences_t prefs;
    size_t headerSize;
    size_t streamSize;

    if (preferencesPtr!=NULL) prefs = *preferencesPtr;
    else memset(&prefs, 0, sizeof(prefs));

    prefs.frameInfo.blockSizeID = LZ5F_optimalBSID(prefs.frameInfo.blockSizeID, srcSize);
    prefs.autoFlush = 1;

    headerSize = maxFHSize;      /* header size, including magic number and frame content size*/
    streamSize = LZ5F_compressBound(srcSize, &prefs);

    return headerSize + streamSize;
}


/* LZ5F_compressFrame()
* Compress an entire srcBuffer into a valid LZ5 frame, as defined by specification v1.5.0, in a single step.
* The most important rule is that dstBuffer MUST be large enough (dstMaxSize) to ensure compression completion even in worst case.
* You can get the minimum value of dstMaxSize by using LZ5F_compressFrameBound()
* If this condition is not respected, LZ5F_compressFrame() will fail (result is an errorCode)
* The LZ5F_preferences_t structure is optional : you can provide NULL as argument. All preferences will then be set to default.
* The result of the function is the number of bytes written into dstBuffer.
* The function outputs an error code if it fails (can be tested using LZ5F_isError())
*/
size_t LZ5F_compressFrame(void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, size_t srcSize, const LZ5F_preferences_t* preferencesPtr)
{
    LZ5F_cctx_t cctxI;
    LZ5F_preferences_t prefs;
    LZ5F_compressOptions_t options;
    LZ5F_errorCode_t errorCode;
    BYTE* const dstStart = (BYTE*) dstBuffer;
    BYTE* dstPtr = dstStart;
    BYTE* const dstEnd = dstStart + dstMaxSize;

    memset(&cctxI, 0, sizeof(cctxI));   /* works because no allocation */
    memset(&options, 0, sizeof(options));

    cctxI.version = LZ5F_VERSION;
    cctxI.maxBufferSize = 5 MB;   /* mess with real buffer size to prevent allocation; works because autoflush==1 & stableSrc==1 */

    if (preferencesPtr!=NULL)
        prefs = *preferencesPtr;
    else
        memset(&prefs, 0, sizeof(prefs));
    if (prefs.frameInfo.contentSize != 0)
        prefs.frameInfo.contentSize = (U64)srcSize;   /* auto-correct content size if selected (!=0) */

    prefs.frameInfo.blockSizeID = LZ5F_optimalBSID(prefs.frameInfo.blockSizeID, srcSize);
    prefs.autoFlush = 1;
    if (srcSize <= LZ5F_getBlockSize(prefs.frameInfo.blockSizeID))
        prefs.frameInfo.blockMode = LZ5F_blockIndependent;   /* no need for linked blocks */

    options.stableSrc = 1;

    if (dstMaxSize < LZ5F_compressFrameBound(srcSize, &prefs))
        return (size_t)-LZ5F_ERROR_dstMaxSize_tooSmall;

    errorCode = LZ5F_compressBegin(&cctxI, dstBuffer, dstMaxSize, &prefs);  /* write header */
    if (LZ5F_isError(errorCode)) return errorCode;
    dstPtr += errorCode;   /* header size */

    errorCode = LZ5F_compressUpdate(&cctxI, dstPtr, dstEnd-dstPtr, srcBuffer, srcSize, &options);
    if (LZ5F_isError(errorCode)) return errorCode;
    dstPtr += errorCode;

    errorCode = LZ5F_compressEnd(&cctxI, dstPtr, dstEnd-dstPtr, &options);   /* flush last block, and generate suffix */
    if (LZ5F_isError(errorCode)) return errorCode;
    dstPtr += errorCode;

    LZ5F_freeStream(&cctxI);

    return (dstPtr - dstStart);
}


/***********************************
*  Advanced compression functions
***********************************/

/* LZ5F_createCompressionContext() :
* The first thing to do is to create a compressionContext object, which will be used in all compression operations.
* This is achieved using LZ5F_createCompressionContext(), which takes as argument a version and an LZ5F_preferences_t structure.
* The version provided MUST be LZ5F_VERSION. It is intended to track potential version differences between different binaries.
* The function will provide a pointer to an allocated LZ5F_compressionContext_t object.
* If the result LZ5F_errorCode_t is not OK_NoError, there was an error during context creation.
* Object can release its memory using LZ5F_freeCompressionContext();
*/
LZ5F_errorCode_t LZ5F_createCompressionContext(LZ5F_compressionContext_t* LZ5F_compressionContextPtr, unsigned version)
{
    LZ5F_cctx_t* cctxPtr;

    cctxPtr = (LZ5F_cctx_t*)ALLOCATOR(sizeof(LZ5F_cctx_t));
    if (cctxPtr==NULL) return (LZ5F_errorCode_t)(-LZ5F_ERROR_allocation_failed);

    cctxPtr->version = version;
    cctxPtr->cStage = 0;   /* Next stage : write header */

    *LZ5F_compressionContextPtr = (LZ5F_compressionContext_t)cctxPtr;

    return LZ5F_OK_NoError;
}


LZ5F_errorCode_t LZ5F_freeCompressionContext(LZ5F_compressionContext_t LZ5F_compressionContext)
{
    LZ5F_cctx_t* cctxPtr = (LZ5F_cctx_t*)LZ5F_compressionContext;

    if (cctxPtr != NULL)   /* null pointers can be safely provided to this function, like free() */
    {
        LZ5F_freeStream(cctxPtr);
        FREEMEM(cctxPtr->tmpBuff);
        FREEMEM(LZ5F_compressionContext);
    }

    return LZ5F_OK_NoError;
}


/* LZ5F_compressBegin() :
* will write the frame header into dstBuffer.
* dstBuffer must be large enough to accommodate a header (dstMaxSize). Maximum header size is LZ5F_MAXHEADERFRAME_SIZE bytes.
* The result of the function is the number of bytes written into dstBuffer for the header
* or an error code (can be tested using LZ5F_isError())
*/
size_t LZ5F_compressBegin(LZ5F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const LZ5F_preferences_t* preferencesPtr)
{
    LZ5F_preferences_t prefNull;
    LZ5F_cctx_t* cctxPtr = (LZ5F_cctx_t*)compressionContext;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* dstPtr = dstStart;
    BYTE* headerStart;
    size_t requiredBuffSize;

    if (dstMaxSize < maxFHSize) return (size_t)-LZ5F_ERROR_dstMaxSize_tooSmall;
    if (cctxPtr->cStage != 0) return (size_t)-LZ5F_ERROR_GENERIC;
    memset(&prefNull, 0, sizeof(prefNull));
    if (preferencesPtr == NULL) preferencesPtr = &prefNull;
    cctxPtr->prefs = *preferencesPtr;
    cctxPtr->prefs.frameInfo.blockMode = LZ5F_blockIndependent;

    /* ctx Management */
    {
        U32 tableID = (cctxPtr->prefs.compressionLevel < minHClevel) ? 1 : 2;  /* 0:nothing ; 1:LZ5_createStream ; 2:LZ5_createStreamHC */
      //  printf("BEFORE lz5CtxLevel=%d tableID=%d compressionLevel=%d minHClevel=%d\n", (int)cctxPtr->lz5CtxLevel, (int)tableID, (int)cctxPtr->prefs.compressionLevel, minHClevel);
        if (cctxPtr->lz5CtxLevel != tableID)
        {
            LZ5F_freeStream(cctxPtr);

            cctxPtr->lz5CtxLevel = tableID;
            if (cctxPtr->lz5CtxLevel == 1)
                cctxPtr->lz5CtxPtr = (void*)LZ5_createStream();
            else
                cctxPtr->lz5CtxPtr = (void*)LZ5_createStreamHC(cctxPtr->prefs.compressionLevel);
        }
    }

    /* Buffer Management */
    if (cctxPtr->prefs.frameInfo.blockSizeID == 0) cctxPtr->prefs.frameInfo.blockSizeID = LZ5F_BLOCKSIZEID_DEFAULT;
    cctxPtr->maxBlockSize = LZ5F_getBlockSize(cctxPtr->prefs.frameInfo.blockSizeID);

    requiredBuffSize = cctxPtr->maxBlockSize + ((cctxPtr->prefs.frameInfo.blockMode == LZ5F_blockLinked) * 2 * LZ5F_DICT_SIZE);
    if (preferencesPtr->autoFlush)
        requiredBuffSize = (cctxPtr->prefs.frameInfo.blockMode == LZ5F_blockLinked) * LZ5F_DICT_SIZE;   /* just needs dict */

    if (cctxPtr->maxBufferSize < requiredBuffSize)
    {
        cctxPtr->maxBufferSize = requiredBuffSize;
        FREEMEM(cctxPtr->tmpBuff);
        cctxPtr->tmpBuff = (BYTE*)ALLOCATOR(requiredBuffSize);
        if (cctxPtr->tmpBuff == NULL) return (size_t)-LZ5F_ERROR_allocation_failed;
    }
    cctxPtr->tmpIn = cctxPtr->tmpBuff;
    cctxPtr->tmpInSize = 0;
    XXH32_reset(&(cctxPtr->xxh), 0);
    if (cctxPtr->prefs.compressionLevel < minHClevel)
        LZ5_resetStream((LZ5_stream_t*)(cctxPtr->lz5CtxPtr));
    else
        LZ5_resetStreamHC((LZ5_streamHC_t*)(cctxPtr->lz5CtxPtr));

    /* Magic Number */
    LZ5F_writeLE32(dstPtr, LZ5F_MAGICNUMBER);
    dstPtr += 4;
    headerStart = dstPtr;

    /* FLG Byte */
    *dstPtr++ = (BYTE)(((1 & _2BITS) << 6)    /* Version('01') */
        + ((cctxPtr->prefs.frameInfo.blockMode & _1BIT ) << 5)    /* Block mode */
        + ((cctxPtr->prefs.frameInfo.contentChecksumFlag & _1BIT ) << 2)   /* Frame checksum */
        + ((cctxPtr->prefs.frameInfo.contentSize > 0) << 3));   /* Frame content size */
    /* BD Byte */
    *dstPtr++ = (BYTE)((cctxPtr->prefs.frameInfo.blockSizeID & _3BITS) << 4);
    /* Optional Frame content size field */
    if (cctxPtr->prefs.frameInfo.contentSize)
    {
        LZ5F_writeLE64(dstPtr, cctxPtr->prefs.frameInfo.contentSize);
        dstPtr += 8;
        cctxPtr->totalInSize = 0;
    }
    /* CRC Byte */
    *dstPtr = LZ5F_headerChecksum(headerStart, dstPtr - headerStart);
    dstPtr++;

    cctxPtr->cStage = 1;   /* header written, now request input data block */

    return (dstPtr - dstStart);
}


/* LZ5F_compressBound() : gives the size of Dst buffer given a srcSize to handle worst case situations.
*                        The LZ5F_frameInfo_t structure is optional :
*                        you can provide NULL as argument, preferences will then be set to cover worst case situations.
* */
size_t LZ5F_compressBound(size_t srcSize, const LZ5F_preferences_t* preferencesPtr)
{
    LZ5F_preferences_t prefsNull;
    memset(&prefsNull, 0, sizeof(prefsNull));
    prefsNull.frameInfo.contentChecksumFlag = LZ5F_contentChecksumEnabled;   /* worst case */
    {
        const LZ5F_preferences_t* prefsPtr = (preferencesPtr==NULL) ? &prefsNull : preferencesPtr;
        LZ5F_blockSizeID_t bid = prefsPtr->frameInfo.blockSizeID;
        size_t blockSize = LZ5F_getBlockSize(bid);
        unsigned nbBlocks = (unsigned)(srcSize / blockSize) + 1;
        size_t lastBlockSize = prefsPtr->autoFlush ? srcSize % blockSize : blockSize;
        size_t blockInfo = 4;   /* default, without block CRC option */
        size_t frameEnd = 4 + (prefsPtr->frameInfo.contentChecksumFlag*4);

        return (blockInfo * nbBlocks) + (blockSize * (nbBlocks-1)) + lastBlockSize + frameEnd;;
    }
}


typedef int (*compressFunc_t)(void* ctx, const char* src, char* dst, int srcSize, int dstSize);

static size_t LZ5F_compressBlock(void* dst, const void* src, size_t srcSize, compressFunc_t compress, void* lz5ctx)
{
    /* compress one block */
    BYTE* cSizePtr = (BYTE*)dst;
    U32 cSize;
    cSize = (U32)compress(lz5ctx, (const char*)src, (char*)(cSizePtr+4), (int)(srcSize), (int)(srcSize-1));
    LZ5F_writeLE32(cSizePtr, cSize);
    if (cSize == 0)   /* compression failed */
    {
        cSize = (U32)srcSize;
        LZ5F_writeLE32(cSizePtr, cSize + LZ5F_BLOCKUNCOMPRESSED_FLAG);
        memcpy(cSizePtr+4, src, srcSize);
    }
    return cSize + 4;
}


static int LZ5F_localLZ5_compress_limitedOutput_withState(void* ctx, const char* src, char* dst, int srcSize, int dstSize)
{
    return LZ5_compress_limitedOutput_withState(ctx, src, dst, srcSize, dstSize);
}

static int LZ5F_localLZ5_compress_limitedOutput_continue(void* ctx, const char* src, char* dst, int srcSize, int dstSize)
{
    return LZ5_compress_limitedOutput_continue((LZ5_stream_t*)ctx, src, dst, srcSize, dstSize);
}

static int LZ5F_localLZ5_compressHC_limitedOutput_continue(void* ctx, const char* src, char* dst, int srcSize, int dstSize)
{
    return LZ5_compress_HC_continue((LZ5_streamHC_t*)ctx, src, dst, srcSize, dstSize);
}

static compressFunc_t LZ5F_selectCompression(LZ5F_blockMode_t blockMode, int level)
{
    if (level < minHClevel)
    {
        if (blockMode == LZ5F_blockIndependent) return LZ5F_localLZ5_compress_limitedOutput_withState;
        return LZ5F_localLZ5_compress_limitedOutput_continue;
    }
    if (blockMode == LZ5F_blockIndependent) return LZ5_compress_HC_extStateHC;
    return LZ5F_localLZ5_compressHC_limitedOutput_continue;
}

static int LZ5F_localSaveDict(LZ5F_cctx_t* cctxPtr)
{
    if (cctxPtr->prefs.compressionLevel < minHClevel)
        return LZ5_saveDict ((LZ5_stream_t*)(cctxPtr->lz5CtxPtr), (char*)(cctxPtr->tmpBuff), 64 KB);
    return LZ5_saveDictHC ((LZ5_streamHC_t*)(cctxPtr->lz5CtxPtr), (char*)(cctxPtr->tmpBuff), 64 KB);
}

typedef enum { notDone, fromTmpBuffer, fromSrcBuffer } LZ5F_lastBlockStatus;

/* LZ5F_compressUpdate()
* LZ5F_compressUpdate() can be called repetitively to compress as much data as necessary.
* The most important rule is that dstBuffer MUST be large enough (dstMaxSize) to ensure compression completion even in worst case.
* If this condition is not respected, LZ5F_compress() will fail (result is an errorCode)
* You can get the minimum value of dstMaxSize by using LZ5F_compressBound()
* The LZ5F_compressOptions_t structure is optional : you can provide NULL as argument.
* The result of the function is the number of bytes written into dstBuffer : it can be zero, meaning input data was just buffered.
* The function outputs an error code if it fails (can be tested using LZ5F_isError())
*/
size_t LZ5F_compressUpdate(LZ5F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const void* srcBuffer, size_t srcSize, const LZ5F_compressOptions_t* compressOptionsPtr)
{
    LZ5F_compressOptions_t cOptionsNull;
    LZ5F_cctx_t* cctxPtr = (LZ5F_cctx_t*)compressionContext;
    size_t blockSize = cctxPtr->maxBlockSize;
    const BYTE* srcPtr = (const BYTE*)srcBuffer;
    const BYTE* const srcEnd = srcPtr + srcSize;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* dstPtr = dstStart;
    LZ5F_lastBlockStatus lastBlockCompressed = notDone;
    compressFunc_t compress;


    if (cctxPtr->cStage != 1) return (size_t)-LZ5F_ERROR_GENERIC;
    if (dstMaxSize < LZ5F_compressBound(srcSize, &(cctxPtr->prefs))) return (size_t)-LZ5F_ERROR_dstMaxSize_tooSmall;
    memset(&cOptionsNull, 0, sizeof(cOptionsNull));
    if (compressOptionsPtr == NULL) compressOptionsPtr = &cOptionsNull;

    /* select compression function */
    compress = LZ5F_selectCompression(cctxPtr->prefs.frameInfo.blockMode, cctxPtr->prefs.compressionLevel);

    /* complete tmp buffer */
    if (cctxPtr->tmpInSize > 0)   /* some data already within tmp buffer */
    {
        size_t sizeToCopy = blockSize - cctxPtr->tmpInSize;
        if (sizeToCopy > srcSize)
        {
            /* add src to tmpIn buffer */
            memcpy(cctxPtr->tmpIn + cctxPtr->tmpInSize, srcBuffer, srcSize);
            srcPtr = srcEnd;
            cctxPtr->tmpInSize += srcSize;
            /* still needs some CRC */
        }
        else
        {
            /* complete tmpIn block and then compress it */
            lastBlockCompressed = fromTmpBuffer;
            memcpy(cctxPtr->tmpIn + cctxPtr->tmpInSize, srcBuffer, sizeToCopy);
            srcPtr += sizeToCopy;

            dstPtr += LZ5F_compressBlock(dstPtr, cctxPtr->tmpIn, blockSize, compress, cctxPtr->lz5CtxPtr);

            if (cctxPtr->prefs.frameInfo.blockMode==LZ5F_blockLinked) cctxPtr->tmpIn += blockSize;
            cctxPtr->tmpInSize = 0;
        }
    }

    while ((size_t)(srcEnd - srcPtr) >= blockSize)
    {
        /* compress full block */
        lastBlockCompressed = fromSrcBuffer;
        dstPtr += LZ5F_compressBlock(dstPtr, srcPtr, blockSize, compress, cctxPtr->lz5CtxPtr);
        srcPtr += blockSize;
    }

    if ((cctxPtr->prefs.autoFlush) && (srcPtr < srcEnd))
    {
        /* compress remaining input < blockSize */
        lastBlockCompressed = fromSrcBuffer;
        dstPtr += LZ5F_compressBlock(dstPtr, srcPtr, srcEnd - srcPtr, compress, cctxPtr->lz5CtxPtr);
        srcPtr  = srcEnd;
    }

    /* preserve dictionary if necessary */
    if ((cctxPtr->prefs.frameInfo.blockMode==LZ5F_blockLinked) && (lastBlockCompressed==fromSrcBuffer))
    {
        if (compressOptionsPtr->stableSrc)
        {
            cctxPtr->tmpIn = cctxPtr->tmpBuff;
        }
        else
        {
            int realDictSize = LZ5F_localSaveDict(cctxPtr);
            if (realDictSize==0) return (size_t)-LZ5F_ERROR_GENERIC;
            cctxPtr->tmpIn = cctxPtr->tmpBuff + realDictSize;
        }
    }

    /* keep tmpIn within limits */
    if ((cctxPtr->tmpIn + blockSize) > (cctxPtr->tmpBuff + cctxPtr->maxBufferSize)   /* necessarily LZ5F_blockLinked && lastBlockCompressed==fromTmpBuffer */
        && !(cctxPtr->prefs.autoFlush))
    {
        int realDictSize = LZ5F_localSaveDict(cctxPtr);
        cctxPtr->tmpIn = cctxPtr->tmpBuff + realDictSize;
    }

    /* some input data left, necessarily < blockSize */
    if (srcPtr < srcEnd)
    {
        /* fill tmp buffer */
        size_t sizeToCopy = srcEnd - srcPtr;
        memcpy(cctxPtr->tmpIn, srcPtr, sizeToCopy);
        cctxPtr->tmpInSize = sizeToCopy;
    }

    if (cctxPtr->prefs.frameInfo.contentChecksumFlag == LZ5F_contentChecksumEnabled)
        XXH32_update(&(cctxPtr->xxh), srcBuffer, srcSize);

    cctxPtr->totalInSize += srcSize;
    return dstPtr - dstStart;
}


/* LZ5F_flush()
* Should you need to create compressed data immediately, without waiting for a block to be filled,
* you can call LZ5_flush(), which will immediately compress any remaining data stored within compressionContext.
* The result of the function is the number of bytes written into dstBuffer
* (it can be zero, this means there was no data left within compressionContext)
* The function outputs an error code if it fails (can be tested using LZ5F_isError())
* The LZ5F_compressOptions_t structure is optional : you can provide NULL as argument.
*/
size_t LZ5F_flush(LZ5F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const LZ5F_compressOptions_t* compressOptionsPtr)
{
    LZ5F_cctx_t* cctxPtr = (LZ5F_cctx_t*)compressionContext;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* dstPtr = dstStart;
    compressFunc_t compress;


    if (cctxPtr->tmpInSize == 0) return 0;   /* nothing to flush */
    if (cctxPtr->cStage != 1) return (size_t)-LZ5F_ERROR_GENERIC;
    if (dstMaxSize < (cctxPtr->tmpInSize + 8)) return (size_t)-LZ5F_ERROR_dstMaxSize_tooSmall;   /* +8 : block header(4) + block checksum(4) */
    (void)compressOptionsPtr;   /* not yet useful */

    /* select compression function */
    compress = LZ5F_selectCompression(cctxPtr->prefs.frameInfo.blockMode, cctxPtr->prefs.compressionLevel);

    /* compress tmp buffer */
    dstPtr += LZ5F_compressBlock(dstPtr, cctxPtr->tmpIn, cctxPtr->tmpInSize, compress, cctxPtr->lz5CtxPtr);
    if (cctxPtr->prefs.frameInfo.blockMode==LZ5F_blockLinked) cctxPtr->tmpIn += cctxPtr->tmpInSize;
    cctxPtr->tmpInSize = 0;

    /* keep tmpIn within limits */
    if ((cctxPtr->tmpIn + cctxPtr->maxBlockSize) > (cctxPtr->tmpBuff + cctxPtr->maxBufferSize))   /* necessarily LZ5F_blockLinked */
    {
        int realDictSize = LZ5F_localSaveDict(cctxPtr);
        cctxPtr->tmpIn = cctxPtr->tmpBuff + realDictSize;
    }

    return dstPtr - dstStart;
}


/* LZ5F_compressEnd()
* When you want to properly finish the compressed frame, just call LZ5F_compressEnd().
* It will flush whatever data remained within compressionContext (like LZ5_flush())
* but also properly finalize the frame, with an endMark and a checksum.
* The result of the function is the number of bytes written into dstBuffer (necessarily >= 4 (endMark size))
* The function outputs an error code if it fails (can be tested using LZ5F_isError())
* The LZ5F_compressOptions_t structure is optional : you can provide NULL as argument.
* compressionContext can then be used again, starting with LZ5F_compressBegin(). The preferences will remain the same.
*/
size_t LZ5F_compressEnd(LZ5F_compressionContext_t compressionContext, void* dstBuffer, size_t dstMaxSize, const LZ5F_compressOptions_t* compressOptionsPtr)
{
    LZ5F_cctx_t* cctxPtr = (LZ5F_cctx_t*)compressionContext;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* dstPtr = dstStart;
    size_t errorCode;

    errorCode = LZ5F_flush(compressionContext, dstBuffer, dstMaxSize, compressOptionsPtr);
    if (LZ5F_isError(errorCode)) return errorCode;
    dstPtr += errorCode;

    LZ5F_writeLE32(dstPtr, 0);
    dstPtr+=4;   /* endMark */

    if (cctxPtr->prefs.frameInfo.contentChecksumFlag == LZ5F_contentChecksumEnabled)
    {
        U32 xxh = XXH32_digest(&(cctxPtr->xxh));
        LZ5F_writeLE32(dstPtr, xxh);
        dstPtr+=4;   /* content Checksum */
    }

    cctxPtr->cStage = 0;   /* state is now re-usable (with identical preferences) */

    if (cctxPtr->prefs.frameInfo.contentSize)
    {
        if (cctxPtr->prefs.frameInfo.contentSize != cctxPtr->totalInSize)
            return (size_t)-LZ5F_ERROR_frameSize_wrong;
    }

    return dstPtr - dstStart;
}


/**********************************
*  Decompression functions
**********************************/

/* Resource management */

/* LZ5F_createDecompressionContext() :
* The first thing to do is to create a decompressionContext object, which will be used in all decompression operations.
* This is achieved using LZ5F_createDecompressionContext().
* The function will provide a pointer to a fully allocated and initialized LZ5F_decompressionContext object.
* If the result LZ5F_errorCode_t is not zero, there was an error during context creation.
* Object can release its memory using LZ5F_freeDecompressionContext();
*/
LZ5F_errorCode_t LZ5F_createDecompressionContext(LZ5F_decompressionContext_t* LZ5F_decompressionContextPtr, unsigned versionNumber)
{
    LZ5F_dctx_t* dctxPtr;

    dctxPtr = (LZ5F_dctx_t*)ALLOCATOR(sizeof(LZ5F_dctx_t));
    if (dctxPtr==NULL) return (LZ5F_errorCode_t)-LZ5F_ERROR_GENERIC;

    dctxPtr->version = versionNumber;
    *LZ5F_decompressionContextPtr = (LZ5F_decompressionContext_t)dctxPtr;
    return LZ5F_OK_NoError;
}

LZ5F_errorCode_t LZ5F_freeDecompressionContext(LZ5F_decompressionContext_t LZ5F_decompressionContext)
{
    LZ5F_errorCode_t result = LZ5F_OK_NoError;
    LZ5F_dctx_t* dctxPtr = (LZ5F_dctx_t*)LZ5F_decompressionContext;
    if (dctxPtr != NULL)   /* can accept NULL input, like free() */
    {
      result = (LZ5F_errorCode_t)dctxPtr->dStage;
      FREEMEM(dctxPtr->tmpIn);
      FREEMEM(dctxPtr->tmpOutBuffer);
      FREEMEM(dctxPtr);
    }
    return result;
}


/* ******************************************************************** */
/* ********************* Decompression ******************************** */
/* ******************************************************************** */

typedef enum { dstage_getHeader=0, dstage_storeHeader,
    dstage_getCBlockSize, dstage_storeCBlockSize,
    dstage_copyDirect,
    dstage_getCBlock, dstage_storeCBlock,
    dstage_decodeCBlock, dstage_decodeCBlock_intoDst,
    dstage_decodeCBlock_intoTmp, dstage_flushOut,
    dstage_getSuffix, dstage_storeSuffix,
    dstage_getSFrameSize, dstage_storeSFrameSize,
    dstage_skipSkippable
} dStage_t;


/* LZ5F_decodeHeader
   return : nb Bytes read from srcVoidPtr (necessarily <= srcSize)
            or an error code (testable with LZ5F_isError())
   output : set internal values of dctx, such as
            dctxPtr->frameInfo and dctxPtr->dStage.
   input  : srcVoidPtr points at the **beginning of the frame**
*/
static size_t LZ5F_decodeHeader(LZ5F_dctx_t* dctxPtr, const void* srcVoidPtr, size_t srcSize)
{
    BYTE FLG, BD, HC;
    unsigned version, blockMode, blockChecksumFlag, contentSizeFlag, contentChecksumFlag, blockSizeID;
    size_t bufferNeeded;
    size_t frameHeaderSize;
    const BYTE* srcPtr = (const BYTE*)srcVoidPtr;

    /* need to decode header to get frameInfo */
    if (srcSize < minFHSize) return (size_t)-LZ5F_ERROR_frameHeader_incomplete;   /* minimal frame header size */
    memset(&(dctxPtr->frameInfo), 0, sizeof(dctxPtr->frameInfo));

    /* special case : skippable frames */
    if ((LZ5F_readLE32(srcPtr) & 0xFFFFFFF0U) == LZ5F_MAGIC_SKIPPABLE_START)
    {
        dctxPtr->frameInfo.frameType = LZ5F_skippableFrame;
        if (srcVoidPtr == (void*)(dctxPtr->header))
        {
            dctxPtr->tmpInSize = srcSize;
            dctxPtr->tmpInTarget = 8;
            dctxPtr->dStage = dstage_storeSFrameSize;
            return srcSize;
        }
        else
        {
            dctxPtr->dStage = dstage_getSFrameSize;
            return 4;
        }
    }

    /* control magic number */
    if (LZ5F_readLE32(srcPtr) != LZ5F_MAGICNUMBER) return (size_t)-LZ5F_ERROR_frameType_unknown;
    dctxPtr->frameInfo.frameType = LZ5F_frame;

    /* Flags */
    FLG = srcPtr[4];
    version = (FLG>>6) & _2BITS;
    blockMode = (FLG>>5) & _1BIT;
    blockChecksumFlag = (FLG>>4) & _1BIT;
    contentSizeFlag = (FLG>>3) & _1BIT;
    contentChecksumFlag = (FLG>>2) & _1BIT;

    /* Frame Header Size */
    frameHeaderSize = contentSizeFlag ? maxFHSize : minFHSize;

    if (srcSize < frameHeaderSize)
    {
        /* not enough input to fully decode frame header */
        if (srcPtr != dctxPtr->header)
            memcpy(dctxPtr->header, srcPtr, srcSize);
        dctxPtr->tmpInSize = srcSize;
        dctxPtr->tmpInTarget = frameHeaderSize;
        dctxPtr->dStage = dstage_storeHeader;
        return srcSize;
    }

    BD = srcPtr[5];
    blockSizeID = (BD>>4) & _3BITS;

    /* validate */
    if (version != 1) return (size_t)-LZ5F_ERROR_headerVersion_wrong;        /* Version Number, only supported value */
    if (blockChecksumFlag != 0) return (size_t)-LZ5F_ERROR_blockChecksum_unsupported; /* Not supported for the time being */
    if (((FLG>>0)&_2BITS) != 0) return (size_t)-LZ5F_ERROR_reservedFlag_set; /* Reserved bits */
    if (((BD>>7)&_1BIT) != 0) return (size_t)-LZ5F_ERROR_reservedFlag_set;   /* Reserved bit */
    if (blockSizeID < 1) return (size_t)-LZ5F_ERROR_maxBlockSize_invalid;    /* 1-7 only supported values for the time being */
    if (((BD>>0)&_4BITS) != 0) return (size_t)-LZ5F_ERROR_reservedFlag_set;  /* Reserved bits */

    /* check */
    HC = LZ5F_headerChecksum(srcPtr+4, frameHeaderSize-5);
    if (HC != srcPtr[frameHeaderSize-1]) return (size_t)-LZ5F_ERROR_headerChecksum_invalid;   /* Bad header checksum error */

    /* save */
    dctxPtr->frameInfo.blockMode = (LZ5F_blockMode_t)blockMode;
    dctxPtr->frameInfo.contentChecksumFlag = (LZ5F_contentChecksum_t)contentChecksumFlag;
    dctxPtr->frameInfo.blockSizeID = (LZ5F_blockSizeID_t)blockSizeID;
    dctxPtr->maxBlockSize = LZ5F_getBlockSize(blockSizeID);
    if (contentSizeFlag)
        dctxPtr->frameRemainingSize = dctxPtr->frameInfo.contentSize = LZ5F_readLE64(srcPtr+6);

    /* init */
    if (contentChecksumFlag) XXH32_reset(&(dctxPtr->xxh), 0);

    /* alloc */
    bufferNeeded = dctxPtr->maxBlockSize + ((dctxPtr->frameInfo.blockMode==LZ5F_blockLinked) * 2 * LZ5F_DICT_SIZE);
    if (bufferNeeded > dctxPtr->maxBufferSize)   /* tmp buffers too small */
    {
        FREEMEM(dctxPtr->tmpIn);
        FREEMEM(dctxPtr->tmpOutBuffer);
        dctxPtr->maxBufferSize = bufferNeeded;
        dctxPtr->tmpIn = (BYTE*)ALLOCATOR(dctxPtr->maxBlockSize);
        if (dctxPtr->tmpIn == NULL) return (size_t)-LZ5F_ERROR_GENERIC;
        dctxPtr->tmpOutBuffer= (BYTE*)ALLOCATOR(dctxPtr->maxBufferSize);
        if (dctxPtr->tmpOutBuffer== NULL) return (size_t)-LZ5F_ERROR_GENERIC;
    }
    dctxPtr->tmpInSize = 0;
    dctxPtr->tmpInTarget = 0;
    dctxPtr->dict = dctxPtr->tmpOutBuffer;
    dctxPtr->dictSize = 0;
    dctxPtr->tmpOut = dctxPtr->tmpOutBuffer;
    dctxPtr->tmpOutStart = 0;
    dctxPtr->tmpOutSize = 0;

    dctxPtr->dStage = dstage_getCBlockSize;

    return frameHeaderSize;
}


/* LZ5F_getFrameInfo()
* This function decodes frame header information, such as blockSize.
* It is optional : you could start by calling directly LZ5F_decompress() instead.
* The objective is to extract header information without starting decompression, typically for allocation purposes.
* LZ5F_getFrameInfo() can also be used *after* starting decompression, on a valid LZ5F_decompressionContext_t.
* The number of bytes read from srcBuffer will be provided within *srcSizePtr (necessarily <= original value).
* You are expected to resume decompression from where it stopped (srcBuffer + *srcSizePtr)
* The function result is an hint of the better srcSize to use for next call to LZ5F_decompress,
* or an error code which can be tested using LZ5F_isError().
*/
LZ5F_errorCode_t LZ5F_getFrameInfo(LZ5F_decompressionContext_t dCtx, LZ5F_frameInfo_t* frameInfoPtr,
                                   const void* srcBuffer, size_t* srcSizePtr)
{
    LZ5F_dctx_t* dctxPtr = (LZ5F_dctx_t*)dCtx;

    if (dctxPtr->dStage > dstage_storeHeader)   /* note : requires dstage_* header related to be at beginning of enum */
    {
        size_t o=0, i=0;
        /* frameInfo already decoded */
        *srcSizePtr = 0;
        *frameInfoPtr = dctxPtr->frameInfo;
        return LZ5F_decompress(dCtx, NULL, &o, NULL, &i, NULL);
    }
    else
    {
        size_t o=0;
        size_t nextSrcSize = LZ5F_decompress(dCtx, NULL, &o, srcBuffer, srcSizePtr, NULL);
        if (dctxPtr->dStage <= dstage_storeHeader)   /* note : requires dstage_* header related to be at beginning of enum */
            return (size_t)-LZ5F_ERROR_frameHeader_incomplete;
        *frameInfoPtr = dctxPtr->frameInfo;
        return nextSrcSize;
    }
}


/* trivial redirector, for common prototype */
static int LZ5F_decompress_safe (const char* source, char* dest, int compressedSize, int maxDecompressedSize, const char* dictStart, int dictSize)
{
    (void)dictStart; (void)dictSize;
    return LZ5_decompress_safe (source, dest, compressedSize, maxDecompressedSize);
}


static void LZ5F_updateDict(LZ5F_dctx_t* dctxPtr, const BYTE* dstPtr, size_t dstSize, const BYTE* dstPtr0, unsigned withinTmp)
{
    if (dctxPtr->dictSize==0)
        dctxPtr->dict = (const BYTE*)dstPtr;   /* priority to dictionary continuity */

    if (dctxPtr->dict + dctxPtr->dictSize == dstPtr)   /* dictionary continuity */
    {
        dctxPtr->dictSize += dstSize;
        return;
    }

    if (dstPtr - dstPtr0 + dstSize >= 64 KB)   /* dstBuffer large enough to become dictionary */
    {
        dctxPtr->dict = (const BYTE*)dstPtr0;
        dctxPtr->dictSize = dstPtr - dstPtr0 + dstSize;
        return;
    }

    if ((withinTmp) && (dctxPtr->dict == dctxPtr->tmpOutBuffer))
    {
        /* assumption : dctxPtr->dict + dctxPtr->dictSize == dctxPtr->tmpOut + dctxPtr->tmpOutStart */
        dctxPtr->dictSize += dstSize;
        return;
    }

    if (withinTmp) /* copy relevant dict portion in front of tmpOut within tmpOutBuffer */
    {
        size_t preserveSize = dctxPtr->tmpOut - dctxPtr->tmpOutBuffer;
        size_t copySize = 64 KB - dctxPtr->tmpOutSize;
        const BYTE* oldDictEnd = dctxPtr->dict + dctxPtr->dictSize - dctxPtr->tmpOutStart;
        if (dctxPtr->tmpOutSize > 64 KB) copySize = 0;
        if (copySize > preserveSize) copySize = preserveSize;

        memcpy(dctxPtr->tmpOutBuffer + preserveSize - copySize, oldDictEnd - copySize, copySize);

        dctxPtr->dict = dctxPtr->tmpOutBuffer;
        dctxPtr->dictSize = preserveSize + dctxPtr->tmpOutStart + dstSize;
        return;
    }

    if (dctxPtr->dict == dctxPtr->tmpOutBuffer)     /* copy dst into tmp to complete dict */
    {
        if (dctxPtr->dictSize + dstSize > dctxPtr->maxBufferSize)   /* tmp buffer not large enough */
        {
            size_t preserveSize = 64 KB - dstSize;   /* note : dstSize < 64 KB */
            memcpy(dctxPtr->tmpOutBuffer, dctxPtr->dict + dctxPtr->dictSize - preserveSize, preserveSize);
            dctxPtr->dictSize = preserveSize;
        }
        memcpy(dctxPtr->tmpOutBuffer + dctxPtr->dictSize, dstPtr, dstSize);
        dctxPtr->dictSize += dstSize;
        return;
    }

    /* join dict & dest into tmp */
    {
        size_t preserveSize = 64 KB - dstSize;   /* note : dstSize < 64 KB */
        if (preserveSize > dctxPtr->dictSize) preserveSize = dctxPtr->dictSize;
        memcpy(dctxPtr->tmpOutBuffer, dctxPtr->dict + dctxPtr->dictSize - preserveSize, preserveSize);
        memcpy(dctxPtr->tmpOutBuffer + preserveSize, dstPtr, dstSize);
        dctxPtr->dict = dctxPtr->tmpOutBuffer;
        dctxPtr->dictSize = preserveSize + dstSize;
    }
}



/* LZ5F_decompress()
* Call this function repetitively to regenerate data compressed within srcBuffer.
* The function will attempt to decode *srcSizePtr from srcBuffer, into dstBuffer of maximum size *dstSizePtr.
*
* The number of bytes regenerated into dstBuffer will be provided within *dstSizePtr (necessarily <= original value).
*
* The number of bytes effectively read from srcBuffer will be provided within *srcSizePtr (necessarily <= original value).
* If the number of bytes read is < number of bytes provided, then the decompression operation is not complete.
* You will have to call it again, continuing from where it stopped.
*
* The function result is an hint of the better srcSize to use for next call to LZ5F_decompress.
* Basically, it's the size of the current (or remaining) compressed block + header of next block.
* Respecting the hint provides some boost to performance, since it allows less buffer shuffling.
* Note that this is just a hint, you can always provide any srcSize you want.
* When a frame is fully decoded, the function result will be 0.
* If decompression failed, function result is an error code which can be tested using LZ5F_isError().
*/
size_t LZ5F_decompress(LZ5F_decompressionContext_t decompressionContext,
                       void* dstBuffer, size_t* dstSizePtr,
                       const void* srcBuffer, size_t* srcSizePtr,
                       const LZ5F_decompressOptions_t* decompressOptionsPtr)
{
    LZ5F_dctx_t* dctxPtr = (LZ5F_dctx_t*)decompressionContext;
    LZ5F_decompressOptions_t optionsNull;
    const BYTE* const srcStart = (const BYTE*)srcBuffer;
    const BYTE* const srcEnd = srcStart + *srcSizePtr;
    const BYTE* srcPtr = srcStart;
    BYTE* const dstStart = (BYTE*)dstBuffer;
    BYTE* const dstEnd = dstStart + *dstSizePtr;
    BYTE* dstPtr = dstStart;
    const BYTE* selectedIn = NULL;
    unsigned doAnotherStage = 1;
    size_t nextSrcSizeHint = 1;


    memset(&optionsNull, 0, sizeof(optionsNull));
    if (decompressOptionsPtr==NULL) decompressOptionsPtr = &optionsNull;
    *srcSizePtr = 0;
    *dstSizePtr = 0;

    /* expect to continue decoding src buffer where it left previously */
    if (dctxPtr->srcExpect != NULL)
    {
        if (srcStart != dctxPtr->srcExpect) return (size_t)-LZ5F_ERROR_srcPtr_wrong;
    }

    /* programmed as a state machine */

    while (doAnotherStage)
    {

        switch(dctxPtr->dStage)
        {

        case dstage_getHeader:
            {
                if ((size_t)(srcEnd-srcPtr) >= maxFHSize)   /* enough to decode - shortcut */
                {
                    LZ5F_errorCode_t errorCode = LZ5F_decodeHeader(dctxPtr, srcPtr, srcEnd-srcPtr);
                    if (LZ5F_isError(errorCode)) return errorCode;
                    srcPtr += errorCode;
                    break;
                }
                dctxPtr->tmpInSize = 0;
                dctxPtr->tmpInTarget = minFHSize;   /* minimum to attempt decode */
                dctxPtr->dStage = dstage_storeHeader;
            }

        case dstage_storeHeader:
            {
                size_t sizeToCopy = dctxPtr->tmpInTarget - dctxPtr->tmpInSize;
                if (sizeToCopy > (size_t)(srcEnd - srcPtr)) sizeToCopy =  srcEnd - srcPtr;
                memcpy(dctxPtr->header + dctxPtr->tmpInSize, srcPtr, sizeToCopy);
                dctxPtr->tmpInSize += sizeToCopy;
                srcPtr += sizeToCopy;
                if (dctxPtr->tmpInSize < dctxPtr->tmpInTarget)
                {
                    nextSrcSizeHint = (dctxPtr->tmpInTarget - dctxPtr->tmpInSize) + BHSize;   /* rest of header + nextBlockHeader */
                    doAnotherStage = 0;   /* not enough src data, ask for some more */
                    break;
                }
                {
                    LZ5F_errorCode_t errorCode = LZ5F_decodeHeader(dctxPtr, dctxPtr->header, dctxPtr->tmpInTarget);
                    if (LZ5F_isError(errorCode)) return errorCode;
                }
                break;
            }

        case dstage_getCBlockSize:
            {
                if ((size_t)(srcEnd - srcPtr) >= BHSize)
                {
                    selectedIn = srcPtr;
                    srcPtr += BHSize;
                }
                else
                {
                /* not enough input to read cBlockSize field */
                    dctxPtr->tmpInSize = 0;
                    dctxPtr->dStage = dstage_storeCBlockSize;
                }
            }

            if (dctxPtr->dStage == dstage_storeCBlockSize)
        case dstage_storeCBlockSize:
            {
                size_t sizeToCopy = BHSize - dctxPtr->tmpInSize;
                if (sizeToCopy > (size_t)(srcEnd - srcPtr)) sizeToCopy = srcEnd - srcPtr;
                memcpy(dctxPtr->tmpIn + dctxPtr->tmpInSize, srcPtr, sizeToCopy);
                srcPtr += sizeToCopy;
                dctxPtr->tmpInSize += sizeToCopy;
                if (dctxPtr->tmpInSize < BHSize) /* not enough input to get full cBlockSize; wait for more */
                {
                    nextSrcSizeHint = BHSize - dctxPtr->tmpInSize;
                    doAnotherStage  = 0;
                    break;
                }
                selectedIn = dctxPtr->tmpIn;
            }

        /* case dstage_decodeCBlockSize: */   /* no more direct access, to prevent scan-build warning */
            {
                size_t nextCBlockSize = LZ5F_readLE32(selectedIn) & 0x7FFFFFFFU;
                if (nextCBlockSize==0)   /* frameEnd signal, no more CBlock */
                {
                    dctxPtr->dStage = dstage_getSuffix;
                    break;
                }
                if (nextCBlockSize > dctxPtr->maxBlockSize) return (size_t)-LZ5F_ERROR_GENERIC; /* invalid cBlockSize */
                dctxPtr->tmpInTarget = nextCBlockSize;
                if (LZ5F_readLE32(selectedIn) & LZ5F_BLOCKUNCOMPRESSED_FLAG)
                {
                    dctxPtr->dStage = dstage_copyDirect;
                    break;
                }
                dctxPtr->dStage = dstage_getCBlock;
                if (dstPtr==dstEnd)
                {
                    nextSrcSizeHint = nextCBlockSize + BHSize;
                    doAnotherStage = 0;
                }
                break;
            }

        case dstage_copyDirect:   /* uncompressed block */
            {
                size_t sizeToCopy = dctxPtr->tmpInTarget;
                if ((size_t)(srcEnd-srcPtr) < sizeToCopy) sizeToCopy = srcEnd - srcPtr;  /* not enough input to read full block */
                if ((size_t)(dstEnd-dstPtr) < sizeToCopy) sizeToCopy = dstEnd - dstPtr;
                memcpy(dstPtr, srcPtr, sizeToCopy);
                if (dctxPtr->frameInfo.contentChecksumFlag) XXH32_update(&(dctxPtr->xxh), srcPtr, sizeToCopy);
                if (dctxPtr->frameInfo.contentSize) dctxPtr->frameRemainingSize -= sizeToCopy;

                /* dictionary management */
                if (dctxPtr->frameInfo.blockMode==LZ5F_blockLinked)
                    LZ5F_updateDict(dctxPtr, dstPtr, sizeToCopy, dstStart, 0);

                srcPtr += sizeToCopy;
                dstPtr += sizeToCopy;
                if (sizeToCopy == dctxPtr->tmpInTarget)   /* all copied */
                {
                    dctxPtr->dStage = dstage_getCBlockSize;
                    break;
                }
                dctxPtr->tmpInTarget -= sizeToCopy;   /* still need to copy more */
                nextSrcSizeHint = dctxPtr->tmpInTarget + BHSize;
                doAnotherStage = 0;
                break;
            }

        case dstage_getCBlock:   /* entry from dstage_decodeCBlockSize */
            {
                if ((size_t)(srcEnd-srcPtr) < dctxPtr->tmpInTarget)
                {
                    dctxPtr->tmpInSize = 0;
                    dctxPtr->dStage = dstage_storeCBlock;
                    break;
                }
                selectedIn = srcPtr;
                srcPtr += dctxPtr->tmpInTarget;
                dctxPtr->dStage = dstage_decodeCBlock;
                break;
            }

        case dstage_storeCBlock:
            {
                size_t sizeToCopy = dctxPtr->tmpInTarget - dctxPtr->tmpInSize;
                if (sizeToCopy > (size_t)(srcEnd-srcPtr)) sizeToCopy = srcEnd-srcPtr;
                memcpy(dctxPtr->tmpIn + dctxPtr->tmpInSize, srcPtr, sizeToCopy);
                dctxPtr->tmpInSize += sizeToCopy;
                srcPtr += sizeToCopy;
                if (dctxPtr->tmpInSize < dctxPtr->tmpInTarget)  /* need more input */
                {
                    nextSrcSizeHint = (dctxPtr->tmpInTarget - dctxPtr->tmpInSize) + BHSize;
                    doAnotherStage=0;
                    break;
                }
                selectedIn = dctxPtr->tmpIn;
                dctxPtr->dStage = dstage_decodeCBlock;
                break;
            }

        case dstage_decodeCBlock:
            {
                if ((size_t)(dstEnd-dstPtr) < dctxPtr->maxBlockSize)   /* not enough place into dst : decode into tmpOut */
                    dctxPtr->dStage = dstage_decodeCBlock_intoTmp;
                else
                    dctxPtr->dStage = dstage_decodeCBlock_intoDst;
                break;
            }

        case dstage_decodeCBlock_intoDst:
            {
                int (*decoder)(const char*, char*, int, int, const char*, int);
                int decodedSize;

                if (dctxPtr->frameInfo.blockMode == LZ5F_blockLinked)
                    decoder = LZ5_decompress_safe_usingDict;
                else
                    decoder = LZ5F_decompress_safe;

                decodedSize = decoder((const char*)selectedIn, (char*)dstPtr, (int)dctxPtr->tmpInTarget, (int)dctxPtr->maxBlockSize, (const char*)dctxPtr->dict, (int)dctxPtr->dictSize);
                if (decodedSize < 0) return (size_t)-LZ5F_ERROR_GENERIC;    /* decompression failed */
                if (dctxPtr->frameInfo.contentChecksumFlag) XXH32_update(&(dctxPtr->xxh), dstPtr, decodedSize);
                if (dctxPtr->frameInfo.contentSize) dctxPtr->frameRemainingSize -= decodedSize;

                /* dictionary management */
                if (dctxPtr->frameInfo.blockMode==LZ5F_blockLinked)
                    LZ5F_updateDict(dctxPtr, dstPtr, decodedSize, dstStart, 0);

                dstPtr += decodedSize;
                dctxPtr->dStage = dstage_getCBlockSize;
                break;
            }

        case dstage_decodeCBlock_intoTmp:
            {
                /* not enough place into dst : decode into tmpOut */
                int (*decoder)(const char*, char*, int, int, const char*, int);
                int decodedSize;

                if (dctxPtr->frameInfo.blockMode == LZ5F_blockLinked)
                    decoder = LZ5_decompress_safe_usingDict;
                else
                    decoder = LZ5F_decompress_safe;

                /* ensure enough place for tmpOut */
                if (dctxPtr->frameInfo.blockMode == LZ5F_blockLinked)
                {
                    if (dctxPtr->dict == dctxPtr->tmpOutBuffer)
                    {
                        if (dctxPtr->dictSize > 128 KB)
                        {
                            memcpy(dctxPtr->tmpOutBuffer, dctxPtr->dict + dctxPtr->dictSize - 64 KB, 64 KB);
                            dctxPtr->dictSize = 64 KB;
                        }
                        dctxPtr->tmpOut = dctxPtr->tmpOutBuffer + dctxPtr->dictSize;
                    }
                    else   /* dict not within tmp */
                    {
                        size_t reservedDictSpace = dctxPtr->dictSize;
                        if (reservedDictSpace > 64 KB) reservedDictSpace = 64 KB;
                        dctxPtr->tmpOut = dctxPtr->tmpOutBuffer + reservedDictSpace;
                    }
                }

                /* Decode */
                decodedSize = decoder((const char*)selectedIn, (char*)dctxPtr->tmpOut, (int)dctxPtr->tmpInTarget, (int)dctxPtr->maxBlockSize, (const char*)dctxPtr->dict, (int)dctxPtr->dictSize);
                if (decodedSize < 0) return (size_t)-LZ5F_ERROR_decompressionFailed;   /* decompression failed */
                if (dctxPtr->frameInfo.contentChecksumFlag) XXH32_update(&(dctxPtr->xxh), dctxPtr->tmpOut, decodedSize);
                if (dctxPtr->frameInfo.contentSize) dctxPtr->frameRemainingSize -= decodedSize;
                dctxPtr->tmpOutSize = decodedSize;
                dctxPtr->tmpOutStart = 0;
                dctxPtr->dStage = dstage_flushOut;
                break;
            }

        case dstage_flushOut:  /* flush decoded data from tmpOut to dstBuffer */
            {
                size_t sizeToCopy = dctxPtr->tmpOutSize - dctxPtr->tmpOutStart;
                if (sizeToCopy > (size_t)(dstEnd-dstPtr)) sizeToCopy = dstEnd-dstPtr;
                memcpy(dstPtr, dctxPtr->tmpOut + dctxPtr->tmpOutStart, sizeToCopy);

                /* dictionary management */
                if (dctxPtr->frameInfo.blockMode==LZ5F_blockLinked)
                    LZ5F_updateDict(dctxPtr, dstPtr, sizeToCopy, dstStart, 1);

                dctxPtr->tmpOutStart += sizeToCopy;
                dstPtr += sizeToCopy;

                /* end of flush ? */
                if (dctxPtr->tmpOutStart == dctxPtr->tmpOutSize)
                {
                    dctxPtr->dStage = dstage_getCBlockSize;
                    break;
                }
                nextSrcSizeHint = BHSize;
                doAnotherStage = 0;   /* still some data to flush */
                break;
            }

        case dstage_getSuffix:
            {
                size_t suffixSize = dctxPtr->frameInfo.contentChecksumFlag * 4;
                if (dctxPtr->frameRemainingSize) return (size_t)-LZ5F_ERROR_frameSize_wrong;   /* incorrect frame size decoded */
                if (suffixSize == 0)   /* frame completed */
                {
                    nextSrcSizeHint = 0;
                    dctxPtr->dStage = dstage_getHeader;
                    doAnotherStage = 0;
                    break;
                }
                if ((srcEnd - srcPtr) < 4)   /* not enough size for entire CRC */
                {
                    dctxPtr->tmpInSize = 0;
                    dctxPtr->dStage = dstage_storeSuffix;
                }
                else
                {
                    selectedIn = srcPtr;
                    srcPtr += 4;
                }
            }

            if (dctxPtr->dStage == dstage_storeSuffix)
        case dstage_storeSuffix:
            {
                size_t sizeToCopy = 4 - dctxPtr->tmpInSize;
                if (sizeToCopy > (size_t)(srcEnd - srcPtr)) sizeToCopy = srcEnd - srcPtr;
                memcpy(dctxPtr->tmpIn + dctxPtr->tmpInSize, srcPtr, sizeToCopy);
                srcPtr += sizeToCopy;
                dctxPtr->tmpInSize += sizeToCopy;
                if (dctxPtr->tmpInSize < 4)  /* not enough input to read complete suffix */
                {
                    nextSrcSizeHint = 4 - dctxPtr->tmpInSize;
                    doAnotherStage=0;
                    break;
                }
                selectedIn = dctxPtr->tmpIn;
            }

        /* case dstage_checkSuffix: */   /* no direct call, to avoid scan-build warning */
            {
                U32 readCRC = LZ5F_readLE32(selectedIn);
                U32 resultCRC = XXH32_digest(&(dctxPtr->xxh));
                if (readCRC != resultCRC) return (size_t)-LZ5F_ERROR_contentChecksum_invalid;
                nextSrcSizeHint = 0;
                dctxPtr->dStage = dstage_getHeader;
                doAnotherStage = 0;
                break;
            }

        case dstage_getSFrameSize:
            {
                if ((srcEnd - srcPtr) >= 4)
                {
                    selectedIn = srcPtr;
                    srcPtr += 4;
                }
                else
                {
                /* not enough input to read cBlockSize field */
                    dctxPtr->tmpInSize = 4;
                    dctxPtr->tmpInTarget = 8;
                    dctxPtr->dStage = dstage_storeSFrameSize;
                }
            }

            if (dctxPtr->dStage == dstage_storeSFrameSize)
        case dstage_storeSFrameSize:
            {
                size_t sizeToCopy = dctxPtr->tmpInTarget - dctxPtr->tmpInSize;
                if (sizeToCopy > (size_t)(srcEnd - srcPtr)) sizeToCopy = srcEnd - srcPtr;
                memcpy(dctxPtr->header + dctxPtr->tmpInSize, srcPtr, sizeToCopy);
                srcPtr += sizeToCopy;
                dctxPtr->tmpInSize += sizeToCopy;
                if (dctxPtr->tmpInSize < dctxPtr->tmpInTarget) /* not enough input to get full sBlockSize; wait for more */
                {
                    nextSrcSizeHint = dctxPtr->tmpInTarget - dctxPtr->tmpInSize;
                    doAnotherStage = 0;
                    break;
                }
                selectedIn = dctxPtr->header + 4;
            }

        /* case dstage_decodeSFrameSize: */   /* no direct access */
            {
                size_t SFrameSize = LZ5F_readLE32(selectedIn);
                dctxPtr->frameInfo.contentSize = SFrameSize;
                dctxPtr->tmpInTarget = SFrameSize;
                dctxPtr->dStage = dstage_skipSkippable;
                break;
            }

        case dstage_skipSkippable:
            {
                size_t skipSize = dctxPtr->tmpInTarget;
                if (skipSize > (size_t)(srcEnd-srcPtr)) skipSize = srcEnd-srcPtr;
                srcPtr += skipSize;
                dctxPtr->tmpInTarget -= skipSize;
                doAnotherStage = 0;
                nextSrcSizeHint = dctxPtr->tmpInTarget;
                if (nextSrcSizeHint) break;
                dctxPtr->dStage = dstage_getHeader;
                break;
            }
        }
    }

    /* preserve dictionary within tmp if necessary */
    if ( (dctxPtr->frameInfo.blockMode==LZ5F_blockLinked)
        &&(dctxPtr->dict != dctxPtr->tmpOutBuffer)
        &&(!decompressOptionsPtr->stableDst)
        &&((unsigned)(dctxPtr->dStage-1) < (unsigned)(dstage_getSuffix-1))
        )
    {
        if (dctxPtr->dStage == dstage_flushOut)
        {
            size_t preserveSize = dctxPtr->tmpOut - dctxPtr->tmpOutBuffer;
            size_t copySize = 64 KB - dctxPtr->tmpOutSize;
            const BYTE* oldDictEnd = dctxPtr->dict + dctxPtr->dictSize - dctxPtr->tmpOutStart;
            if (dctxPtr->tmpOutSize > 64 KB) copySize = 0;
            if (copySize > preserveSize) copySize = preserveSize;

            memcpy(dctxPtr->tmpOutBuffer + preserveSize - copySize, oldDictEnd - copySize, copySize);

            dctxPtr->dict = dctxPtr->tmpOutBuffer;
            dctxPtr->dictSize = preserveSize + dctxPtr->tmpOutStart;
        }
        else
        {
            size_t newDictSize = dctxPtr->dictSize;
            const BYTE* oldDictEnd = dctxPtr->dict + dctxPtr->dictSize;
            if ((newDictSize) > 64 KB) newDictSize = 64 KB;

            memcpy(dctxPtr->tmpOutBuffer, oldDictEnd - newDictSize, newDictSize);

            dctxPtr->dict = dctxPtr->tmpOutBuffer;
            dctxPtr->dictSize = newDictSize;
            dctxPtr->tmpOut = dctxPtr->tmpOutBuffer + newDictSize;
        }
    }

    /* require function to be called again from position where it stopped */
    if (srcPtr<srcEnd)
        dctxPtr->srcExpect = srcPtr;
    else
        dctxPtr->srcExpect = NULL;

    *srcSizePtr = (srcPtr - srcStart);
    *dstSizePtr = (dstPtr - dstStart);
    return nextSrcSizeHint;
}
