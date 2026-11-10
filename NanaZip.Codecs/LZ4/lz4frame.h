/*
   LZ4F - LZ4-Frame library
   Header File
   Copyright (c) Yann Collet. All rights reserved.

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
   - LZ4 source repository : https://github.com/lz4/lz4
   - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* LZ4F is a stand-alone API able to create and decode LZ4 frames
 * conformant with specification v1.6.1 in doc/lz4_Frame_format.md .
 * Generated frames are compatible with `lz4` CLI.
 *
 * LZ4F also offers streaming capabilities.
 *
 * lz4.h is not required when using lz4frame.h,
 * except to extract common constants such as LZ4_VERSION_NUMBER.
 * */

#ifndef LZ4F_H_09782039843
#define LZ4F_H_09782039843

/* ---   Dependency   --- */
#include <stddef.h>   /* size_t */

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * Introduction
 *
 * lz4frame.h implements LZ4 frame specification: see doc/lz4_Frame_format.md .
 * LZ4 Frames are compatible with `lz4` CLI,
 * and designed to be interoperable with any system.
**/

/*-***************************************************************
 *  Compiler specifics
 *****************************************************************/
/*  LZ4_DLL_EXPORT :
 *  Enable exporting of functions when building a Windows DLL
 *  LZ4FLIB_VISIBILITY :
 *  Control library symbols visibility.
 */
#ifndef LZ4FLIB_VISIBILITY
#  if defined(__GNUC__) && (__GNUC__ >= 4)
#    define LZ4FLIB_VISIBILITY __attribute__ ((visibility ("default")))
#  else
#    define LZ4FLIB_VISIBILITY
#  endif
#endif
#if defined(LZ4_DLL_EXPORT) && (LZ4_DLL_EXPORT==1)
#  define LZ4FLIB_API __declspec(dllexport) LZ4FLIB_VISIBILITY
#elif defined(LZ4_DLL_IMPORT) && (LZ4_DLL_IMPORT==1)
#  define LZ4FLIB_API __declspec(dllimport) LZ4FLIB_VISIBILITY
#else
#  define LZ4FLIB_API LZ4FLIB_VISIBILITY
#endif

/* LZ4F_DEPRECATE(x) : deprecated enumerator or type x, replaced by LZ4F_##x
 * LZ4F_DEPRECATED(message) : precedes the declaration of a deprecated function */
#ifdef LZ4F_DISABLE_DEPRECATE_WARNINGS
#  define LZ4F_DEPRECATE(x) x
#  define LZ4F_DEPRECATED(message)
#else
#  if defined(_MSC_VER)
#    define LZ4F_DEPRECATE(x) x   /* __declspec(deprecated) x - only works with C++ */
#  elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 6))
#    define LZ4F_DEPRECATE(x) x __attribute__((deprecated("use LZ4F_" #x " instead")))
#  else
#    define LZ4F_DEPRECATE(x) x   /* no deprecation warning for this compiler */
#  endif
#  if defined (__cplusplus) && (__cplusplus >= 201402) /* C++14 or greater */
#    define LZ4F_DEPRECATED(message) [[deprecated(message)]]
#  elif defined(_MSC_VER)
#    define LZ4F_DEPRECATED(message) __declspec(deprecated(message))
#  elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ * 10 + __GNUC_MINOR__ >= 45))
#    define LZ4F_DEPRECATED(message) __attribute__((deprecated(message)))
#  elif defined(__GNUC__) && (__GNUC__ * 10 + __GNUC_MINOR__ >= 31)
#    define LZ4F_DEPRECATED(message) __attribute__((deprecated))
#  else
#    define LZ4F_DEPRECATED(message)   /* no deprecation warning for this compiler */
#  endif
#endif


/*-************************************
 *  Error management
 **************************************/
typedef size_t LZ4F_errorCode_t;

LZ4FLIB_API unsigned    LZ4F_isError(LZ4F_errorCode_t code);   /**< tells when a function result is an error code */
LZ4FLIB_API const char* LZ4F_getErrorName(LZ4F_errorCode_t code);   /**< return error code string; for debugging */


/*-************************************
 *  Frame compression types
 ************************************* */
/* #define LZ4F_ENABLE_OBSOLETE_ENUMS   // uncomment to enable obsolete enums */
#ifdef LZ4F_ENABLE_OBSOLETE_ENUMS
#  define LZ4F_OBSOLETE_ENUM(x) , LZ4F_DEPRECATE(x) = LZ4F_##x
#else
#  define LZ4F_OBSOLETE_ENUM(x)
#endif

/* The larger the block size, the (slightly) better the compression ratio,
 * though there are diminishing returns.
 * Larger blocks also increase memory usage on both compression and decompression sides.
 */
typedef enum {
    LZ4F_default=0,
    LZ4F_max64KB=4,
    LZ4F_max256KB=5,
    LZ4F_max1MB=6,
    LZ4F_max4MB=7
    LZ4F_OBSOLETE_ENUM(max64KB)
    LZ4F_OBSOLETE_ENUM(max256KB)
    LZ4F_OBSOLETE_ENUM(max1MB)
    LZ4F_OBSOLETE_ENUM(max4MB)
} LZ4F_blockSizeID_t;

/* Linked blocks sharply reduce inefficiencies when using small blocks,
 * they compress better.
 * However, some LZ4 decoders are only compatible with independent blocks */
typedef enum {
    LZ4F_blockLinked=0,
    LZ4F_blockIndependent
    LZ4F_OBSOLETE_ENUM(blockLinked)
    LZ4F_OBSOLETE_ENUM(blockIndependent)
} LZ4F_blockMode_t;

typedef enum {
    LZ4F_noContentChecksum=0,
    LZ4F_contentChecksumEnabled
    LZ4F_OBSOLETE_ENUM(noContentChecksum)
    LZ4F_OBSOLETE_ENUM(contentChecksumEnabled)
} LZ4F_contentChecksum_t;

typedef enum {
    LZ4F_noBlockChecksum=0,
    LZ4F_blockChecksumEnabled
} LZ4F_blockChecksum_t;

typedef enum {
    LZ4F_frame=0,
    LZ4F_skippableFrame
    LZ4F_OBSOLETE_ENUM(skippableFrame)
} LZ4F_frameType_t;

