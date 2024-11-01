/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Wrappers.ZSTDMT.cpp
 * PURPOSE:   Implementation for NanaZip.Codecs ZSTDMT Wrappers
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.Wrappers.ZSTDMT.h"

typedef struct _NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT
{
    PVOID Buffer;
    SIZE_T Size;
    SIZE_T Allocated;
} NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT, *PNANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT;

static int NanaZipCodecsCommonRead(
    PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context,
    PNANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT Input)
{
    const UINT32 BlockSize = static_cast<UINT32>(1) << 31;

    HRESULT hr = S_OK;
    SIZE_T UnprocessedSize = Input->Size;
    SIZE_T ProcessedSize = 0;
    PVOID CurrentBuffer = Input->Buffer;

    while (UnprocessedSize)
    {
        UINT32 CurrentSize =
            UnprocessedSize < BlockSize
            ? static_cast<UINT32>(UnprocessedSize)
            : BlockSize;
        UINT32 CurrentProcessedSize = 0;
        hr = Context->InputStream->Read(
            CurrentBuffer,
            CurrentSize,
            &CurrentProcessedSize);
        ProcessedSize += CurrentProcessedSize;
        CurrentBuffer = static_cast<PVOID>(
            static_cast<PBYTE>(CurrentBuffer) + CurrentProcessedSize);
        UnprocessedSize -= CurrentProcessedSize;
        if (S_OK != hr)
        {
            break;
        }
        if (CurrentProcessedSize == 0)
        {
            hr = S_OK;
            break;
        }
    }

    // catch errors
    if (E_ABORT == hr)
    {
        return -2;
    }
    else if (E_OUTOFMEMORY == hr)
    {
        return -3;
    }

    // some other error -> read_fail
    if (S_OK != hr)
    {
        return -1;
    }

    Input->Size = ProcessedSize;
    *Context->ProcessedInputSize += ProcessedSize;

    return 0;
}

static int NanaZipCodecsCommonWrite(
    PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context,
    PNANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT Output)
{
    UINT32 Todo = static_cast<UINT32>(Output->Size);
    UINT32 Done = 0;

    while (Todo)
    {
        UINT32 Block = 0;

        HRESULT hr = Context->OutputStream->Write(
            reinterpret_cast<PBYTE>(Output->Buffer) + Done,
            Todo,
            &Block);

        // catch errors
        if (E_ABORT == hr)
        {
            return -2;
        }
        else if (E_OUTOFMEMORY == hr)
        {
            return -3;
        }

        Done += Block;

        if (SEVENZIP_ERROR_WRITING_WAS_CUT == hr)
        {
            break;
        }

        // some other error -> write_fail
        if (S_OK != hr)
        {
            return -1;
        }

        if (!Block)
        {
            return -1;
        }
            
        Todo -= Block;
    }

    *Context->ProcessedOutputSize += Done;

    // we need no lock here, cause only one thread can write
    if (Context->Progress)
    {
        Context->Progress->SetRatioInfo(
            Context->ProcessedInputSize,
            Context->ProcessedOutputSize);
    }

    return 0;
}

EXTERN_C int NanaZipCodecsBrotliRead(
    void* Context,
    BROTLIMT_Buffer* Input)
{
    NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT ConvertedInput;
    ConvertedInput.Buffer = Input->buf;
    ConvertedInput.Size = Input->size;
    ConvertedInput.Allocated = Input->allocated;
    return ::NanaZipCodecsCommonRead(
        reinterpret_cast<PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT>(Context),
        &ConvertedInput);
}

EXTERN_C int NanaZipCodecsBrotliWrite(
    void* Context,
    BROTLIMT_Buffer* Output)
{
    NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT ConvertedOutput;
    ConvertedOutput.Buffer = Output->buf;
    ConvertedOutput.Size = Output->size;
    ConvertedOutput.Allocated = Output->allocated;
    return ::NanaZipCodecsCommonWrite(
        reinterpret_cast<PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT>(Context),
        &ConvertedOutput);
}
