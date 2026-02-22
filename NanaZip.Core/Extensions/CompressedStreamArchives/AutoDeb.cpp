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
            LookupArchiveInfo(&CLSID_ArHandler),
        };

        return CompressedStreamArchiveInfo{
            LookupArchiveInfo(&CLSID_CompressedTarHandler),
            CodecInfos,
            ARRAYSIZE(CodecInfos),
            2,
            1
        };
    }

    static IInArchive* CreateArc()
    {
        static CompressedStreamArchiveInfo Info = CreateInnerInfo();
        if (!Info.InnerArc)
        {
            return nullptr;
        }
        return new CompressedStreamArchive(&Info);
    }

    static const CArcInfo ArcInfo = {
        NArcInfoFlags::kPureStartOpen |
        NArcInfoFlags::kCompositeArc,
        0x74,
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

    struct CRegisterArc
    {
        CRegisterArc()
        {
            RegisterArc(&ArcInfo);
        }
    };

    static CRegisterArc g_RegisterArc;
}