#ifdef LZ4F_ENABLE_OBSOLETE_ENUMS
typedef LZ4F_blockSizeID_t LZ4F_DEPRECATE(blockSizeID_t);
typedef LZ4F_blockMode_t LZ4F_DEPRECATE(blockMode_t);
typedef LZ4F_frameType_t LZ4F_DEPRECATE(frameType_t);
typedef LZ4F_contentChecksum_t LZ4F_DEPRECATE(contentChecksum_t);
#endif

/*! LZ4F_frameInfo_t :
 *  makes it possible to set or read frame parameters.
 *  Structure must be first init to 0, using memset() or LZ4F_INIT_FRAMEINFO,
 *  setting all parameters to default.
 *  It's then possible to update selectively some parameters */
typedef struct {
  LZ4F_blockSizeID_t     blockSizeID;         /* max64KB, max256KB, max1MB, max4MB; 0 == default (LZ4F_max64KB) */
  LZ4F_blockMode_t       blockMode;           /* LZ4F_blockLinked, LZ4F_blockIndependent; 0 == default (LZ4F_blockLinked) */
  LZ4F_contentChecksum_t contentChecksumFlag; /* 1: add a 32-bit checksum of frame's decompressed data; 0 == default (disabled) */
  LZ4F_frameType_t       frameType;           /* read-only field : LZ4F_frame or LZ4F_skippableFrame */
  unsigned long long     contentSize;         /* Size of uncompressed content ; 0 == unknown */
  unsigned               dictID;              /* Dictionary ID, sent by compressor to help decoder select correct dictionary; 0 == no dictID provided */
  LZ4F_blockChecksum_t   blockChecksumFlag;   /* 1: each block followed by a checksum of block's compressed data; 0 == default (disabled) */
} LZ4F_frameInfo_t;

#define LZ4F_INIT_FRAMEINFO   { LZ4F_max64KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0ULL, 0U, LZ4F_noBlockChecksum }    /* v1.8.3+ */

/*! LZ4F_preferences_t :
 *  makes it possible to supply advanced compression instructions to streaming interface.
 *  Structure must be first init to 0, using memset() or LZ4F_INIT_PREFERENCES,
 *  setting all parameters to default.
 *  All reserved fields must be set to zero.
 *  Note : since v1.11.0, reserved fields are a list of scalar fields (formerly an array, reserved[3]) :
 *  a future field takes the place of a reserved one, without changing positional initializers. */
typedef struct {
  LZ4F_frameInfo_t frameInfo;
  int      compressionLevel;    /* 0: default (fast mode); values > LZ4HC_CLEVEL_MAX count as LZ4HC_CLEVEL_MAX; values < 0 trigger "fast acceleration" */
  unsigned autoFlush;           /* 1: always flush; reduces usage of internal buffers */
  unsigned favorDecSpeed;       /* 1: parser favors decompression speed vs compression ratio. Only works for high compression modes (>= LZ4HC_CLEVEL_OPT_MIN) */  /* v1.8.2+ */
  unsigned nbWorkers;           /* 0: single-threaded (default); >= 1: nb of worker threads, see below (experimental) */  /* v1.11.0+ */
  unsigned reserved1;           /* must be zero for forward compatibility */
  unsigned reserved0;           /* idem */
} LZ4F_preferences_t;

#define LZ4F_INIT_PREFERENCES   { LZ4F_INIT_FRAMEINFO, 0, 0u, 0u, 0u, 0u, 0u }    /* v1.8.3+ */

/* nbWorkers : experimental
 *  0 (default) : single-threaded, unchanged behavior.
 *  >= 1 : input is cut into jobs, compressed by nbWorkers worker threads (capped at 1024, 128 on 32-bit systems),
 *         while the calling thread fills and emits them (it compresses a flushed job itself when no other job is running).
 *  - Output only depends on content, parameters and flush points :
 *    never on nbWorkers value (>= 1), scheduling, input slicing, nor stableSrc.
 *  - With independent blocks, output is identical to nbWorkers == 0.
 *  - Jobs are 1 MB (whole nb of blocks), or a single block when larger.
 *    With linked blocks, each job restarts with previous 64 KB as dictionary :
 *    output can differ (slightly) from nbWorkers == 0.
 *  - Streaming (LZ4F_compressUpdate()) : output lags behind input,
 *    up to ~ 2 * nbWorkers jobs in flight, emitted by LZ4F_flush() and LZ4F_compressEnd() :
 *    invoke them again while they fill dstBuffer (at least one job per invocation with LZ4F_compressBound(0)).
 *    LZ4F_compressBound() counts jobs instead of blocks : updates defer completed jobs not fitting into dstBuffer,
 *    so that output is only required to start a job while all slots are busy.
 *    Without autoFlush, the amount emitted by each invocation depends on scheduling :
 *    only the whole frame is deterministic. Completed jobs are only emitted if they fit :
 *    with a smaller dstCapacity, LZ4F_compressUpdate() only fails if output must be emitted (ex: all job slots busy).
 *    Input is copied, unless stableSrc == LZ4F_srcStableUntilFlush (jobs still running after an invocation returns
 *    then read it in place), or autoFlush, which completes all jobs within each invocation.
 *    LZ4F_srcStable (historical value 1) still only requires the last 64 KB to remain present, as with nbWorkers == 0.
 *  - One-shot (LZ4F_compressFrame()) : success is still guaranteed with LZ4F_compressFrameBound().
 *    Inputs fitting into a single job are compressed by the calling thread.
 *  - Worker threads are started on demand, when jobs wait for them (up to nbWorkers), with their buffers :
 *    small inputs employ few threads and little memory. They are kept within the compression context for next frames.
 *    If no worker thread can be created at frame start, LZ4F_compressBegin*() and LZ4F_compressFrame*() fail
 *    (LZ4F_ERROR_allocation_failed) ; failing to start more (ex: system limits), compression proceeds with current ones.
 *    A context with worker threads must not be employed after fork(), in the child process.
 *    On Windows, it must not be employed, nor freed, while the loader lock is held (within DllMain(),
 *    or constructors and destructors of a DLL's global objects) : worker threads can neither start nor exit then.
 *    On POSIX systems, worker threads block signals, except synchronous ones (SIGBUS, SIGSEGV, SIGFPE, SIGILL) :
 *    a fault while reading input (ex: memory-mapped file truncated) invokes the application's handler, in a worker thread.
 *  - Memory is allocated by the calling thread (LZ4F_CustomMem doesn't need to be thread-safe).
 *  Multi-threading requires lz4frame.c to be compiled with LZ4F_MULTITHREAD=1 (and -pthread on POSIX systems) :
 *  by default, the dynamic library is built with multi-threading support, and the static library without.
 *  Otherwise, independent blocks are compressed in single-threaded mode (same output),
 *  and jobs of linked blocks are processed serially by the calling thread (same output, slightly slower).
 *  LZ4F_getMaxNbWorkers() (static linking only) tells if multi-threading is supported. */


