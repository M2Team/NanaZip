/*
 * PROJECT:    NanaZip
 * FILE:       CompressedTar.cpp
 * PURPOSE:    Implementation for compressed tarball support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */

#include "CoreExports.hpp"
#include "CompressedStreamArchive.hpp"

#include "../../SevenZip/CPP/7zip/Common/RegisterArc.h"

namespace NanaZip::Core::Archive
{
    static const Byte XzSignature[] = { 0xFD, '7', 'z', 'X', 'Z', 0x00 };

    // All handler names below must be synced with 7-Zip and 7-Zip ZS.
    static const char* InnerHandlerName = "tar";
    static const CodecInfo CodecInfos[] = {
        CodecInfo {
            "brotli",
            NArchive::NBROTLI::CreateArcExported,
            NArchive::NBROTLI::IsArc_Brotli,
            nullptr,
            0
        },
        CodecInfo {
            "bzip2",
            NArchive::NBz2::CreateArcExported,
            NArchive::NBz2::IsArc_BZip2,
            nullptr,
            0
        },
        CodecInfo {
            "gzip",
            NArchive::NGz::CreateArcExported,
            NArchive::NGz::IsArc_Gz,
            nullptr,
            0
        },
        CodecInfo {
            "lizard",
            NArchive::NLIZARD::CreateArcExported,
            NArchive::NLIZARD::IsArc_lizard,
            nullptr,
            0
        },
        CodecInfo {
            "lz4",
            NArchive::NLZ4::CreateArcExported,
            NArchive::NLZ4::IsArc_lz4,
            nullptr,
            0
        },
        CodecInfo {
            "lz5",
            NArchive::NLZ5::CreateArcExported,
            NArchive::NLZ5::IsArc_lz5,
            nullptr,
            0
        },
        CodecInfo {
            "lzip",
            NArchive::NLz::CreateArcExported,
            NArchive::NLz::IsArc_Lz,
            nullptr,
            0
        },
        CodecInfo {
            "xz",
            NArchive::NXz::CreateArcExported,
            nullptr,
            XzSignature,
            sizeof(XzSignature)
        },
        CodecInfo {
            "Z",
            NArchive::NZ::CreateArcExported,
            NArchive::NZ::IsArc_Z,
            nullptr,
            0
        },
        CodecInfo {
            "zstd",
            NArchive::NZSTD::CreateArcExported,
            NArchive::NZSTD::IsArc_zstd,
            nullptr,
            0
        },
        // Handlers with neither IsArc nor Signature must be at the end.
    };

    static const CompressedStreamArchiveInfo CompressedTarInfo = {
        InnerHandlerName, // InnerHandlerName
        NArchive::NTar::CreateArcExported, // CreateArcExported
        CodecInfos, // CodecInfos
        ARRAYSIZE(CodecInfos), // CodecInfoCount
    };

    static IInArchive* CreateArc()
    {
        return new CompressedStreamArchive(&CompressedTarInfo);
    }

    extern const CArcInfo CompressedTarArcInfo = {
        NArcInfoFlags::kStartOpen |
        NArcInfoFlags::kSymLinks |
        NArcInfoFlags::kHardLinks |
        NArcInfoFlags::kMTime |
        NArcInfoFlags::kMTime_Default,
        0xA,
        0,
        0,
        nullptr,
        "CompressedStreamArchive",
        "br brotli tbr "
            "bz2 bzip2 tbz2 tbz "
            "gz gzip tgz tpz apk "
            "liz tliz "
            "lz4 tlz4 "
            "lz5 tlz5 "
            "lz tlz "
            "xz txz "
            "z taz "
            "zst zstd tzst tzstd",
        nullptr,
        ((UInt32)1 << (
            NArcInfoTimeFlags::kTime_Prec_Mask_bit_index +
            (NFileTimeType::kWindows))) |
        ((UInt32)1 << (
            NArcInfoTimeFlags::kTime_Prec_Mask_bit_index +
            (NFileTimeType::kUnix))) |
        ((UInt32)1 << (
            NArcInfoTimeFlags::kTime_Prec_Mask_bit_index +
            (NFileTimeType::k1ns))) |
        ((UInt32)(NFileTimeType::kUnix) <<
            NArcInfoTimeFlags::kTime_Prec_Default_bit_index),
        CreateArc,
        0,
        nullptr,
    };

    struct CRegisterArc
    {
        CRegisterArc()
        {
            RegisterArc(&CompressedTarArcInfo);
        }
    };

    static CRegisterArc g_RegisterArc;
}
