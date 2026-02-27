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

using namespace NanaZip::Core::Archive;

namespace
{
    static CompressedStreamArchiveInfo CreateInnerInfo()
    {
        static const CArcInfo* CodecInfos[] = {
            LookupArchiveInfo(&CLSID_BrotliHandler),
            LookupArchiveInfo(&CLSID_Bzip2Handler),
            LookupArchiveInfo(&CLSID_GzipHandler),
            LookupArchiveInfo(&CLSID_LizardHandler),
            LookupArchiveInfo(&CLSID_Lz4Handler),
            LookupArchiveInfo(&CLSID_Lz5Handler),
            LookupArchiveInfo(&CLSID_LzipHandler),
            LookupArchiveInfo(&CLSID_LzmaHandler),
            LookupArchiveInfo(&CLSID_XzHandler),
            LookupArchiveInfo(&CLSID_ZHandler),
            LookupArchiveInfo(&CLSID_ZstdHandler),
            // Handlers with neither IsArc nor Signature must be at the end.
        };

        return CompressedStreamArchiveInfo{
            LookupArchiveInfo(&CLSID_TarHandler),
            CodecInfos,
            ARRAYSIZE(CodecInfos),
            1,
            0
        };
    }

    static IInArchive* CreateArc()
    {
        static CompressedStreamArchiveInfo Info =
            CreateInnerInfo();
        if (!Info.InnerArc)
        {
            return nullptr;
        }
        return new CompressedStreamArchive(&Info);
    }

    static const CArcInfo ArcInfo = {
        NArcInfoFlags::kPureStartOpen |
        NArcInfoFlags::kSymLinks |
        NArcInfoFlags::kHardLinks |
        NArcInfoFlags::kMTime |
        NArcInfoFlags::kMTime_Default |
        NArcInfoFlags::kCompositeArc |
        NArcInfoFlags::kLongExtension,
        0x70,
        0,
        0,
        nullptr,
        "CompressedTar",
        "tar.br tar.brotli tbr "
            "tar.bz2 tar.bzip2 tbz2 tbz "
            "tar.gz tar.gzip tgz tpz "
            "tar.liz tliz "
            "tar.lz4 tlz4 "
            "tar.lz5 tlz5 "
            "tar.lz tlz "
            "tar.xz txz "
            "tar.z taz "
            "tar.zst tar.zstd tzst tzstd",
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
