/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.MultiThreadWrapper.Brotli.cpp
 * PURPOSE:   Implementation for Brotli Multi Thread Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.MultiThreadWrapper.Brotli.h"

#include <cstddef>

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

EXTERN_C HRESULT WINAPI NanaZipCodecsBrotliDecode(
    _In_ PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT StreamContext,
    _In_ UINT32 NumberOfThreads,
    _In_ UINT32 InputSize)
{
    BROTLIMT_RdWr_t ReadWrite = { 0 };
    ReadWrite.fn_read = ::NanaZipCodecsBrotliRead;
    ReadWrite.fn_write = ::NanaZipCodecsBrotliWrite;
    ReadWrite.arg_read = reinterpret_cast<void*>(&StreamContext);
    ReadWrite.arg_write = reinterpret_cast<void*>(&StreamContext);

    BROTLIMT_DCtx* Context = ::BROTLIMT_createDCtx(NumberOfThreads, InputSize);
    if (!Context)
    {
        return S_FALSE;
    }

    std::size_t Result = ::BROTLIMT_decompressDCtx(Context, &ReadWrite);
    if (::BROTLIMT_isError(Result))
    {
        if (MT_ERROR(canceled) == Result)
        {
            return E_ABORT;
        }

        return E_FAIL;
    }

    ::BROTLIMT_freeDCtx(Context);

    return S_OK;
}
