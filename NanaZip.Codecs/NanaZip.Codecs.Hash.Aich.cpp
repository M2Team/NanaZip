/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Hash.Aich.cpp
 * PURPOSE:    Implementation for EMule AICH hash algorithm
 *
 * LICENSE:    The MIT License
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
        bool Initialized = false;

    public:

        Aich()
        {
            this->Init();
        }

        Aich(const Aich &) = delete;
        Aich &operator=(const Aich &) = delete;
        Aich(Aich &&other) = delete;
        Aich &operator=(Aich &&other) = delete;

        ~Aich()
        {
            if (this->Initialized)
            {
                ::rhash_aich_cleanup(&this->Context);
                this->Initialized = false;
            }
        }

        void STDMETHODCALLTYPE Init()
        {
            if (this->Initialized)
            {
                ::rhash_aich_cleanup(&this->Context);
            }
            ::rhash_aich_init(&this->Context);
            this->Initialized = true;
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
            // rhash_aich_final calls rhash_aich_cleanup
            this->Initialized = false;
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
