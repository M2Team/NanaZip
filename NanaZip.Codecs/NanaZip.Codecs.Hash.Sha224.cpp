/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Sha224.cpp
 * PURPOSE:   Implementation for SHA-224 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <sha256.h>

namespace NanaZip::Codecs::Hash
{
    struct Sha224 : public Mile::ComObject<Sha224, IHasher>
    {
    private:

        sha256_ctx Context;

    public:

        Sha224()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_sha224_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_sha256_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_sha256_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return sha224_hash_size;
        }
    };

    IHasher* CreateSha224()
    {
        return new Sha224();
    }
}
