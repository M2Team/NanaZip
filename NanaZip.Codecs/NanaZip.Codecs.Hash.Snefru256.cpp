/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Snefru256.cpp
 * PURPOSE:   Implementation for Snefru-256 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <snefru.h>

namespace NanaZip::Codecs::Hash
{
    struct Snefru256 : public Mile::ComObject<Snefru256, IHasher>
    {
    private:

        snefru_ctx Context;

    public:

        Snefru256()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_snefru256_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_snefru_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_snefru_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return snefru256_hash_length;
        }
    };

    IHasher* CreateSnefru256()
    {
        return new Snefru256();
    }
}
