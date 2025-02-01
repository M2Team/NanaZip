/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.ElectronAsar.cpp
 * PURPOSE:   Implementation for Electron Archive (asar) readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

namespace NanaZip::Codecs::Archive
{
    IInArchive* CreateElectronAsar()
    {
        return nullptr;
    }
}
