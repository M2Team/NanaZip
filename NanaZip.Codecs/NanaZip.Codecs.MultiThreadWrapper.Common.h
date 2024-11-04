/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.MultiThreadWrapper.Common.h
 * PURPOSE:   Definition for Common Multi Thread Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_CODECS_MULTI_THREAD_WRAPPER_COMMON
#define NANAZIP_CODECS_MULTI_THREAD_WRAPPER_COMMON

#if defined(ZIP7_INC_COMPILER_H) || defined(__7Z_COMPILER_H)
#include <Windows.h>
#else
#include <NanaZip.Specification.SevenZip.h>
#endif

typedef struct _NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT
{
    ISequentialInStream* InputStream;
    ISequentialOutStream* OutputStream;
    ICompressProgressInfo* Progress;
    PUINT64 ProcessedInputSize;
    PUINT64 ProcessedOutputSize;
} NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT, *PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT;

typedef struct _NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT
{
    PVOID Buffer;
    SIZE_T Size;
    SIZE_T Allocated;
} NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT, *PNANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT;

EXTERN_C int NanaZipCodecsCommonRead(
    PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context,
    PNANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT Input);

EXTERN_C int NanaZipCodecsCommonWrite(
    PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context,
    PNANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT Output);

#endif // !NANAZIP_CODECS_MULTI_THREAD_WRAPPER_COMMON
