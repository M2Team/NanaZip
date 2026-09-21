/*
 * PROJECT:    NanaZip
 * FILE:       AutoDeb.cpp
 * PURPOSE:    Implementation for DEB unpacking support
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
            LookupArchiveInfo(&CLSID_Bzip2Handler),
            LookupArchiveInfo(&CLSID_GzipHandler),
            LookupArchiveInfo(&CLSID_LzmaHandler),
            LookupArchiveInfo(&CLSID_XzHandler),
            LookupArchiveInfo(&CLSID_ZstdHandler),
        };

        return CompressedStreamArchiveInfo{
            LookupArchiveInfo(&CLSID_TarHandler),
            CodecInfos,
            ARRAYSIZE(CodecInfos),
            1,
            0
        };
    }

    static IInArchive* CreateInnerArc()
    {
        static CompressedStreamArchiveInfo Info = CreateInnerInfo();
        if (!Info.InnerArc)
        {
            return nullptr;
        }
        return new CompressedStreamArchive(&Info);
    }

    static const CArcInfo InnerArcInfo = {
        0,
        0x74,
        0,
        0,
        nullptr,
        "CompressedTarDeb",
        nullptr,
        nullptr,
        0,
        CreateInnerArc,
        0,
        nullptr,
    };

    static CompressedStreamArchiveInfo CreateInfo()
    {
        static const CArcInfo* CodecInfos[] = {
            LookupArchiveInfo(&CLSID_ArHandler),
        };

        return CompressedStreamArchiveInfo{
            &InnerArcInfo,
            CodecInfos,
            ARRAYSIZE(CodecInfos),
            2,
            1
        };
    }

    static IInArchive* CreateArc()
    {
        static CompressedStreamArchiveInfo Info = CreateInfo();
        if (!Info.InnerArc)
        {
            return nullptr;
        }
        return new CompressedStreamArchive(&Info);
    }

    static const CArcInfo ArcInfo = {
        NArcInfoFlags::kPureStartOpen |
        NArcInfoFlags::kCompositeArc,
        0x75,
        0,
        0,
        nullptr,
        "AutoDeb",
        "deb",
        nullptr,
        0,
        CreateArc,
        0,
        nullptr,
    };

    static CompressedStreamArchiveInfo CreateUncompressedInfo()
    {
        static const CArcInfo* CodecInfos[] = {
            LookupArchiveInfo(&CLSID_ArHandler),
        };

        return CompressedStreamArchiveInfo{
            LookupArchiveInfo(&CLSID_TarHandler),
            CodecInfos,
            ARRAYSIZE(CodecInfos),
            2,
            1
        };
    }

    static IInArchive* CreateUncompressedArc()
    {
        static CompressedStreamArchiveInfo Info = CreateUncompressedInfo();
        if (!Info.InnerArc)
        {
            return nullptr;
        }
        return new CompressedStreamArchive(&Info);
    }

    static const CArcInfo UncompressedArcInfo = {
        NArcInfoFlags::kPureStartOpen |
        NArcInfoFlags::kCompositeArc,
        0x76,
        0,
        0,
        nullptr,
        "AutoDebUncompressed",
        "deb",
        nullptr,
        0,
        CreateUncompressedArc,
        0,
        nullptr,
    };

    struct CRegisterArc
    {
        CRegisterArc()
        {
            RegisterArc(&ArcInfo);
            RegisterArc(&UncompressedArcInfo);
        }
    };

    static CRegisterArc g_RegisterArc;
}
