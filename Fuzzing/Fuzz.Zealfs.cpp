/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.Zealfs.cpp
 * PURPOSE:    libFuzzer harness for the ZealFS handler
 *
 * LICENSE:    The MIT License
 */

#include "NanaZip.Codecs.Fuzz.h"

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::RunFuzzCase(4, Data, Size);
    return 0;
}