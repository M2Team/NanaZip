/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.MultiThreadWrapper.LZ5.h
 * PURPOSE:   Definition for LZ5 Multi Thread Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_CODECS_MULTI_THREAD_WRAPPER_LZ5
#define NANAZIP_CODECS_MULTI_THREAD_WRAPPER_LZ5

#include "NanaZip.Codecs.MultiThreadWrapper.Common.h"

#include <stdint.h>
#include <lz5-mt.h>

EXTERN_C int NanaZipCodecsLz5Read(
    void* Context,
    LZ5MT_Buffer* Input);

EXTERN_C int NanaZipCodecsLz5Write(
    void* Context,
    LZ5MT_Buffer* Output);

#endif // !NANAZIP_CODECS_MULTI_THREAD_WRAPPER_LZ5
