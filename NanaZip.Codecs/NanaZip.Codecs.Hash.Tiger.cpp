/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Tiger.cpp
 * PURPOSE:   Implementation for Tiger hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <tiger.h>

namespace NanaZip::Codecs::Hash
{
    struct Tiger : public Mile::ComObject<Tiger, IHasher>
    {
    private:

        tiger_ctx Context;

    public:

        Tiger()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_tiger_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_tiger_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_tiger_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return tiger_hash_length;
        }
    };

    IHasher* CreateTiger()
    {
        return new Tiger();
    }
}
