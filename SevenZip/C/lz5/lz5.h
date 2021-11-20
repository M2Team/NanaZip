/*
   LZ5 - Fast LZ compression algorithm
   Header File
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
#pragma once

#if defined (__cplusplus)
extern "C" {
#endif

/*
 * lz5.h provides block compression functions, and gives full buffer control to programmer.
 * If you need to generate inter-operable compressed data (respecting LZ5 frame specification),
 * and can let the library handle its own memory, please use lz5frame.h instead.
*/

/**************************************
*  Version
**************************************/
#define LZ5_VERSION          "v1.5.0"
#define LZ5_VERSION_MAJOR    1    /* for breaking interface changes  */
#define LZ5_VERSION_MINOR    5    /* for new (non-breaking) interface capabilities */
#define LZ5_VERSION_RELEASE  0    /* for tweaks, bug-fixes, or development */
#define LZ5_VERSION_NUMBER (LZ5_VERSION_MAJOR *100*100 + LZ5_VERSION_MINOR *100 + LZ5_VERSION_RELEASE)
int LZ5_versionNumber (void);

#define LZ5HC_MAX_CLEVEL     15


/**************************************
*  Tuning parameter
**************************************/
/*
 * LZ5_MEMORY_USAGE :
 * Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB; etc.)
 * Increasing memory usage improves compression ratio
 * Reduced memory usage can improve speed, due to cache effect
 */
#define LZ5_MEMORY_USAGE 20


/**************************************
*  Simple Functions
**************************************/

int LZ5_compress_default(const char* source, char* dest, int sourceSize, int maxDestSize);
int LZ5_decompress_safe (const char* source, char* dest, int compressedSize, int maxDecompressedSize);

/*
LZ5_compress_default() :
    Compresses 'sourceSize' bytes from buffer 'source'
    into already allocated 'dest' buffer of size 'maxDestSize'.
    Compression is guaranteed to succeed if 'maxDestSize' >= LZ5_compressBound(sourceSize).
    It also runs faster, so it's a recommended setting.
    If the function cannot compress 'source' into a more limited 'dest' budget,
    compression stops *immediately*, and the function result is zero.
    As a consequence, 'dest' content is not valid.
    This function never writes outside 'dest' buffer, nor read outside 'source' buffer.
        sourceSize  : Max supported value is LZ5_MAX_INPUT_VALUE
        maxDestSize : full or partial size of buffer 'dest' (which must be already allocated)
        return : the number of bytes written into buffer 'dest' (necessarily <= maxOutputSize)
              or 0 if compression fails

LZ5_decompress_safe() :
    compressedSize : is the precise full size of the compressed block.
    maxDecompressedSize : is the size of destination buffer, which must be already allocated.
    return : the number of bytes decompressed into destination buffer (necessarily <= maxDecompressedSize)
             If destination buffer is not large enough, decoding will stop and output an error code (<0).
             If the source stream is detected malformed, the function will stop decoding and return a negative result.
             This function is protected against buffer overflow exploits, including malicious data packets.
             It never writes outside output buffer, nor reads outside input buffer.
*/


/**************************************
*  Advanced Functions
**************************************/
#define LZ5_MAX_INPUT_SIZE        0x7E000000   /* 2 113 929 216 bytes */
#define LZ5_COMPRESSBOUND(isize)  ((unsigned)(isize) > (unsigned)LZ5_MAX_INPUT_SIZE ? 0 : (isize) + ((isize)/128) + 16)

/*
LZ5_compressBound() :
    Provides the maximum size that LZ5 compression may output in a "worst case" scenario (input data not compressible)
    This function is primarily useful for memory allocation purposes (destination buffer size).
    Macro LZ5_COMPRESSBOUND() is also provided for compilation-time evaluation (stack memory allocation for example).
    Note that LZ5_compress_default() compress faster when dest buffer size is >= LZ5_compressBound(srcSize)
        inputSize  : max supported value is LZ5_MAX_INPUT_SIZE
        return : maximum output size in a "worst case" scenario
              or 0, if input size is too large ( > LZ5_MAX_INPUT_SIZE)
*/
int LZ5_compressBound(int inputSize);

