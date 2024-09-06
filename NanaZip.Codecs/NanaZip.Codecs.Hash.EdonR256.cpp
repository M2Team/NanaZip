/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.EdonR256.cpp
 * PURPOSE:   Implementation for EDON-R 256 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <edonr.h>

namespace NanaZip::Codecs::Hash
{
    struct EdonR256 : public Mile::ComObject<EdonR256, IHasher>
    {
    private:

        edonr_ctx Context;

    public:

        EdonR256()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_edonr256_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_edonr256_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_edonr256_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return edonr256_hash_size;
        }
    };

    IHasher* CreateEdonR256()
    {
        return new EdonR256();
    }
}
