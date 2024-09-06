/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Tth.cpp
 * PURPOSE:   Implementation for Tiger Tree Hash (TTH) hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <tth.h>

namespace NanaZip::Codecs::Hash
{
    struct Tth : public Mile::ComObject<Tth, IHasher>
    {
    private:

        tth_ctx Context;

    public:

        Tth()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_tth_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_tth_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_tth_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 24;
        }
    };

    IHasher* CreateTth()
    {
        return new Tth();
    }
}