/*
LZ5_compress_fast() :
    Same as LZ5_compress_default(), but allows to select an "acceleration" factor.
    The larger the acceleration value, the faster the algorithm, but also the lesser the compression.
    It's a trade-off. It can be fine tuned, with each successive value providing roughly +~3% to speed.
    An acceleration value of "1" is the same as regular LZ5_compress_default()
    Values <= 0 will be replaced by ACCELERATION_DEFAULT (see lz5.c), which is 1.
*/
int LZ5_compress_fast (const char* source, char* dest, int sourceSize, int maxDestSize, int acceleration);


/*
LZ5_compress_fast_extState() :
    Same compression function, just using an externally allocated memory space to store compression state.
    Use LZ5_sizeofState() to know how much memory must be allocated,
    and allocate it on 8-bytes boundaries (using malloc() typically).
    Then, provide it as 'void* state' to compression function.
*/
int LZ5_sizeofState(void);
int LZ5_compress_fast_extState (void* state, const char* source, char* dest, int inputSize, int maxDestSize, int acceleration);


/*
LZ5_compress_destSize() :
    Reverse the logic, by compressing as much data as possible from 'source' buffer
    into already allocated buffer 'dest' of size 'targetDestSize'.
    This function either compresses the entire 'source' content into 'dest' if it's large enough,
    or fill 'dest' buffer completely with as much data as possible from 'source'.
        *sourceSizePtr : will be modified to indicate how many bytes where read from 'source' to fill 'dest'.
                         New value is necessarily <= old value.
        return : Nb bytes written into 'dest' (necessarily <= targetDestSize)
              or 0 if compression fails
*/
int LZ5_compress_destSize (const char* source, char* dest, int* sourceSizePtr, int targetDestSize);


/*
LZ5_decompress_fast() :
    originalSize : is the original and therefore uncompressed size
    return : the number of bytes read from the source buffer (in other words, the compressed size)
             If the source stream is detected malformed, the function will stop decoding and return a negative result.
             Destination buffer must be already allocated. Its size must be a minimum of 'originalSize' bytes.
    note : This function fully respect memory boundaries for properly formed compressed data.
           It is a bit faster than LZ5_decompress_safe().
           However, it does not provide any protection against intentionally modified data stream (malicious input).
           Use this function in trusted environment only (data to decode comes from a trusted source).
*/
int LZ5_decompress_fast (const char* source, char* dest, int originalSize);

/*
LZ5_decompress_safe_partial() :
    This function decompress a compressed block of size 'compressedSize' at position 'source'
    into destination buffer 'dest' of size 'maxDecompressedSize'.
    The function tries to stop decompressing operation as soon as 'targetOutputSize' has been reached,
    reducing decompression time.
    return : the number of bytes decoded in the destination buffer (necessarily <= maxDecompressedSize)
       Note : this number can be < 'targetOutputSize' should the compressed block to decode be smaller.
             Always control how many bytes were decoded.
             If the source stream is detected malformed, the function will stop decoding and return a negative result.
             This function never writes outside of output buffer, and never reads outside of input buffer. It is therefore protected against malicious data packets
*/
int LZ5_decompress_safe_partial (const char* source, char* dest, int compressedSize, int targetOutputSize, int maxDecompressedSize);


/***********************************************
*  Streaming Compression Functions
***********************************************/
#define LZ5_STREAMSIZE_U64 ((1 << (LZ5_MEMORY_USAGE-3)) + 4)
#define LZ5_STREAMSIZE     (LZ5_STREAMSIZE_U64 * sizeof(long long))
/*
 * LZ5_stream_t
 * information structure to track an LZ5 stream.
 * important : init this structure content before first use !
 * note : only allocated directly the structure if you are statically linking LZ5
 *        If you are using liblz5 as a DLL, please use below construction methods instead.
 */
typedef struct { long long table[LZ5_STREAMSIZE_U64]; } LZ5_stream_t;

/*
 * LZ5_resetStream
 * Use this function to init an allocated LZ5_stream_t structure
 */
void LZ5_resetStream (LZ5_stream_t* streamPtr);

/*
 * LZ5_createStream will allocate and initialize an LZ5_stream_t structure
 * LZ5_freeStream releases its memory.
 * In the context of a DLL (liblz5), please use these methods rather than the static struct.
 * They are more future proof, in case of a change of LZ5_stream_t size.
 */
LZ5_stream_t* LZ5_createStream(void);
int           LZ5_freeStream (LZ5_stream_t* streamPtr);

