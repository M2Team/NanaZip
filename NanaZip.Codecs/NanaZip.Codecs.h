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

#include <Mile.Helpers.CppBase.h>

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
     * @brief The ID base value of NanaZip Hash Providers.
     * @remark 0x4823374B is little-endian representation of K7#H (H -> Hash)
     */
    const UINT64 HashProviderIdBase = 0x4823374B00000000;

    /**
     * @brief The ID base value of NanaZip Decoder Providers.
     * @remark 0x4423374B is little-endian representation of K7#D (D -> Decoder)
     */
    const UINT64 DecoderProviderIdBase = 0x4423374B00000000;

    /**
     * @brief The ID base value of NanaZip Encoder Providers.
     * @remark 0x4523374B is little-endian representation of K7#E (E -> Encoder)
     */
    const UINT64 EncoderProviderIdBase = 0x4523374B00000000;

    /**
     * @brief The ID base value of NanaZip Archiver Providers.
     * @remark 0x4523374B is little-endian representation of K7#A (A -> Archiver)
     */
    const UINT64 ArchiverProviderIdBase = 0x4123374B00000000;
}

namespace NanaZip::Codecs::Hash
{
    IHasher* CreateMd2();
    IHasher* CreateMd4();
    IHasher* CreateMd5();
    IHasher* CreateSha1();
    IHasher* CreateSha256();
    IHasher* CreateSha384();
    IHasher* CreateSha512();
    IHasher* CreateBlake3();
    IHasher* CreateSm3();
    IHasher* CreateAich();
    IHasher* CreateBlake2b();
    IHasher* CreateEd2k();
    IHasher* CreateEdonR224();
    IHasher* CreateEdonR256();
    IHasher* CreateEdonR384();
    IHasher* CreateEdonR512();
    IHasher* CreateGost94();
    IHasher* CreateGost94CryptoPro();
    IHasher* CreateGost12256();
    IHasher* CreateGost12512();
    IHasher* CreateHas160();
    IHasher* CreateRipemd160();
    IHasher* CreateSha224();
    IHasher* CreateSha3224();
    IHasher* CreateSha3256();
    IHasher* CreateSha3384();
    IHasher* CreateSha3512();
    IHasher* CreateSnefru128();
    IHasher* CreateSnefru256();
    IHasher* CreateTiger();
    IHasher* CreateTiger2();
    IHasher* CreateTorrent();
    IHasher* CreateTth();
    IHasher* CreateWhirlpool();
    IHasher* CreateXxh32();
    IHasher* CreateXxh64();
    IHasher* CreateXxh364();
    IHasher* CreateXxh3128();
}

namespace NanaZip::Codecs::Decoder
{

}

namespace NanaZip::Codecs::Encoder
{

}

namespace NanaZip::Codecs::Archive
{
    IInArchive* CreateUfs();
    IInArchive* CreateDotNetSingleFile();
    IInArchive* CreateElectronAsar();
    IInArchive* CreateRomfs();
    IInArchive* CreateZealfs();
    IInArchive* CreateWebAssembly();
    IInArchive* CreateLittlefs();
}

#endif // !NANAZIP_CODECS
