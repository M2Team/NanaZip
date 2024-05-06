/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Md2.cpp
 * PURPOSE:   Implementation for MD2 hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <winrt/Windows.Foundation.h>

EXTERN_C_START
#include <md2.h>
EXTERN_C_END

namespace NanaZip::Codecs::Hash
{
    struct Md2 : public winrt::implements<Md2, IHasher>
    {
    private:

        MD2_CTX Context;

    public:

        Md2()
        {
            this->Init();
        }

        void STDMETHODCALLTYPE Init()
        {
            ::MD2_Init(
                &this->Context);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::MD2_Update(
                &this->Context,
                reinterpret_cast<const void*>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::MD2_Final(
                Digest,
                &this->Context);
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return MD2_DIGEST_LENGTH;
        }
    };

    IHasher* CreateMd2()
    {
        return winrt::make<Md2>().detach();
    }
}