/*
 * LZ5_loadDict
 * Use this function to load a static dictionary into LZ5_stream.
 * Any previous data will be forgotten, only 'dictionary' will remain in memory.
 * Loading a size of 0 is allowed.
 * Return : dictionary size, in bytes (necessarily <= 64 KB)
 */
int LZ5_loadDict (LZ5_stream_t* streamPtr, const char* dictionary, int dictSize);

/*
 * LZ5_compress_fast_continue
 * Compress buffer content 'src', using data from previously compressed blocks as dictionary to improve compression ratio.
 * Important : Previous data blocks are assumed to still be present and unmodified !
 * 'dst' buffer must be already allocated.
 * If maxDstSize >= LZ5_compressBound(srcSize), compression is guaranteed to succeed, and runs faster.
 * If not, and if compressed data cannot fit into 'dst' buffer size, compression stops, and function returns a zero.
 */
int LZ5_compress_fast_continue (LZ5_stream_t* streamPtr, const char* src, char* dst, int srcSize, int maxDstSize, int acceleration);

/*
 * LZ5_saveDict
 * If previously compressed data block is not guaranteed to remain available at its memory location
 * save it into a safer place (char* safeBuffer)
 * Note : you don't need to call LZ5_loadDict() afterwards,
 *        dictionary is immediately usable, you can therefore call LZ5_compress_fast_continue()
 * Return : saved dictionary size in bytes (necessarily <= dictSize), or 0 if error
 */
int LZ5_saveDict (LZ5_stream_t* streamPtr, char* safeBuffer, int dictSize);


/************************************************
*  Streaming Decompression Functions
************************************************/

#define LZ5_STREAMDECODESIZE_U64  4
#define LZ5_STREAMDECODESIZE     (LZ5_STREAMDECODESIZE_U64 * sizeof(unsigned long long))
typedef struct { unsigned long long table[LZ5_STREAMDECODESIZE_U64]; } LZ5_streamDecode_t;
/*
 * LZ5_streamDecode_t
 * information structure to track an LZ5 stream.
 * init this structure content using LZ5_setStreamDecode or memset() before first use !
 *
 * In the context of a DLL (liblz5) please prefer usage of construction methods below.
 * They are more future proof, in case of a change of LZ5_streamDecode_t size in the future.
 * LZ5_createStreamDecode will allocate and initialize an LZ5_streamDecode_t structure
 * LZ5_freeStreamDecode releases its memory.
 */
LZ5_streamDecode_t* LZ5_createStreamDecode(void);
int                 LZ5_freeStreamDecode (LZ5_streamDecode_t* LZ5_stream);

/*
 * LZ5_setStreamDecode
 * Use this function to instruct where to find the dictionary.
 * Setting a size of 0 is allowed (same effect as reset).
 * Return : 1 if OK, 0 if error
 */
int LZ5_setStreamDecode (LZ5_streamDecode_t* LZ5_streamDecode, const char* dictionary, int dictSize);

/*
*_continue() :
    These decoding functions allow decompression of multiple blocks in "streaming" mode.
    Previously decoded blocks *must* remain available at the memory position where they were decoded (up to 64 KB)
    In the case of a ring buffers, decoding buffer must be either :
    - Exactly same size as encoding buffer, with same update rule (block boundaries at same positions)
      In which case, the decoding & encoding ring buffer can have any size, including very small ones ( < 64 KB).
    - Larger than encoding buffer, by a minimum of maxBlockSize more bytes.
      maxBlockSize is implementation dependent. It's the maximum size you intend to compress into a single block.
      In which case, encoding and decoding buffers do not need to be synchronized,
      and encoding ring buffer can have any size, including small ones ( < 64 KB).
    - _At least_ 64 KB + 8 bytes + maxBlockSize.
      In which case, encoding and decoding buffers do not need to be synchronized,
      and encoding ring buffer can have any size, including larger than decoding buffer.
    Whenever these conditions are not possible, save the last 64KB of decoded data into a safe buffer,
    and indicate where it is saved using LZ5_setStreamDecode()
*/
int LZ5_decompress_safe_continue (LZ5_streamDecode_t* LZ5_streamDecode, const char* source, char* dest, int compressedSize, int maxDecompressedSize);
int LZ5_decompress_fast_continue (LZ5_streamDecode_t* LZ5_streamDecode, const char* source, char* dest, int originalSize);


