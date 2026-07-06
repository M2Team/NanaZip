/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.Romfs.cpp
 * PURPOSE:    libFuzzer harness for the ROMFS handler
 *
 * LICENSE:    The MIT License
 *
 * ROMFS binary format (Linux romfs):
 *   bytes [0..7]:  magic "-rom1fs-"
 *   bytes [8..11]: FullSize (BE u32) - total filesystem size
 *   bytes [12..15]: Checksum (BE u32) - not verified by NanaZip
 *   bytes [16..]: volume name (null-terminated, 16-byte aligned) then file headers
 *
 * The custom mutator fixes the magic and caps FullSize to input length.
 */

#include "NanaZip.Codecs.Fuzz.h"
#include <cstring>
#include <cstdint>

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

static constexpr std::size_t RomfsMinSize = 16;  // magic(8) + fullsize(4) + checksum(4)

static void WriteBE32(std::uint8_t* P, std::uint32_t V)
{
    P[0] = static_cast<std::uint8_t>(V >> 24);
    P[1] = static_cast<std::uint8_t>(V >> 16);
    P[2] = static_cast<std::uint8_t>(V >> 8);
    P[3] = static_cast<std::uint8_t>(V);
}

static std::uint32_t ReadBE32(const std::uint8_t* P)
{
    return (static_cast<std::uint32_t>(P[0]) << 24)
         | (static_cast<std::uint32_t>(P[1]) << 16)
         | (static_cast<std::uint32_t>(P[2]) << 8)
         |  static_cast<std::uint32_t>(P[3]);
}

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* Data, size_t Size, size_t MaxSize, unsigned int /*Seed*/)
{
    size_t NewSize = LLVMFuzzerMutate(Data, Size, MaxSize);

    if (NewSize < RomfsMinSize)
    {
        if (MaxSize < RomfsMinSize)
            return NewSize;
        std::memset(Data + NewSize, 0, RomfsMinSize - NewSize);
        NewSize = RomfsMinSize;
    }

    // Fix magic so every iteration reaches the parser
    static const std::uint8_t Magic[8] = {'-','r','o','m','1','f','s','-'};
    std::memcpy(Data, Magic, 8);

    // Cap FullSize (BE u32 at offset 8) to input length.
    // The parser uses FullSize to bound filename reads and as the overall
    // filesystem size. If FullSize > input length, ReadFileStream fails
    // on every file header read, wasting the iteration.
    std::uint32_t FullSize = ReadBE32(Data + 8);
    if (FullSize > static_cast<std::uint32_t>(NewSize))
    {
        FullSize = static_cast<std::uint32_t>(NewSize);
        WriteBE32(Data + 8, FullSize);
    }

    return NewSize;
}

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::RunFuzzCase(3, Data, Size);
    return 0;
}