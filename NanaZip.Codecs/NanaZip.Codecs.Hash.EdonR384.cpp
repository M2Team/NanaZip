/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.EdonR384.cpp
 * PURPOSE:   Implementation for EDON-R 384 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <edonr.h>

namespace NanaZip::Codecs::Hash
{
    struct EdonR384 : public Mile::ComObject<EdonR384, IHasher>
    {
    private:

        edonr_ctx Context;

    public:

        EdonR384()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_edonr384_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_edonr512_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_edonr512_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return edonr384_hash_size;
        }
    };

    IHasher* CreateEdonR384()
    {
        return new EdonR384();
    }
}
