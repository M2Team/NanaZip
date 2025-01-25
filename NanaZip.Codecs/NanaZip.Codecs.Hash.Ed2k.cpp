/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Hash.Ed2k.cpp
 * PURPOSE:   Implementation for ED2K hash algorithm
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <K7Pal.h>

// Comments copy from RHash's ed2k.c
//
// This file implements eMule-compatible version of algorithm. Note that
// eDonkey and eMule ed2k hashes are different only for files containing
// exactly multiple of 9728000 bytes.
// The file data is divided into full chunks of 9500 KiB (9728000 bytes) plus a
// remainder chunk, and a separate 128-bit MD4 hash is computed for each. If the
// file length is an exact multiple of 9500 KiB, the remainder zero size chunk
// is still used at the end of the hash list. The ed2k hash is computed by
// concatenating the chunks' MD4 hashes in order and hashing the result using
// MD4. Although, if the file is composed of a single non-full chunk, its MD4
// hash is returned with no further modifications.
// See http://en.wikipedia.org/wiki/EDonkey_network for algorithm description.

// Each hashed file is divided into 9500 KiB sized chunks
#define ED2K_CHUNK_SIZE 9728000

#define MD4_HASH_SIZE 16
#define ED2K_HASH_SIZE MD4_HASH_SIZE

namespace NanaZip::Codecs::Hash
{
    struct Ed2k : public Mile::ComObject<Ed2k, IHasher>
    {
    private:

        // MD4 context to hash file blocks.
        K7_PAL_HASH_HANDLE m_BlockHashHandle = nullptr;

        // MD4 context to hash block hashes.
        K7_PAL_HASH_HANDLE m_HashesHashHandle = nullptr;

        // false for eMule ED2K algorithm.
        bool m_NotEmule = false;

        // The processed size for hash file blocks.
        UINT32 m_BlockProcessedSize = 0;

        // The processed size for hash block hashes.
        UINT32 m_HashesProcessedSize = 0;

        void DestroyContext()
        {
            if (this->m_BlockHashHandle)
            {
                ::K7PalHashDestroy(this->m_BlockHashHandle);
                this->m_BlockHashHandle = nullptr;
            }
            if (this->m_HashesHashHandle)
            {
                ::K7PalHashDestroy(this->m_HashesHashHandle);
                this->m_HashesHashHandle = nullptr;
            }
        }

    public:

        Ed2k()
        {
            this->Init();
        }

        ~Ed2k()
        {
            this->DestroyContext();
        }

        void STDMETHODCALLTYPE Init()
        {
            this->DestroyContext();
            ::K7PalHashCreate(
                &this->m_BlockHashHandle,
                BCRYPT_MD4_ALGORITHM,
                nullptr,
                0);
            ::K7PalHashCreate(
                &this->m_HashesHashHandle,
                BCRYPT_MD4_ALGORITHM,
                nullptr,
                0);
            this->m_NotEmule = false;
            this->m_BlockProcessedSize = 0;
        }

        void STDMETHODCALLTYPE Update(
            _In_ LPCVOID Data,
            _In_ UINT32 Size)
        {
            BYTE BlockHash[MD4_HASH_SIZE];
            UINT32 BlockLeft = ED2K_CHUNK_SIZE - this->m_BlockProcessedSize;

            // Note: eMule-compatible algorithm hashes by md4_inner the messages
            // which sizes are multiple of 9728000 and then processes obtained
            // hash by external MD4.

            while (Size >= BlockLeft)
            {
                if (Size == BlockLeft && this->m_NotEmule)
                {
                    break;
                }

                // If internal ED2K chunk is full, then finalize it.
                ::K7PalHashUpdate(
                    this->m_BlockHashHandle,
                    const_cast<LPVOID>(Data),
                    BlockLeft);
                this->m_BlockProcessedSize += BlockLeft;
                Data = reinterpret_cast<LPCVOID>(
                    reinterpret_cast<UINT_PTR>(Data) + BlockLeft);
                Size -= BlockLeft;
                BlockLeft = ED2K_CHUNK_SIZE;

                // Just finished an ED2K chunk, updating MD4 external context.
                ::K7PalHashFinal(
                    this->m_BlockHashHandle,
                    BlockHash,
                    MD4_HASH_SIZE);
                this->m_BlockProcessedSize = 0;
                ::K7PalHashUpdate(
                    this->m_HashesHashHandle,
                    BlockHash,
                    MD4_HASH_SIZE);
                this->m_HashesProcessedSize += MD4_HASH_SIZE;
            }

            if (Size)
            {
                // Hash leftovers.
                ::K7PalHashUpdate(
                    this->m_BlockHashHandle,
                    const_cast<LPVOID>(Data),
                    Size);
                this->m_BlockProcessedSize += Size;
            }
        }

        void STDMETHODCALLTYPE Final(
            _Out_ PBYTE Digest)
        {
            // Check if hashed message size is greater or equal to
            // ED2K_CHUNK_SIZE.
            if (this->m_HashesProcessedSize)
            {
                // Note: Weird eMule algorithm always processes the inner MD4
                // context, no matter if it contains data or is empty.

                // If any data are left in the m_BlockHashHandle.
                if (this->m_BlockProcessedSize || !this->m_NotEmule)
                {
                    // eMule algorithm processes aditional block, even if it's
                    // empty.

                    BYTE BlockHash[MD4_HASH_SIZE];
                    ::K7PalHashFinal(
                        this->m_BlockHashHandle,
                        BlockHash,
                        MD4_HASH_SIZE);
                    this->m_BlockProcessedSize = 0;
                    ::K7PalHashUpdate(
                        this->m_HashesHashHandle,
                        BlockHash,
                        MD4_HASH_SIZE);
                    this->m_HashesProcessedSize += MD4_HASH_SIZE;
                }

                // Call final to flush MD4 buffer and finalize the hash value.
                ::K7PalHashFinal(
                    this->m_HashesHashHandle,
                    Digest,
                    ED2K_HASH_SIZE);
                this->m_HashesProcessedSize = 0;
            }
            else
            {
                // Just return the message MD4 hash.
                ::K7PalHashFinal(
                    this->m_BlockHashHandle,
                    Digest,
                    MD4_HASH_SIZE);
                this->m_BlockProcessedSize = 0;
            }
        }

        UINT32 STDMETHODCALLTYPE GetDigestSize()
        {
            return ED2K_HASH_SIZE;
        }
    };

    IHasher* CreateEd2k()
    {
        return new Ed2k();
    }
}
