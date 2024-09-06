/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Ripemd160.cpp
 * PURPOSE:   Implementation for RIPEMD-160 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <ripemd-160.h>

namespace NanaZip::Codecs::Hash
{
    struct Ripemd160 : public Mile::ComObject<Ripemd160, IHasher>
    {
    private:

        ripemd160_ctx Context;

    public:

        Ripemd160()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_ripemd160_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_ripemd160_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_ripemd160_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return ripemd160_hash_size;
        }
    };

    IHasher* CreateRipemd160()
    {
        return new Ripemd160();
    }
}
