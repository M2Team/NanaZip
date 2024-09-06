/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Xxh3128.cpp
 * PURPOSE:   Implementation for XXH3_128bits hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#define XXH_STATIC_LINKING_ONLY
#include <xxhash.h>

namespace NanaZip::Codecs::Hash
{
    struct Xxh3128 : public Mile::ComObject<Xxh3128, IHasher>
    {
    private:

        XXH3_state_t* Context;

    public:

        Xxh3128()
        {
            this->Context = ::XXH3_createState();
        }

        ~Xxh3128()
        {
            ::XXH3_freeState(
                this->Context);
        }

        void STDMETHODCALLTYPE Init()
        {
            ::XXH3_128bits_reset(
                this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::XXH3_128bits_update(
                this->Context,
                Data,
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            XXH128_hash_t FinalDigest = ::XXH3_128bits_digest(
                this->Context);
            std::memcpy(
                Digest,
                &FinalDigest,
                this->GetDigestSize());
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 16;
        }
    };

    IHasher* CreateXxh3128()
    {
        return new Xxh3128();
    }
}
