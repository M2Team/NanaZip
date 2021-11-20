/*
 * Copyright (c) 2017-present, Conor McCarthy
 * All rights reserved.
 * Based on zstd.h copyright Yann Collet
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
*/
#if defined (__cplusplus)
extern "C" {
#endif

#ifndef FAST_LZMA2_H
#define FAST_LZMA2_H

/* ======   Dependency   ======*/
#include <stddef.h>   /* size_t */


/* =====   FL2LIB_API : control library symbols visibility   ===== */
#ifndef FL2LIB_VISIBILITY
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define FL2LIB_VISIBILITY __attribute__ ((visibility ("default")))
#  else
#    define FL2LIB_VISIBILITY
#  endif
#endif
#if defined(FL2_DLL_EXPORT) && (FL2_DLL_EXPORT==1)
#  define FL2LIB_API __declspec(dllexport) FL2LIB_VISIBILITY
#elif defined(FL2_DLL_IMPORT) && (FL2_DLL_IMPORT==1)
#  define FL2LIB_API __declspec(dllimport) FL2LIB_VISIBILITY /* It isn't required but allows to generate better code, saving a function pointer load from the IAT and an indirect jump.*/
#else
#  define FL2LIB_API FL2LIB_VISIBILITY
#endif

/* ======   Calling convention   ======*/

#if !defined _WIN32 || defined __x86_64__s || defined _M_X64 || (defined __SIZEOF_POINTER__ && __SIZEOF_POINTER__ == 8)
#  define FL2LIB_CALL
#elif defined(__GNUC__)
#  define FL2LIB_CALL __attribute__((cdecl))
#elif defined(_MSC_VER)
#  define FL2LIB_CALL __cdecl
#else
#  define FL2LIB_CALL
#endif

/*******************************************************************************************************
Introduction

*********************************************************************************************************/

/*------   Version   ------*/
#define FL2_VERSION_MAJOR    1
#define FL2_VERSION_MINOR    0
#define FL2_VERSION_RELEASE  1

#define FL2_VERSION_NUMBER  (FL2_VERSION_MAJOR *100*100 + FL2_VERSION_MINOR *100 + FL2_VERSION_RELEASE)
FL2LIB_API unsigned FL2LIB_CALL FL2_versionNumber(void);   /**< useful to check dll version */

#define FL2_LIB_VERSION FL2_VERSION_MAJOR.FL2_VERSION_MINOR.FL2_VERSION_RELEASE
#define FL2_QUOTE(str) #str
#define FL2_EXPAND_AND_QUOTE(str) FL2_QUOTE(str)
#define FL2_VERSION_STRING FL2_EXPAND_AND_QUOTE(FL2_LIB_VERSION)
FL2LIB_API const char* FL2LIB_CALL FL2_versionString(void);


#define FL2_MAXTHREADS 200


/***************************************
*  Simple API
***************************************/

/*! FL2_compress() :
 *  Compresses `src` content as a single LZMA2 compressed stream into already allocated `dst`.
 *  Call FL2_compressMt() to use > 1 thread. Specify nbThreads = 0 to use all cores.
 *  @return : compressed size written into `dst` (<= `dstCapacity),
 *            or an error code if it fails (which can be tested using FL2_isError()). */
FL2LIB_API size_t FL2LIB_CALL FL2_compress(void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    int compressionLevel);

FL2LIB_API size_t FL2LIB_CALL FL2_compressMt(void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    int compressionLevel,
    unsigned nbThreads);

/*! FL2_decompress() :
 *  Decompresses a single LZMA2 compressed stream from `src` into already allocated `dst`.
 *  `compressedSize` : must be at least the size of the LZMA2 stream.
 *  `dstCapacity` is the original, uncompressed size to regenerate, returned by calling
 *  FL2_findDecompressedSize().
 *  Call FL2_decompressMt() to use > 1 thread. Specify nbThreads = 0 to use all cores. The stream
 *  must contain dictionary resets to use multiple threads. These are inserted during compression by
 *  default. The frequency can be changed/disabled with the FL2_p_resetInterval parameter setting.
 *  @return : the number of bytes decompressed into `dst` (<= `dstCapacity`),
 *            or an errorCode if it fails (which can be tested using FL2_isError()). */
FL2LIB_API size_t FL2LIB_CALL FL2_decompress(void* dst, size_t dstCapacity,
    const void* src, size_t compressedSize);

FL2LIB_API size_t FL2LIB_CALL FL2_decompressMt(void* dst, size_t dstCapacity,
    const void* src, size_t compressedSize,
    unsigned nbThreads);

/*! FL2_findDecompressedSize()
 *  `src` should point to the start of a LZMA2 encoded stream.
 *  `srcSize` must be at least as large as the LZMA2 stream including end marker.
 *  A property byte is assumed to exist at position 0 in `src`. If the stream was created without one,
 *  subtract 1 byte from `src` when passing it to the function.
 *  @return : - decompressed size of the stream in `src`, if known
 *            - FL2_CONTENTSIZE_ERROR if an error occurred (e.g. corruption, srcSize too small)
 *   note 1 : a 0 return value means the stream is valid but "empty".
 *   note 2 : decompressed size can be very large (64-bits value),
 *            potentially larger than what local system can handle as a single memory segment.
 *            In which case, it's necessary to use streaming mode to decompress data.
 *   note 5 : If source is untrusted, decompressed size could be wrong or intentionally modified.
 *            Always ensure return value fits within application's authorized limits.
 *            Each application can set its own limits. */
#define FL2_CONTENTSIZE_ERROR (size_t)-1
FL2LIB_API unsigned long long FL2LIB_CALL FL2_findDecompressedSize(const void *src, size_t srcSize);


/*======  Helper functions  ======*/
FL2LIB_API size_t      FL2LIB_CALL FL2_compressBound(size_t srcSize); /*!< maximum compressed size in worst case scenario */
FL2LIB_API unsigned    FL2LIB_CALL FL2_isError(size_t code);          /*!< tells if a `size_t` function result is an error code */
FL2LIB_API unsigned    FL2LIB_CALL FL2_isTimedOut(size_t code);       /*!< tells if a `size_t` function result is the timeout code */
FL2LIB_API const char* FL2LIB_CALL FL2_getErrorName(size_t code);     /*!< provides readable string from an error code */
FL2LIB_API int         FL2LIB_CALL FL2_maxCLevel(void);               /*!< maximum compression level available */
FL2LIB_API int         FL2LIB_CALL FL2_maxHighCLevel(void);           /*!< maximum compression level available in high mode */


/***************************************
*  Explicit memory management
***************************************/

/*= Compression context
 *  When compressing many times, it is recommended to allocate a context just once,
 *  and re-use it for each successive compression operation. This will make workload
 *  friendlier for system's memory. The context may not use the number of threads requested
 *  if the library is compiled for single-threaded compression or nbThreads > FL2_MAXTHREADS.
 *  Call FL2_getCCtxThreadCount to obtain the actual number allocated. */
typedef struct FL2_CCtx_s FL2_CCtx;
FL2LIB_API FL2_CCtx* FL2LIB_CALL FL2_createCCtx(void);
FL2LIB_API FL2_CCtx* FL2LIB_CALL FL2_createCCtxMt(unsigned nbThreads);
FL2LIB_API void      FL2LIB_CALL FL2_freeCCtx(FL2_CCtx* cctx);

FL2LIB_API unsigned FL2LIB_CALL FL2_getCCtxThreadCount(const FL2_CCtx* cctx);

/*! FL2_compressCCtx() :
 *  Same as FL2_compress(), but requires an allocated FL2_CCtx (see FL2_createCCtx()). */
FL2LIB_API size_t FL2LIB_CALL FL2_compressCCtx(FL2_CCtx* cctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcSize,
    int compressionLevel);

/*! FL2_getCCtxDictProp() :
 *  Get the dictionary size property.
 *  Intended for use with the FL2_p_omitProperties parameter for creating a
 *  7-zip or XZ compatible LZMA2 stream. */
FL2LIB_API unsigned char FL2LIB_CALL FL2_getCCtxDictProp(FL2_CCtx* cctx);


/****************************
*  Decompression
****************************/

/*= Decompression context
 *  When decompressing many times, it is recommended to allocate a context only once,
 *  and re-use it for each successive decompression operation. This will make the workload
 *  friendlier for the system's memory.
 *  The context may not allocate the number of threads requested if the library is
 *  compiled for single-threaded compression or nbThreads > FL2_MAXTHREADS.
 *  Call FL2_getDCtxThreadCount to obtain the actual number allocated.
 *  At least nbThreads dictionary resets must exist in the stream to use all of the
 *  threads. Dictionary resets are inserted into the stream according to the
 *  FL2_p_resetInterval parameter used in the compression context. */
typedef struct FL2_DCtx_s FL2_DCtx;
FL2LIB_API FL2_DCtx* FL2LIB_CALL FL2_createDCtx(void);
FL2LIB_API FL2_DCtx* FL2LIB_CALL FL2_createDCtxMt(unsigned nbThreads);
FL2LIB_API size_t    FL2LIB_CALL FL2_freeDCtx(FL2_DCtx* dctx);

FL2LIB_API unsigned FL2LIB_CALL FL2_getDCtxThreadCount(const FL2_DCtx* dctx);


/*! FL2_initDCtx() :
 *  Use only when a property byte is not present at input byte 0. No init is necessary otherwise.
 *  The caller must store the result from FL2_getCCtxDictProp() and pass it to this function. */
FL2LIB_API size_t FL2LIB_CALL FL2_initDCtx(FL2_DCtx* dctx, unsigned char prop);

/*! FL2_decompressDCtx() :
 *  Same as FL2_decompress(), requires an allocated FL2_DCtx (see FL2_createDCtx()) */
FL2LIB_API size_t FL2LIB_CALL FL2_decompressDCtx(FL2_DCtx* cctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcSize);

/****************************
*  Streaming
****************************/

typedef struct {
    const void* src;    /**< start of input buffer */
    size_t size;        /**< size of input buffer */
    size_t pos;         /**< position where reading stopped. Will be updated. Necessarily 0 <= pos <= size */
} FL2_inBuffer;

typedef struct {
    void*  dst;         /**< start of output buffer */
    size_t size;        /**< size of output buffer */
    size_t pos;         /**< position where writing stopped. Will be updated. Necessarily 0 <= pos <= size */
} FL2_outBuffer;

/*** Push/pull structs ***/

typedef struct {
    void*  dst;         /**< start of available dict buffer */
    unsigned long size; /**< size of dict remaining */
} FL2_dictBuffer;

typedef struct {
    const void* src;    /**< start of compressed data */
    size_t size;        /**< size of compressed data */
} FL2_cBuffer;

/*-***********************************************************************
 *  Streaming compression
 *
 *  A FL2_CStream object is required to track streaming operation.
 *  Use FL2_createCStream() and FL2_freeCStream() to create/release resources.
 *  FL2_CStream objects can be reused multiple times on consecutive compression operations.
 *  It is recommended to re-use FL2_CStream in situations where many streaming operations will be done
 *  consecutively, since it will reduce allocation and initialization time.
 *
 *  Call FL2_createCStreamMt() with a nonzero dualBuffer parameter to use two input dictionary buffers.
 *  The stream will not block on FL2_compressStream() and continues to accept data while compression is
 *  underway, until both buffers are full. Useful when I/O is slow.
 *  To compress with a single thread with dual buffering, call FL2_createCStreamMt with nbThreads=1.
 *
 *  Use FL2_initCStream() on the FL2_CStream object to start a new compression operation.
 *
 *  Use FL2_compressStream() repetitively to consume input stream.
 *  The function will automatically update the `pos` field.
 *  It will always consume the entire input unless an error occurs or the dictionary buffer is filled,
 *  unlike the decompression function.
 *
 *  The radix match finder allows compressed data to be stored in its match table during encoding.
 *  Applications may call streaming compression functions with output == NULL. In this case,
 *  when the function returns 1, the compressed data must be read from the internal buffers.
 *  Call FL2_getNextCompressedBuffer() repeatedly until it returns 0.
 *  Each call returns buffer information in the FL2_inBuffer parameter. Applications typically will 
 *  passed this to an I/O write function or downstream filter.
 *  Alternately, applications may pass an FL2_outBuffer object pointer to receive the output. In this
 *  case the return value is 1 if the buffer is full and more compressed data remains.
 *
 *  FL2_endStream() instructs to finish a stream. It will perform a flush and write the LZMA2
 *  termination byte (required). Call FL2_endStream() repeatedly until it returns 0.
 *
 *  Most functions may return a size_t error code, which can be tested using FL2_isError().
 *
 * *******************************************************************/

typedef struct FL2_CCtx_s FL2_CStream;

/*===== FL2_CStream management functions =====*/
FL2LIB_API FL2_CStream* FL2LIB_CALL FL2_createCStream(void);
FL2LIB_API FL2_CStream* FL2LIB_CALL FL2_createCStreamMt(unsigned nbThreads, int dualBuffer);
FL2LIB_API void FL2LIB_CALL FL2_freeCStream(FL2_CStream * fcs);

/*===== Streaming compression functions =====*/

/*! FL2_initCStream() :
 *  Call this function before beginning a new compressed data stream. To keep the stream object's
 *  current parameters, specify zero for the compression level. The object is set to the default
 *  level upon creation. */
FL2LIB_API size_t FL2LIB_CALL FL2_initCStream(FL2_CStream* fcs, int compressionLevel);

/*! FL2_setCStreamTimeout() :
 *  Sets a timeout in milliseconds. Zero disables the timeout (default). If a nonzero timout is set, functions
 *  FL2_compressStream(), FL2_getDictionaryBuffer(), FL2_updateDictionary(), FL2_getNextCompressedBuffer(),
 *  FL2_flushStream(), and FL2_endStream() may return a timeout code before compression of the current
 *  dictionary of data completes. FL2_isError() returns true for the timeout code, so check the code with
 *  FL2_isTimedOut() before testing for errors. With the exception of FL2_updateDictionary(), the above
 *  functions may be called again to wait for completion. A typical application for timeouts is to update the
 *  user on compression progress. */
FL2LIB_API size_t FL2LIB_CALL FL2_setCStreamTimeout(FL2_CStream * fcs, unsigned timeout);

/*! FL2_compressStream() :
 *  Reads data from input into the dictionary buffer. Compression will begin if the buffer fills up.
 *  A dual buffering stream will fill the second buffer while compression proceeds on the first.
 *  A call to FL2_compressStream() will wait for ongoing compression to complete if all dictionary space
 *  is filled. FL2_compressStream() must not be called with output == NULL unless the caller has read all
 *  compressed data from the CStream object.
 *  Returns 1 to indicate compressed data must be read (or output is full), or 0 otherwise. */
FL2LIB_API size_t FL2LIB_CALL FL2_compressStream(FL2_CStream* fcs, FL2_outBuffer *output, FL2_inBuffer* input);

/*! FL2_copyCStreamOutput() :
 *  Copies compressed data to the output buffer until the buffer is full or all available data is copied.
 *  If asynchronous compression is in progress, the function returns 0 without waiting.
 *  Returns 1 to indicate some compressed data remains, or 0 otherwise. */
FL2LIB_API size_t FL2LIB_CALL FL2_copyCStreamOutput(FL2_CStream* fcs, FL2_outBuffer *output);

/*** Push/pull functions ***/

/*! FL2_getDictionaryBuffer() :
 *  Returns a buffer in the FL2_outBuffer object, which the caller can directly read data into.
 *  Applications will normally pass this buffer to an I/O read function or upstream filter.
 *  Returns 0, or an error or timeout code. */
FL2LIB_API size_t FL2LIB_CALL FL2_getDictionaryBuffer(FL2_CStream* fcs, FL2_dictBuffer* dict);

/*! FL2_updateDictionary() :
 *  Informs the CStream how much data was added to the buffer. Compression begins if the dictionary
 *  was filled. Returns 1 to indicate compressed data must be read, 0 if not, or an error code. */
FL2LIB_API size_t FL2LIB_CALL FL2_updateDictionary(FL2_CStream* fcs, size_t addedSize);

/*! FL2_getNextCompressedBuffer() :
 *  Returns a buffer containing a slice of the compressed data. Call this function and process the data
 *  until the function returns zero. In most cases it will return a buffer for each compression thread
 *  used. It is sometimes less but never more than nbThreads. If asynchronous compression is in progress,
 *  this function will wait for completion before returning, or it will return the timeout code. */
FL2LIB_API size_t FL2LIB_CALL FL2_getNextCompressedBuffer(FL2_CStream* fcs, FL2_cBuffer* cbuf);

/******/

/*! FL2_getCStreamProgress() :
 *  Returns the number of bytes processed since the stream was initialized. This is a synthetic
 *  estimate because the match finder does not proceed sequentially through the data. If
 *  outputSize is not NULL, returns the number of bytes of compressed data generated. */
FL2LIB_API unsigned long long FL2LIB_CALL FL2_getCStreamProgress(const FL2_CStream * fcs, unsigned long long *outputSize);

/*! FL2_waitCStream() :
 *  Waits for compression to end. This function returns after the timeout set using
 *  FL2_setCStreamTimeout has elapsed. Unnecessary when no timeout is set.
 *  Returns 1 if compressed output is available, 0 if not, or the timeout code. */
FL2LIB_API size_t FL2LIB_CALL FL2_waitCStream(FL2_CStream * fcs);

/*! FL2_cancelCStream() :
 *  Cancels any compression operation underway. Useful only when dual buffering and/or timeouts
 *  are enabled. The stream will be returned to an uninitialized state. */
FL2LIB_API void FL2LIB_CALL FL2_cancelCStream(FL2_CStream *fcs);

/*! FL2_remainingOutputSize() :
 *  The amount of compressed data remaining to be read from the CStream object. */
FL2LIB_API size_t FL2LIB_CALL FL2_remainingOutputSize(const FL2_CStream* fcs);

/*! FL2_flushStream() :
 *  Compress all data remaining in the dictionary buffer(s). It may be necessary to call
 *  FL2_flushStream() more than once. If output == NULL the compressed data must be read from the
 *  CStream object after each call.
 *  Flushing is not normally useful and produces larger output.
 *  Returns 1 if input or output still exists in the CStream object, 0 if complete, or an error code. */
FL2LIB_API size_t FL2LIB_CALL FL2_flushStream(FL2_CStream* fcs, FL2_outBuffer *output);

/*! FL2_endStream() :
 *  Compress all data remaining in the dictionary buffer(s) and write the stream end marker. It may
 *  be necessary to call FL2_endStream() more than once. If output == NULL the compressed data must
 *  be read from the CStream object after each call.
 *  Returns 0 when compression is complete and all output has been flushed, 1 if not complete, or
 *  an error code. */
FL2LIB_API size_t FL2LIB_CALL FL2_endStream(FL2_CStream* fcs, FL2_outBuffer *output);

/*-***************************************************************************
 *  Streaming decompression
 *
 *  A FL2_DStream object is required to track streaming operations.
 *  Use FL2_createDStream() and FL2_freeDStream() to create/release resources.
 *  FL2_DStream objects can be re-used multiple times.
 *
 *  Use FL2_initDStream() to start a new decompression operation.
 *  @return : zero or an error code
 *
 *  Use FL2_decompressStream() repetitively to consume your input.
 *  The function will update both `pos` fields.
 *  If `input.pos < input.size`, some input has not been consumed.
 *  It's up to the caller to present again the remaining data.
 *  If `output.pos < output.size`, decoder has flushed everything it could.
 *  @return : 0 when a stream is completely decoded and fully flushed,
 *            1, which means there is still some decoding to do to complete the stream,
 *            or an error code, which can be tested using FL2_isError().
 * *******************************************************************************/

typedef struct FL2_DStream_s FL2_DStream;

/*===== FL2_DStream management functions =====*/
FL2LIB_API FL2_DStream* FL2LIB_CALL FL2_createDStream(void);
FL2LIB_API FL2_DStream* FL2LIB_CALL FL2_createDStreamMt(unsigned nbThreads);
FL2LIB_API size_t FL2LIB_CALL FL2_freeDStream(FL2_DStream* fds);

/*! FL2_setDStreamMemoryLimitMt() :
 *  Set a total size limit for multithreaded decoder input and output buffers. MT decoder memory
 *  usage is unknown until the input is parsed. If the limit is exceeded, the decoder switches to
 *  using a single thread.
 *  MT decoding memory usage is typically dictionary_size * 4 * nbThreads for the output
 *  buffers plus the size of the compressed input for that amount of output. */
FL2LIB_API void FL2LIB_CALL FL2_setDStreamMemoryLimitMt(FL2_DStream* fds, size_t limit);

/*! FL2_setDStreamTimeout() :
 *  Sets a timeout in milliseconds. Zero disables the timeout. If a nonzero timout is set,
 *  FL2_decompressStream() may return a timeout code before decompression of the available data
 *  completes. FL2_isError() returns true for the timeout code, so check the code with FL2_isTimedOut()
 *  before testing for errors. After a timeout occurs, do not call FL2_decompressStream() again unless
 *  a call to FL2_waitDStream() returns 1. A typical application for timeouts is to update the user on
 *  decompression progress. */
FL2LIB_API size_t FL2LIB_CALL FL2_setDStreamTimeout(FL2_DStream * fds, unsigned timeout);

/*! FL2_waitDStream() :
 *  Waits for decompression to end after a timeout has occurred. This function returns after the
 *  timeout set using FL2_setDStreamTimeout() has elapsed, or when decompression of available input is
 *  complete. Unnecessary when no timeout is set.
 *  Returns 0 if the stream is complete, 1 if not complete, or an error code. */
FL2LIB_API size_t FL2LIB_CALL FL2_waitDStream(FL2_DStream * fds);

/*! FL2_cancelDStream() :
 *  Frees memory allocated for MT decoding. If a timeout is set and the caller is waiting
 *  for completion of MT decoding, decompression in progress will be canceled. */
FL2LIB_API void FL2LIB_CALL FL2_cancelDStream(FL2_DStream *fds);

/*! FL2_getDStreamProgress() :
 *  Returns the number of bytes decoded since the stream was initialized. */
FL2LIB_API unsigned long long FL2LIB_CALL FL2_getDStreamProgress(const FL2_DStream * fds);

/*===== Streaming decompression functions =====*/

/*! FL2_initDStream() :
 *  Call this function before decompressing a stream. FL2_initDStream_withProp()
 *  must be used for streams which do not include a property byte at position zero.
 *  The caller is responsible for storing and passing the property byte.
 *  Returns 0 if okay, or an error if the stream object is still in use from a
 *  previous call to FL2_decompressStream() (see timeout info above). */
FL2LIB_API size_t FL2LIB_CALL FL2_initDStream(FL2_DStream* fds);
FL2LIB_API size_t FL2LIB_CALL FL2_initDStream_withProp(FL2_DStream* fds, unsigned char prop);

/*! FL2_decompressStream() :
 *  Reads data from input and decompresses to output.
 *  Returns 1 if the stream is unfinished, 0 if the terminator was encountered (he'll be back)
 *  and all data was written to output, or an error code. Call this function repeatedly if
 *  necessary, removing data from output and/or loading data into input before each call. */
FL2LIB_API size_t FL2LIB_CALL FL2_decompressStream(FL2_DStream* fds, FL2_outBuffer* output, FL2_inBuffer* input);

/*-***************************************************************************
 *  Compression parameters
 *
 *  Any function that takes a 'compressionLevel' parameter will replace any
 *  parameters affected by compression level that are already set.
 *  To use a preset level and modify it, call FL2_CCtx_setParameter with
 *  FL2_p_compressionLevel to set the level, then call FL2_CCtx_setParameter again
 *  with any other settings to change.
 *  Specify a compressionLevel of 0 when calling a compression function to keep
 *  the current parameters.
 * *******************************************************************************/

#define FL2_DICTLOG_MIN      20
#define FL2_DICTLOG_MAX_32   27
#define FL2_DICTLOG_MAX_64   30
#define FL2_DICTLOG_MAX      ((unsigned)(sizeof(size_t) == 4 ? FL2_DICTLOG_MAX_32 : FL2_DICTLOG_MAX_64))
#define FL2_DICTSIZE_MAX     (1U << FL2_DICTLOG_MAX)
#define FL2_DICTSIZE_MIN     (1U << FL2_DICTLOG_MIN)
#define FL2_BLOCK_OVERLAP_MIN 0
#define FL2_BLOCK_OVERLAP_MAX 14
#define FL2_RESET_INTERVAL_MIN 1
#define FL2_RESET_INTERVAL_MAX 16  /* small enough to fit FL2_DICTSIZE_MAX * FL2_RESET_INTERVAL_MAX in 32-bit size_t */
#define FL2_BUFFER_RESIZE_MIN 0
#define FL2_BUFFER_RESIZE_MAX 4
#define FL2_BUFFER_RESIZE_DEFAULT 2
#define FL2_CHAINLOG_MIN       4
#define FL2_CHAINLOG_MAX       14
#define FL2_HYBRIDCYCLES_MIN    1
#define FL2_HYBRIDCYCLES_MAX   64
#define FL2_SEARCH_DEPTH_MIN 6
#define FL2_SEARCH_DEPTH_MAX 254
#define FL2_FASTLENGTH_MIN    6   /* only used by optimizer */
#define FL2_FASTLENGTH_MAX  273   /* only used by optimizer */
#define FL2_LC_MIN 0
#define FL2_LC_MAX 4
#define FL2_LP_MIN 0
#define FL2_LP_MAX 4
#define FL2_PB_MIN 0
#define FL2_PB_MAX 4
#define FL2_LCLP_MAX 4

typedef enum {
    FL2_fast,
    FL2_opt,
    FL2_ultra
} FL2_strategy;

typedef struct {
    size_t   dictionarySize;   /* largest match distance : larger == more compression, more memory needed during decompression; > 64Mb == more memory per byte, slower */
    unsigned overlapFraction;  /* overlap between consecutive blocks in 1/16 units: larger == more compression, slower */
    unsigned chainLog;         /* HC3 sliding window : larger == more compression, slower; hybrid mode only (ultra) */
    unsigned cyclesLog;        /* nb of searches : larger == more compression, slower; hybrid mode only (ultra) */
    unsigned searchDepth;      /* maximum depth for resolving string matches : larger == more compression, slower */
    unsigned fastLength;       /* acceptable match size for parser : larger == more compression, slower; fast bytes parameter from 7-Zip */
    unsigned divideAndConquer; /* split long chains of 2-byte matches into shorter chains with a small overlap : faster, somewhat less compression; enabled by default */
    FL2_strategy strategy;     /* encoder strategy : fast, optimized or ultra (hybrid) */
} FL2_compressionParameters;

typedef enum {
    /* compression parameters */
    FL2_p_compressionLevel, /* Update all compression parameters according to pre-defined cLevel table
                             * Default level is FL2_CLEVEL_DEFAULT==6.
                             * Setting FL2_p_highCompression to 1 switches to an alternate cLevel table. */
    FL2_p_highCompression,  /* Maximize compression ratio for a given dictionary size.
                             * Levels 1..10 = dictionaryLog 20..29 (1 Mb..512 Mb).
                             * Typically provides a poor speed/ratio tradeoff. */
    FL2_p_dictionaryLog,    /* Maximum allowed back-reference distance, expressed as power of 2.
                             * Must be clamped between FL2_DICTLOG_MIN and FL2_DICTLOG_MAX.
                             * Default = 24 */
    FL2_p_dictionarySize,   /* Same as above but expressed as an absolute value. 
                             * Must be clamped between FL2_DICTSIZE_MIN and FL2_DICTSIZE_MAX.
                             * Default = 16 Mb */
    FL2_p_overlapFraction,  /* The radix match finder is block-based, so some overlap is retained from
                             * each block to improve compression of the next. This value is expressed
                             * as n / 16 of the block size (dictionary size). Larger values are slower.
                             * Values above 2 mostly yield only a small improvement in compression.
                             * A large value for a small dictionary may worsen multithreaded compression.
                             * Default = 2 */
    FL2_p_resetInterval,    /* For multithreaded decompression. A dictionary reset will occur
                             * after each dictionarySize * resetInterval bytes of input.
                             * Default = 4 */
    FL2_p_bufferResize,     /* Buffering speeds up the matchfinder. Buffer resize determines the percentage of
                             * the normal buffer size used, which depends on dictionary size.
                             * 0=50, 1=75, 2=100, 3=150, 4=200. Higher number = slower, better
                             * compression, higher memory usage. A CPU with a large memory cache
                             * may make effective use of a larger buffer.
                             * Default = 2 */
    FL2_p_hybridChainLog,   /* Size of the hybrid mode HC3 hash chain, as a power of 2.
                             * Resulting table size is (1 << (chainLog+2)) bytes.
                             * Larger tables result in better and slower compression.
                             * This parameter is only used by the hybrid "ultra" strategy.
                             * Default = 9 */
    FL2_p_hybridCycles,     /* Number of search attempts made by the HC3 match finder.
                             * Used only by the hybrid "ultra" strategy.
                             * More attempts result in slightly better and slower compression.
                             * Default = 1 */
    FL2_p_searchDepth,      /* Match finder will resolve string matches up to this length. If a longer
                             * match exists further back in the input, it will not be found.
                             * Default = 42 */
    FL2_p_fastLength,       /* Only useful for strategies >= opt.
                             * Length of match considered "good enough" to stop search.
                             * Larger values make compression stronger and slower.
                             * Default = 48 */
    FL2_p_divideAndConquer, /* Split long chains of 2-byte matches into shorter chains with a small overlap
                             * for further processing. Allows buffering of all chains at length 2.
                             * Faster, less compression. Generally a good tradeoff.
                             * Default = enabled */
    FL2_p_strategy,         /* 1 = fast; 2 = optimized, 3 = ultra (hybrid mode).
                             * The higher the value of the selected strategy, the more complex it is,
                             * resulting in stronger and slower compression.
                             * Default = ultra */
    FL2_p_literalCtxBits,   /* lc value for LZMA2 encoder
                             * Default = 3 */
    FL2_p_literalPosBits,   /* lp value for LZMA2 encoder
                             * Default = 0 */
    FL2_p_posBits,          /* pb value for LZMA2 encoder
                             * Default = 2 */
    FL2_p_omitProperties,   /* Omit the property byte at the start of the stream. For use within 7-zip */
                            /* or other containers which store the property byte elsewhere. */
                            /* A stream compressed under this setting cannot be decoded by this library. */
#ifndef NO_XXHASH
    FL2_p_doXXHash,         /* Calculate a 32-bit xxhash value from the input data and store it 
                             * after the stream terminator. The value will be checked on decompression.
                             * 0 = do not calculate; 1 = calculate (default) */
#endif
#ifdef RMF_REFERENCE
    FL2_p_useReferenceMF    /* Use the reference matchfinder for development purposes. SLOW. */
#endif
} FL2_cParameter;


/*! FL2_CCtx_setParameter() :
 *  Set one compression parameter, selected by enum FL2_cParameter.
 *  @result : informational value (typically, the one being set, possibly corrected),
 *            or an error code (which can be tested with FL2_isError()). */
FL2LIB_API size_t FL2LIB_CALL FL2_CCtx_setParameter(FL2_CCtx* cctx, FL2_cParameter param, size_t value);

/*! FL2_CCtx_getParameter() :
 *  Get one compression parameter, selected by enum FL2_cParameter.
 *  @result : the parameter value, or the parameter_unsupported error code
 *            (which can be tested with FL2_isError()). */
FL2LIB_API size_t FL2LIB_CALL FL2_CCtx_getParameter(FL2_CCtx* cctx, FL2_cParameter param);

/*! FL2_CStream_setParameter() :
 *  Set one compression parameter, selected by enum FL2_cParameter.
 *  @result : informational value (typically, the one being set, possibly corrected),
 *            or an error code (which can be tested with FL2_isError()). */
FL2LIB_API size_t FL2LIB_CALL FL2_CStream_setParameter(FL2_CStream* fcs, FL2_cParameter param, size_t value);

/*! FL2_CStream_getParameter() :
 *  Get one compression parameter, selected by enum FL2_cParameter.
 *  @result : the parameter value, or the parameter_unsupported error code
 *            (which can be tested with FL2_isError()). */
FL2LIB_API size_t FL2LIB_CALL FL2_CStream_getParameter(FL2_CStream* fcs, FL2_cParameter param);

/*! FL2_getLevelParameters() :
 *  Get all compression parameter values defined by the preset compressionLevel.
 *  @result : the values in a FL2_compressionParameters struct, or the parameter_outOfBound error code
 *            (which can be tested with FL2_isError()) if compressionLevel is invalid. */
FL2LIB_API size_t FL2LIB_CALL FL2_getLevelParameters(int compressionLevel, int high, FL2_compressionParameters *params);


/***************************************
*  Context memory usage
***************************************/

/*! FL2_estimate*() :
*  These functions estimate memory usage of a CCtx before its creation or before any operation has begun.
*  FL2_estimateCCtxSize() will provide a budget large enough for any compression level up to selected one.
*  To use FL2_estimateCCtxSize_usingCCtx, set the compression level and any other settings for the context,
*  then call the function. Some allocation occurs when the context is created, but the large memory buffers
*  used for string matching are allocated only when compression is initialized. */

FL2LIB_API size_t FL2LIB_CALL FL2_estimateCCtxSize(int compressionLevel, unsigned nbThreads); /*!< memory usage determined by level */
FL2LIB_API size_t FL2LIB_CALL FL2_estimateCCtxSize_byParams(const FL2_compressionParameters *params, unsigned nbThreads); /*!< memory usage determined by params */
FL2LIB_API size_t FL2LIB_CALL FL2_estimateCCtxSize_usingCCtx(const FL2_CCtx* cctx);           /*!< memory usage determined by settings */
FL2LIB_API size_t FL2LIB_CALL FL2_estimateCStreamSize(int compressionLevel, unsigned nbThreads, int dualBuffer); /*!< memory usage determined by level */
FL2LIB_API size_t FL2LIB_CALL FL2_estimateCStreamSize_byParams(const FL2_compressionParameters *params, unsigned nbThreads, int dualBuffer); /*!< memory usage determined by params */
FL2LIB_API size_t FL2LIB_CALL FL2_estimateCStreamSize_usingCStream(const FL2_CStream* fcs);   /*!< memory usage determined by settings */

/*! FL2_getDictSizeFromProp() :
 *  Get the dictionary size from the property byte for a stream. The property byte is the first byte
*   in the stream, unless omitProperties was enabled, in which case the caller must store it. */
FL2LIB_API size_t FL2LIB_CALL FL2_getDictSizeFromProp(unsigned char prop);

/*! FL2_estimateDCtxSize() :
 *  The size of a DCtx does not include a dictionary buffer because the caller must supply one. */
FL2LIB_API size_t FL2LIB_CALL FL2_estimateDCtxSize(unsigned nbThreads);

/*! FL2_estimateDStreamSize() :
 *  Estimate decompression memory use from the dictionary size and number of threads.
 *  For nbThreads == 0 the number of available cores will be used.
 *  Obtain dictSize by passing the property byte to FL2_getDictSizeFromProp. */
FL2LIB_API size_t FL2LIB_CALL FL2_estimateDStreamSize(size_t dictSize, unsigned nbThreads); /*!<  obtain dictSize from FL2_getDictSizeFromProp() */

#endif  /* FAST_LZMA2_H */

#if defined (__cplusplus)
}
#endif