/*-*********************************
*  Simple compression function
***********************************/

/*! LZ4F_compressFrame() :
 *  Compress srcBuffer content into an LZ4-compressed frame.
 *  It's a one shot operation, all input content is consumed, and all output is generated.
 *
 *  Note : it's a stateless operation (no LZ4F_cctx state needed).
 *  In order to reduce load on the allocator, LZ4F_compressFrame(), by default,
 *  uses the stack to allocate space for the compression state and some table.
 *  If this usage of the stack is too much for your application,
 *  consider compiling `lz4frame.c` with compile-time macro LZ4F_HEAPMODE set to 1 instead.
 *  All state allocations will use the Heap.
 *  It also means each invocation of LZ4F_compressFrame() will trigger several internal alloc/free invocations.
 *  Multi-threaded compression (preferencesPtr->nbWorkers >= 1) also allocates worker threads, states and job buffers.
 *
 *  Success is guaranteed when @dstCapacity >= LZ4F_compressFrameBound(srcSize, preferencesPtr).
 *  A smaller @dstCapacity is allowed : compression is attempted,
 *  and fails with LZ4F_ERROR_dstCapacity_tooSmall if the frame doesn't fit.
 * @preferencesPtr is optional : one can provide NULL, in which case all preferences are set to default.
 * @return : number of bytes written into dstBuffer.
 *           or an error code if it fails (can be tested using LZ4F_isError())
 */
LZ4FLIB_API size_t LZ4F_compressFrame(void* dstBuffer, size_t dstCapacity,
                                const void* srcBuffer, size_t srcSize,
                                const LZ4F_preferences_t* preferencesPtr);

/*! LZ4F_compressFrameBound() :
 *  Returns the maximum possible compressed size with LZ4F_compressFrame() given srcSize and preferences.
 * `preferencesPtr` is optional. It can be replaced by NULL, in which case, the function will assume default preferences.
 *  Note : this result is only usable with LZ4F_compressFrame().
 *         It may also be relevant to LZ4F_compressUpdate() _only if_ no flush() operation is ever performed.
 */
LZ4FLIB_API size_t LZ4F_compressFrameBound(size_t srcSize, const LZ4F_preferences_t* preferencesPtr);


/*! LZ4F_compressionLevel_max() :
 * @return maximum allowed compression level (currently: 12)
 */
LZ4FLIB_API int LZ4F_compressionLevel_max(void);   /* v1.8.0+ */


/*-***********************************
*  Advanced compression functions
*************************************/
typedef struct LZ4F_cctx_s LZ4F_cctx;   /* incomplete type */
typedef LZ4F_cctx* LZ4F_compressionContext_t;  /* for compatibility with older APIs, prefer using LZ4F_cctx */

/* stableSrc : what remains available after an invocation of LZ4F_compressUpdate() returns */
typedef enum {
    LZ4F_srcUnstable = 0,         /* default : src can be modified or released as soon as the invocation returns */
    LZ4F_srcStable = 1,           /* the last 64 KB of input remain present and unmodified during next invocations,
                                   * including after LZ4F_flush() : linked blocks reference them directly, instead of copying them */
    LZ4F_srcStableUntilFlush = 2  /* stronger than LZ4F_srcStable : in addition to the last 64 KB of input,
                                   * all of src remains present and unmodified until LZ4F_flush() or LZ4F_compressEnd() returns
                                   * (with autoFlush : until the invocation returns), or the frame ends otherwise :
                                   * invalidated by an error, or abandoned (LZ4F_compressBegin*(), LZ4F_freeCompressionContext()).
                                   * Note : LZ4F_compressBegin*() rejecting its arguments leaves the frame in progress.
                                   * Worker threads (nbWorkers >= 1) read src in place, instead of copying it */  /* v1.11.0+ */
} LZ4F_stableSrc_t;

/*! LZ4F_compressOptions_t :
 *  Structure must be first init to 0, using memset(), setting all options to default.
 *  All reserved fields must be set to zero.
 *  Note : since v1.11.0, reserved fields are a list of scalar fields (formerly an array, reserved[3]) :
 *  a future field takes the place of a reserved one, without changing positional initializers. */
typedef struct {
  unsigned stableSrc;    /* LZ4F_stableSrc_t value ; 0 (default) == LZ4F_srcUnstable */
  unsigned uncompressed; /* 1 : LZ4F_compressUpdate() stores src as uncompressed blocks ; 0 (default) : compressed ; other values : reserved */  /* v1.11.0+ */
  unsigned reserved1;    /* must be set to zero for forward compatibility */
  unsigned reserved0;    /* idem */
} LZ4F_compressOptions_t;

/*---   Resource Management   ---*/

#define LZ4F_VERSION 100    /* This number can be used to check for an incompatible API breaking change */
LZ4FLIB_API unsigned LZ4F_getVersion(void);

/*! LZ4F_createCompressionContext() :
 *  The first thing to do is to create a compressionContext object,
 *  which will keep track of operation state during streaming compression.
 *  This is achieved using LZ4F_createCompressionContext(), which takes as argument a version,
 *  and a pointer to LZ4F_cctx*, to write the resulting pointer into.
 *  @version provided MUST be LZ4F_VERSION. It is intended to track potential version mismatch, notably when using DLL.
 *  The function provides a pointer to a fully allocated LZ4F_cctx object.
 *  @cctxPtr MUST be != NULL.
 *  If @return != zero, context creation failed.
 *  A created compression context can be employed multiple times for consecutive streaming operations.
 *  Once all streaming compression jobs are completed,
 *  the state object can be released using LZ4F_freeCompressionContext().
 *  Note1 : LZ4F_freeCompressionContext() is always successful. Its return value can be ignored.
 *  Note2 : LZ4F_freeCompressionContext() works fine with NULL input pointers (do nothing).
**/
LZ4FLIB_API LZ4F_errorCode_t LZ4F_createCompressionContext(LZ4F_cctx** cctxPtr, unsigned version);
LZ4FLIB_API LZ4F_errorCode_t LZ4F_freeCompressionContext(LZ4F_cctx* cctx);


