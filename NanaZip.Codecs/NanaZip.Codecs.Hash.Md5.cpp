/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Md5.cpp
 * PURPOSE:   Implementation for MD5 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <md5.h>

namespace NanaZip::Codecs::Hash
{
    struct Md5 : public Mile::ComObject<Md5, IHasher>
    {
    private:

        md5_ctx Context;

    public:

        Md5()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_md5_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_md5_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_md5_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return md5_hash_size;
        }
    };

    IHasher* CreateMd5()
    {
        return new Md5();
    }
}
