/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Has160.cpp
 * PURPOSE:   Implementation for HAS-160 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <has160.h>

namespace NanaZip::Codecs::Hash
{
    struct Has160 : public Mile::ComObject<Has160, IHasher>
    {
    private:

        has160_ctx Context;

    public:

        Has160()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_has160_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_has160_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_has160_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return has160_hash_size;
        }
    };

    IHasher* CreateHas160()
    {
        return new Has160();
    }
}
