/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.EdonR224.cpp
 * PURPOSE:   Implementation for EDON-R 224 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <edonr.h>

namespace NanaZip::Codecs::Hash
{
    struct EdonR224 : public Mile::ComObject<EdonR224, IHasher>
    {
    private:

        edonr_ctx Context;

    public:

        EdonR224()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_edonr224_init(
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
            return edonr224_hash_size;
        }
    };

    IHasher* CreateEdonR224()
    {
        return new EdonR224();
    }
}
