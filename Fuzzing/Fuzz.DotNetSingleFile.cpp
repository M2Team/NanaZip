/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.DotNetSingleFile.cpp
 * PURPOSE:    libFuzzer harness for the .NET single-file handler
 *
 * LICENSE:    The MIT License
 *
 * .NET SingleFile bundle layout:
 *   [0..1]   : "MZ" (PE signature check)
 *   [2..9]   : int64_t BundleHeaderOffset
 *   [10..41] : 32-byte SHA-256 signature (g_BundleSignature)
 *   [42+]    : bundle header (version, file count, entries...)
 *
 * The handler scans up to 20 MiB looking for the signature byte-by-byte.
 * Without a mutator, most iterations waste time on inputs where the signature
 * is never found. We place the MZ + offset + signature at fixed positions so
 * every iteration reaches the bundle header parser immediately.
 */

#include "NanaZip.Codecs.Fuzz.h"
#include <cstring>
#include <cstdint>

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

// Bundle signature: SHA-256 of ".net core bundle"
static const std::uint8_t BundleSignature[32] = {
    0x8b, 0x12, 0x02, 0xb9, 0x6a, 0x61, 0x20, 0x38,
    0x72, 0x7b, 0x93, 0x02, 0x14, 0xd7, 0xa0, 0x32,
    0x13, 0xf5, 0xb9, 0xe6, 0xef, 0xae, 0x33, 0x18,
    0xee, 0x3b, 0x2d, 0xce, 0x24, 0xb3, 0x6a, 0xae
};

// Fixed layout:
//   [0]  'M'
//   [1]  'Z'
//   [2]  int64_t LE = 42  (BundleHeaderOffset, points right after signature)
//   [10] signature (32 bytes)
//   [42] start of bundle header (mutator leaves this to libFuzzer)
static constexpr std::size_t SignatureOffset = 10;
static constexpr std::size_t HeaderStart = SignatureOffset + 32;  // 42
static constexpr std::size_t MinDotNet = HeaderStart + 13;  // version(8) + count(4) + idlen(1)

static void WriteLE64(std::uint8_t* P, std::int64_t V)
{
    auto U = static_cast<std::uint64_t>(V);
    for (int I = 0; I < 8; ++I)
    {
        P[I] = static_cast<std::uint8_t>(U >> (I * 8));
    }
}

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* Data, size_t Size, size_t MaxSize, unsigned int /*Seed*/)
{
    size_t NewSize = LLVMFuzzerMutate(Data, Size, MaxSize);

    if (NewSize < MinDotNet)
    {
        if (MaxSize < MinDotNet)
            return NewSize;
        std::memset(Data + NewSize, 0, MinDotNet - NewSize);
        NewSize = MinDotNet;
    }

    // Fix the header stub so the signature scan succeeds instantly
    Data[0] = 'M';
    Data[1] = 'Z';
    WriteLE64(Data + 2, static_cast<std::int64_t>(HeaderStart));
    std::memcpy(Data + SignatureOffset, BundleSignature, 32);

    return NewSize;
}

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::RunFuzzCase(1, Data, Size);
    return 0;
}