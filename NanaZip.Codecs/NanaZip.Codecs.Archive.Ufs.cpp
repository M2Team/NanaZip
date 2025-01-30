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
    // Win32 time epoch is 00:00:00, January 1 1601.
    // UNIX time epoch is 00:00:00, January 1 1970.
    // There are 11644473600 seconds between these two epochs.
    const std::uint64_t SecondsBetweenWin32TimeAndUnixTime = 11644473600ULL;

    FILETIME ToFileTime(
        std::uint64_t UnixTimeSeconds,
        std::uint64_t UnixTimeNanoseconds)
    {
        std::uint64_t RawResult = UnixTimeSeconds;
        RawResult += SecondsBetweenWin32TimeAndUnixTime;
        RawResult *= 1000 * 1000 * 10;
        RawResult += UnixTimeNanoseconds / 100;
        FILETIME Result;
        Result.dwLowDateTime = static_cast<DWORD>(RawResult);
        Result.dwHighDateTime = static_cast<DWORD>(RawResult >> 32);
        return Result;
    }

    const std::int32_t g_SuperBlockSearchList[] = SBLOCKSEARCH;

    struct ArchivePropertyItem
    {
        PROPID Property;
        VARTYPE Type;
    };

    const ArchivePropertyItem g_ArchivePropertyItems[] =
    {
        { SevenZipArchiveModifiedTime, VT_FILETIME },
        { SevenZipArchiveFreeSpace, VT_UI8 },
        { SevenZipArchiveClusterSize, VT_UI4 },
        { SevenZipArchiveVolumeName, VT_BSTR },
    };

    const std::size_t g_ArchivePropertyItemsCount =
        sizeof(g_ArchivePropertyItems) / sizeof(*g_ArchivePropertyItems);
}

namespace NanaZip::Codecs::Archive
{
    struct Ufs : public Mile::ComObject<Ufs, IInArchive>
    {
    private:

        bool m_IsUfs2 = false;
        bool m_IsBigEndian = false;
        fs m_SuperBlock = { 0 };
        IInStream* m_FileStream = nullptr;
        bool m_IsInitialized = false;

    private:

        std::uint8_t ReadUInt8(
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
            return Base[0];
        }

