/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.WebAssembly.cpp
 * PURPOSE:   Implementation for WebAssembly (WASM) binary file readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

#include <map>

namespace NanaZip::Codecs::Archive
{
    IInArchive* CreateWebAssembly()
    {
        return nullptr;
    }
}
