/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.Ufs.cpp
 * PURPOSE:   Implementation for UFS/UFS2 file system image readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

#ifdef _MSC_VER
#if _MSC_VER > 1000
#pragma once
#endif
#if (_MSC_VER >= 1200)
#pragma warning(push)
#endif
#pragma warning(disable:4201) // nameless struct/union
#endif

#include "FreeBSD/fs.h"
#include "FreeBSD/dir.h"

#ifdef _MSC_VER
#if (_MSC_VER >= 1200)
#pragma warning(pop)
#else
#pragma warning(default:4201) // nameless struct/union
#endif
#endif

namespace
{
    const std::int32_t g_SuperBlockSearchList[] = SBLOCKSEARCH;
}

namespace NanaZip::Codecs::Archive
{
    struct Ufs : public Mile::ComObject<Ufs, IInArchive>
    {
    private:

        bool m_IsUfs2 = false;
        bool m_IsBigEndian = false;
        fs m_SuperBlock = { 0 };

    private:

        std::uint8_t ReadUInt8(
            void* BaseAddress)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            return Base[0];
        }

        std::uint16_t ReadUInt16(
            void* BaseAddress)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            if (this->m_IsBigEndian)
            {
                return
                    (static_cast<std::uint16_t>(Base[0]) << 8) |
                    (static_cast<std::uint16_t>(Base[1]));
            }
            else
            {
                return
                    (static_cast<std::uint16_t>(Base[0])) |
                    (static_cast<std::uint16_t>(Base[1]) << 8);
            }
        }

        std::uint32_t ReadUInt32(
            void* BaseAddress)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            if (this->m_IsBigEndian)
            {
                return
                    (static_cast<std::uint32_t>(Base[0]) << 24) |
                    (static_cast<std::uint32_t>(Base[1]) << 16) |
                    (static_cast<std::uint32_t>(Base[2]) << 8) |
                    (static_cast<std::uint32_t>(Base[3]));
            }
            else
            {
                return
                    (static_cast<std::uint32_t>(Base[0])) |
                    (static_cast<std::uint32_t>(Base[1]) << 8) |
                    (static_cast<std::uint32_t>(Base[2]) << 16) |
                    (static_cast<std::uint32_t>(Base[3]) << 24);
            }
        }

        std::uint64_t ReadUInt64(
            void* BaseAddress)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            if (this->m_IsBigEndian)
            {
                return
                    (static_cast<std::uint64_t>(Base[0]) << 56) |
                    (static_cast<std::uint64_t>(Base[1]) << 48) |
                    (static_cast<std::uint64_t>(Base[2]) << 40) |
                    (static_cast<std::uint64_t>(Base[3]) << 32) |
                    (static_cast<std::uint64_t>(Base[4]) << 24) |
                    (static_cast<std::uint64_t>(Base[5]) << 16) |
                    (static_cast<std::uint64_t>(Base[6]) << 8) |
                    (static_cast<std::uint64_t>(Base[7]));
            }
            else
            {
                return
                    (static_cast<std::uint64_t>(Base[0])) |
                    (static_cast<std::uint64_t>(Base[1]) << 8) |
                    (static_cast<std::uint64_t>(Base[2]) << 16) |
                    (static_cast<std::uint64_t>(Base[3]) << 24) |
                    (static_cast<std::uint64_t>(Base[4]) << 32) |
                    (static_cast<std::uint64_t>(Base[5]) << 40) |
                    (static_cast<std::uint64_t>(Base[6]) << 48) |
                    (static_cast<std::uint64_t>(Base[7]) << 56);
            }
        }

    public:

        Ufs()
        {

        }

        HRESULT STDMETHODCALLTYPE Open(
            _In_ IInStream* Stream,
            _In_opt_ const PUINT64 MaxCheckStartPosition,
            _In_opt_ IArchiveOpenCallback* OpenCallback)
        {
            UNREFERENCED_PARAMETER(Stream);
            UNREFERENCED_PARAMETER(MaxCheckStartPosition);
            UNREFERENCED_PARAMETER(OpenCallback);

            if (!Stream)
            {
                return E_INVALIDARG;
            }

            HRESULT hr = S_OK;
            SIZE_T NumberOfBytesRead = 0;

            do
            {
                this->Close();

                for (size_t i = 0; -1 != g_SuperBlockSearchList[i]; ++i)
                {
                    hr = Stream->Seek(
                        g_SuperBlockSearchList[i],
                        STREAM_SEEK_SET,
                        nullptr);
                    if (FAILED(hr))
                    {
                        break;
                    }

                    NumberOfBytesRead = 0;
                    hr = ::NanaZipCodecsReadInputStream(
                        Stream,
                        &this->m_SuperBlock,
                        sizeof(this->m_SuperBlock),
                        &NumberOfBytesRead);
                    if (FAILED(hr))
                    {
                        break;
                    }
                    if (sizeof(fs) != NumberOfBytesRead)
                    {
                        hr = S_FALSE;
                        break;
                    }

                    this->m_IsBigEndian = false;
                    std::uint32_t Signature = this->ReadUInt32(
                        &this->m_SuperBlock.fs_magic);
                    if (FS_UFS2_MAGIC == Signature)
                    {
                        this->m_IsUfs2 = true;
                    }
                    else if (FS_UFS1_MAGIC == Signature)
                    {
                        this->m_IsUfs2 = false;
                    }
                    else
                    {
                        this->m_IsBigEndian = true;
                        Signature = this->ReadUInt32(
                            &this->m_SuperBlock.fs_magic);
                        if (FS_UFS2_MAGIC == Signature)
                        {
                            this->m_IsUfs2 = true;
                        }
                        else if (FS_UFS1_MAGIC == Signature)
                        {
                            this->m_IsUfs2 = false;
                        }
                        else
                        {
                            hr = S_FALSE;
                            continue;
                        }
                    }

                    std::int64_t SuperBlockLocation = static_cast<std::int64_t>(
                        this->ReadUInt64(&this->m_SuperBlock.fs_sblockloc));
                    if (this->m_IsUfs2)
                    {
                        if (SBLOCK_UFS2 != SuperBlockLocation)
                        {
                            hr = S_FALSE;
                            continue;
                        }
                    }
                    else
                    {
                        if (SuperBlockLocation < 0 ||
                            SuperBlockLocation > SBLOCK_UFS1)
                        {
                            hr = S_FALSE;
                            continue;
                        }
                    }

                    std::int32_t FragmentsCount = static_cast<std::int32_t>(
                        this->ReadUInt32(&this->m_SuperBlock.fs_frag));
                    if (FragmentsCount < 1)
                    {
                        hr = S_FALSE;
                        continue;
                    }

                    std::uint32_t CylinderGroupsCount =
                        this->ReadUInt32(&this->m_SuperBlock.fs_ncg);
                    if (CylinderGroupsCount < 1)
                    {
                        hr = S_FALSE;
                        continue;
                    }

                    std::int32_t BlockSize = static_cast<std::int32_t>(
                        this->ReadUInt32(&this->m_SuperBlock.fs_bsize));
                    if (BlockSize < MINBSIZE)
                    {
                        hr = S_FALSE;
                        continue;
                    }

                    std::int32_t SuperBlockSize = static_cast<std::int32_t>(
                        this->ReadUInt32(&this->m_SuperBlock.fs_sbsize));
                    if (SuperBlockSize > SBLOCKSIZE ||
                        SuperBlockSize < sizeof(fs))
                    {
                        hr = S_FALSE;
                        continue;
                    }

                    hr = S_OK;
                    break;
                }
                if (FAILED(hr))
                {
                    break;
                }

                // this->m_SuperBlock = this->m_SuperBlock;

            } while (false);

            if (FAILED(hr))
            {
                this->Close();
            }

            return hr;
        }

        HRESULT STDMETHODCALLTYPE Close()
        {
            std::memset(&this->m_SuperBlock, 0, sizeof(this->m_SuperBlock));
            this->m_IsBigEndian = false;
            this->m_IsUfs2 = false;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfItems(
            _Out_ PUINT32 NumItems)
        {
            if (!NumItems)
            {
                return E_INVALIDARG;
            }
            *NumItems = 0;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetProperty(
            _In_ UINT32 Index,
            _In_ PROPID PropID,
            _Inout_ LPPROPVARIANT Value)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(Value);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Extract(
            _In_ const PUINT32 Indices,
            _In_ UINT32 NumItems,
            _In_ BOOL TestMode,
            _In_ IArchiveExtractCallback* ExtractCallback)
        {
            UNREFERENCED_PARAMETER(Indices);
            UNREFERENCED_PARAMETER(NumItems);
            UNREFERENCED_PARAMETER(TestMode);
            UNREFERENCED_PARAMETER(ExtractCallback);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetArchiveProperty(
            _In_ PROPID PropID,
            _Inout_ LPPROPVARIANT Value)
        {
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(Value);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfProperties(
            _Out_ PUINT32 NumProps)
        {
            if (!NumProps)
            {
                return E_INVALIDARG;
            }
            *NumProps = 0;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetPropertyInfo(
            _In_ UINT32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropID,
            _Out_ VARTYPE* VarType)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(Name);
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(VarType);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfArchiveProperties(
            _Out_ PUINT32 NumProps)
        {
            if (!NumProps)
            {
                return E_INVALIDARG;
            }
            *NumProps = 0;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetArchivePropertyInfo(
            _In_ UINT32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropID,
            _Out_ VARTYPE* VarType)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(Name);
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(VarType);
            return S_OK;
        }
    };

    IInArchive* CreateUfs()
    {
        return new Ufs();
    }
}
