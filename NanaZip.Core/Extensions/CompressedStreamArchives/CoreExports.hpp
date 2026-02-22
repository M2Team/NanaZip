#pragma once

#include "../../SevenZip/CPP/7zip/Archive/IArchive.h"

#include "../../SevenZip/CPP/7zip/Archive/Tar/TarIn.h"
#include "../../SevenZip/CPP/7zip/Archive/Tar/TarHandler.h"

namespace NArchive
{
    namespace NBROTLI
    {
        EXTERN_C UInt32 WINAPI IsArc_Brotli(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NBz2
    {
        EXTERN_C UInt32 WINAPI IsArc_BZip2(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NGz
    {
        EXTERN_C UInt32 WINAPI IsArc_Gz(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NLIZARD
    {
        EXTERN_C UInt32 WINAPI IsArc_lizard(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NLZ4
    {
        EXTERN_C UInt32 WINAPI IsArc_lz4(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NLZ5
    {
        EXTERN_C UInt32 WINAPI IsArc_lz5(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NLz
    {
        EXTERN_C UInt32 WINAPI IsArc_Lz(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NXz
    {
        IInArchive* CreateArcExported();
    }

    namespace NZ
    {
        EXTERN_C UInt32 WINAPI IsArc_Z(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NZSTD
    {
        EXTERN_C UInt32 WINAPI IsArc_zstd(const Byte* p, size_t size);
        IInArchive* CreateArcExported();
    }

    namespace NTar
    {
        IInArchive* CreateArcExported();
    }
}
