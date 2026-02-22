#pragma once

#include "../../SevenZip/CPP/Common/MyWindows.h"

#include "../../SevenZip/CPP/7zip/IDecl.h"

#define NZ_DEFINE_HANDLER_GUID(Name, Id) \
    Z7_DEFINE_GUID(CLSID_ ## Name ## Handler, \
        k_7zip_GUID_Data1, \
        k_7zip_GUID_Data2, \
        k_7zip_GUID_Data3_Common, \
        0x10, 0x00, 0x00, 0x01, 0x10, Id, 0x00, 0x00)

namespace NanaZip::Core::Archive
{
    NZ_DEFINE_HANDLER_GUID(Bzip2, 0x02);
    NZ_DEFINE_HANDLER_GUID(Z, 0x05);
    NZ_DEFINE_HANDLER_GUID(Xz, 0x0C);
    NZ_DEFINE_HANDLER_GUID(Zstd, 0x0E);
    NZ_DEFINE_HANDLER_GUID(Lz4, 0x0F);
    NZ_DEFINE_HANDLER_GUID(Lzma, 0x0A);
    NZ_DEFINE_HANDLER_GUID(Lz5, 0x10);
    NZ_DEFINE_HANDLER_GUID(Lizard, 0x11);
    NZ_DEFINE_HANDLER_GUID(Lzip, 0x12);
    NZ_DEFINE_HANDLER_GUID(Brotli, 0x1F);
    NZ_DEFINE_HANDLER_GUID(CompressedTar, 0x70);
    NZ_DEFINE_HANDLER_GUID(Rpm, 0xEB);
    NZ_DEFINE_HANDLER_GUID(Ar, 0xEC);
    NZ_DEFINE_HANDLER_GUID(Cpio, 0xED);
    NZ_DEFINE_HANDLER_GUID(Tar, 0xEE);
    NZ_DEFINE_HANDLER_GUID(Gzip, 0xEF);
}
