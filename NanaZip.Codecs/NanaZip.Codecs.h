/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.h
 * PURPOSE:   Definition for NanaZip.Codecs
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_CODECS
#define NANAZIP_CODECS

#include <Windows.h>

#include <NanaZip.Specification.SevenZip.h>

namespace NanaZip::Codecs::Hash
{
    IHasher* CreateBlake3();
}

#endif // !NANAZIP_CODECS
