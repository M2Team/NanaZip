/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Ed2k.cpp
 * PURPOSE:   Implementation for ED2K hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <ed2k.h>

namespace NanaZip::Codecs::Hash
{
    struct Ed2k : public Mile::ComObject<Ed2k, IHasher>
    {
    private:

        ed2k_ctx Context;

    public:

        Ed2k()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_ed2k_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_ed2k_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_ed2k_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 16;
        }
    };

    IHasher* CreateEd2k()
    {
        return new Ed2k();
    }
}
