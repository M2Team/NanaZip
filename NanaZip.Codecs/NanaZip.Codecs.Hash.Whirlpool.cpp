/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Whirlpool.cpp
 * PURPOSE:   Implementation for Whirlpool hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <whirlpool.h>

namespace NanaZip::Codecs::Hash
{
    struct Whirlpool : public Mile::ComObject<Whirlpool, IHasher>
    {
    private:

        whirlpool_ctx Context;

    public:

        Whirlpool()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::rhash_whirlpool_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::rhash_whirlpool_update(
                &this->Context,
                reinterpret_cast<const unsigned char*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::rhash_whirlpool_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 64;
        }
    };

    IHasher* CreateWhirlpool()
    {
        return new Whirlpool();
    }
}
