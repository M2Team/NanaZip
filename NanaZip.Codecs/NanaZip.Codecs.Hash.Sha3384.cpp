/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Sha3384.cpp
 * PURPOSE:   Implementation for SHA3-384 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <sha3.h>

namespace NanaZip::Codecs::Hash
{
    struct Sha3384 : public Mile::ComObject<Sha3384, IHasher>
    {
    private:

        sha3_ctx Context;

    public:

        Sha3384()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_sha3_384_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_sha3_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_sha3_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return sha3_384_hash_size;
        }
    };

    IHasher* CreateSha3384()
    {
        return new Sha3384();
    }
}
