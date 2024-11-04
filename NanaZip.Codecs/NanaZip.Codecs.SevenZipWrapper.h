/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.SevenZipWrapper.h
 * PURPOSE:   Definition for 7-Zip Codec Interface Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_CODECS_SEVENZIP_WRAPPER
#define NANAZIP_CODECS_SEVENZIP_WRAPPER

#if defined(ZIP7_INC_COMPILER_H) || defined(__7Z_COMPILER_H)
#include <Windows.h>
#else
#include <NanaZip.Specification.SevenZip.h>
#endif

EXTERN_C HRESULT WINAPI NanaZipCodecsReadInputStream(
    _In_ ISequentialInStream* InputStream,
    _Out_ PVOID Buffer,
    _In_ SIZE_T NumberOfBytesToRead,
    _Out_opt_ PSIZE_T NumberOfBytesRead);

#endif // !NANAZIP_CODECS_SEVENZIP_WRAPPER
