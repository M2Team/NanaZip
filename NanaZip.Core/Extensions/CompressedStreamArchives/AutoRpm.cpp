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
    static CompressedStreamArchiveInfo CreateInfo()
    {
        static const CArcInfo* CodecInfos[] = {
            LookupArchiveInfo(&CLSID_RpmHandler),
        };

        return CompressedStreamArchiveInfo{
            LookupArchiveInfo(&CLSID_CpioHandler),
            CodecInfos,
            ARRAYSIZE(CodecInfos),
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
        0,
        0x71,
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

    struct CRegisterArc
    {
        CRegisterArc()
        {
            RegisterArc(&ArcInfo);
        }
    };

    static CRegisterArc g_RegisterArc;
}
