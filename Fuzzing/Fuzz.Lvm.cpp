/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.Lvm.cpp
 * PURPOSE:    libFuzzer harness for the upstream-7-Zip LvmHandler that
 *             NanaZip ships but 7-Zip's official 7z.dll does not
 *
 * LICENSE:    The MIT License
 *
 * File layout (custom mutator enforces this on every iteration):
 *
 *   Sector 0 (bytes 0..511) = metadata area:
 *     [0x00..0x03]  metadata header CRC  (CRC of bytes [4..511])
 *     [0x04..0x13]  FMTT_MAGIC  " LVM2 x[5A%r0N*>"
 *     [0x14..0x17]  version = 1
 *     [0x18..0x1F]  mda_header.Start = 0
 *     [0x20..0x27]  mda_header.Size  = 512
 *     [0x28..0x3F]  raw_locn[0] {Offset=0x58, Size=VgConfigSize,
 *                                Checksum=CRC(vg_config), Flags=0}
 *     [0x40..0x57]  raw_locn terminator (24 zero bytes)
 *     [0x58..0x1FF] VG config text (fuzzed region, 424 bytes)
 *
 *   Sector 1 (bytes 512..1023) = PV label:
 *     [+0x00..+0x07]  "LABELONE"
 *     [+0x08..+0x0F]  sector_xl = 1
 *     [+0x10..+0x13]  label CRC  (CRC of label bytes [20..511])
 *     [+0x14..+0x17]  offsetToCont = 32
 *     [+0x18..+0x1F]  "LVM2 001"
 *     [+0x20..+0x3F]  pv_id (32 bytes, fuzzed)
 *     [+0x40..+0x47]  device_size_xl (fuzzed)
 *     [+0x48..+0x57]  data disk_locn[0] = {0,0} (terminated)
 *     [+0x58..+0x5F]  meta disk_locn offset = 0
 *     [+0x60..+0x67]  meta disk_locn size   = 512
 *
 * Three CRC layers are recomputed after every mutation:
 *   1. VG config CRC  -> raw_locn[0].Checksum
 *   2. Metadata header CRC -> meta[0..3]
 *   3. Label sector CRC -> label[16..19]
 */

#include "NanaZip.Core.Fuzz.h"
#include <cstring>
#include <cstdint>
#include <algorithm>

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

// ---- Inline CRC-32 (same polynomial / table as 7-Zip's C/7zCrc.c) --------
static std::uint32_t Crc32Table[256];
static bool Crc32Initialized = false;

static void InitCrc32Table()
{
    for (std::uint32_t I = 0; I < 256; ++I)
    {
        std::uint32_t C = I;
        for (int J = 0; J < 8; ++J)
            C = (C >> 1) ^ (C & 1 ? 0xEDB88320u : 0u);
        Crc32Table[I] = C;
    }
    Crc32Initialized = true;
}

static std::uint32_t CrcUpdate(
    std::uint32_t Crc,
    const std::uint8_t* Data,
    std::size_t Length)
{
    if (!Crc32Initialized) InitCrc32Table();
    for (std::size_t I = 0; I < Length; ++I)
        Crc = (Crc >> 8) ^ Crc32Table[(Crc ^ Data[I]) & 0xFF];
    return Crc;
}

static void WriteLE32(std::uint8_t* P, std::uint32_t V)
{
    P[0] = static_cast<std::uint8_t>(V);
    P[1] = static_cast<std::uint8_t>(V >> 8);
    P[2] = static_cast<std::uint8_t>(V >> 16);
    P[3] = static_cast<std::uint8_t>(V >> 24);
}

static void WriteLE64(std::uint8_t* P, std::uint64_t V)
{
    WriteLE32(P, static_cast<std::uint32_t>(V));
    WriteLE32(P + 4, static_cast<std::uint32_t>(V >> 32));
}

static constexpr std::size_t   SectorSize     = 512;
static constexpr std::size_t   MinLvm         = SectorSize * 2;  // 1024
static constexpr std::uint32_t LvmCrcInit     = 0xf597a6cfu;

