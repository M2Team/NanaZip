/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.Avb.cpp
 * PURPOSE:    libFuzzer harness for the upstream-7-Zip AvbHandler that
 *             NanaZip ships but 7-Zip's official 7z.dll does not
 *
 * LICENSE:    The MIT License
 *
 * AVB layout (AvbHandler.cpp):
 *   bytes [0 .. vbmeta_size-1] : vbmeta block (header + auth + aux/descriptors)
 *   bytes [vbmeta_size .. fileSize-65] : partition data (ext image etc.)
 *   bytes [fileSize-64 .. fileSize-1] : footer
 *
 * Footer gates (Open2):
 *   [0..7]   memcmp "AVBf\0\0\0\1"
 *   [28..35] vbmeta_size in [256, 65536]
 *   [36..63] all zeros (reserved)
 *
 * Vbmeta header gates (AvbVBMetaImageHeader::Parse):
 *   [0..3]   BE u32 == 0x41564230 ("AVB0")
 *   [4..7]   required_libavb_version_major (BE u32) == 1
 *
 * Without a custom mutator, random byte changes to the 64-byte footer (whose
 * POSITION shifts whenever libFuzzer changes the input length) almost never
 * satisfy all constraints at once. We fix the footer + vbmeta-header after
 * every mutation so the fuzzer always reaches the descriptor-walk code.
 *
 * The mutator also pins authentication_data_block_size=0, descriptors_offset=0,
 * and sets auxiliary_data_block_size = descriptors_size = (vbmeta_size - 256),
 * with vbmeta_size rounded so the aux block is 64-byte aligned.
 * This ensures the descriptor parsing loop always runs over the fuzzed bytes.
 */

#include "NanaZip.Core.Fuzz.h"
#include <cstring>
#include <algorithm>

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

static void WriteBE32(std::uint8_t* P, std::uint32_t V)
{
    P[0] = static_cast<std::uint8_t>(V >> 24);
    P[1] = static_cast<std::uint8_t>(V >> 16);
    P[2] = static_cast<std::uint8_t>(V >> 8);
    P[3] = static_cast<std::uint8_t>(V);
}

static void WriteBE64(std::uint8_t* P, std::uint64_t V)
{
    WriteBE32(P, static_cast<std::uint32_t>(V >> 32));
    WriteBE32(P + 4, static_cast<std::uint32_t>(V));
}

// Minimum useful size: 256 (vbmeta header) + 64 (footer) = 320
static constexpr std::size_t MinAvb = 320;
static constexpr std::size_t FooterSize = 64;
static constexpr std::size_t VbmetaHeaderSize = 256;

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* Data, size_t Size, size_t MaxSize, unsigned int /*Seed*/)
{
    size_t NewSize = LLVMFuzzerMutate(Data, Size, MaxSize);

    // Ensure minimum size for a parseable AVB image.
    if (NewSize < MinAvb)
    {
        if (MaxSize < MinAvb)
            return NewSize;          // can't fix, let it fail fast
        // Zero-extend
        std::memset(Data + NewSize, 0, MinAvb - NewSize);
        NewSize = MinAvb;
    }

    // --- Fix vbmeta header (always at offset 0) ---
    WriteBE32(Data + 0, 0x41564230);   // "AVB0"
    WriteBE32(Data + 4, 1);            // required_libavb_version_major

    // authentication_data_block_size = 0 (skip auth block so
    // descriptors start right after the 256-byte header)
    WriteBE64(Data + 12, 0);

    // descriptors_offset = 0 (relative to end of header + auth block)
    WriteBE64(Data + 96, 0);

    // --- Fix footer (last 64 bytes) ---
    std::uint8_t* Footer = Data + NewSize - FooterSize;

    // Magic "AVBf\0\0\0\1"
    static const std::uint8_t Signature[8] = {
        'A','V','B','f', 0, 0, 0, 1
    };
    std::memcpy(Footer, Signature, 8);

    // version_minor = 0
    std::memset(Footer + 8, 0, 4);

    // original_image_size: keep whatever the mutator put in bytes 12..19
    // (the handler reads it but only uses it for the sub-stream size)

    // vbmeta_offset = 0  (BE u64)
    WriteBE64(Footer + 20, 0);

    // vbmeta_size = (NewSize - 64), rounded DOWN so aux block is 64-byte aligned
    std::uint64_t VbmetaSize = NewSize - FooterSize;
    if (VbmetaSize > 65536) VbmetaSize = 65536;
    if (VbmetaSize < VbmetaHeaderSize) VbmetaSize = VbmetaHeaderSize;
    // Round so (VbmetaSize - 256) is a multiple of 64.
    std::uint64_t AuxSize = (VbmetaSize - VbmetaHeaderSize) & ~std::uint64_t(63);
    VbmetaSize = VbmetaHeaderSize + AuxSize;
    WriteBE64(Footer + 28, VbmetaSize);

    // auxiliary_data_block_size must equal the actual aux block size.
    WriteBE64(Data + 20, AuxSize);

    // descriptors_size = AuxSize (all aux space is descriptors)
    WriteBE64(Data + 104, AuxSize);

    // Reserved bytes 36..63 must be zero.
    std::memset(Footer + 36, 0, 28);

    return NewSize;
}

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::Core::RunFuzzCaseByName(L"AVB", Data, Size);
    return 0;
}