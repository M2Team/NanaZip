/*
 * PROJECT:    NanaZip
 * FILE:       Fuzz.Ufs.cpp
 * PURPOSE:    libFuzzer harness for the NanaZip UFS/UFS2 handler
 *
 * LICENSE:    The MIT License
 *
 * The UFS handler's Open() checks these superblock fields (all must pass or
 * the handler immediately returns S_FALSE and the entire iteration is wasted):
 *
 *   fs_magic        (+1372, int32)  == 0x011954 (UFS1) or 0x19540119 (UFS2)
 *   fs_sblockloc    (+1000, int64)  == 8192 (UFS1) or 65536 (UFS2)
 *   fs_frag         (+56,   int32)  >= 1
 *   fs_ncg          (+44,   uint32) >= 1
 *   fs_bsize        (+48,   int32)  >= 4096 (MINBSIZE)
 *   fs_sbsize       (+104,  int32)  in [sizeof(fs)=1376, SBLOCKSIZE=8192]
 *   fs_fsize        (+52,   int32)  -- must equal fs_bsize / fs_frag
 *
 * After Open, GetAllPaths -> GetInodeInformation reads the root inode's
 * di_size (attacker-controlled uint64) and loops adding BlockSize per
 * iteration until ActualFileSize >= di_size. With di_size=2^64-1 and
 * BlockSize=4096, that's 4.5e15 vector pushes -> instant OOM/hang.
 *
 * The custom mutator fixes the superblock gate fields and caps the root
 * inode's di_size so every mutation reaches the parser's deep logic.
 */

#include "NanaZip.Codecs.Fuzz.h"
#include <cstring>
#include <cstdint>

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

static void WriteLE32(std::uint8_t* P, std::int32_t V)
{
    auto U = static_cast<std::uint32_t>(V);
    P[0] = static_cast<std::uint8_t>(U);
    P[1] = static_cast<std::uint8_t>(U >> 8);
    P[2] = static_cast<std::uint8_t>(U >> 16);
    P[3] = static_cast<std::uint8_t>(U >> 24);
}

static void WriteLE64(std::uint8_t* P, std::int64_t V)
{
    auto U = static_cast<std::uint64_t>(V);
    WriteLE32(P, static_cast<std::int32_t>(U));
    WriteLE32(P + 4, static_cast<std::int32_t>(U >> 32));
}

static std::uint64_t ReadLE64(const std::uint8_t* P)
{
    return static_cast<std::uint64_t>(P[0])
        | (static_cast<std::uint64_t>(P[1]) << 8)
        | (static_cast<std::uint64_t>(P[2]) << 16)
        | (static_cast<std::uint64_t>(P[3]) << 24)
        | (static_cast<std::uint64_t>(P[4]) << 32)
        | (static_cast<std::uint64_t>(P[5]) << 40)
        | (static_cast<std::uint64_t>(P[6]) << 48)
        | (static_cast<std::uint64_t>(P[7]) << 56);
}

// Superblock field offsets (relative to start of struct fs, sizeof==1376)
static constexpr int FsIblknoOffset      = 16;
static constexpr int FsNcgOffset         = 44;
static constexpr int FsBsizeOffset       = 48;
static constexpr int FsFsizeOffset       = 52;
static constexpr int FsFragOffset        = 56;
static constexpr int FsSbsizeOffset      = 104;
static constexpr int FsIpgOffset         = 184;
static constexpr int FsFpgOffset         = 188;
static constexpr int FsSblocklocOffset   = 1000;
static constexpr int FsMaxsymlinklenOffset = 1320;
static constexpr int FsMagicOffset       = 1372;

// UFS constants
static constexpr std::int32_t  FsUfs1Magic   = 0x011954;
static constexpr std::int32_t  FsUfs2Magic   = 0x19540119;
static constexpr std::size_t   SuperBlockUfs1 = 8192;
static constexpr std::size_t   SuperBlockUfs2 = 65536;
static constexpr std::int32_t  SuperBlockSize = 8192;
static constexpr std::int32_t  BlockSize      = 4096;
static constexpr std::int32_t  FsStructSize   = 1376;
static constexpr std::uint32_t UfsRootInode   = 2;

