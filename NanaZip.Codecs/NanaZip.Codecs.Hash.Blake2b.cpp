/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Blake2b.cpp
 * PURPOSE:   Implementation for BLAKE2b hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <blake2b.h>

namespace NanaZip::Codecs::Hash
{
    struct Blake2b : public Mile::ComObject<Blake2b, IHasher>
    {
    private:

        blake2b_ctx Context;

    public:

        Blake2b()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_blake2b_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_blake2b_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_blake2b_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return blake2b_hash_size;
        }
    };

    IHasher* CreateBlake2b()
    {
        return new Blake2b();
    }
}
