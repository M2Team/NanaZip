
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

#ifndef BROTLIMT_H
#define BROTLIMT_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <stddef.h>   /* size_t */

/* current maximum the library will accept */
#define BROTLIMT_THREAD_MAX 128
#define BROTLIMT_LEVEL_MIN    0
#define BROTLIMT_LEVEL_MAX   11

#define BROTLIMT_MAGICNUMBER     0x5242U /* BR */
#define BROTLIMT_MAGIC_SKIPPABLE 0x184D2A50U

#define BROTLI_VERSION_MAJOR 1
#define BROTLI_VERSION_MINOR 0

/* **************************************
 * Error Handling
 ****************************************/

typedef enum {
  BROTLIMT_error_no_error,
  BROTLIMT_error_memory_allocation,
  BROTLIMT_error_read_fail,
  BROTLIMT_error_write_fail,
  BROTLIMT_error_data_error,
  BROTLIMT_error_frame_compress,
  BROTLIMT_error_frame_decompress,
  BROTLIMT_error_compressionParameter_unsupported,
  BROTLIMT_error_compression_library,
  BROTLIMT_error_canceled,
  BROTLIMT_error_maxCode
} BROTLIMT_ErrorCode;

#define PREFIX(name) BROTLIMT_error_##name
#define MT_ERROR(name)  ((size_t)-PREFIX(name))
extern unsigned BROTLIMT_isError(size_t code);
extern const char* BROTLIMT_getErrorString(size_t code);

/* **************************************
 * Structures
 ****************************************/

typedef struct {
	void *buf;		/* ptr to data */
	size_t size;		/* current filled in buf */
	size_t allocated;	/* length of buf */
} BROTLIMT_Buffer;

/**
 * reading and writing functions
 * - you can use stdio functions or plain read/write
 * - just write some wrapper on your own
 * - a sample is given in 7-Zip ZS or bromt.c
 * - the function should return -1 on error and zero on success
 * - the read or written bytes will go to in->size or out->size
 */
typedef int (fn_read) (void *args, BROTLIMT_Buffer * in);
typedef int (fn_write) (void *args, BROTLIMT_Buffer * out);

typedef struct {
	fn_read *fn_read;
	void *arg_read;
	fn_write *fn_write;
	void *arg_write;
} BROTLIMT_RdWr_t;

/* **************************************
 * Compression
 ****************************************/

typedef struct BROTLIMT_CCtx_s BROTLIMT_CCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @level   - 1 .. 9
 * @threads - 1 .. BROTLIMT_THREAD_MAX
 * @inputsize - if zero, becomes some optimal value for the level
 *            - if nonzero, the given value is taken
 */
BROTLIMT_CCtx *BROTLIMT_createCCtx(int threads, int level, int inputsize);

/**
 * 2) threaded compression
 * - errorcheck via 
 */
size_t BROTLIMT_compressCCtx(BROTLIMT_CCtx * ctx, BROTLIMT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t BROTLIMT_GetFramesCCtx(BROTLIMT_CCtx * ctx);
size_t BROTLIMT_GetInsizeCCtx(BROTLIMT_CCtx * ctx);
size_t BROTLIMT_GetOutsizeCCtx(BROTLIMT_CCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void BROTLIMT_freeCCtx(BROTLIMT_CCtx * ctx);

/* **************************************
 * Decompression
 ****************************************/

typedef struct BROTLIMT_DCtx_s BROTLIMT_DCtx;

/**
 * 1) allocate new cctx
 * - return cctx or zero on error
 *
 * @threads - 1 .. BROTLIMT_THREAD_MAX
 * @ inputsize - used for single threaded standard bro format without skippable frames
 */
BROTLIMT_DCtx *BROTLIMT_createDCtx(int threads, int inputsize);

/**
 * 2) threaded compression
 * - return -1 on error
 */
size_t BROTLIMT_decompressDCtx(BROTLIMT_DCtx * ctx, BROTLIMT_RdWr_t * rdwr);

/**
 * 3) get some statistic
 */
size_t BROTLIMT_GetFramesDCtx(BROTLIMT_DCtx * ctx);
size_t BROTLIMT_GetInsizeDCtx(BROTLIMT_DCtx * ctx);
size_t BROTLIMT_GetOutsizeDCtx(BROTLIMT_DCtx * ctx);

/**
 * 4) free cctx
 * - no special return value
 */
void BROTLIMT_freeDCtx(BROTLIMT_DCtx * ctx);

#if defined (__cplusplus)
}
#endif
#endif				/* BROTLIMT_H */