/*----    Compression    ----*/

#define LZ4F_HEADER_SIZE_MIN  7   /* LZ4 Frame header size can vary, depending on selected parameters */
#define LZ4F_HEADER_SIZE_MAX 19

/* Size in bytes of a block header in little-endian format. Highest bit indicates if block data is uncompressed */
#define LZ4F_BLOCK_HEADER_SIZE 4

/* Size in bytes of a block checksum footer in little-endian format. */
#define LZ4F_BLOCK_CHECKSUM_SIZE 4

/* Size in bytes of the content checksum. */
#define LZ4F_CONTENT_CHECKSUM_SIZE 4

/* Size in bytes of the endmark. */
#define LZ4F_ENDMARK_SIZE 4

/*! LZ4F_compressBegin() :
 *  will write the frame header into dstBuffer.
 *  Success is guaranteed when dstCapacity >= LZ4F_HEADER_SIZE_MAX bytes.
 *  Actual header size is between LZ4F_HEADER_SIZE_MIN and LZ4F_HEADER_SIZE_MAX, depending on preferences,
 *  and only this size is required.
 *  Arguments are validated first : rejecting them (ex: dstCapacity too small) doesn't modify `cctx`,
 *  so that a frame in progress continues. Other failures (ex: allocation) invalidate it (see LZ4F_compressUpdate()).
 * `prefsPtr` is optional : NULL can be provided to set all preferences to default.
 * @return : number of bytes written into dstBuffer for the header
 *           or an error code (which can be tested using LZ4F_isError())
 */
LZ4FLIB_API size_t LZ4F_compressBegin(LZ4F_cctx* cctx,
                                      void* dstBuffer, size_t dstCapacity,
                                      const LZ4F_preferences_t* prefsPtr);

/*! LZ4F_compressBound() :
 *  Provides minimum dstCapacity required to guarantee success of
 *  LZ4F_compressUpdate(), given a srcSize and preferences, for a worst case scenario.
 *  Smaller capacities are allowed, but then the operation may fail.
 *  When srcSize==0, LZ4F_compressBound() provides a dstCapacity for LZ4F_flush() and LZ4F_compressEnd() instead :
 *  in single-threaded mode, their output remains strictly below it, so that a single invocation completes them ;
 *  with nbWorkers >= 1, each invocation emits at least one job, see LZ4F_preferences_t.nbWorkers.
 *  Note that the result is only valid for a single invocation of LZ4F_compressUpdate().
 *  When invoking LZ4F_compressUpdate() multiple times,
 *  if the output buffer is gradually filled up instead of emptied and re-used from its start,
 *  one must check if there is enough remaining capacity before each invocation, using LZ4F_compressBound().
 * @return is always the same for a srcSize and prefsPtr.
 *  prefsPtr is optional : when NULL is provided, preferences will be set to cover worst case scenario.
 *  tech details :
 * @return if automatic flushing is not enabled, counts (srcSize / blockSize + 1) blocks, stored uncompressed :
 *  internal buffer might already be filled by up to (blockSize-1) bytes, emitted as a separate partial block
 *  when switching between compressed and uncompressed input, or remaining from an incomplete LZ4F_flush().
 *  With nbWorkers >= 1 (and no autoFlush), jobs replace blocks : it counts ceil(srcSize / jobSize) jobs (1 job if srcSize == 0),
 *  see LZ4F_preferences_t.nbWorkers.
 *  It also includes frame footer (ending + checksum), since it might be generated by LZ4F_compressEnd().
 * @return doesn't include frame header, as it was already generated by LZ4F_compressBegin().
 */
LZ4FLIB_API size_t LZ4F_compressBound(size_t srcSize, const LZ4F_preferences_t* prefsPtr);

/*! LZ4F_compressUpdate() :
 *  LZ4F_compressUpdate() can be called repetitively to compress as much data as necessary.
 *  Success is guaranteed when dstCapacity >= LZ4F_compressBound(srcSize, prefsPtr).
 *  A smaller dstCapacity is allowed : compression is attempted,
 *  and fails with LZ4F_ERROR_dstCapacity_tooSmall if the output doesn't fit.
 *  After an error, the frame is invalidated : it can't be continued,
 *  and subsequent operations fail, until a new frame is started with LZ4F_compressBegin().
 *  With cOptPtr->uncompressed == 1 (v1.11.0+), src is stored as uncompressed blocks instead.
 *  A switch between compressed and uncompressed input first flushes buffered data, as a separate block.
 *  With linked blocks, in single-threaded mode, blocks compressed after uncompressed input
 *  don't reference anything before them (compression ratio can be slightly lower).
 * `cOptPtr` is optional : NULL can be provided, in which case all options are set to default.
 * @return : number of bytes written into `dstBuffer` (it can be zero, meaning input data was just buffered).
 *           or an error code if it fails (which can be tested using LZ4F_isError())
 */
LZ4FLIB_API size_t LZ4F_compressUpdate(LZ4F_cctx* cctx,
                                       void* dstBuffer, size_t dstCapacity,
                                 const void* srcBuffer, size_t srcSize,
                                 const LZ4F_compressOptions_t* cOptPtr);

/*! LZ4F_flush() :
 *  When data must be generated and sent immediately, without waiting for a block to be completely filled,
 *  it's possible to call LZ4_flush(). It will immediately compress any data buffered within cctx.
 *  Each invocation fills dstBuffer, or completes the flush (v1.11.0+) :
 *  while it returns dstCapacity, output may remain : invoke LZ4F_flush() again, with any dstCapacity.
 *  In single-threaded mode, a single invocation completes the flush when dstCapacity >= LZ4F_compressBound(0, prefsPtr).
 *  The first invocation compresses buffered input : after it returns, input is no longer read
 *  (see LZ4F_srcStableUntilFlush). LZ4F_compressUpdate() may follow an incomplete flush : remaining output comes first.
 * `cOptPtr` is optional : it's possible to provide NULL, all options will be set to default.
 * @return : nb of bytes written into dstBuffer (can be zero, when there is no data stored within cctx)
 *           or an error code if it fails (which can be tested using LZ4F_isError())
 *  dstCapacity == 0 is rejected (LZ4F_ERROR_dstCapacity_tooSmall) when output remains, without modifying the frame.
 *  LZ4F_flush() requires a frame in progress.
 */
