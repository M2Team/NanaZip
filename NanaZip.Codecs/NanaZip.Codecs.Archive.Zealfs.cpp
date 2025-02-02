/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.Zealfs.cpp
 * PURPOSE:   Implementation for ZealFS file system image readonly support
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
    IInArchive* CreateZealfs()
    {
        return nullptr;
    }
}
