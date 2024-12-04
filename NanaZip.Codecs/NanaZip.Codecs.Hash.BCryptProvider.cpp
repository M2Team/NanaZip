/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.BCryptProvider.cpp
 * PURPOSE:   Implementation for Windows CNG Hash Algorithm Provider
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#define WIN32_NO_STATUS

#include "NanaZip.Codecs.h"

#include <Mile.Internal.h>

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace NanaZip::Codecs::Hash
{
    struct BCryptProvider : public Mile::ComObject<BCryptProvider, IHasher>
    {
    private:

        BCRYPT_ALG_HANDLE m_AlgorithmHandle = nullptr;
        BCRYPT_HASH_HANDLE m_HashHandle = nullptr;
        PUCHAR m_HashObject = nullptr;
        DWORD m_HashLength = 0;

        void CloseAlgorithmProvider()
        {
            if (this->m_AlgorithmHandle)
            {
                ::BCryptCloseAlgorithmProvider(this->m_AlgorithmHandle, 0);
                this->m_AlgorithmHandle = nullptr;
            }
        }

        void DestroyHash()
        {
            if (this->m_HashHandle)
            {
                ::BCryptDestroyHash(this->m_HashHandle);
                this->m_HashHandle = nullptr;
            }

            this->m_HashLength = 0;

            if (this->m_HashObject)
            {
                ::MileFreeMemory(this->m_HashObject);
                this->m_HashObject = nullptr;
            }
        }

    public:

        BCryptProvider(
            _In_ LPCWSTR AlgorithmIdentifier)
        {
            if (!NT_SUCCESS(::BCryptOpenAlgorithmProvider(
                &this->m_AlgorithmHandle,
                AlgorithmIdentifier,
                nullptr,
                0)))
            {
                this->CloseAlgorithmProvider();
            }

            this->Init();
        }

        ~BCryptProvider()
        {
            this->DestroyHash();
            this->CloseAlgorithmProvider();
        }

        void STDMETHODCALLTYPE Init()
        {
            this->DestroyHash();

            ULONG ReturnLength = 0;

            DWORD HashObjectLength = 0;
            {
                ReturnLength = 0;
                if (!NT_SUCCESS(::BCryptGetProperty(
                    this->m_AlgorithmHandle,
                    BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&HashObjectLength),
                    sizeof(HashObjectLength),
                    &ReturnLength,
                    0)))
                {
                    return;
                }
            }

            DWORD HashLength = 0;
            {
                ReturnLength = 0;
                if (!NT_SUCCESS(::BCryptGetProperty(
                    this->m_AlgorithmHandle,
                    BCRYPT_HASH_LENGTH,
                    reinterpret_cast<PUCHAR>(&HashLength),
                    sizeof(HashLength),
                    &ReturnLength,
                    0)))
                {
                    return;
                }
            }
            this->m_HashLength = HashLength;

            this->m_HashObject = reinterpret_cast<PUCHAR>(
                ::MileAllocateMemory(HashObjectLength));
            if (!this->m_HashObject)
            {
                return;
            }

            if (!NT_SUCCESS(::BCryptCreateHash(
                this->m_AlgorithmHandle,
                &this->m_HashHandle,
                this->m_HashObject,
                HashObjectLength,
                nullptr,
                0,
                0)))
            {
                this->DestroyHash();
            }
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::BCryptHashData(
                this->m_HashHandle,
                reinterpret_cast<PUCHAR>(const_cast<LPVOID>(Data)),
                Size,
                0);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            if (!NT_SUCCESS(::BCryptFinishHash(
                this->m_HashHandle,
                Digest,
                this->m_HashLength,
                0)))
            {
                if (Digest)
                {
                    std::memset(Digest, 0, this->m_HashLength);
                }
            }

            this->DestroyHash();
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return this->m_HashLength;
        }
    };

    IHasher* CreateMd2()
    {
        return new BCryptProvider(BCRYPT_MD2_ALGORITHM);
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
