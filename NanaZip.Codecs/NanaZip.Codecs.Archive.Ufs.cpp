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

    private:

        std::uint8_t ReadUInt8(
            void* BaseAddress,
            std::size_t Offset = 0)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            return Base[Offset];
        }

        std::uint16_t ReadUInt16(
            void* BaseAddress,
            std::size_t Offset = 0)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            if (this->m_IsBigEndian)
            {
                return
                    (static_cast<std::uint16_t>(Base[Offset]) << 8) |
                    (static_cast<std::uint16_t>(Base[Offset + 1]));
            }
            else
            {
                return
                    (static_cast<std::uint16_t>(Base[Offset])) |
                    (static_cast<std::uint16_t>(Base[Offset + 1]) << 8);
            }
        }

        std::uint32_t ReadUInt32(
            void* BaseAddress,
            std::size_t Offset = 0)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            if (this->m_IsBigEndian)
            {
                return
                    (static_cast<std::uint32_t>(Base[Offset]) << 24) |
                    (static_cast<std::uint32_t>(Base[Offset + 1]) << 16) |
                    (static_cast<std::uint32_t>(Base[Offset + 2]) << 8) |
                    (static_cast<std::uint32_t>(Base[Offset + 3]));
            }
            else
            {
                return
                    (static_cast<std::uint32_t>(Base[Offset])) |
                    (static_cast<std::uint32_t>(Base[Offset + 1]) << 8) |
                    (static_cast<std::uint32_t>(Base[Offset + 2]) << 16) |
                    (static_cast<std::uint32_t>(Base[Offset + 3]) << 24);
            }
        }

        std::uint64_t ReadUInt64(
            void* BaseAddress,
            std::size_t Offset = 0)
        {
            std::uint8_t* Base = reinterpret_cast<std::uint8_t*>(BaseAddress);
            if (this->m_IsBigEndian)
            {
                return
                    (static_cast<std::uint64_t>(Base[Offset]) << 56) |
                    (static_cast<std::uint64_t>(Base[Offset + 1]) << 48) |
                    (static_cast<std::uint64_t>(Base[Offset + 2]) << 40) |
                    (static_cast<std::uint64_t>(Base[Offset + 3]) << 32) |
                    (static_cast<std::uint64_t>(Base[Offset + 4]) << 24) |
                    (static_cast<std::uint64_t>(Base[Offset + 5]) << 16) |
                    (static_cast<std::uint64_t>(Base[Offset + 6]) << 8) |
                    (static_cast<std::uint64_t>(Base[Offset + 7]));
            }
            else
            {
                return
                    (static_cast<std::uint64_t>(Base[Offset])) |
                    (static_cast<std::uint64_t>(Base[Offset + 1]) << 8) |
                    (static_cast<std::uint64_t>(Base[Offset + 2]) << 16) |
                    (static_cast<std::uint64_t>(Base[Offset + 3]) << 24) |
                    (static_cast<std::uint64_t>(Base[Offset + 4]) << 32) |
                    (static_cast<std::uint64_t>(Base[Offset + 5]) << 40) |
                    (static_cast<std::uint64_t>(Base[Offset + 6]) << 48) |
                    (static_cast<std::uint64_t>(Base[Offset + 7]) << 56);
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
            fs SuperBlock = { 0 };

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
                        &SuperBlock,
                        sizeof(SuperBlock),
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
                    std::uint32_t Signature =
                        this->ReadUInt32(&SuperBlock.fs_magic);
                    if (FS_UFS2_MAGIC == Signature)
                    {
                        this->m_IsUfs2 = true;
                        break;
                    }
                    else if (FS_UFS1_MAGIC == Signature)
                    {
                        this->m_IsUfs2 = false;
                        break;
                    }
                    else
                    {
                        this->m_IsBigEndian = true;
                        Signature = this->ReadUInt32(&SuperBlock.fs_magic);
                        if (FS_UFS2_MAGIC == Signature)
                        {
                            this->m_IsUfs2 = true;
                            break;
                        }
                        else if (FS_UFS1_MAGIC == Signature)
                        {
                            this->m_IsUfs2 = false;
                            break;
                        }
                    }

                    hr = S_FALSE;
                }
                if (FAILED(hr))
                {
                    break;
                }

                SuperBlock = SuperBlock;

            } while (false);

            if (FAILED(hr))
            {
                this->Close();
            }

            return hr;
        }

        HRESULT STDMETHODCALLTYPE Close()
        {
            this->m_IsBigEndian = false;
            this->m_IsUfs2 = false;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfItems(
            _Out_ PUINT32 NumItems)
        {
            UNREFERENCED_PARAMETER(NumItems);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetProperty(
            _In_ UINT32 Index,
            _In_ PROPID PropID,
            _Inout_ LPPROPVARIANT Value)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(Value);
            return E_NOTIMPL;
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
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfProperties(
            _Out_ PUINT32 NumProps)
        {
            UNREFERENCED_PARAMETER(NumProps);
            return E_NOTIMPL;
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
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfArchiveProperties(
            _Out_ PUINT32 NumProps)
        {
            UNREFERENCED_PARAMETER(NumProps);
            return E_NOTIMPL;
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
            return E_NOTIMPL;
        }
    };

    IInArchive* CreateUfs()
    {
        return new Ufs();
    }
}
