/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.MultiThreadWrapper.Lizard.cpp
 * PURPOSE:   Implementation for Lizard Multi Thread Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.MultiThreadWrapper.Lizard.h"

#include <cstddef>

EXTERN_C int NanaZipCodecsLizardRead(
    void* Context,
    LIZARDMT_Buffer* Input)
{
    NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT ConvertedInput;
    ConvertedInput.Buffer = Input->buf;
    ConvertedInput.Size = Input->size;
    ConvertedInput.Allocated = Input->allocated;
    return ::NanaZipCodecsCommonRead(
        reinterpret_cast<PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT>(Context),
        &ConvertedInput);
}

EXTERN_C int NanaZipCodecsLizardWrite(
    void* Context,
    LIZARDMT_Buffer* Output)
{
    NANAZIP_CODECS_ZSTDMT_BUFFER_CONTEXT ConvertedOutput;
    ConvertedOutput.Buffer = Output->buf;
    ConvertedOutput.Size = Output->size;
    ConvertedOutput.Allocated = Output->allocated;
    return ::NanaZipCodecsCommonWrite(
        reinterpret_cast<PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT>(Context),
        &ConvertedOutput);
}

EXTERN_C HRESULT WINAPI NanaZipCodecsLizardDecode(
    _In_ PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT StreamContext,
    _In_ UINT32 NumberOfThreads,
    _In_ UINT32 InputSize)
{
    LIZARDMT_RdWr_t ReadWrite = { 0 };
    ReadWrite.fn_read = ::NanaZipCodecsLizardRead;
    ReadWrite.fn_write = ::NanaZipCodecsLizardWrite;
    ReadWrite.arg_read = reinterpret_cast<void*>(&StreamContext);
    ReadWrite.arg_write = reinterpret_cast<void*>(&StreamContext);

    LIZARDMT_DCtx* Context = ::LIZARDMT_createDCtx(NumberOfThreads, InputSize);
    if (!Context)
    {
        return S_FALSE;
    }

    std::size_t Result = ::LIZARDMT_decompressDCtx(Context, &ReadWrite);
    if (::LIZARDMT_isError(Result))
    {
        if (ERROR(canceled) == Result)
        {
            return E_ABORT;
        }

        return E_FAIL;
    }

    ::LIZARDMT_freeDCtx(Context);

    return S_OK;
}
