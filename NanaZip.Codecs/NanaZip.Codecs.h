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

/**
 * Definition of NanaZip interface constants
 * 
 * I had mentioned one of the reasons that I call this project NanaZip because
 * Nana is the romaji of なな which means seven in Japanese. But I had not
 * mentioned the way I confirm that: I had recalled one of the Japanese VTubers
 * called Kagura Nana when I waiting my elder sister for dinner at Taiyanggong
 * subway station, Beijing. For playing more puns, NanaZip uses the K7 prefix in
 * some definitions. (K -> Kagura, 7 -> Nana)
 */

namespace NanaZip::Codecs
{
    /**
     * @brief The ID base value of NanaZip Hash Provider.
     * @remark 0x4823374B is big-endian representation of K7#H (H -> Hash)
     */
    const UINT64 HashProviderIdBase = 0x4823374B00000000;
}

namespace NanaZip::Codecs::Hash
{
    IHasher* CreateBlake3();
    IHasher* CreateSm3();
}

#endif // !NANAZIP_CODECS