// Inode sizes
static constexpr std::size_t Ufs1DinodeSize = 128;
static constexpr std::size_t Ufs2DinodeSize = 256;

// di_size offsets within dinode structs
static constexpr int Ufs1DiSizeOffset = 8;   // uint64 at offset 8
static constexpr int Ufs2DiSizeOffset = 16;  // uint64 at offset 16

// Maximum di_size we allow (1 MiB) -- prevents OOM in GetInodeInformation
static constexpr std::uint64_t MaxDiSize = 1048576;

// Minimum image size: UFS1 superblock at 8192, needs at least sb + 1376 bytes
static constexpr std::size_t MinUfs1 = SuperBlockUfs1 + FsStructSize;  // 9568
// UFS2 at 65536
static constexpr std::size_t MinUfs2 = SuperBlockUfs2 + FsStructSize;  // 66912

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* Data, size_t Size, size_t MaxSize, unsigned int Seed)
{
    size_t NewSize = LLVMFuzzerMutate(Data, Size, MaxSize);

    // Decide UFS1 vs UFS2 based on seed parity -- gives both formats coverage
    bool IsUfs2 = (Seed & 1) != 0;
    std::size_t SbOffset = IsUfs2 ? SuperBlockUfs2 : SuperBlockUfs1;
    std::size_t MinSize = IsUfs2 ? MinUfs2 : MinUfs1;

    if (NewSize < MinSize)
    {
        if (MaxSize < MinSize)
            return NewSize;
        std::memset(Data + NewSize, 0, MinSize - NewSize);
        NewSize = MinSize;
    }

    std::uint8_t* Sb = Data + SbOffset;

    // --- Fix superblock gate fields ---
    WriteLE32(Sb + FsMagicOffset, IsUfs2 ? FsUfs2Magic : FsUfs1Magic);
    WriteLE64(Sb + FsSblocklocOffset, static_cast<std::int64_t>(SbOffset));
    WriteLE32(Sb + FsBsizeOffset, BlockSize);
    WriteLE32(Sb + FsFsizeOffset, BlockSize);
    WriteLE32(Sb + FsFragOffset, 1);
    WriteLE32(Sb + FsNcgOffset, 1);
    WriteLE32(Sb + FsSbsizeOffset, SuperBlockSize);
    WriteLE32(Sb + FsIblknoOffset, 4);
    WriteLE32(Sb + FsIpgOffset, 16);
    WriteLE32(Sb + FsFpgOffset, 100);
    WriteLE32(Sb + FsMaxsymlinklenOffset, 60);

    // --- Cap di_size for ALL inodes in the inode block to prevent OOM ---
    // iblkno(4) * fsize(4096) = 16384 is the inode block start.
    // fs_ipg = 16, so inodes 0..15 are in this block.
    // Cap each inode's di_size to MaxDiSize to prevent the
    // GetInodeInformation loop from doing O(di_size/BlockSize) pushes.
    std::size_t InodeSize = IsUfs2 ? Ufs2DinodeSize : Ufs1DinodeSize;
    int DiSizeFieldOffset = IsUfs2 ? Ufs2DiSizeOffset : Ufs1DiSizeOffset;
    std::size_t InodeBlockOffset = 4 * BlockSize;  // iblkno * fsize

    for (unsigned Ino = 0; Ino < 16; ++Ino)
    {
        std::size_t Offset = InodeBlockOffset + Ino * InodeSize + DiSizeFieldOffset;
        if (Offset + 8 > NewSize)
            break;
        std::uint64_t DiSize = ReadLE64(Data + Offset);
        if (DiSize > MaxDiSize)
        {
            DiSize = (DiSize % MaxDiSize) + 1;
            WriteLE64(Data + Offset, static_cast<std::int64_t>(DiSize));
        }
    }

    return NewSize;
}

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* Data,
    std::size_t Size)
{
    NanaZip::Fuzz::RunFuzzCase(0, Data, Size);
    return 0;
}