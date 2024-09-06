/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Snefru128.cpp
 * PURPOSE:   Implementation for Snefru-128 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <snefru.h>

namespace NanaZip::Codecs::Hash
{
    struct Snefru128 : public Mile::ComObject<Snefru128, IHasher>
    {
    private:

        snefru_ctx Context;

    public:

        Snefru128()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_snefru128_init(
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
            return snefru128_hash_length;
        }
    };

    IHasher* CreateSnefru128()
    {
        return new Snefru128();
    }
}