LZ4FLIB_API size_t LZ4F_flush(LZ4F_cctx* cctx,
                              void* dstBuffer, size_t dstCapacity,
                        const LZ4F_compressOptions_t* cOptPtr);

/*! LZ4F_compressEnd() :
 *  To properly finish an LZ4 frame, invoke LZ4F_compressEnd().
 *  It will flush whatever data remained within `cctx` (like LZ4_flush())
 *  and properly finalize the frame, with an endMark and a checksum.
 *  Each invocation fills dstBuffer, or completes the frame (v1.11.0+) :
 *  while it returns dstCapacity, output may remain : invoke LZ4F_compressEnd() again, with any dstCapacity.
 *  In single-threaded mode, a single invocation completes the frame when dstCapacity >= LZ4F_compressBound(0, prefsPtr).
 *  Until completion, only LZ4F_compressEnd() can continue the frame (LZ4F_compressBegin() abandons it).
 * `cOptPtr` is optional : NULL can be provided, in which case all options will be set to default.
 * @return : nb of bytes written into dstBuffer,
 *           or an error code if it fails (which can be tested using LZ4F_isError())
 *  dstCapacity == 0 is rejected (LZ4F_ERROR_dstCapacity_tooSmall), without modifying the frame.
 *  Once completed, or after an error, `cctx` is available again for another compression task.
 */
LZ4FLIB_API size_t LZ4F_compressEnd(LZ4F_cctx* cctx,
                                    void* dstBuffer, size_t dstCapacity,
                              const LZ4F_compressOptions_t* cOptPtr);


/*-*********************************
*  Decompression functions
***********************************/
typedef struct LZ4F_dctx_s LZ4F_dctx;   /* incomplete type */
typedef LZ4F_dctx* LZ4F_decompressionContext_t;   /* compatibility with previous API versions */

typedef struct {
  unsigned stableDst;     /* pledges that last 64KB decompressed data is present right before @dstBuffer pointer.
                           * This optimization skips internal storage operations.
                           * Once set, this pledge must remain valid up to the end of current frame. */
  unsigned skipChecksums; /* disable checksum calculation and verification, even when one is present in frame, to save CPU time.
                           * Setting this option to 1 once disables all checksums for the rest of the frame. */
  unsigned reserved1;     /* must be set to zero for forward compatibility */
  unsigned reserved0;     /* idem */
} LZ4F_decompressOptions_t;


/* Resource management */

/*! LZ4F_createDecompressionContext() :
 *  Create an LZ4F_dctx object, to track all decompression operations.
 *  @version provided MUST be LZ4F_VERSION.
 *  @dctxPtr MUST be valid.
 *  The function fills @dctxPtr with the value of a pointer to an allocated and initialized LZ4F_dctx object.
 *  The @return is an errorCode, which can be tested using LZ4F_isError().
 *  dctx memory can be released using LZ4F_freeDecompressionContext();
 *  Result of LZ4F_freeDecompressionContext() indicates current state of decompressionContext when being released.
 *  That is, it should be == 0 if decompression has been completed fully and correctly.
 */
LZ4FLIB_API LZ4F_errorCode_t LZ4F_createDecompressionContext(LZ4F_dctx** dctxPtr, unsigned version);
LZ4FLIB_API LZ4F_errorCode_t LZ4F_freeDecompressionContext(LZ4F_dctx* dctx);


/*-***********************************
*  Streaming decompression functions
*************************************/

#define LZ4F_MAGICNUMBER 0x184D2204U
#define LZ4F_MAGIC_SKIPPABLE_START 0x184D2A50U
#define LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH 5

/*! LZ4F_headerSize() : v1.9.0+
 *  Provide the header size of a frame starting at `src`.
 * `srcSize` must be >= LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH,
 *  which is enough to decode the header length.
 * @return : size of frame header
 *           or an error code, which can be tested using LZ4F_isError()
 *  note : Frame header size is variable, but is guaranteed to be
 *         >= LZ4F_HEADER_SIZE_MIN bytes, and <= LZ4F_HEADER_SIZE_MAX bytes.
 */
LZ4FLIB_API size_t LZ4F_headerSize(const void* src, size_t srcSize);

/*! LZ4F_getFrameInfo() :
 *  This function extracts frame parameters (max blockSize, dictID, etc.).
 *  Its usage is optional: user can also invoke LZ4F_decompress() directly.
 *
 *  Extracted information will fill an existing LZ4F_frameInfo_t structure.
 *  This can be useful for allocation and dictionary identification purposes.
 *
 *  LZ4F_getFrameInfo() can work in the following situations :
 *
 *  1) At the beginning of a new frame, before any invocation of LZ4F_decompress().
 *     It will decode header from `srcBuffer`,
 *     consuming the header and starting the decoding process.
 *
 *     Input size must be large enough to contain the full frame header.
 *     Frame header size can be known beforehand by LZ4F_headerSize().
 *     Frame header size is variable, but is guaranteed to be >= LZ4F_HEADER_SIZE_MIN bytes,
 *     and not more than <= LZ4F_HEADER_SIZE_MAX bytes.
 *     Hence, blindly providing LZ4F_HEADER_SIZE_MAX bytes or more will always work.
 *     It's allowed to provide more input data than the header size,
 *     LZ4F_getFrameInfo() will only consume the header.
 *
 *     If input size is not large enough,
 *     aka if it's smaller than header size,
 *     function will fail and return an error code.
 *
 *  2) After decoding has been started,
 *     it's possible to invoke LZ4F_getFrameInfo() anytime
 *     to extract already decoded frame parameters stored within dctx.
 *
 *     Note that, if decoding has barely started,
 *     and not yet read enough information to decode the header,
 *     LZ4F_getFrameInfo() will fail.
 *
 *  The number of bytes consumed from srcBuffer will be updated in *srcSizePtr (necessarily <= original value).
 *  LZ4F_getFrameInfo() only consumes bytes when decoding has not yet started,
 *  and when decoding the header has been successful.
 *  Decompression must then resume from (srcBuffer + *srcSizePtr).
 *
 * @return : a hint about how many srcSize bytes LZ4F_decompress() expects for next call,
 *           or an error code which can be tested using LZ4F_isError().
 *  note 1 : in case of error, dctx is not modified. Decoding operation can resume from beginning safely.
 *  note 2 : frame parameters are *copied into* an already allocated LZ4F_frameInfo_t structure.
 */
