/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.MultiThreadWrapper.Lizard.h
 * PURPOSE:   Definition for Lizard Multi Thread Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_CODECS_MULTI_THREAD_WRAPPER_LIZARD
#define NANAZIP_CODECS_MULTI_THREAD_WRAPPER_LIZARD

#include "NanaZip.Codecs.MultiThreadWrapper.Common.h"

#include <stdint.h>
#include <lizard-mt.h>

EXTERN_C int NanaZipCodecsLizardRead(
    void* Context,
    LIZARDMT_Buffer* Input);

EXTERN_C int NanaZipCodecsLizardWrite(
    void* Context,
    LIZARDMT_Buffer* Output);

EXTERN_C HRESULT WINAPI NanaZipCodecsLizardDecode(
    _In_ PNANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT StreamContext,
    _In_ UINT32 NumberOfThreads,
    _In_ UINT32 InputSize);

#endif // !NANAZIP_CODECS_MULTI_THREAD_WRAPPER_LIZARD
