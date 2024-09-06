/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Blake3.cpp
 * PURPOSE:   Implementation for BLAKE3 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <blake3.h>

namespace NanaZip::Codecs::Hash
{
    struct Blake3 : public Mile::ComObject<Blake3, IHasher>
    {
    private:

        blake3_hasher Context;

    public:

        Blake3()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::blake3_hasher_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::blake3_hasher_update(
                &this->Context,
                Data,
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::blake3_hasher_finalize(
                &this->Context,
                Digest,
                this->GetDigestSize());
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return BLAKE3_OUT_LEN;
        }
    };

    IHasher* CreateBlake3()
    {
        return new Blake3();
    }
}
