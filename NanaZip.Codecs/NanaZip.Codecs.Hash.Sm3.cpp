/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Sm3.cpp
 * PURPOSE:   Implementation for SM3 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <sm3.h>

namespace NanaZip::Codecs::Hash
{
    struct Sm3 : public Mile::ComObject<Sm3, IHasher>
    {
    private:

        SM3_CTX Context;

    public:

        Sm3()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::sm3_init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::sm3_update(
                &this->Context,
                reinterpret_cast<const std::uint8_t*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::sm3_finish(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return SM3_DIGEST_SIZE;
        }
    };

    IHasher* CreateSm3()
    {
        return new Sm3();
    }
}
