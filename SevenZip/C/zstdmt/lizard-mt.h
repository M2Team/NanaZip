
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

#ifndef LIZARDMT_H
#define LIZARDMT_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <stddef.h>   /* size_t */

/* current maximum the library will accept */
#define LIZARDMT_THREAD_MAX 128
#define LIZARDMT_LEVEL_MIN   10
#define LIZARDMT_LEVEL_MAX   49

#define LIZARDFMT_MAGICNUMBER     0x184D2206U
#define LIZARDFMT_MAGIC_SKIPPABLE 0x184D2A50U

/* **************************************
 * Error Handling
 ****************************************/

extern size_t lizardmt_errcode;

typedef enum {
  LIZARDMT_error_no_error,
  LIZARDMT_error_memory_allocation,
  LIZARDMT_error_read_fail,
  LIZARDMT_error_write_fail,
  LIZARDMT_error_data_error,
  LIZARDMT_error_frame_compress,
  LIZARDMT_error_frame_decompress,
  LIZARDMT_error_compressionParameter_unsupported,
  LIZARDMT_error_compression_library,
  LIZARDMT_error_canceled,
  LIZARDMT_error_maxCode
} LIZARDMT_ErrorCode;

#ifdef ERROR
#  undef ERROR   /* reported already defined on VS 2015 (Rich Geldreich) */
#endif
#define PREFIX(name) LIZARDMT_error_##name
#define ERROR(name)  ((size_t)-PREFIX(name))
extern unsigned LIZARDMT_isError(size_t code);
extern const char* LIZARDMT_getErrorString(size_t code);

/* **************************************
 * Structures
 ****************************************/

typedef struct {
	void *buf;		/* ptr to data */
	size_t size;		/* current filled in buf */
	size_t allocated;	/* length of buf */
} LIZARDMT_Buffer;

/**
 * reading and writing functions
 * - you can use stdio functions or plain read/write
 * - just write some wrapper on your own
 * - a sample is given in 7-Zip ZS or lizardmt.c
 * - the function should return -1 on error and zero on success
 * - the read or written bytes will go to in->size or out->size
 */
typedef int (fn_read) (void *args, LIZARDMT_Buffer * in);
typedef int (fn_write) (void *args, LIZARDMT_Buffer * out);

typedef struct {
	fn_read *fn_read;
	void *arg_read;
	fn_write *fn_write;
	void *arg_write;
} LIZARDMT_RdWr_t;

/* **************************************
 * Compression
 ****************************************/

typedef struct LIZARDMT_CCtx_s LIZARDMT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @level   - 1 .. 9
 * @threads - 1 .. LIZARDMT_THREAD_MAX
 * @inputsize - if zero, becomes some optimal value for the level
 *            - if nonzero, the given value is taken
 */
LIZARDMT_CCtx *LIZARDMT_createCCtx(int threads, int level, int inputsize);

/**
 * 2) threaded compression
 * - errorcheck via 
 */
size_t LIZARDMT_compressCCtx(LIZARDMT_CCtx * ctx, LIZARDMT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LIZARDMT_GetFramesCCtx(LIZARDMT_CCtx * ctx);
size_t LIZARDMT_GetInsizeCCtx(LIZARDMT_CCtx * ctx);
size_t LIZARDMT_GetOutsizeCCtx(LIZARDMT_CCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LIZARDMT_freeCCtx(LIZARDMT_CCtx * ctx);

/* **************************************
 * Decompression
 ****************************************/

typedef struct LIZARDMT_DCtx_s LIZARDMT_DCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @threads - 1 .. LIZARDMT_THREAD_MAX
 * @ inputsize - used for single threaded standard lizard format without skippable frames
 */
LIZARDMT_DCtx *LIZARDMT_createDCtx(int threads, int inputsize);

/**
 * 2) threaded compression
 * - return -1 on error
 */
size_t LIZARDMT_decompressDCtx(LIZARDMT_DCtx * ctx, LIZARDMT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t LIZARDMT_GetFramesDCtx(LIZARDMT_DCtx * ctx);
size_t LIZARDMT_GetInsizeDCtx(LIZARDMT_DCtx * ctx);
size_t LIZARDMT_GetOutsizeDCtx(LIZARDMT_DCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void LIZARDMT_freeDCtx(LIZARDMT_DCtx * ctx);

#if defined (__cplusplus)
}
#endif
#endif				/* LIZARDMT_H */
