/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Gost94.cpp
 * PURPOSE:   Implementation for GOST R 34.11-94 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <gost94.h>

namespace NanaZip::Codecs::Hash
{
    struct Gost94 : public Mile::ComObject<Gost94, IHasher>
    {
    private:

        gost94_ctx Context;

    public:

        Gost94()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_gost94_init(
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

    IHasher* CreateGost94()
    {
        return new Gost94();
    }
}