        std::uint16_t ReadUInt16(
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
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
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
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
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
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

        std::int8_t ReadInt8(
            const void* BaseAddress)
        {
            return static_cast<std::int8_t>(this->ReadUInt8(BaseAddress));
        }

        std::int16_t ReadInt16(
            const void* BaseAddress)
        {
            return static_cast<std::int16_t>(this->ReadUInt16(BaseAddress));
        }

        std::int32_t ReadInt32(
            const void* BaseAddress)
        {
            return static_cast<std::int32_t>(this->ReadUInt32(BaseAddress));
        }

        std::int64_t ReadInt64(
            const void* BaseAddress)
        {
            return static_cast<std::int64_t>(this->ReadUInt64(BaseAddress));
        }

    private:

        std::uint64_t GetTotalBlocks()
        {
            return this->m_IsUfs2
                ? this->ReadInt64(&this->m_SuperBlock.fs_dsize)
                : this->ReadInt32(&this->m_SuperBlock.fs_old_dsize);
        }

        std::uint64_t GetCylinderGroupStart(
            std::uint64_t const& CylinderGroup)
        {
            std::uint64_t Result = CylinderGroup;
            Result *= this->ReadInt32(&this->m_SuperBlock.fs_fpg);
            if (!this->m_IsUfs2)
            {
                std::int32_t Offset = this->ReadInt32(
                    &this->m_SuperBlock.fs_old_cgoffset);
                std::int32_t Mask = this->ReadInt32(
                    &this->m_SuperBlock.fs_old_cgmask);
                Result += Offset * (CylinderGroup & ~Mask);
            }
            return Result;
        }

        std::int32_t GetFragmentBlockSize()
        {
            return this->ReadInt32(&this->m_SuperBlock.fs_fsize);
        }

        std::int32_t GetBlockSize()
        {
            return this->ReadInt32(&this->m_SuperBlock.fs_bsize);
        }

        std::uint64_t GetDirectoryInodeSize()
        {
            return this->m_IsUfs2 ? sizeof(ufs2_dinode) : sizeof(ufs1_dinode);
        }

        std::uint64_t GetInodeOffset(
            std::uint64_t const& Inode)
        {
            std::uint32_t InodePerCylinderGroup = this->ReadInt32(
                &this->m_SuperBlock.fs_fpg);
            std::uint64_t CylinderGroup = Inode / InodePerCylinderGroup;
            std::uint64_t SubIndex = Inode % InodePerCylinderGroup;
            std::uint64_t Result = this->GetCylinderGroupStart(CylinderGroup);
            Result += this->ReadInt32(&this->m_SuperBlock.fs_iblkno);
            Result *= this->GetFragmentBlockSize();
            Result += SubIndex * this->GetDirectoryInodeSize();
            return Result;
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

                    std::int64_t SuperBlockLocation = this->ReadInt64(
                        &this->m_SuperBlock.fs_sblockloc);
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

                    std::int32_t FragmentsCount = this->ReadInt32(
                        &this->m_SuperBlock.fs_frag);
                    if (FragmentsCount < 1)
                    {
                        hr = S_FALSE;
                        continue;
                    }

                    std::uint32_t CylinderGroupsCount = this->ReadUInt32(
                        &this->m_SuperBlock.fs_ncg);
                    if (CylinderGroupsCount < 1)
                    {
                        hr = S_FALSE;
                        continue;
                    }

                    std::int32_t BlockSize = this->ReadInt32(
                        &this->m_SuperBlock.fs_bsize);
                    if (BlockSize < MINBSIZE)
                    {
                        hr = S_FALSE;
                        continue;
                    }

                    std::int32_t SuperBlockSize = this->ReadInt32(
                        &this->m_SuperBlock.fs_sbsize);
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

                this->m_FileStream = Stream;
                this->m_FileStream->AddRef();

                if (OpenCallback)
                {
                    std::uint64_t TotalFiles =
                        this->ReadUInt32(&this->m_SuperBlock.fs_ncg);
                    TotalFiles *= this->ReadUInt32(&this->m_SuperBlock.fs_ipg);
                    TotalFiles -= UFS_ROOTINO;
                    std::uint64_t TotalBytes =
                        this->GetTotalBlocks() * this->GetFragmentBlockSize();
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                if (SUCCEEDED(Stream->Seek(
                    this->GetInodeOffset(UFS_ROOTINO),
                    STREAM_SEEK_SET,
                    nullptr)))
                {
                    std::vector<std::uint8_t> DirectoryInodeBuffer(
                        this->GetDirectoryInodeSize());
                    NumberOfBytesRead = 0;
                    if (SUCCEEDED(::NanaZipCodecsReadInputStream(
                        Stream,
                        &DirectoryInodeBuffer[0],
                        DirectoryInodeBuffer.size(),
                        &NumberOfBytesRead)))
                    {
                        ufs2_dinode* DirectoryInode =
                            reinterpret_cast<ufs2_dinode*>(
                                &DirectoryInodeBuffer[0]);

                        std::uint64_t FileSize =
                            this->ReadUInt64(&DirectoryInode->di_size);

                        std::uint64_t ActualFileSize = 0;

                        std::vector<std::uint64_t> BlockOffsets;

                        for (size_t i = 0; i < UFS_NDADDR; ++i)
                        {
                            BlockOffsets.emplace_back(
                                this->ReadInt64(&DirectoryInode->di_db[i]));
                            ActualFileSize += GetBlockSize();
                            if (ActualFileSize >= FileSize)
                            {
                                break;
                            }
                        }

                        if (ActualFileSize < FileSize)
                        {

                        }


                        DirectoryInode = DirectoryInode;
                    }
                }

            } while (false);

            if (FAILED(hr))
            {
                this->Close();
            }
            else
            {
                this->m_IsInitialized = true;
            }

            return hr;
        }

        HRESULT STDMETHODCALLTYPE Close()
        {
            this->m_IsInitialized = false;
            if (this->m_FileStream)
            {
                this->m_FileStream->Release();
                this->m_FileStream = nullptr;
            }
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
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(PropId);
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
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value)
        {
            if (!this->m_IsInitialized)
            {
                return S_FALSE;
            }

            if (!Value)
            {
                return E_INVALIDARG;
            }

            switch (PropId)
            {
            case SevenZipArchiveModifiedTime:
            {
                std::uint64_t PosixModifiedTimeSecond = 0;
                if (this->m_IsUfs2)
                {
                    PosixModifiedTimeSecond = this->ReadInt64(
                        &this->m_SuperBlock.fs_time);
                }
                else
                {
                    PosixModifiedTimeSecond = this->ReadInt32(
                        &this->m_SuperBlock.fs_old_time);
                }

                Value->filetime = ::ToFileTime(PosixModifiedTimeSecond, 0);
                Value->vt = VT_FILETIME;
                break;
            }
            case SevenZipArchivePhysicalSize:
            {
                Value->uhVal.QuadPart =
                    this->GetTotalBlocks() * this->GetFragmentBlockSize();
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveFreeSpace:
            {
                std::uint64_t FreeBlocks = 0;
                if (this->m_IsUfs2)
                {
                    FreeBlocks = this->ReadInt64(
                        &this->m_SuperBlock.fs_cstotal.cs_nbfree);
                }
                else
                {
                    FreeBlocks = this->ReadInt32(
                        &this->m_SuperBlock.fs_old_cstotal.cs_nbfree);
                }
                FreeBlocks *= this->ReadInt32(
                    &this->m_SuperBlock.fs_frag);
                if (this->m_IsUfs2)
                {
                    FreeBlocks += this->ReadInt64(
                        &this->m_SuperBlock.fs_cstotal.cs_nffree);
                }
                else
                {
                    FreeBlocks += this->ReadInt32(
                        &this->m_SuperBlock.fs_old_cstotal.cs_nffree);
                }
                FreeBlocks +=
                    this->ReadInt64(&this->m_SuperBlock.fs_pendingblocks)
                    >> this->ReadInt32(&this->m_SuperBlock.fs_fsbtodb);

                std::uint64_t FreeSize = this->GetFragmentBlockSize();
                FreeSize *= FreeBlocks;

                Value->uhVal.QuadPart = FreeSize;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveClusterSize:
            {
                Value->ulVal = this->GetFragmentBlockSize();
                Value->vt = VT_UI4;
                break;
            }
            case SevenZipArchiveVolumeName:
            {
                std::string VolumeName = std::string(
                    reinterpret_cast<char*>(this->m_SuperBlock.fs_volname),
                    MAXVOLLEN);
                VolumeName.resize(std::strlen(VolumeName.c_str()));
                Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                    CP_UTF8,
                    VolumeName).c_str());
                if (Value->bstrVal)
                {
                    Value->vt = VT_BSTR;
                }
                break;
            }
            default:
                break;
            }

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
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(Name);
            UNREFERENCED_PARAMETER(PropId);
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
            *NumProps = g_ArchivePropertyItemsCount;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetArchivePropertyInfo(
            _In_ UINT32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType)
        {
            if (!(Index < g_ArchivePropertyItemsCount))
            {
                return E_INVALIDARG;
            }

            if (!Name || !PropId || !VarType)
            {
                return E_INVALIDARG;
            }

            *Name = nullptr;
            *PropId = g_ArchivePropertyItems[Index].Property;
            *VarType = g_ArchivePropertyItems[Index].Type;
            return S_OK;
        }
    };

    IInArchive* CreateUfs()
    {
        return new Ufs();
    }
}
