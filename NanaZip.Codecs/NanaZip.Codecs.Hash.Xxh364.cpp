/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Xxh364.cpp
 * PURPOSE:   Implementation for XXH3_64bits hash algorithm
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
    struct Xxh364 : public Mile::ComObject<Xxh364, IHasher>
    {
    private:

        XXH3_state_t* Context;

    public:

        Xxh364()
        {
            this->Context = ::XXH3_createState();
        }

        ~Xxh364()
        {
            ::XXH3_freeState(
                this->Context);
        }

        void STDMETHODCALLTYPE Init()
        {
            ::XXH3_64bits_reset(
                this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::XXH3_64bits_update(
                this->Context,
                Data,
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            XXH64_hash_t FinalDigest = ::XXH3_64bits_digest(
                this->Context);
            std::memcpy(
                Digest,
                &FinalDigest,
                this->GetDigestSize());
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return 8;
        }
    };

    IHasher* CreateXxh364()
    {
        return new Xxh364();
    }
}
