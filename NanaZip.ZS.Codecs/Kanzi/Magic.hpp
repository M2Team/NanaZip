/*
Copyright 2011-2026 Frederic Langlet
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
you may obtain a copy of the License at

                http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#ifndef knz_Magic
#define knz_Magic

#include "Memory.hpp"


namespace kanzi
{
   struct Magic {
       static const uint NO_MAGIC = 0;
       static const uint JPG_MAGIC = 0xFFD8FFE0;
       static const uint GIF_MAGIC = 0x47494638;
       static const uint PDF_MAGIC = 0x25504446;
       static const uint ZIP_MAGIC = 0x504B0304; // Works for jar & office docs
       static const uint LZMA_MAGIC = 0x377ABCAF; // Works for 7z  37 7A BC AF 27 1C
       static const uint RAR_MAGIC = 0x52617221; // 52 61 72 21 1A 07 00
       static const uint PNG_MAGIC = 0x89504E47;
       static const uint ELF_MAGIC = 0x7F454C46;
       static const uint MAC_MAGIC32 = 0xFEEDFACE;
       static const uint MAC_CIGAM32 = 0xCEFAEDFE;
       static const uint MAC_MAGIC64 = 0xFEEDFACF;
       static const uint MAC_CIGAM64 = 0xCFFAEDFE;
       static const uint ZSTD_MAGIC = 0x28B52FFD;
       static const uint BROTLI_MAGIC = 0x81CFB2CE;
       static const uint RIFF_MAGIC = 0x52494646; // WAV, AVI, WEBP
       static const uint CAB_MAGIC = 0x4D534346;
       static const uint FLAC_MAGIC = 0x664C6143;
       static const uint XZ_MAGIC = 0xFD377A58; // FD 37 7A 58 5A 00
       static const uint KNZ_MAGIC = 0x4B414E5A;

       static const uint BZIP2_MAGIC = 0x425A68;
       static const uint MP3_ID3_MAGIC = 0x494433;

       static const uint GZIP_MAGIC = 0x1F8B;
       static const uint BMP_MAGIC = 0x424D;
       static const uint WIN_MAGIC = 0x4D5A;
       static const uint PBM_MAGIC = 0x5034; // bin only
       static const uint PGM_MAGIC = 0x5035; // bin only
       static const uint PPM_MAGIC = 0x5036; // bin only

       static uint getType(const byte src[]);

       static bool isCompressed(uint magic);

       static bool isMultimedia(uint magic);

       static bool isExecutable(uint magic);
    };


    // 4 bytes must be readable in src
    inline uint Magic::getType(const byte src[])
    {
        static const uint KEYS32[18] = {
            GIF_MAGIC, PDF_MAGIC, ZIP_MAGIC, LZMA_MAGIC, PNG_MAGIC,
            ELF_MAGIC, MAC_MAGIC32, MAC_CIGAM32, MAC_MAGIC64, MAC_CIGAM64,
            ZSTD_MAGIC, BROTLI_MAGIC, CAB_MAGIC, RIFF_MAGIC, FLAC_MAGIC,
            XZ_MAGIC, KNZ_MAGIC, RAR_MAGIC
        };

        static const uint KEYS16[3] = {
            GZIP_MAGIC, BMP_MAGIC, WIN_MAGIC
        };

        const uint key = uint(BigEndian::readInt32(&src[0]));

        if ((key & ~0x0F) == JPG_MAGIC)
            return key;

        if (((key >> 8) == BZIP2_MAGIC)  || ((key >> 8) == MP3_ID3_MAGIC))
            return key >> 8;

        const int n = sizeof(KEYS32) / sizeof(uint);

        for (int i = 0; i < n; i++) {
            if (key == KEYS32[i])
               return key;
        }

        const uint key16 = key >> 16;

        for (int i = 0; i < 3; i++) {
            if (key16 == KEYS16[i])
                return key16;
        }

        if ((key16 == PBM_MAGIC) || (key16 == PGM_MAGIC) || (key16 == PPM_MAGIC)) {
            const uint subkey = (key >> 8) & 0xFF;

            if ((subkey == 0x07) || (subkey == 0x0A) || (subkey == 0x0D) || (subkey == 0x20))
                return key16;
        }

        return NO_MAGIC;
    }


    inline bool Magic::isCompressed(uint magic) {
        switch (magic) {
            case JPG_MAGIC:
            case GIF_MAGIC:
            case PNG_MAGIC:
            //case RIFF_MAGIC: may or may not be
            case LZMA_MAGIC:
            case ZSTD_MAGIC:
            case BROTLI_MAGIC:
            case CAB_MAGIC:
            case ZIP_MAGIC:
            case GZIP_MAGIC:
            case BZIP2_MAGIC:
            case FLAC_MAGIC:
            case MP3_ID3_MAGIC:
            case XZ_MAGIC:
            case KNZ_MAGIC:
            case RAR_MAGIC:
                return true;
				
            default:
                return false;
        }
    }

    inline bool Magic::isMultimedia(uint magic) {
        switch (magic) {
            case JPG_MAGIC:
            case GIF_MAGIC:
            case PNG_MAGIC:
            case RIFF_MAGIC:
            case FLAC_MAGIC:
            case MP3_ID3_MAGIC:
            case BMP_MAGIC:
            case PBM_MAGIC:
            case PGM_MAGIC:
            case PPM_MAGIC:
                return true;
				
            default:
                return false;
        }
    }

    inline bool Magic::isExecutable(uint magic) {
        switch (magic) {
            case ELF_MAGIC:
            case WIN_MAGIC:
            case MAC_MAGIC32:
            case MAC_CIGAM32:
            case MAC_MAGIC64:
            case MAC_CIGAM64:
                return true;
				
            default:
                return false;
        }
    }

}

#endif

