/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.ElectronAsar.cpp
 * PURPOSE:    libFuzzer harness for the Electron asar handler
 *
 * LICENSE:    The MIT License
 *
 * asar layout: [u32 pickle_var=4][u32 header_size][u32 header_buf_size]
 *              [u32 header_string_size][JSON string][padding to 4][payload]
 * with the parser-enforced relationships (see ElectronAsar.cpp:Open):
 *   pickle_var       == 4
 *   header_size      == 4 + header_buf_size
 *   header_string_size <= header_buf_size
 *
 * libFuzzer's generic byte-mutator essentially never satisfies all four at
 * once, so the parser bails at the very first sanity check. We override
 * LLVMFuzzerCustomMutator to (a) delegate to libFuzzer's mutator and then
 * (b) rewrite the 16-byte pickle header so the mutated input is always
 * well-formed enough to reach the JSON parse step.
 */

#include "NanaZip.Codecs.Fuzz.h"
#include <cstring>

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

static void WriteLE32(std::uint8_t* P, std::uint32_t V)
{
    P[0] = static_cast<std::uint8_t>(V);
    P[1] = static_cast<std::uint8_t>(V >> 8);
    P[2] = static_cast<std::uint8_t>(V >> 16);
    P[3] = static_cast<std::uint8_t>(V >> 24);
}

static std::uint32_t ReadLE32(const std::uint8_t* P)
{
    return static_cast<std::uint32_t>(P[0])
        | (static_cast<std::uint32_t>(P[1]) << 8)
        | (static_cast<std::uint32_t>(P[2]) << 16)
        | (static_cast<std::uint32_t>(P[3]) << 24);
}

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* Data, size_t Size, size_t MaxSize, unsigned int /*Seed*/)
{
    size_t NewSize = LLVMFuzzerMutate(Data, Size, MaxSize);
    if (NewSize < 16)
    {
        return NewSize;
    }

    // Treat bytes 12..15 (header_string_size) as the authoritative knob the
    // mutator gets to vary, clamp it to what the buffer can hold, then derive
    // the other three fields from it.
    std::uint32_t HeaderStringSize = ReadLE32(Data + 12);
    std::uint32_t MaxStringSize =
        static_cast<std::uint32_t>(NewSize - 16);
    if (HeaderStringSize > MaxStringSize)
    {
        HeaderStringSize = MaxStringSize;
    }

    // header_buf_size must be >= header_string_size; pad it up so all
    // downstream bounds checks see a self-consistent prefix.
    std::uint32_t HeaderBufferSize = MaxStringSize;
    std::uint32_t HeaderSize = 4 + HeaderBufferSize;

    WriteLE32(Data +  0, 4);
    WriteLE32(Data +  4, HeaderSize);
    WriteLE32(Data +  8, HeaderBufferSize);
    WriteLE32(Data + 12, HeaderStringSize);
    return NewSize;
}

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::RunFuzzCase(2, Data, Size);
    return 0;
}