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