LZ4FLIB_API size_t
LZ4F_getFrameInfo(LZ4F_dctx* dctx,
                  LZ4F_frameInfo_t* frameInfoPtr,
            const void* srcBuffer, size_t* srcSizePtr);

/**
 * @brief Incrementally decompresses an LZ4 frame into user-provided buffers.
 *
 * Call repeatedly until the return value is 0 (frame fully decoded) or an error is reported.
 * On each call, the function consumes up to *srcSizePtr bytes from @p srcBuffer and
 * produces up to *dstSizePtr bytes into @p dstBuffer. It updates both size pointers with
 * the actual number of bytes consumed/produced. There is no separate flush step.
 *
 * Typical loop:
 *  - Provide whatever input you have and an available output buffer.
 *  - Read how much input was consumed and how much output was produced.
 *  - Use the returned value as a hint for how many source bytes are ideal next time.
 *
 * @param[in]      dctx        A valid decompression context created by LZ4F_createDecompressionContext().
 * @param[out]     dstBuffer   Destination buffer for decompressed bytes. May change between calls.
 * @param[in,out]  dstSizePtr  In: capacity of @p dstBuffer in bytes. Out: number of bytes written (<= input value).
 * @param[in]      srcBuffer   Source buffer containing (more) compressed data. May point to the middle of a larger buffer.
 * @param[in,out]  srcSizePtr  In: number of available bytes in @p srcBuffer. Out: number of bytes consumed (<= input value).
 * @param[in]      optionsPtr  Optional decompression options; pass NULL for defaults.
 *
 * @return See @retval cases.
 * @retval >0  Hint (in bytes) for how many source bytes are ideal to provide on the next call.
 *             This also indicates the current frame is not yet complete: the decompressor
 *             expects more input, or may require additional output space to make progress. 
 *             User can always pass any amount of input; this value is only a performance hint.
 *             The hint always extends up to the end of next block header (endMark included),
 *             or up to the end of frame once endMark is consumed. Hence, providing exactly the hinted
 *             amount of input stops decoding right after a block header, which locates block starts
 *             (see LZ4F_resumeFrame() in the static section).
 * @retval 0   The current frame is fully decoded. If *srcSizePtr is less than the provided value, 
 *             the unconsumed tail is the start of another frame (if any).
 * @retval error  An error code; test with LZ4F_isError(ret). The size pointers still report
 *                the bytes consumed and produced before the error, but produced bytes may be invalid.
 *                After an error, dctx is not resumable: call LZ4F_resetDecompressionContext() before reusing it.
 *
 * @pre  @p dctx is a valid state created by LZ4F_createDecompressionContext().
 * @post *srcSizePtr and *dstSizePtr are updated with the actual bytes consumed/produced.
 *       @p dstBuffer contents in [0, *dstSizePtr) are decompressed data.
 *       The content checksum, if present, is only verified at the end of the frame (return 0).
 *
 * @note  The function may not consume all provided input on each call. Always check *srcSizePtr.
 *        Present any unconsumed source bytes again on the next call.
 * @note  @p dstBuffer content is overwritten; it does not need to be stable across calls.
 * @note  After finishing a frame (return==0), you may immediately start feeding the next frame
 *        into the same @p dctx (optionally, one can use LZ4F_resetDecompressionContext()).
 *
 * @warning If you called LZ4F_getFrameInfo() beforehand, you must advance @p srcBuffer and
 *          decrease *srcSizePtr by the number of bytes it consumed (the frame header). Failing
 *          to do so can cause decompression failure or, worse, silent corruption.
 *
 * @see LZ4F_getFrameInfo(), LZ4F_isError(), LZ4F_resetDecompressionContext(),
 *      LZ4F_createDecompressionContext(), LZ4F_freeDecompressionContext()
 */
LZ4FLIB_API size_t
LZ4F_decompress(LZ4F_dctx* dctx,
                void* dstBuffer, size_t* dstSizePtr,
          const void* srcBuffer, size_t* srcSizePtr,
          const LZ4F_decompressOptions_t* dOptPtr);


/*! LZ4F_resetDecompressionContext() : added in v1.8.0
 *  In case of an error, the context is left in "undefined" state.
 *  In which case, it's necessary to reset it, before re-using it.
 *  This method can also be used to abruptly stop any unfinished decompression,
 *  and start a new one using same context resources. */
LZ4FLIB_API void LZ4F_resetDecompressionContext(LZ4F_dctx* dctx);   /* always successful */


/**********************************
 *  Dictionary compression API
 *********************************/

/* A Dictionary is useful for the compression of small messages (KB range).
 * It dramatically improves compression efficiency.
 *
 * LZ4 can ingest any input as dictionary, though only the last 64 KB are useful.
 * Better results are generally achieved by using Zstandard's Dictionary Builder
 * to generate a high-quality dictionary from a set of samples.
 *
 * The same dictionary will have to be used on the decompression side
 * for decoding to be successful.
 * To help identify the correct dictionary at decoding stage,
 * the frame header allows optional embedding of a dictID field.
 */

/*! LZ4F_compressBegin_usingDict() : stable since v1.10
 *  Inits dictionary compression streaming, and writes the frame header into dstBuffer.
 *  Success is guaranteed when @dstCapacity >= LZ4F_HEADER_SIZE_MAX bytes (see LZ4F_compressBegin()).
 * @prefsPtr is optional : one may provide NULL as argument,
 *  however, it's the only way to provide dictID in the frame header.
 * @dictBuffer must outlive the compression session.
 * @return : number of bytes written into dstBuffer for the header,
 *           or an error code (which can be tested using LZ4F_isError())
 *  With independent blocks, each block uses the dictionary, as with a CDict (LZ4F_compressBegin_usingCDict()),
 *  also with linked blocks when nbWorkers >= 1 (multi-threaded compression).
 *  Linked blocks with nbWorkers == 0 employ the dictionary as history, preceding the first block.
 *  The dictionary is digested at each frame start, for its compression level : faster than LZ4F_createCDict(),
 *  but less thoroughly at fast levels (frames can be slightly larger than with a CDict, or smaller at level 2).
 *  To compress multiple frames, prefer a CDict.
 */
