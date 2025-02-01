/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.Romfs.cpp
 * PURPOSE:   Implementation for ROMFS file system image readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

namespace
{
    struct PropertyItem
    {
        PROPID Property;
        VARTYPE Type;
    };

    const PropertyItem g_PropertyItems[] =
    {
        { SevenZipArchivePath, VT_BSTR },
        { SevenZipArchiveSize, VT_UI8 },
    };

    const std::size_t g_PropertyItemsCount =
        sizeof(g_PropertyItems) / sizeof(*g_PropertyItems);

    // Reference: https://www.kernel.org/doc/html/v6.12/filesystems/romfs.html
    // Every multi byte value (32 bit words, I’ll use the longwords term from
    // now on) must be in big endian order.

    const char g_RomfsSignature[] = { '-','r','o','m','1','f','s','-' };

    struct RomfsHeader
    {
        // The ASCII representation of those bytes. (i.e. "-rom1fs-")
        char Signature[8];
        // The number of accessible bytes in this fs.
        std::uint32_t FullSize;
        // The checksum of the FIRST 512 BYTES.
        std::uint32_t CheckSum;
        // The zero terminated name of the volume, padded to 16 byte boundary.
        char VolumeName[1];
    };

    enum RomfsFileType
    {
        HardLink,
        Directory,
        RegularFile,
        SymbolicLink,
        BlockDevice,
        CharDevice,
        Socket,
        Fifo,
        Mask = Fifo,
    };

    struct RomfsFileHeader
    {
        // The offset of the next file header. (zero if no more files)
        std::uint32_t NextOffset;
        // Info for directories/hard links/devices.
        std::uint32_t FileType;
        // The size of this file in bytes.
        std::uint32_t Size;
        // Covering the meta data, including the file name, and padding.
        std::uint32_t CheckSum;
        // The zero terminated name of the file, padded to 16 byte boundary.
        char FileName[1];
    };
}

namespace NanaZip::Codecs::Archive
{
    IInArchive* CreateRomfs()
    {
        return nullptr;
    }
}
