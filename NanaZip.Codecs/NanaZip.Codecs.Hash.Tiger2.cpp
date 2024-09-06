/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Tiger2.cpp
 * PURPOSE:   Implementation for Tiger2 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <tiger.h>

namespace NanaZip::Codecs::Hash
{
    struct Tiger2 : public Mile::ComObject<Tiger2, IHasher>
    {
    private:

        tiger_ctx Context;

    public:

        Tiger2()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_tiger2_init(
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

    IHasher* CreateTiger2()
    {
        return new Tiger2();
    }
}
