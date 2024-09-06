/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Gost94CryptoPro.cpp
 * PURPOSE:   Implementation for GOST R 34.11-94 CryptoPro hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <gost94.h>

namespace NanaZip::Codecs::Hash
{
    struct Gost94CryptoPro : public Mile::ComObject<Gost94CryptoPro, IHasher>
    {
    private:

        gost94_ctx Context;

    public:

        Gost94CryptoPro()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_gost94_cryptopro_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_gost94_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_gost94_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return gost94_hash_length;
        }
    };

    IHasher* CreateGost94CryptoPro()
    {
        return new Gost94CryptoPro();
    }
}
