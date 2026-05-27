/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.Littlefs.cpp
 * PURPOSE:    libFuzzer harness for the littlefs handler
 *
 * LICENSE:    The MIT License
 */

#include "NanaZip.Codecs.Fuzz.h"

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::RunFuzzCase(6, Data, Size);
    return 0;
}