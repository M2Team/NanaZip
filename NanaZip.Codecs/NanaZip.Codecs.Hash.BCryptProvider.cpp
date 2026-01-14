/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Hash.BCryptProvider.cpp
 * PURPOSE:    Implementation for Windows CNG Hash Algorithm Provider
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <K7Base.h>

#include <string>

namespace NanaZip::Codecs::Hash
{
    struct BCryptProvider : public Mile::ComObject<BCryptProvider, IHasher>
    {
    private:

        K7_BASE_HASH_ALGORITHM_TYPE m_Algorithm;
        K7_BASE_HASH_HANDLE m_HashHandle = nullptr;

        void DestroyContext()
        {
            if (this->m_HashHandle)
            {
                ::K7BaseHashDestroy(this->m_HashHandle);
                this->m_HashHandle = nullptr;
            }
        }

    public:

        BCryptProvider(
            _In_ K7_BASE_HASH_ALGORITHM_TYPE Algorithm)
        {
            this->m_Algorithm = Algorithm;
            this->Init();
        }

        ~BCryptProvider()
        {
            this->DestroyContext();
        }

        void STDMETHODCALLTYPE Init()
        {
            this->DestroyContext();
            ::K7BaseHashCreate(
                &this->m_HashHandle,
                this->m_Algorithm,
                nullptr,
                0);
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            ::K7BaseHashUpdate(
                this->m_HashHandle,
                const_cast<LPVOID>(Data),
                Size);
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            ::K7BaseHashFinal(
                this->m_HashHandle,
                Digest,
                this->GetDigestSize());
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            UINT32 HashSize = 0;
            ::K7BaseHashGetSize(this->m_HashHandle, &HashSize);
            return HashSize;
        }
    };

    IHasher* CreateMd2()
    {
        return new BCryptProvider(K7_BASE_HASH_ALGORITHM_MD2);
    }

    IHasher* CreateMd4()
    {
        return new BCryptProvider(K7_BASE_HASH_ALGORITHM_MD4);
    }

    IHasher* CreateMd5()
    {
        return new BCryptProvider(K7_BASE_HASH_ALGORITHM_MD5);
    }

    IHasher* CreateSha1()
    {
        return new BCryptProvider(K7_BASE_HASH_ALGORITHM_SHA1);
    }

    IHasher* CreateSha256()
    {
        return new BCryptProvider(K7_BASE_HASH_ALGORITHM_SHA256);
    }

    IHasher* CreateSha384()
    {
        return new BCryptProvider(K7_BASE_HASH_ALGORITHM_SHA384);
    }

    IHasher* CreateSha512()
    {
        return new BCryptProvider(K7_BASE_HASH_ALGORITHM_SHA512);
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

    ::K7BaseHashCreate(
        &ctx->context,
        K7_BASE_HASH_ALGORITHM_SHA1,
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

    ::K7BaseHashUpdate(
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

    ::K7BaseHashFinal(
        ctx->context,
        ctx->hash,
        sha1_hash_size);
    if (result)
    {
        std::memcpy(result, ctx->hash, sha1_hash_size);
    }
}

#pragma endregion
