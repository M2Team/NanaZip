/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.WebAssembly.cpp
 * PURPOSE:    libFuzzer harness for the WebAssembly (wasm) handler
 *
 * LICENSE:    The MIT License
 *
 * WebAssembly binary format (MVP):
 *   bytes [0..3]: magic   \0asm  (0x00 0x61 0x73 0x6D)
 *   bytes [4..7]: version 1      (0x01 0x00 0x00 0x00)
 *   bytes [8.. ]: sections (type:1 + ULEB128 size + payload)
 *
 * Without a custom mutator, random byte changes break the 8-byte header
 * and the handler bails at the memcmp check, wasting the iteration.
 * We rewrite the magic+version after each mutation.
 */

#include "NanaZip.Codecs.Fuzz.h"
#include <cstring>
#include <cstdint>

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

static constexpr std::size_t WasmHeaderSize = 8;
static const std::uint8_t WasmHeader[WasmHeaderSize] = {
    0x00, 0x61, 0x73, 0x6D,   // \0asm
    0x01, 0x00, 0x00, 0x00    // version 1
};

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* Data, size_t Size, size_t MaxSize, unsigned int /*Seed*/)
{
    size_t NewSize = LLVMFuzzerMutate(Data, Size, MaxSize);

    if (NewSize < WasmHeaderSize)
    {
        if (MaxSize < WasmHeaderSize)
            return NewSize;
        std::memset(Data + NewSize, 0, WasmHeaderSize - NewSize);
        NewSize = WasmHeaderSize;
    }

    // Fix magic + version so every iteration reaches the section parser
    std::memcpy(Data, WasmHeader, WasmHeaderSize);

    // Clamp ULEB128 runs to prevent multi-GB allocations from tiny inputs.
    // ULEB128 uses bit 7 as a continuation flag. A run of N continuation
    // bytes encodes a value up to 2^(7*N)-1. We allow at most 2 continuation
    // bytes (max decoded value = 16383 = 16 KiB), which is generous for any
    // section size within a 64 KiB input. Clearing bit 7 on the 3rd+
    // consecutive high-bit byte terminates the ULEB128 without changing
    // the byte count or shifting subsequent positions.
    int Run = 0;
    for (std::size_t I = WasmHeaderSize; I < NewSize; ++I)
    {
        if (Data[I] & 0x80)
        {
            ++Run;
            if (Run >= 2)
                Data[I] &= 0x7F;  // clear continuation -> cap value
        }
        else
        {
            Run = 0;
        }
    }

    return NewSize;
}

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::RunFuzzCase(5, Data, Size);
    return 0;
}