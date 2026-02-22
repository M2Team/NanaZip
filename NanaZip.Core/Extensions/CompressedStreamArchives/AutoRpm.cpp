/*
 * PROJECT:    NanaZip
 * FILE:       AutoRpm.cpp
 * PURPOSE:    Implementation for RPM unpacking support
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
            LookupArchiveInfo(&CLSID_CpioHandler),
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
        0x71,
        0,
        0,
        nullptr,
        "CompressedCpioRpm",
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
            LookupArchiveInfo(&CLSID_RpmHandler),
        };

        return CompressedStreamArchiveInfo{
            &InnerArcInfo,
            CodecInfos,
            ARRAYSIZE(CodecInfos),
            1,
            0
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
        0x72,
        0,
        0,
        nullptr,
        "AutoRpm",
        "rpm",
        nullptr,
        0,
        CreateArc,
        0,
        nullptr,
    };

    static CompressedStreamArchiveInfo CreateUncompressedInfo()
    {
        static const CArcInfo* CodecInfos[] = {
            LookupArchiveInfo(&CLSID_RpmHandler),
        };

        return CompressedStreamArchiveInfo{
            LookupArchiveInfo(&CLSID_CpioHandler),
            CodecInfos,
            ARRAYSIZE(CodecInfos),
            1,
            0
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
        0x73,
        0,
        0,
        nullptr,
        "AutoRpmUncompressed",
        "rpm",
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