// Metadata area lives in sector 0. VG config text starts right after the
// raw_locn terminator: offset 0x28 (first raw_locn) + 0x18 (entry) +
// 0x18 (terminator) = 0x58.
static constexpr std::size_t   VgConfigOffset = 0x58;
static constexpr std::size_t   VgConfigSize   = SectorSize - VgConfigOffset;

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* Data, size_t Size, size_t MaxSize, unsigned int /*Seed*/)
{
    size_t NewSize = LLVMFuzzerMutate(Data, Size, MaxSize);

    if (NewSize < MinLvm)
    {
        if (MaxSize < MinLvm)
            return NewSize;
        std::memset(Data + NewSize, 0, MinLvm - NewSize);
        NewSize = MinLvm;
    }

    std::uint8_t* Label = Data + SectorSize;   // sector 1
    std::uint8_t* Meta  = Data;                // sector 0

    // ---- Label sector (sector 1) structural gates ----

    std::memcpy(Label, "LABELONE", 8);              // +0
    WriteLE64(Label + 8, 1);                         // +8  sector_xl
    WriteLE32(Label + 20, 32);                       // +20 offsetToCont
    std::memcpy(Label + 24, "LVM2 001", 8);          // +24

    // Terminate data disk_locn list at label+72.
    std::memset(Label + 72, 0, 16);

    // Metadata disk_locn at label+88: points to sector 0.
    WriteLE64(Label + 88, 0);                        // offset = 0
    WriteLE64(Label + 96, SectorSize);               // size   = 512

    // ---- Metadata area (sector 0) structural gates ----

    // FMTT_MAGIC at meta[4..19]
    static const std::uint8_t FmttMagic[16] = {
        ' ','L','V','M','2',' ','x','[',
        '5','A','%','r','0','N','*','>'
    };
    std::memcpy(Meta + 4, FmttMagic, 16);            // +0x04
    WriteLE32(Meta + 0x14, 1);                        // version = 1
    WriteLE64(Meta + 0x18, 0);                        // Start
    WriteLE64(Meta + 0x20, SectorSize);               // Size

    // raw_locn[0] at meta+0x28: points to VG config text.
    WriteLE64(Meta + 0x28, VgConfigOffset);           // Offset
    WriteLE64(Meta + 0x30, VgConfigSize);             // Size
    // Checksum filled below after VG config CRC is computed.
    WriteLE32(Meta + 0x3C, 0);                        // Flags

    // raw_locn terminator at meta+0x40 (24 zero bytes).
    std::memset(Meta + 0x40, 0, 0x18);

    // ---- Pin VG config text wrapper so the parser always sees exactly
    //      one root item ("VolGroup00 { ... }").  The fuzzer mutates the
    //      interior; without these pins, random byte flips corrupt the
    //      name or braces and cfg.Root.Items.Size() != 1 on every
    //      iteration, blocking all deeper coverage. ----
    static const char VgPrefix[] = "VolGroup00{";
    static constexpr std::size_t PrefixLen = sizeof(VgPrefix) - 1; // 11
    std::memcpy(Meta + VgConfigOffset, VgPrefix, PrefixLen);
    // Closing brace + null at the end of the config region.
    Meta[SectorSize - 2] = '}';
    Meta[SectorSize - 1] = '\0';
    // Forward pass: replace null bytes with spaces (so SetFrom_CalcLen
    // doesn't truncate) and replace any '}' that would close the
    // VolGroup00 block prematurely.
    int Depth = 0;
    for (std::size_t I = VgConfigOffset + PrefixLen;
         I < SectorSize - 2; ++I)
    {
        if (Meta[I] == 0)
            Meta[I] = ' ';
        if (Meta[I] == '{')
            Depth++;
        else if (Meta[I] == '}')
        {
            if (Depth > 0)
                Depth--;
            else
                Meta[I] = ' ';
        }
    }
    // Backward pass: remove unclosed '{' so the pinned '}' at the end
    // closes VolGroup00 rather than a nested block.
    for (std::size_t I = SectorSize - 3;
         Depth > 0 && I >= VgConfigOffset + PrefixLen; --I)
    {
        if (Meta[I] == '{')
        {
            Meta[I] = ' ';
            Depth--;
        }
        if (I == VgConfigOffset + PrefixLen)
            break;
    }

    // ---- Recompute all three CRC layers (inside-out) ----

    // 1. VG config CRC -> raw_locn[0].Checksum
    std::uint32_t VgCrc = CrcUpdate(
        LvmCrcInit, Meta + VgConfigOffset, VgConfigSize);
    WriteLE32(Meta + 0x38, VgCrc);

    // 2. Metadata header CRC -> meta[0..3]
    std::uint32_t MetaCrc = CrcUpdate(
        LvmCrcInit, Meta + 4, SectorSize - 4);
    WriteLE32(Meta, MetaCrc);

    // 3. Label sector CRC -> label[16..19]
    std::uint32_t LabelCrc = CrcUpdate(
        LvmCrcInit, Label + 20, SectorSize - 20);
    WriteLE32(Label + 16, LabelCrc);

    return NewSize;
}

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::Core::RunFuzzCaseByName(L"LVM", Data, Size);
    return 0;
}