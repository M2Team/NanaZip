/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Xxh32.cpp
 * PURPOSE:   Implementation for XXH32 hash algorithm
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
    struct Xxh32 : public Mile::ComObject<Xxh32, IHasher>
    {
    private:

        XXH32_state_t* Context;

    public:

        Xxh32()
        {
            this->Context = ::XXH32_createState();
        }


        ~Xxh32()
        {
            ::XXH32_freeState(
                this->Context);
        }

        void STDMETHODCALLTYPE Init()
        {
            ::XXH32_reset(
                this->Context,
                0);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::XXH32_update(
                this->Context,
                Data,
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            XXH32_hash_t FinalDigest = ::XXH32_digest(
                this->Context);
            std::memcpy(
                Digest,
                &FinalDigest,
                this->GetDigestSize());
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 4;
        }
    };

    IHasher* CreateXxh32()
    {
        return new Xxh32();
    }
}