LZ4FLIB_API size_t
LZ4F_compressBegin_usingDict(LZ4F_cctx* cctx,
                            void* dstBuffer, size_t dstCapacity,
                      const void* dictBuffer, size_t dictSize,
                      const LZ4F_preferences_t* prefsPtr);

/*! LZ4F_decompress_usingDict() : stable since v1.10
 *  Same as LZ4F_decompress(), using a predefined dictionary.
 *  Dictionary is used "in place", without any preprocessing.
**  It must remain accessible throughout the entire frame decoding. */
LZ4FLIB_API size_t
LZ4F_decompress_usingDict(LZ4F_dctx* dctxPtr,
                          void* dstBuffer, size_t* dstSizePtr,
                    const void* srcBuffer, size_t* srcSizePtr,
                    const void* dict, size_t dictSize,
                    const LZ4F_decompressOptions_t* decompressOptionsPtr);

/*****************************************
 *  Bulk processing dictionary compression
 *****************************************/

/* Loading a dictionary has a cost, since it involves construction of tables.
 * The Bulk processing dictionary API makes it possible to share this cost
 * over an arbitrary number of compression jobs, even concurrently,
 * markedly improving compression latency for these cases.
 *
 * Note that there is no corresponding bulk API for the decompression side,
 * because dictionary does not carry any initialization cost for decompression.
 * Use the regular LZ4F_decompress_usingDict() there.
 */
typedef struct LZ4F_CDict_s LZ4F_CDict;

/*! LZ4_createCDict() : stable since v1.10
 *  When compressing multiple messages / blocks using the same dictionary, it's recommended to initialize it just once.
 *  LZ4_createCDict() will create a digested dictionary, ready to start future compression operations without startup delay.
 *  LZ4_CDict can be created once and shared by multiple threads concurrently, since its usage is read-only.
 * @dictBuffer can be released after LZ4_CDict creation, since its content is copied within CDict. */
LZ4FLIB_API LZ4F_CDict* LZ4F_createCDict(const void* dictBuffer, size_t dictSize);
LZ4FLIB_API void        LZ4F_freeCDict(LZ4F_CDict* CDict);

/*! LZ4_compressFrame_usingCDict() : stable since v1.10
 *  Compress an entire srcBuffer into a valid LZ4 frame using a digested Dictionary.
 * @cctx must point to a context created by LZ4F_createCompressionContext().
 *  If @cdict==NULL, compress without a dictionary.
 *  Success is guaranteed when @dstCapacity >= LZ4F_compressFrameBound(srcSize, preferencesPtr).
 *  A smaller @dstCapacity is allowed : compression is attempted,
 *  and fails with LZ4F_ERROR_dstCapacity_tooSmall if the frame doesn't fit.
 *  The LZ4F_preferences_t structure is optional : one may provide NULL as argument,
 *  but it's not recommended, as it's the only way to provide @dictID in the frame header.
 * @return : number of bytes written into dstBuffer.
 *           or an error code if it fails (can be tested using LZ4F_isError())
 *  Note: for larger inputs generating multiple independent blocks,
 *        this entry point uses the dictionary for each block. */
LZ4FLIB_API size_t
LZ4F_compressFrame_usingCDict(LZ4F_cctx* cctx,
                              void* dst, size_t dstCapacity,
                        const void* src, size_t srcSize,
                        const LZ4F_CDict* cdict,
                        const LZ4F_preferences_t* preferencesPtr);

/*! LZ4F_compressBegin_usingCDict() : stable since v1.10
 *  Inits streaming dictionary compression, and writes the frame header into dstBuffer.
 *  Success is guaranteed when @dstCapacity >= LZ4F_HEADER_SIZE_MAX bytes (see LZ4F_compressBegin()).
 * @prefsPtr is optional : one may provide NULL as argument,
 *  note however that it's the only way to insert a @dictID in the frame header.
 * @cdict must outlive the compression session.
 * @return : number of bytes written into dstBuffer for the header,
 *           or an error code, which can be tested using LZ4F_isError(). */
LZ4FLIB_API size_t
LZ4F_compressBegin_usingCDict(LZ4F_cctx* cctx,
                              void* dstBuffer, size_t dstCapacity,
                        const LZ4F_CDict* cdict,
                        const LZ4F_preferences_t* prefsPtr);


#if defined (__cplusplus)
}
#endif

#endif  /* LZ4F_H_09782039843 */

#if defined(LZ4F_STATIC_LINKING_ONLY) && !defined(LZ4F_H_STATIC_09782039843)
#define LZ4F_H_STATIC_09782039843

/* Note :
 * The below declarations are not stable and may change in the future.
 * They are therefore only safe to depend on
 * when the caller is statically linked against the library.
 * To access their declarations, define LZ4F_STATIC_LINKING_ONLY.
 *
 * By default, these symbols aren't published into shared/dynamic libraries.
 * You can override this behavior and force them to be published
 * by defining LZ4F_PUBLISH_STATIC_FUNCTIONS.
 * Use at your own risk.
 */

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef LZ4F_PUBLISH_STATIC_FUNCTIONS
# define LZ4FLIB_STATIC_API LZ4FLIB_API
#else
# define LZ4FLIB_STATIC_API
#endif


/* ---   Error List   --- */
#define LZ4F_LIST_ERRORS(ITEM) \
        ITEM(OK_NoError) \
        ITEM(ERROR_GENERIC) \
        ITEM(ERROR_maxBlockSize_invalid) \
        ITEM(ERROR_blockMode_invalid) \
        ITEM(ERROR_parameter_invalid) \
        ITEM(ERROR_compressionLevel_invalid) \
        ITEM(ERROR_headerVersion_wrong) \
        ITEM(ERROR_blockChecksum_invalid) \
        ITEM(ERROR_reservedFlag_set) \
        ITEM(ERROR_allocation_failed) \
        ITEM(ERROR_srcSize_tooLarge) \
        ITEM(ERROR_dstCapacity_tooSmall) \
        ITEM(ERROR_frameHeader_incomplete) \
        ITEM(ERROR_frameType_unknown) \
        ITEM(ERROR_frameSize_wrong) \
        ITEM(ERROR_srcPtr_wrong) \
        ITEM(ERROR_decompressionFailed) \
        ITEM(ERROR_headerChecksum_invalid) \
        ITEM(ERROR_contentChecksum_invalid) \
        ITEM(ERROR_frameDecoding_alreadyStarted) \
        ITEM(ERROR_compressionState_uninitialized) \
        ITEM(ERROR_parameter_null) \
        ITEM(ERROR_io_write) \
        ITEM(ERROR_io_read) \
        ITEM(ERROR_maxCode)

