/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Gost12512.cpp
 * PURPOSE:   Implementation for GOST R 34.11-2012 512 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <gost12.h>

namespace NanaZip::Codecs::Hash
{
    struct Gost12512 : public Mile::ComObject<Gost12512, IHasher>
    {
    private:

        gost12_ctx Context;

    public:

        Gost12512()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_gost12_512_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_gost12_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_gost12_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return gost12_512_hash_size;
        }
    };

    IHasher* CreateGost12512()
    {
        return new Gost12512();
    }
}
