/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Xxh64.cpp
 * PURPOSE:   Implementation for XXH64 hash algorithm
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
    struct Xxh64 : public Mile::ComObject<Xxh64, IHasher>
    {
    private:

        XXH64_state_t* Context;

    public:

        Xxh64()
        {
            this->Context = ::XXH64_createState();
        }

        ~Xxh64()
        {
            ::XXH64_freeState(
                this->Context);
        }

        void STDMETHODCALLTYPE Init()
        {
            ::XXH64_reset(
                this->Context,
                0);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::XXH64_update(
                this->Context,
                Data,
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            XXH64_hash_t FinalDigest = ::XXH64_digest(
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

    IHasher* CreateXxh64()
    {
        return new Xxh64();
    }
}
