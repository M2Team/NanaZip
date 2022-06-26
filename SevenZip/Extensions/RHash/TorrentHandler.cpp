/*
 * PROJECT:   NanaZip
 * FILE:      TorrentHandler.cpp
 * PURPOSE:   Implementation for BitTorrent InfoHash (BTIH) hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "../../C/CpuArch.h"
#include "../../CPP/Common/MyCom.h"
#include "../../CPP/7zip/Common/RegisterCodec.h"

#include "../../../ThirdParty/RHash/torrent.h"

class CTorrentHandler :
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

    MY_UNKNOWN_IMP1(IHasher)
    INTERFACE_IHasher(;)
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
