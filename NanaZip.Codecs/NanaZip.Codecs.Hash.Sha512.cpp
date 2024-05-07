/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Sha512.cpp
 * PURPOSE:   Implementation for SHA-512 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <winrt/Windows.Foundation.h>

#include <sha512.h>

namespace NanaZip::Codecs::Hash
{
    struct Sha512 : public winrt::implements<Sha512, IHasher>
    {
    private:

        sha512_ctx Context;

    public:

        Sha512()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_sha512_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_sha512_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_sha512_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return sha512_hash_size;
        }
    };

    IHasher* CreateSha512()
    {
        return winrt::make<Sha512>().detach();
    }
}