#define LZ4F_GENERATE_ENUM(ENUM) LZ4F_##ENUM,

/* enum list is exposed, to handle specific errors */
typedef enum { LZ4F_LIST_ERRORS(LZ4F_GENERATE_ENUM)
              _LZ4F_dummy_error_enum_for_c89_never_used } LZ4F_errorCodes;

/* former name, kept for compatibility */
#define LZ4F_ERROR_dstMaxSize_tooSmall LZ4F_ERROR_dstCapacity_tooSmall

LZ4FLIB_STATIC_API LZ4F_errorCodes LZ4F_getErrorCode(size_t functionResult);

/**********************************
 *  Advanced compression operations
 *********************************/

/*! LZ4F_getBlockSize() :
 * @return, in scalar format (size_t),
 *          the maximum block size associated with @blockSizeID,
 *          or an error code (can be tested using LZ4F_isError()) if @blockSizeID is invalid.
**/
LZ4FLIB_STATIC_API size_t LZ4F_getBlockSize(LZ4F_blockSizeID_t blockSizeID);

/*! LZ4F_getMaxNbWorkers() : v1.11.0+ (experimental, see LZ4F_preferences_t.nbWorkers)
 * @return : the maximum nb of worker threads a compression context can employ :
 *           0 when this library is compiled without multi-threading support (LZ4F_MULTITHREAD),
 *           otherwise the cap applied to LZ4F_preferences_t.nbWorkers (1024, or 128 on 32-bit systems).
**/
LZ4FLIB_STATIC_API unsigned LZ4F_getMaxNbWorkers(void);

/*! LZ4F_uncompressedUpdate() : deprecated (v1.11.0)
 *  Same as LZ4F_compressUpdate() with LZ4F_compressOptions_t.uncompressed == 1 : prefer this option.
 * `cOptPtr` is optional : NULL can be provided, in which case all other options are set to default.
 */
LZ4F_DEPRECATED("use LZ4F_compressUpdate() with LZ4F_compressOptions_t.uncompressed instead")
LZ4FLIB_STATIC_API size_t
LZ4F_uncompressedUpdate(LZ4F_cctx* cctx,
                        void* dstBuffer, size_t dstCapacity,
                  const void* srcBuffer, size_t srcSize,
                  const LZ4F_compressOptions_t* cOptPtr);

/**********************************
 *  Advanced decompression operations
 *********************************/

/*! LZ4F_resumeFrame() : v1.11.0+
 *  Prepares @dctx to decode a frame starting from one of its blocks, instead of from its beginning.
 *  Typical use case is random access, using information recorded during a previous decoding pass.
 *  Block starts can be located by providing LZ4F_decompress() with exactly the hinted amount of input :
 *  decoding then stops right after a block header (the block starts at the beginning of this header).
 * @frameInfo  : frame parameters, as provided by LZ4F_getFrameInfo().
 * @decodedPos : position of the block within the frame's decoded content.
 * @dict, @dictSize : data the block may refer to, which must be saved during the previous decoding pass :
 *     - linked blocks : a copy of the (up to) 64 KB decoded right before the block.
 *       When less than 64 KB were decoded, they must be preceded by the end of the frame's dictionary, if any.
 *       Saving 64 KB per resume point is the cost of random access within frames using linked blocks.
 *     - independent blocks : the frame's dictionary if any, or NULL. No history needed.
 *     @dict must remain accessible until the end of the frame.
 *  Then, provide LZ4F_decompress() with compressed data starting at the block header.
 *  Block checksums and content size remain verified,
 *  but the content checksum, which covers the whole frame, is skipped.
 *  Any decoding in progress within @dctx is abandoned.
 * @return : 0, or an error code (can be tested using LZ4F_isError())
 */
LZ4FLIB_STATIC_API size_t
LZ4F_resumeFrame(LZ4F_dctx* dctx, const LZ4F_frameInfo_t* frameInfo,
                 unsigned long long decodedPos,
                 const void* dict, size_t dictSize);

/**********************************
 *  Custom memory allocation
 *********************************/

/*! Custom memory allocation : v1.9.4+
 *  These prototypes make it possible to pass custom allocation/free functions.
 *  LZ4F_customMem is provided at state creation time, using LZ4F_create*_advanced() listed below.
 *  All allocation/free operations will be completed using these custom variants instead of regular <stdlib.h> ones.
 */
typedef void* (*LZ4F_AllocFunction) (void* opaqueState, size_t size);
typedef void* (*LZ4F_CallocFunction) (void* opaqueState, size_t size);
typedef void  (*LZ4F_FreeFunction) (void* opaqueState, void* address);
typedef struct {
    LZ4F_AllocFunction customAlloc;
    LZ4F_CallocFunction customCalloc; /* optional; when not defined, uses customAlloc + memset */
    LZ4F_FreeFunction customFree;
    void* opaqueState;
} LZ4F_CustomMem;
static
#ifdef __GNUC__
__attribute__((__unused__))
#endif
LZ4F_CustomMem const LZ4F_defaultCMem = { NULL, NULL, NULL, NULL };  /**< this constant defers to stdlib's functions */

LZ4FLIB_STATIC_API LZ4F_cctx* LZ4F_createCompressionContext_advanced(LZ4F_CustomMem customMem, unsigned version);
LZ4FLIB_STATIC_API LZ4F_dctx* LZ4F_createDecompressionContext_advanced(LZ4F_CustomMem customMem, unsigned version);
LZ4FLIB_STATIC_API LZ4F_CDict* LZ4F_createCDict_advanced(LZ4F_CustomMem customMem, const void* dictBuffer, size_t dictSize);

/*! Context size inspection : v1.11.0+
 *  These functions return the total memory footprint of the provided context.
 */
LZ4FLIB_STATIC_API size_t LZ4F_cctx_size(const LZ4F_cctx* cctx);
LZ4FLIB_STATIC_API size_t LZ4F_dctx_size(const LZ4F_dctx* dctx);

#if defined (__cplusplus)
}
#endif

#endif  /* defined(LZ4F_STATIC_LINKING_ONLY) && !defined(LZ4F_H_STATIC_09782039843) */
