
/**
 * Copyright (c) 2016 - 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

/* ***************************************
 * Defines
 ****************************************/

#ifndef LZ4MT_H
#define LZ4MT_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <stddef.h>   /* size_t */

/* current maximum the library will accept */
#define LZ4MT_THREAD_MAX 128
#define LZ4MT_LEVEL_MIN    1
#define LZ4MT_LEVEL_MAX   12

#define LZ4FMT_MAGICNUMBER     0x184D2204U
#define LZ4FMT_MAGIC_SKIPPABLE 0x184D2A50U

/* **************************************
 * Error Handling
 ****************************************/

extern size_t lz4mt_errcode;

typedef enum {
  LZ4MT_error_no_error,
  LZ4MT_error_memory_allocation,
  LZ4MT_error_read_fail,
  LZ4MT_error_write_fail,
  LZ4MT_error_data_error,
  LZ4MT_error_frame_compress,
  LZ4MT_error_frame_decompress,
  LZ4MT_error_compressionParameter_unsupported,
  LZ4MT_error_compression_library,
  LZ4MT_error_canceled,
  LZ4MT_error_maxCode
} LZ4MT_ErrorCode;

#ifdef ERROR
#  undef ERROR   /* reported already defined on VS 2015 (Rich Geldreich) */
#endif
#define PREFIX(name) LZ4MT_error_##name
#define ERROR(name)  ((size_t)-PREFIX(name))
extern unsigned LZ4MT_isError(size_t code);
extern const char* LZ4MT_getErrorString(size_t code);

/* **************************************
 * Structures
 ****************************************/

typedef struct {
	void *buf;		/* ptr to data */
	size_t size;		/* current filled in buf */
	size_t allocated;	/* length of buf */
} LZ4MT_Buffer;

/**
 * reading and writing functions
 * - you can use stdio functions or plain read/write
 * - just write some wrapper on your own
 * - a sample is given in 7-Zip ZS or lz4mt.c
 * - the function should return -1 on error and zero on success
 * - the read or written bytes will go to in->size or out->size
 */
typedef int (fn_read) (void *args, LZ4MT_Buffer * in);
typedef int (fn_write) (void *args, LZ4MT_Buffer * out);

typedef struct {
	fn_read *fn_read;
	void *arg_read;
	fn_write *fn_write;
	void *arg_write;
} LZ4MT_RdWr_t;

/* **************************************
 * Compression
 ****************************************/

typedef struct LZ4MT_CCtx_s LZ4MT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @level   - 1 .. 9
 * @threads - 1 .. LZ4MT_THREAD_MAX
 * @inputsize - if zero, becomes some optimal value for the level
 *            - if nonzero, the given value is taken
 */
LZ4MT_CCtx *LZ4MT_createCCtx(int threads, int level, int inputsize);

/**
 * 2) threaded compression
 * - errorcheck via 
 */
size_t LZ4MT_compressCCtx(LZ4MT_CCtx * ctx, LZ4MT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZ4MT_GetFramesCCtx(LZ4MT_CCtx * ctx);
size_t LZ4MT_GetInsizeCCtx(LZ4MT_CCtx * ctx);
size_t LZ4MT_GetOutsizeCCtx(LZ4MT_CCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZ4MT_freeCCtx(LZ4MT_CCtx * ctx);

/* **************************************
 * Decompression
 ****************************************/

typedef struct LZ4MT_DCtx_s LZ4MT_DCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @threads - 1 .. LZ4MT_THREAD_MAX
 * @ inputsize - used for single threaded standard lz4 format without skippable frames
 */
LZ4MT_DCtx *LZ4MT_createDCtx(int threads, int inputsize);

/**
 * 2) threaded compression
 * - return -1 on error
 */
size_t LZ4MT_decompressDCtx(LZ4MT_DCtx * ctx, LZ4MT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZ4MT_GetFramesDCtx(LZ4MT_DCtx * ctx);
size_t LZ4MT_GetInsizeDCtx(LZ4MT_DCtx * ctx);
size_t LZ4MT_GetOutsizeDCtx(LZ4MT_DCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZ4MT_freeDCtx(LZ4MT_DCtx * ctx);

#if defined (__cplusplus)
}
#endif
#endif				/* LZ4MT_H */
