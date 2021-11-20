
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

#ifndef LZ5MT_H
#define LZ5MT_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <stddef.h>   /* size_t */

/* current maximum the library will accept */
#define LZ5MT_THREAD_MAX 128
#define LZ5MT_LEVEL_MIN    1
#define LZ5MT_LEVEL_MAX   15

#define LZ5FMT_MAGICNUMBER     0x184D2205U
#define LZ5FMT_MAGIC_SKIPPABLE 0x184D2A50U

/* **************************************
 * Error Handling
 ****************************************/

extern size_t lz5mt_errcode;

typedef enum {
  LZ5MT_error_no_error,
  LZ5MT_error_memory_allocation,
  LZ5MT_error_read_fail,
  LZ5MT_error_write_fail,
  LZ5MT_error_data_error,
  LZ5MT_error_frame_compress,
  LZ5MT_error_frame_decompress,
  LZ5MT_error_compressionParameter_unsupported,
  LZ5MT_error_compression_library,
  LZ5MT_error_canceled,
  LZ5MT_error_maxCode
} LZ5MT_ErrorCode;

#ifdef ERROR
#  undef ERROR   /* reported already defined on VS 2015 (Rich Geldreich) */
#endif
#define PREFIX(name) LZ5MT_error_##name
#define ERROR(name)  ((size_t)-PREFIX(name))
extern unsigned LZ5MT_isError(size_t code);
extern const char* LZ5MT_getErrorString(size_t code);

/* **************************************
 * Structures
 ****************************************/

typedef struct {
	void *buf;		/* ptr to data */
	size_t size;		/* current filled in buf */
	size_t allocated;	/* length of buf */
} LZ5MT_Buffer;

/**
 * reading and writing functions
 * - you can use stdio functions or plain read/write
 * - just write some wrapper on your own
 * - a sample is given in 7-Zip ZS or lz5mt.c
 * - the function should return -1 on error and zero on success
 * - the read or written bytes will go to in->size or out->size
 */
typedef int (fn_read) (void *args, LZ5MT_Buffer * in);
typedef int (fn_write) (void *args, LZ5MT_Buffer * out);

typedef struct {
	fn_read *fn_read;
	void *arg_read;
	fn_write *fn_write;
	void *arg_write;
} LZ5MT_RdWr_t;

/* **************************************
 * Compression
 ****************************************/

typedef struct LZ5MT_CCtx_s LZ5MT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @level   - 1 .. 9
 * @threads - 1 .. LZ5MT_THREAD_MAX
 * @inputsize - if zero, becomes some optimal value for the level
 *            - if nonzero, the given value is taken
 */
LZ5MT_CCtx *LZ5MT_createCCtx(int threads, int level, int inputsize);

/**
 * 2) threaded compression
 * - errorcheck via 
 */
size_t LZ5MT_compressCCtx(LZ5MT_CCtx * ctx, LZ5MT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZ5MT_GetFramesCCtx(LZ5MT_CCtx * ctx);
size_t LZ5MT_GetInsizeCCtx(LZ5MT_CCtx * ctx);
size_t LZ5MT_GetOutsizeCCtx(LZ5MT_CCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZ5MT_freeCCtx(LZ5MT_CCtx * ctx);

/* **************************************
 * Decompression
 ****************************************/

typedef struct LZ5MT_DCtx_s LZ5MT_DCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @threads - 1 .. LZ5MT_THREAD_MAX
 * @ inputsize - used for single threaded standard lz5 format without skippable frames
 */
LZ5MT_DCtx *LZ5MT_createDCtx(int threads, int inputsize);

/**
 * 2) threaded compression
 * - return -1 on error
 */
size_t LZ5MT_decompressDCtx(LZ5MT_DCtx * ctx, LZ5MT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LZ5MT_GetFramesDCtx(LZ5MT_DCtx * ctx);
size_t LZ5MT_GetInsizeDCtx(LZ5MT_DCtx * ctx);
size_t LZ5MT_GetOutsizeDCtx(LZ5MT_DCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LZ5MT_freeDCtx(LZ5MT_DCtx * ctx);

#if defined (__cplusplus)
}
#endif
#endif				/* LZ5MT_H */
