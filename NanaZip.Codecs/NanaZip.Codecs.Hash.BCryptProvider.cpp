/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.BCryptProvider.cpp
 * PURPOSE:   Implementation for Windows CNG Hash Algorithm Provider
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <K7Pal.h>

namespace NanaZip::Codecs::Hash
{
    struct BCryptProvider : public Mile::ComObject<BCryptProvider, IHasher>
    {
    private:

        K7_PAL_HASH_HANDLE m_HashHandle = nullptr;

    public:

        BCryptProvider(
            _In_ LPCWSTR AlgorithmIdentifier)
        {
            ::K7PalHashCreate(
                &this->m_HashHandle,
                AlgorithmIdentifier,
                nullptr,
                0);
        }

        ~BCryptProvider()
        {
            if (this->m_HashHandle)
            {
                ::K7PalHashDestroy(this->m_HashHandle);
                this->m_HashHandle = nullptr;
            }
        }

        void STDMETHODCALLTYPE Init()
        {
            ::K7PalHashReset(this->m_HashHandle);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::K7PalHashUpdate(
                this->m_HashHandle,
                const_cast<LPVOID>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::K7PalHashFinal(
                this->m_HashHandle,
                Digest,
                this->GetDigestSize());
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            UINT32 HashSize = 0;
            ::K7PalHashGetSize(this->m_HashHandle, &HashSize);
            return HashSize;
        }
    };

    IHasher* CreateMd2()
    {
        return new BCryptProvider(BCRYPT_MD2_ALGORITHM);
    }

    IHasher* CreateMd4()
    {
        return new BCryptProvider(BCRYPT_MD4_ALGORITHM);
    }

    IHasher* CreateMd5()
    {
        return new BCryptProvider(BCRYPT_MD5_ALGORITHM);
    }

    IHasher* CreateSha384()
    {
        return new BCryptProvider(BCRYPT_SHA384_ALGORITHM);
    }

    IHasher* CreateSha512()
    {
        return new BCryptProvider(BCRYPT_SHA512_ALGORITHM);
    }
}
