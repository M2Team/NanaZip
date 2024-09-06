/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Aich.cpp
 * PURPOSE:   Implementation for EMule AICH hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <aich.h>

namespace NanaZip::Codecs::Hash
{
    struct Aich : public Mile::ComObject<Aich, IHasher>
    {
    private:

        aich_ctx Context;

    public:

        Aich()
        {
            this->Init();
        }

        ~Aich()
        {
            ::rhash_aich_cleanup(
                &this->Context);
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_aich_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_aich_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_aich_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 20;
        }
    };

    IHasher* CreateAich()
    {
        return new Aich();
    }
}
