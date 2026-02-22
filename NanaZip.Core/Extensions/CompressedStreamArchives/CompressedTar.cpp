/*
 * PROJECT:    NanaZip
 * FILE:       CompressedTar.cpp
 * PURPOSE:    Implementation for compressed tarball support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */

#include "CompressedStreamArchive.hpp"
#include "CoreExports.hpp"

namespace NanaZip::Core::Archive
{
    static CompressedStreamArchiveInfo CreateInfo()
    {
        static const CArcInfo* CodecInfos[] = {
            LookupArchiveInfo(&CLSID_BrotliHandler),
            LookupArchiveInfo(&CLSID_Bzip2Handler),
            LookupArchiveInfo(&CLSID_GzipHandler),
            LookupArchiveInfo(&CLSID_LizardHandler),
            LookupArchiveInfo(&CLSID_Lz4Handler),
            LookupArchiveInfo(&CLSID_Lz5Handler),
            LookupArchiveInfo(&CLSID_LzipHandler),
            LookupArchiveInfo(&CLSID_XzHandler),
            LookupArchiveInfo(&CLSID_ZHandler),
            LookupArchiveInfo(&CLSID_ZstdHandler),
            // Handlers with neither IsArc nor Signature must be at the end.
        };

        return CompressedStreamArchiveInfo{
            LookupArchiveInfo(&CLSID_TarHandler),
            CodecInfos,
            ARRAYSIZE(CodecInfos),
        };
    }

    static IInArchive* CreateArc()
    {
        static CompressedStreamArchiveInfo Info =
            CreateInfo();
        if (!Info.InnerArc)
        {
            return nullptr;
        }
        return new CompressedStreamArchive(&Info);
    }

    static const CArcInfo ArcInfo = {
        NArcInfoFlags::kStartOpen |
        NArcInfoFlags::kSymLinks |
        NArcInfoFlags::kHardLinks |
        NArcInfoFlags::kMTime |
        NArcInfoFlags::kMTime_Default,
        0x70,
        0,
        0,
        nullptr,
        "CompressedTar",
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
            RegisterArc(&ArcInfo);
        }
    };

    static CRegisterArc g_RegisterArc;
}
