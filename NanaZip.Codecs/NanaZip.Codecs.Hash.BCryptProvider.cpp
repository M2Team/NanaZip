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

#include <string>

namespace NanaZip::Codecs::Hash
{
    struct BCryptProvider : public Mile::ComObject<BCryptProvider, IHasher>
    {
    private:

        std::wstring m_AlgorithmIdentifier;
        K7_PAL_HASH_HANDLE m_HashHandle = nullptr;

        void DestroyContext()
        {
            if (this->m_HashHandle)
            {
                ::K7PalHashDestroy(this->m_HashHandle);
                this->m_HashHandle = nullptr;
            }
        }

    public:

        BCryptProvider(
            _In_ LPCWSTR AlgorithmIdentifier)
        {
            this->m_AlgorithmIdentifier = std::wstring(AlgorithmIdentifier);
            this->Init();
        }

        ~BCryptProvider()
        {
            this->DestroyContext();
        }

        void STDMETHODCALLTYPE Init()
        {
            this->DestroyContext();
            ::K7PalHashCreate(
                &this->m_HashHandle,
                this->m_AlgorithmIdentifier.c_str(),
                nullptr,
                0);
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

    IHasher* CreateSha1()
    {
        return new BCryptProvider(BCRYPT_SHA1_ALGORITHM);
    }

    IHasher* CreateSha256()
    {
        return new BCryptProvider(BCRYPT_SHA256_ALGORITHM);
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

#pragma region RHash Wrappers

#include "RHash/sha1.h"

void rhash_sha1_init(
    sha1_ctx* ctx)
{
    if (!ctx)
    {
        return;
    }
    std::memset(ctx, 0, sizeof(sha1_ctx));

    ::K7PalHashCreate(
        &ctx->context,
        BCRYPT_SHA1_ALGORITHM,
        nullptr,
        0);
}

void rhash_sha1_update(
    sha1_ctx* ctx,
    const unsigned char* msg,
    size_t size)
{
    if (!ctx)
    {
        return;
    }

    ::K7PalHashUpdate(
        ctx->context,
        const_cast<LPVOID>(reinterpret_cast<LPCVOID>(msg)),
        static_cast<UINT32>(size));
    ctx->length += size;
}

void rhash_sha1_final(
    sha1_ctx* ctx,
    unsigned char* result)
{
    if (!ctx)
    {
        return;
    }

    ::K7PalHashFinal(
        ctx->context,
        ctx->hash,
        sha1_hash_size);
    if (result)
    {
        std::memcpy(result, ctx->hash, sha1_hash_size);
    }
}

#pragma endregion
