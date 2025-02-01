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

#include <map>

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

    struct PropertyItem
    {
        PROPID Property;
        VARTYPE Type;
    };

    const PropertyItem g_ArchivePropertyItems[] =
    {
        { SevenZipArchiveModifiedTime, VT_FILETIME },
        { SevenZipArchiveFreeSpace, VT_UI8 },
        { SevenZipArchiveClusterSize, VT_UI4 },
        { SevenZipArchiveVolumeName, VT_BSTR },
    };

    const std::size_t g_ArchivePropertyItemsCount =
        sizeof(g_ArchivePropertyItems) / sizeof(*g_ArchivePropertyItems);

    const PropertyItem g_PropertyItems[] =
    {
        { SevenZipArchivePath, VT_BSTR },
        { SevenZipArchiveIsDirectory, VT_BOOL },
        { SevenZipArchiveSize, VT_UI8 },
        { SevenZipArchivePackSize, VT_UI8 },
        { SevenZipArchiveCreationTime, VT_FILETIME },
        { SevenZipArchiveAccessTime, VT_FILETIME },
        { SevenZipArchiveModifiedTime, VT_FILETIME },
        { SevenZipArchiveLinks, VT_UI8 },
        { SevenZipArchivePosixAttributes, VT_UI4 },
        { SevenZipArchiveSymbolicLink, VT_BSTR },
        { SevenZipArchiveInode, VT_UI8 },
        { SevenZipArchiveUserId, VT_UI4 },
        { SevenZipArchiveGroupId, VT_UI4 },
    };

    const std::size_t g_PropertyItemsCount =
        sizeof(g_PropertyItems) / sizeof(*g_PropertyItems);

    struct UfsInodeInformation
    {
        std::uint16_t Mode = 0;
        std::uint16_t NumberOfHardLinks = 0;
        std::uint32_t OwnerUserId = 0;
        std::uint32_t GroupId = 0;
        std::int64_t DeviceId = 0;
        std::uint64_t FileSize = 0;
        std::uint64_t AllocatedBlocks = 0;
        std::int64_t LastAccessTimeSeconds = 0;
        std::int64_t LastWriteTimeSeconds = 0;
        std::int64_t ChangeTimeSeconds = 0;
        std::int64_t BirthTimeSeconds = 0;
        std::int32_t LastAccessTimeNanoseconds = 0;
        std::int32_t LastWriteTimeNanoseconds = 0;
        std::int32_t ChangeTimeNanoseconds = 0;
        std::int32_t BirthTimeNanoseconds = 0;
        std::string EmbeddedSymbolLink;
        std::vector<std::uint64_t> BlockOffsets;
    };

    struct UfsFilePathInformation
    {
        std::string Path;
        std::uint32_t Inode = 0;
        UfsInodeInformation Information;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct Ufs : public Mile::ComObject<Ufs, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
        bool m_IsUfs2 = false;
        bool m_IsBigEndian = false;
        fs m_SuperBlock = { 0 };
        std::map<std::string, std::uint32_t> m_TemporaryFilePaths;
        std::vector<UfsFilePathInformation> m_FilePaths;
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

        HRESULT ReadFileStream(
            _In_ INT64 Offset,
            _Out_ PVOID Buffer,
            _In_ SIZE_T NumberOfBytesToRead)
        {
            if (!this->m_FileStream)
            {
                return S_FALSE;
            }

            if (SUCCEEDED(this->m_FileStream->Seek(
                Offset,
                STREAM_SEEK_SET,
                nullptr)))
            {
                SIZE_T NumberOfBytesRead = 0;
                if (SUCCEEDED(::NanaZipCodecsReadInputStream(
                    this->m_FileStream,
                    Buffer,
                    NumberOfBytesToRead,
                    &NumberOfBytesRead)))
                {
                    if (NumberOfBytesToRead == NumberOfBytesRead)
                    {
                        return S_OK;
                    }
                }
            }

            return S_FALSE;
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

        std::int32_t GetMaximumEmbeddedSymbolLinkLength()
        {
            return this->ReadInt32(&this->m_SuperBlock.fs_maxsymlinklen);
        }

        std::uint64_t GetInodeOffset(
            std::uint32_t const& Inode)
        {
            std::uint32_t InodePerCylinderGroup = this->ReadUInt32(
                &this->m_SuperBlock.fs_ipg);
            std::uint32_t CylinderGroup = Inode / InodePerCylinderGroup;
            std::uint32_t SubIndex = Inode % InodePerCylinderGroup;
            std::uint64_t Result = this->GetCylinderGroupStart(CylinderGroup);
            Result += this->ReadInt32(&this->m_SuperBlock.fs_iblkno);
            Result *= this->GetFragmentBlockSize();
            Result += SubIndex * this->GetDirectoryInodeSize();
            return Result;
        }

        bool GetInodeInformation(
            std::uint32_t const& Inode,
            UfsInodeInformation& Information)
        {
            Information = UfsInodeInformation();

            std::vector<std::uint8_t> DirectoryInodeBuffer(
                this->GetDirectoryInodeSize());
            ufs1_dinode* Ufs1DirectoryInode = reinterpret_cast<ufs1_dinode*>(
                &DirectoryInodeBuffer[0]);
            ufs2_dinode* Ufs2DirectoryInode = reinterpret_cast<ufs2_dinode*>(
                &DirectoryInodeBuffer[0]);

            if (FAILED(this->ReadFileStream(
                this->GetInodeOffset(Inode),
                &DirectoryInodeBuffer[0],
                DirectoryInodeBuffer.size())))
            {
                return false;
            }

            Information.Mode = this->ReadUInt16(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_mode
                : &Ufs1DirectoryInode->di_mode);
            Information.NumberOfHardLinks = this->ReadUInt16(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_nlink
                : &Ufs1DirectoryInode->di_nlink);
            Information.OwnerUserId = this->ReadUInt32(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_uid
                : &Ufs1DirectoryInode->di_uid);
            Information.GroupId = this->ReadUInt32(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_gid
                : &Ufs1DirectoryInode->di_gid);
            Information.DeviceId = this->m_IsUfs2
                ? this->ReadInt64(&Ufs2DirectoryInode->di_db[0])
                : this->ReadInt32(&Ufs1DirectoryInode->di_db[0]);
            Information.FileSize = this->ReadUInt64(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_size
                : &Ufs1DirectoryInode->di_size);
            Information.AllocatedBlocks = this->m_IsUfs2
                ? this->ReadInt64(&Ufs2DirectoryInode->di_blocks)
                : this->ReadInt32(&Ufs1DirectoryInode->di_blocks);
            Information.LastAccessTimeSeconds = this->m_IsUfs2
                ? this->ReadInt64(&Ufs2DirectoryInode->di_atime)
                : this->ReadInt32(&Ufs1DirectoryInode->di_atime);
            Information.LastWriteTimeSeconds = this->m_IsUfs2
                ? this->ReadInt64(&Ufs2DirectoryInode->di_mtime)
                : this->ReadInt32(&Ufs1DirectoryInode->di_mtime);
            Information.ChangeTimeSeconds = this->m_IsUfs2
                ? this->ReadInt64(&Ufs2DirectoryInode->di_ctime)
                : this->ReadInt32(&Ufs1DirectoryInode->di_ctime);
            if (this->m_IsUfs2)
            {
                Information.BirthTimeSeconds = this->ReadInt64(
                    &Ufs2DirectoryInode->di_birthtime);
            }
            Information.LastAccessTimeNanoseconds = this->ReadInt32(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_atimensec
                : &Ufs1DirectoryInode->di_atimensec);
            Information.LastWriteTimeNanoseconds = this->ReadInt32(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_mtimensec
                : &Ufs1DirectoryInode->di_mtimensec);
            Information.ChangeTimeNanoseconds = this->ReadInt32(
                this->m_IsUfs2
                ? &Ufs2DirectoryInode->di_ctimensec
                : &Ufs1DirectoryInode->di_ctimensec);
            if (this->m_IsUfs2)
            {
                Information.BirthTimeNanoseconds = this->ReadInt32(
                    &Ufs2DirectoryInode->di_birthnsec);
            }

            if ((Information.Mode & IFMT) == IFLNK)
            {
                if (Information.FileSize
                    <= this->GetMaximumEmbeddedSymbolLinkLength())
                {
                    std::string EmbeddedSymbolLink = std::string(
                        reinterpret_cast<char*>(this->m_IsUfs2
                            ? Ufs2DirectoryInode->di_shortlink
                            : Ufs1DirectoryInode->di_shortlink),
                        this->m_IsUfs2
                        ? sizeof(Ufs2DirectoryInode->di_shortlink)
                        : sizeof(Ufs1DirectoryInode->di_shortlink));
                    EmbeddedSymbolLink.resize(
                        std::strlen(EmbeddedSymbolLink.c_str()));
                    Information.EmbeddedSymbolLink = EmbeddedSymbolLink;
                    return true;
                }
            }

            std::int32_t BlockSize = this->GetBlockSize();
            std::int32_t FragmentBlockSize = this->GetFragmentBlockSize();
            std::uint64_t ActualFileSize = 0;

            for (std::size_t i = 0; i < UFS_NDADDR; ++i)
            {
                std::int64_t CurrentBlock = this->m_IsUfs2
                    ? this->ReadInt64(&Ufs2DirectoryInode->di_db[i])
                    : this->ReadInt32(&Ufs1DirectoryInode->di_db[i]);
                Information.BlockOffsets.emplace_back(
                    CurrentBlock * FragmentBlockSize);
                ActualFileSize += BlockSize;
                if (ActualFileSize >= Information.FileSize)
                {
                    return true;
                }
            }

            std::vector<std::uint8_t> IndirectBuffer0(BlockSize);
            std::vector<std::uint8_t> IndirectBuffer1(BlockSize);
            std::vector<std::uint8_t> IndirectBuffer2(BlockSize);

            std::int32_t* Ufs1IndirectBlocks0 =
                reinterpret_cast<std::int32_t*>(&IndirectBuffer0[0]);
            std::int32_t* Ufs1IndirectBlocks1 =
                reinterpret_cast<std::int32_t*>(&IndirectBuffer1[0]);
            std::int32_t* Ufs1IndirectBlocks2 =
                reinterpret_cast<std::int32_t*>(&IndirectBuffer2[0]);
            std::uint64_t Ufs1IndirectBlockMaximum =
                BlockSize / sizeof(std::int32_t);

            std::int64_t* Ufs2IndirectBlocks0 =
                reinterpret_cast<std::int64_t*>(&IndirectBuffer0[0]);
            std::int64_t* Ufs2IndirectBlocks1 =
                reinterpret_cast<std::int64_t*>(&IndirectBuffer1[0]);
            std::int64_t* Ufs2IndirectBlocks2 =
                reinterpret_cast<std::int64_t*>(&IndirectBuffer2[0]);
            std::uint64_t Ufs2IndirectBlockMaximum =
                BlockSize / sizeof(std::int64_t);

            std::uint64_t IndirectBlockMaximum = this->m_IsUfs2
                ? Ufs2IndirectBlockMaximum
                : Ufs1IndirectBlockMaximum;

            if (FAILED(this->ReadFileStream(
                FragmentBlockSize * (this->m_IsUfs2
                    ? this->ReadInt64(&Ufs2DirectoryInode->di_ib[0])
                    : this->ReadInt32(&Ufs1DirectoryInode->di_ib[0])),
                &IndirectBuffer0[0],
                BlockSize)))
            {
                return false;
            }

            for (std::size_t i = 0; i < IndirectBlockMaximum; ++i)
            {
                std::int64_t CurrentBlock = this->m_IsUfs2
                    ? this->ReadInt64(&Ufs2IndirectBlocks0[i])
                    : this->ReadInt32(&Ufs1IndirectBlocks0[i]);
                Information.BlockOffsets.emplace_back(
                    CurrentBlock * FragmentBlockSize);
                ActualFileSize += BlockSize;
                if (ActualFileSize >= Information.FileSize)
                {
                    return true;
                }
            }

            if (FAILED(this->ReadFileStream(
                FragmentBlockSize * (this->m_IsUfs2
                    ? this->ReadInt64(&Ufs2DirectoryInode->di_ib[1])
                    : this->ReadInt32(&Ufs1DirectoryInode->di_ib[1])),
                &IndirectBuffer1[0],
                BlockSize)))
            {
                return false;
            }

            for (std::size_t j = 0; j < IndirectBlockMaximum; ++j)
            {
                if (FAILED(this->ReadFileStream(
                    FragmentBlockSize * (this->m_IsUfs2
                        ? this->ReadInt64(&Ufs2IndirectBlocks1[j])
                        : this->ReadInt32(&Ufs1IndirectBlocks1[j])),
                    &IndirectBuffer0[0],
                    BlockSize)))
                {
                    return false;
                }

                for (std::size_t i = 0; i < IndirectBlockMaximum; ++i)
                {
                    std::int64_t CurrentBlock = this->m_IsUfs2
                        ? this->ReadInt64(&Ufs2IndirectBlocks0[i])
                        : this->ReadInt32(&Ufs1IndirectBlocks0[i]);
                    Information.BlockOffsets.emplace_back(
                        CurrentBlock * FragmentBlockSize);
                    ActualFileSize += BlockSize;
                    if (ActualFileSize >= Information.FileSize)
                    {
                        return true;
                    }
                }
            }

            if (FAILED(this->ReadFileStream(
                FragmentBlockSize * (this->m_IsUfs2
                    ? this->ReadInt64(&Ufs2DirectoryInode->di_ib[2])
                    : this->ReadInt32(&Ufs1DirectoryInode->di_ib[2])),
                &IndirectBuffer2[0],
                BlockSize)))
            {
                return false;
            }

            for (std::size_t k = 0; k < IndirectBlockMaximum; ++k)
            {
                if (FAILED(this->ReadFileStream(
                    FragmentBlockSize * (this->m_IsUfs2
                        ? this->ReadInt64(&Ufs2IndirectBlocks2[k])
                        : this->ReadInt32(&Ufs1IndirectBlocks2[k])),
                    &IndirectBuffer1[0],
                    BlockSize)))
                {
                    return false;
                }

                for (std::size_t j = 0; j < IndirectBlockMaximum; ++j)
                {
                    if (FAILED(this->ReadFileStream(
                        FragmentBlockSize * (this->m_IsUfs2
                            ? this->ReadInt64(&Ufs2IndirectBlocks1[j])
                            : this->ReadInt32(&Ufs1IndirectBlocks1[j])),
                        &IndirectBuffer0[0],
                        BlockSize)))
                    {
                        return false;
                    }

                    for (std::size_t i = 0; i < IndirectBlockMaximum; ++i)
                    {
                        std::int64_t CurrentBlock = this->m_IsUfs2
                            ? this->ReadInt64(&Ufs2IndirectBlocks0[i])
                            : this->ReadInt32(&Ufs1IndirectBlocks0[i]);
                        Information.BlockOffsets.emplace_back(
                            CurrentBlock * FragmentBlockSize);
                        ActualFileSize += BlockSize;
                        if (ActualFileSize >= Information.FileSize)
                        {
                            return true;
                        }
                    }
                }
            }

            return true;
        }

        void GetAllPaths(
            std::uint32_t const& RootInode,
            std::string const& RootPath)
        {
            UfsInodeInformation Information;
            if (!this->GetInodeInformation(RootInode, Information))
            {
                return;
            }

            if (!RootPath.empty())
            {
                UfsFilePathInformation Current;
                Current.Path = RootPath;
                // Remove the slash.
                Current.Path.resize(Current.Path.size() - 1);
                Current.Inode = RootInode;
                Current.Information = Information;
                this->m_FilePaths.emplace_back(Current);
            }

            std::int32_t BlockSize = this->GetBlockSize();

            std::size_t BlockOffsetsCount = Information.BlockOffsets.size();

            std::vector<std::uint8_t> Buffer(BlockOffsetsCount * BlockSize);
            direct* DirectoryEntries = reinterpret_cast<direct*>(&Buffer[0]);
            for (std::size_t i = 0; i < BlockOffsetsCount; ++i)
            {
                if (FAILED(this->ReadFileStream(
                    Information.BlockOffsets[i],
                    &Buffer[i * BlockSize],
                    BlockSize)))
                {
                    return;
                }
            }

            direct* End = reinterpret_cast<direct*>(
                &Buffer[Information.FileSize]);

            for (direct* Current = DirectoryEntries; Current < End;)
            {
                std::uint32_t Inode = this->ReadUInt32(&Current->d_ino);
                std::uint16_t RecordLength = this->ReadUInt16(&Current->d_reclen);
                std::uint8_t Type = this->ReadUInt8(&Current->d_type);
                std::uint8_t NameLength = this->ReadUInt8(&Current->d_namlen);
                Current->d_name[NameLength] = '\0';
                std::string Name = std::string(Current->d_name);
                if (Name == "." || Name == "..")
                {
                    // Just Skip
                }
                else if (DT_DIR == Type)
                {
                    this->GetAllPaths(Inode, RootPath + Name + "/");
                }
                else
                {
                    this->m_TemporaryFilePaths.emplace(RootPath + Name, Inode);
                }
                Current = reinterpret_cast<direct*>(
                    reinterpret_cast<std::size_t>(Current) + RecordLength);
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
            UNREFERENCED_PARAMETER(MaxCheckStartPosition);
            UNREFERENCED_PARAMETER(OpenCallback);

            if (!Stream)
            {
                return E_INVALIDARG;
            }

            HRESULT hr = S_FALSE;

            do
            {
                this->Close();

                this->m_FileStream = Stream;
                this->m_FileStream->AddRef();

                for (size_t i = 0; -1 != g_SuperBlockSearchList[i]; ++i)
                {
                    if (FAILED(this->ReadFileStream(
                        g_SuperBlockSearchList[i],
                        &this->m_SuperBlock,
                        sizeof(this->m_SuperBlock))))
                    {
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
                            continue;
                        }
                    }

                    std::int64_t SuperBlockLocation = this->ReadInt64(
                        &this->m_SuperBlock.fs_sblockloc);
                    if (this->m_IsUfs2)
                    {
                        if (SBLOCK_UFS2 != SuperBlockLocation)
                        {
                            continue;
                        }
                    }
                    else
                    {
                        if (SuperBlockLocation < 0 ||
                            SuperBlockLocation > SBLOCK_UFS1)
                        {
                            continue;
                        }
                    }

                    std::int32_t FragmentsCount = this->ReadInt32(
                        &this->m_SuperBlock.fs_frag);
                    if (FragmentsCount < 1)
                    {
                        continue;
                    }

                    std::uint32_t CylinderGroupsCount = this->ReadUInt32(
                        &this->m_SuperBlock.fs_ncg);
                    if (CylinderGroupsCount < 1)
                    {
                        continue;
                    }

                    std::int32_t BlockSize = this->ReadInt32(
                        &this->m_SuperBlock.fs_bsize);
                    if (BlockSize < MINBSIZE)
                    {
                        continue;
                    }

                    std::int32_t SuperBlockSize = this->ReadInt32(
                        &this->m_SuperBlock.fs_sbsize);
                    if (SuperBlockSize > SBLOCKSIZE ||
                        SuperBlockSize < sizeof(fs))
                    {
                        continue;
                    }

                    hr = S_OK;
                    break;
                }
                if (S_OK != hr)
                {
                    break;
                }

                std::uint64_t TotalFiles = 0;
                std::uint64_t TotalBytes =
                    this->GetTotalBlocks() * this->GetFragmentBlockSize();

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                this->m_FilePaths.clear();

                this->GetAllPaths(UFS_ROOTINO, "");
                TotalFiles = this->m_FilePaths.size();

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                for (auto const& Item : this->m_TemporaryFilePaths)
                {
                    UfsFilePathInformation Current;
                    Current.Path = Item.first;
                    Current.Inode = Item.second;
                    if (!this->GetInodeInformation(
                        Current.Inode,
                        Current.Information))
                    {
                        continue;
                    }
                    this->m_FilePaths.emplace_back(Current);
                }
                this->m_TemporaryFilePaths.clear();

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
            this->m_FilePaths.clear();
            std::memset(&this->m_SuperBlock, 0, sizeof(this->m_SuperBlock));
            this->m_IsBigEndian = false;
            this->m_IsUfs2 = false;
            if (this->m_FileStream)
            {
                this->m_FileStream->Release();
                this->m_FileStream = nullptr;
            }
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfItems(
            _Out_ PUINT32 NumItems)
        {
            if (!this->m_IsInitialized)
            {
                return S_FALSE;
            }

            if (!NumItems)
            {
                return E_INVALIDARG;
            }

            *NumItems = static_cast<UINT32>(this->m_FilePaths.size());
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetProperty(
            _In_ UINT32 Index,
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value)
        {
            if (!this->m_IsInitialized)
            {
                return S_FALSE;
            }

            if (!(Index < this->m_FilePaths.size()))
            {
                return E_INVALIDARG;
            }

            UfsInodeInformation& Information =
                this->m_FilePaths[Index].Information;

            switch (PropId)
            {
            case SevenZipArchivePath:
            {
                Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                    CP_UTF8,
                    this->m_FilePaths[Index].Path).c_str());
                if (Value->bstrVal)
                {
                    Value->vt = VT_BSTR;
                }
                break;
            }
            case SevenZipArchiveIsDirectory:
            {
                Value->boolVal =
                    Information.Mode & IFDIR
                    ? VARIANT_TRUE
                    : VARIANT_FALSE;
                Value->vt = VT_BOOL;
                break;
            }
            case SevenZipArchiveSize:
            {
                Value->uhVal.QuadPart = Information.FileSize;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchivePackSize:
            {
                // block size used in the stat struct
                const std::uint64_t S_BLKSIZE = 512;
                Value->uhVal.QuadPart = Information.AllocatedBlocks * S_BLKSIZE;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveCreationTime:
            {
                if (this->m_IsUfs2)
                {
                    Value->filetime = ::ToFileTime(
                        Information.BirthTimeSeconds,
                        Information.BirthTimeNanoseconds);
                    Value->vt = VT_FILETIME;
                }
                break;
            }
            case SevenZipArchiveAccessTime:
            {
                Value->filetime = ::ToFileTime(
                    Information.LastAccessTimeSeconds,
                    Information.LastAccessTimeNanoseconds);
                Value->vt = VT_FILETIME;
                break;
            }
            case SevenZipArchiveModifiedTime:
            {
                Value->filetime = ::ToFileTime(
                    Information.LastWriteTimeSeconds,
                    Information.LastWriteTimeNanoseconds);
                Value->vt = VT_FILETIME;
                break;
            }
            case SevenZipArchiveLinks:
            {
                Value->uhVal.QuadPart = Information.NumberOfHardLinks;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchivePosixAttributes:
            {
                Value->ulVal = Information.Mode;
                Value->vt = VT_UI4;
                break;
            }
            case SevenZipArchiveSymbolicLink:
            {
                if ((Information.Mode & IFMT) == IFLNK)
                {
                    std::string SymbolLink;

                    if (!Information.EmbeddedSymbolLink.empty())
                    {
                        SymbolLink = Information.EmbeddedSymbolLink;
                    }
                    else
                    {
                        std::int32_t BlockSize =
                            this->GetBlockSize();
                        std::size_t BlockOffsetsCount =
                            Information.BlockOffsets.size();
                        std::vector<std::uint8_t> Buffer(
                            BlockOffsetsCount * BlockSize);
                        for (std::size_t i = 0; i < BlockOffsetsCount; ++i)
                        {
                            if (FAILED(this->ReadFileStream(
                                Information.BlockOffsets[i],
                                &Buffer[i * BlockSize],
                                BlockSize)))
                            {
                                Buffer.clear();
                                break;
                            }
                        }
                        if (!Buffer.empty())
                        {
                            SymbolLink = std::string(
                                reinterpret_cast<char*>(&Buffer[0]),
                                Information.FileSize);
                        }
                    }

                    if (!SymbolLink.empty())
                    {
                        Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                            CP_UTF8,
                            SymbolLink).c_str());
                        if (Value->bstrVal)
                        {
                            Value->vt = VT_BSTR;
                        }
                    }
                }
                break;
            }
            case SevenZipArchiveInode:
            {
                Value->uhVal.QuadPart = this->m_FilePaths[Index].Inode;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveUserId:
            {
                Value->ulVal = Information.OwnerUserId;
                Value->vt = VT_UI4;
                break;
            }
            case SevenZipArchiveGroupId:
            {
                Value->ulVal = Information.GroupId;
                Value->vt = VT_UI4;
                break;
            }
            default:
                break;
            }

            UNREFERENCED_PARAMETER(Value);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Extract(
            _In_opt_ const PUINT32 Indices,
            _In_ UINT32 NumItems,
            _In_ BOOL TestMode,
            _In_ IArchiveExtractCallback* ExtractCallback)
        {
            if (!this->m_IsInitialized)
            {
                return S_FALSE;
            }

            HRESULT hr = S_OK;

            INT32 AskMode = TestMode
                ? SevenZipExtractAskModeTest
                : SevenZipExtractAskModeExtract;

            const bool AllFilesMode =
                static_cast<UINT32>(-1) == NumItems;
            if (AllFilesMode)
            {
                NumItems = static_cast<UINT32>(this->m_FilePaths.size());
            }

            UINT64 TotalSize = 0;
            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                UfsInodeInformation& Information =
                    this->m_FilePaths[ActualFileIndex].Information;
                TotalSize += Information.FileSize;
            }
            ExtractCallback->SetTotal(TotalSize);

            UINT64 Completed = 0;
            
            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                UfsInodeInformation& Information =
                    this->m_FilePaths[ActualFileIndex].Information;

                Completed += Information.FileSize;
                hr = ExtractCallback->SetCompleted(&Completed);
                if (FAILED(hr))
                {
                    continue;
                }

                hr = ExtractCallback->PrepareOperation(AskMode);
                if (FAILED(hr))
                {
                    continue;
                }

                ISequentialOutStream* OutputStream = nullptr;
                hr = ExtractCallback->GetStream(
                    Indices[i],
                    &OutputStream,
                    AskMode);
                if (FAILED(hr))
                {
                    continue;
                }
                if (!OutputStream)
                {
                    continue;
                }

                std::uint32_t BlockSize = this->GetBlockSize();

                UINT32 Todo = static_cast<UINT32>(Information.FileSize);
                UINT32 Done = 0;

                bool Failed = false;
                for (std::uint64_t const& Offset : Information.BlockOffsets)
                {
                    if (!Todo)
                    {
                        // Done
                        break;
                    }

                    UINT32 CurrentDo = Todo > BlockSize ? BlockSize : Todo;

                    std::vector<std::uint8_t> Buffer(CurrentDo);
                    if (FAILED(this->ReadFileStream(
                        Offset,
                        &Buffer[0],
                        CurrentDo)))
                    {
                        Failed = true;
                        break;
                    }

                    UINT32 ProceededSize = 0;
                    hr = OutputStream->Write(
                        &Buffer[0],
                        CurrentDo,
                        &ProceededSize);
                    if (FAILED(hr))
                    {
                        Failed = true;
                        break;
                    }

                    Todo -= CurrentDo;
                    Done += CurrentDo;
                }

                OutputStream->Release();

                ExtractCallback->SetOperationResult(
                    Failed
                    ? SevenZipExtractOperationResultUnavailable
                    : SevenZipExtractOperationResultSuccess);
            }

            return S_OK;
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

            *NumProps = g_PropertyItemsCount;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetPropertyInfo(
            _In_ UINT32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType)
        {
            if (!(Index < g_PropertyItemsCount))
            {
                return E_INVALIDARG;
            }

            if (!Name || !PropId || !VarType)
            {
                return E_INVALIDARG;
            }

            *Name = nullptr;
            *PropId = g_PropertyItems[Index].Property;
            *VarType = g_PropertyItems[Index].Type;
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
