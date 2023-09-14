/*
 * PROJECT:   NanaZip
 * FILE:      TorrentHandler.cpp
 * PURPOSE:   Implementation for BitTorrent InfoHash (BTIH) hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: MouriNaruto (KurikoMouri@outlook.jp)
 */

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include <torrent.h>

class CTorrentHandler final :
    public IHasher,
    public CMyUnknownImp
{
    torrent_ctx Context;
    Byte mtDummy[1 << 7];

public:

    CTorrentHandler()
    {
        this->Init();
    }

    ~CTorrentHandler()
    {
        ::bt_cleanup(&this->Context);
    }

    Z7_IFACES_IMP_UNK_1(IHasher)
};

STDMETHODIMP_(void) CTorrentHandler::Init() throw()
{
    ::bt_init(&this->Context);
}

STDMETHODIMP_(void) CTorrentHandler::Update(
    const void* data,
    UInt32 size) throw()
{
    ::bt_update(
        &this->Context,
        reinterpret_cast<const unsigned char*>(data),
        size);
}

STDMETHODIMP_(void) CTorrentHandler::Final(Byte* digest) throw()
{
    ::bt_final(&this->Context, digest);
}

REGISTER_HASHER(
    CTorrentHandler,
    0x3C1,
    "BTIH",
    btih_hash_size)
