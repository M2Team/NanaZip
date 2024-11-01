/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Wrappers.ZSTDMT.h
 * PURPOSE:   Definition for NanaZip.Codecs ZSTDMT Wrappers
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_CODECS_WRAPPERS_ZSTDMT
#define NANAZIP_CODECS_WRAPPERS_ZSTDMT

#ifdef ZIP7_INC_ICODER_H
#include <Windows.h>
#else
#include <NanaZip.Specification.SevenZip.h>
#endif

#include <stdint.h>

#include <brotli-mt.h>

typedef struct _NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT
{
    ISequentialInStream* InputStream;
    ISequentialOutStream* OutputStream;
    ICompressProgressInfo* Progress;
    PUINT64 ProcessedInputSize;
    PUINT64 ProcessedOutputSize;
} NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT, *PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT;

EXTERN_C int NanaZipCodecsBrotliRead(
    void* RawContext,
    BROTLIMT_Buffer* Input);

EXTERN_C int NanaZipCodecsBrotliWrite(
    void* Context,
    BROTLIMT_Buffer* Output);

#endif // !NANAZIP_CODECS_WRAPPERS_ZSTDMT
