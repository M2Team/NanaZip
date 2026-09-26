/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Hash.Torrent.cpp
 * PURPOSE:    Implementation for BitTorrent InfoHash (BTIH) hash algorithm
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <torrent.h>

namespace NanaZip::Codecs::Hash
{
    struct Torrent : public Mile::ComObject<Torrent, IHasher>
    {
    private:

        torrent_ctx Context;
        bool Initialized = false;

    public:

        Torrent()
        {
            this->Init();
        }

        Torrent(const Torrent &) = delete;
        Torrent &operator=(const Torrent &) = delete;
        Torrent(Torrent &&other) = delete;
        Torrent &operator=(Torrent &&other) = delete;

        ~Torrent()
        {
            if (this->Initialized)
            {
                ::bt_cleanup(&this->Context);
                this->Initialized = false;
            }
        }

        void STDMETHODCALLTYPE Init()
        {
            if (this->Initialized)
            {
                ::bt_cleanup(&this->Context);
            }
            ::bt_init(&this->Context);
            this->Initialized = true;
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::bt_update(
                &this->Context,
                reinterpret_cast<const void*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::bt_final(
                &this->Context,
                Digest);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return btih_hash_size;
        }
    };

    IHasher* CreateTorrent()
    {
        return new Torrent();
    }
}
