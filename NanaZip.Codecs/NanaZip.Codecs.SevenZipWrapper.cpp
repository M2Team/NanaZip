/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.SevenZipWrapper.cpp
 * PURPOSE:   Implementation for 7-Zip Codec Interface Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.SevenZipWrapper.h"

namespace
{
    const UINT32 BlockSize = static_cast<UINT32>(1) << 31;
}

EXTERN_C HRESULT WINAPI NanaZipCodecsReadInputStream(
    _In_ ISequentialInStream* InputStream,
    _Out_ PVOID Buffer,
    _In_ SIZE_T NumberOfBytesToRead,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    SIZE_T ProcessedBytes = 0;
    while (0 != NumberOfBytesToRead)
    {
        UINT32 CurrentSize =
            NumberOfBytesToRead < BlockSize
            ? static_cast<UINT32>(NumberOfBytesToRead)
            : BlockSize;
        UINT32 CurrentProcessedBytes = 0;
        HRESULT hr = InputStream->Read(
            Buffer,
            CurrentSize,
            &CurrentProcessedBytes);
        ProcessedBytes += CurrentProcessedBytes;
        Buffer = static_cast<PVOID>(
            static_cast<PBYTE>(Buffer) + CurrentProcessedBytes);
        NumberOfBytesToRead -= CurrentProcessedBytes;
        if (S_OK != hr)
        {
            return hr;
        }
        if (0 == CurrentProcessedBytes)
        {
            return S_OK;
        }
    }
    if (NumberOfBytesRead)
    {
        *NumberOfBytesRead = ProcessedBytes;
    }
    return S_OK;
}
