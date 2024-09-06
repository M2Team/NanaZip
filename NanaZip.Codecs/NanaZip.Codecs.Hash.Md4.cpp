/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Md4.cpp
 * PURPOSE:   Implementation for MD4 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <md4.h>

namespace NanaZip::Codecs::Hash
{
    struct Md4 : public Mile::ComObject<Md4, IHasher>
    {
    private:

        md4_ctx Context;

    public:

        Md4()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_md4_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_md4_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_md4_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return md4_hash_size;
        }
    };

    IHasher* CreateMd4()
    {
        return new Md4();
    }
}