/*
Advanced decoding functions :
*_usingDict() :
    These decoding functions work the same as
    a combination of LZ5_setStreamDecode() followed by LZ5_decompress_x_continue()
    They are stand-alone. They don't need nor update an LZ5_streamDecode_t structure.
*/
int LZ5_decompress_safe_usingDict (const char* source, char* dest, int compressedSize, int maxDecompressedSize, const char* dictStart, int dictSize);
int LZ5_decompress_fast_usingDict (const char* source, char* dest, int originalSize, const char* dictStart, int dictSize);


/**************************************
*  Obsolete Functions
**************************************/
/* Deprecate Warnings */
/* Should these warnings messages be a problem,
   it is generally possible to disable them,
   with -Wno-deprecated-declarations for gcc
   or _CRT_SECURE_NO_WARNINGS in Visual for example.
   Otherwise, you can also define LZ5_DISABLE_DEPRECATE_WARNINGS */
#define LZ5_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#ifdef LZ5_DISABLE_DEPRECATE_WARNINGS
#  define LZ5_DEPRECATED()   /* disable deprecation warnings */
#else
#  if (LZ5_GCC_VERSION >= 405) || defined(__clang__)
#    define LZ5_DEPRECATED(message) __attribute__((deprecated(message)))
#  elif (LZ5_GCC_VERSION >= 301)
#    define LZ5_DEPRECATED(message) __attribute__((deprecated))
#  elif defined(_MSC_VER)
#    define LZ5_DEPRECATED(message) __declspec(deprecated(message))
#  else
#    pragma message("WARNING: You need to implement LZ5_DEPRECATED for this compiler")
#    define LZ5_DEPRECATED(message)
#  endif
#endif /* LZ5_DISABLE_DEPRECATE_WARNINGS */

/* Obsolete compression functions */
/* These functions will generate warnings in a future release */
int LZ5_compress               (const char* source, char* dest, int sourceSize);
int LZ5_compress_limitedOutput (const char* source, char* dest, int sourceSize, int maxOutputSize);
int LZ5_compress_withState               (void* state, const char* source, char* dest, int inputSize);
int LZ5_compress_limitedOutput_withState (void* state, const char* source, char* dest, int inputSize, int maxOutputSize);
int LZ5_compress_continue                (LZ5_stream_t* LZ5_streamPtr, const char* source, char* dest, int inputSize);
int LZ5_compress_limitedOutput_continue  (LZ5_stream_t* LZ5_streamPtr, const char* source, char* dest, int inputSize, int maxOutputSize);

/* Obsolete decompression functions */
/* These function names are completely deprecated and must no longer be used.
   They are only provided in lz5.c for compatibility with older programs.
    - LZ5_uncompress is the same as LZ5_decompress_fast
    - LZ5_uncompress_unknownOutputSize is the same as LZ5_decompress_safe
   These function prototypes are now disabled; uncomment them only if you really need them.
   It is highly recommended to stop using these prototypes and migrate to maintained ones */
/* int LZ5_uncompress (const char* source, char* dest, int outputSize); */
/* int LZ5_uncompress_unknownOutputSize (const char* source, char* dest, int isize, int maxOutputSize); */

/* Obsolete streaming functions; use new streaming interface whenever possible */
LZ5_DEPRECATED("use LZ5_createStream() instead") void* LZ5_create (char* inputBuffer);
LZ5_DEPRECATED("use LZ5_createStream() instead") int   LZ5_sizeofStreamState(void);
LZ5_DEPRECATED("use LZ5_resetStream() instead")  int   LZ5_resetStreamState(void* state, char* inputBuffer);
LZ5_DEPRECATED("use LZ5_saveDict() instead")     char* LZ5_slideInputBuffer (void* state);

/* Obsolete streaming decoding functions */
LZ5_DEPRECATED("use LZ5_decompress_safe_usingDict() instead") int LZ5_decompress_safe_withPrefix64k (const char* src, char* dst, int compressedSize, int maxDstSize);
LZ5_DEPRECATED("use LZ5_decompress_fast_usingDict() instead") int LZ5_decompress_fast_withPrefix64k (const char* src, char* dst, int originalSize);


#if defined (__cplusplus)
}
#endif
