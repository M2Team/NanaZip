/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.MultiThreadWrapper.LZ4.cpp
 * PURPOSE:   Implementation for LZ4 Multi Thread Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.MultiThreadWrapper.LZ4.h"

#include <cstddef>

EXTERN_C int NanaZipCodecsLz4Read(
    void* Context,
    LZ4MT_Buffer* Input)
{
    NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT ConvertedInput;
    ConvertedInput.Buffer = Input->buf;
    ConvertedInput.Size = Input->size;
    ConvertedInput.Allocated = Input->allocated;
    return ::NanaZipCodecsCommonRead(
        reinterpret_cast<PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT>(Context),
        &ConvertedInput);
}

EXTERN_C int NanaZipCodecsLz4Write(
    void* Context,
    LZ4MT_Buffer* Output)
{
    NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT ConvertedOutput;
    ConvertedOutput.Buffer = Output->buf;
    ConvertedOutput.Size = Output->size;
    ConvertedOutput.Allocated = Output->allocated;
    return ::NanaZipCodecsCommonWrite(
        reinterpret_cast<PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT>(Context),
        &ConvertedOutput);
}

EXTERN_C HRESULT WINAPI NanaZipCodecsLz4Decode(
    _In_ PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT StreamContext,
    _In_ UINT32 NumberOfThreads,
    _In_ UINT32 InputSize)
{
    LZ4MT_RdWr_t ReadWrite = { 0 };
    ReadWrite.fn_read = ::NanaZipCodecsLz4Read;
    ReadWrite.fn_write = ::NanaZipCodecsLz4Write;
    ReadWrite.arg_read = reinterpret_cast<void*>(&StreamContext);
    ReadWrite.arg_write = reinterpret_cast<void*>(&StreamContext);

    LZ4MT_DCtx* Context = ::LZ4MT_createDCtx(NumberOfThreads, InputSize);
    if (!Context)
    {
        return S_FALSE;
    }

    std::size_t Result = ::LZ4MT_decompressDCtx(Context, &ReadWrite);
    if (::LZ4MT_isError(Result))
    {
        if (ERROR(canceled) == Result)
        {
            return E_ABORT;
        }

        return E_FAIL;
    }

    ::LZ4MT_freeDCtx(Context);

    return S_OK;
}
