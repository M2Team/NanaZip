/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Archive.Littlefs.cpp
 * PURPOSE:    Implementation for littlefs file system image readonly support
 *
 * LICENSE:    The MIT License
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
// unary minus operator applied to unsigned type, result still unsigned
#pragma warning(disable:4146)
#endif

#include "LittleFS/lfs.h"

#ifdef _MSC_VER
#if (_MSC_VER >= 1200)
#pragma warning(pop)
#else
// unary minus operator applied to unsigned type, result still unsigned
#pragma warning(default:4146)
#endif
#endif

#include "Mile.Helpers.Portable.Base.Unstaged.h"

namespace
{
    struct PropertyItem
    {
        PROPID Property;
        VARTYPE Type;
    };

    const PropertyItem g_ArchivePropertyItems[] =
    {
        { SevenZipArchiveFreeSpace, VT_UI8 },
        { SevenZipArchiveClusterSize, VT_UI4 },
        { SevenZipArchiveUnpackVersion, VT_UI8 },
    };

    const std::size_t g_ArchivePropertyItemsCount =
        sizeof(g_ArchivePropertyItems) / sizeof(*g_ArchivePropertyItems);

    const PropertyItem g_PropertyItems[] =
    {
        { SevenZipArchivePath, VT_BSTR },
        { SevenZipArchiveIsDirectory, VT_BOOL },
        { SevenZipArchiveSize, VT_UI8 },
        { SevenZipArchivePackSize, VT_UI8 },
        { SevenZipArchiveInode, VT_UI8 },
    };

    const std::size_t g_PropertyItemsCount =
        sizeof(g_PropertyItems) / sizeof(*g_PropertyItems);

    // References:
    // - https://github.com/littlefs-project/littlefs/blob/v1-prefix/SPEC.md
    // - https://github.com/littlefs-project/littlefs/blob/v1-prefix/lfs1.h
    // - https://github.com/littlefs-project/littlefs/blob/v2-prefix/SPEC.md
    // - https://github.com/littlefs-project/littlefs/blob/v2-prefix/lfs2.h

    // Notes:
    // - All values in littlefs v1 are stored in little-endian byte order.
    // - All values in littlefs v2 are stored in little-endian byte order, only
    //   except the tags are stored in commits are stored in big-endian.
    // - Block pointers are stored in 32 bits, with the special value 0xFFFFFFFF
    //   representing a null block address.

    const std::uint32_t LfsNullBlock = 0xFFFFFFFF;

    const char LfsSuperBlockMagic[] = { 'l', 'i', 't', 't', 'l', 'e', 'f', 's' };

    /**
     * @brief Pair-pointer for pointing to the specific metadata-pair in the
     *        filesystem. A null pair-pointer (0xffffffff, 0xffffffff) indicates
     *        the end of the list.
     */
    struct Lfs1MetadataPairPointer
    {
        std::uint32_t Lower;
        std::uint32_t Upper;
    };

    // Metadata pairs form the backbone of the littlefs and provide a system for
    // atomic updates. Even the superblock is stored in a metadata pair.
    // As their name suggests, a metadata pair is stored in two blocks, with one
    // block acting as a redundant backup in case the other is corrupted. These
    // two blocks could be anywhere in the disk and may not be next to each
    // other, so any pointers to directory pairs need to be stored as two block
    // pointers.

    /**
     * @brief The beginning of the metadata used in the metadata pairs.
     */
    struct Lfs1MetadataHeader
    {
        /**
         * @brief Incremented every update, only the uncorrupted metadata-block
         *        with the most recent revision count contains the valid
         *        metadata. Comparison between revision counts must use sequence
         *        comparison since the revision counts may overflow.
         */
        std::uint32_t Revision;

        /**
         * @brief Size in bytes of the contents in the current metadata block,
         *        including the metadata-pair metadata. Additionally, the
         *        highest bit of the dir size may be set to indicate that the
         *        directory's contents continue on the next metadata-pair
         *        pointed to by the tail pointer.
         */
        std::uint32_t Size;

        /**
         * @brief Pointer to the next metadata-pair in the filesystem. A null
         *        pair-pointer (0xffffffff, 0xffffffff) indicates the end of the
         *        list. If the highest bit in the dir size is set, this points
         *        to the next metadata-pair in the current directory, otherwise
         *        it points to an arbitrary metadata-pair. Starting with the
         *        superblock, the tail-pointers form a linked-list containing
         *        all metadata-pairs in the filesystem.
         */
        Lfs1MetadataPairPointer Tail;
    };

    /**
     * @brief The ending‌ of the metadata used in the metadata pairs.
     */
    struct Lfs1MetadataFooter
    {
        /**
         * @brief 32-bit CRC used to detect corruption from power-lost, from
         *        block end-of-life, or just from noise on the storage bus. The
         *        CRC is appended to the end of each metadata-block. The
         *        littlefs uses the standard CRC-32, which uses a polynomial of
         *        0x04c11db7, initialized with 0xffffffff.
         */
        std::uint32_t Crc;
    };

    /**
     * @brief Type of the entry, currently this is limited to the following.
     *        Additionally, the type is broken into two 4 bit nibbles, with the
     *        upper nibble specifying the type's data structure used when
     *        scanning the filesystem. The lower nibble clarifies the type
     *        further when multiple entries share the same data structure.
     *        The highest bit is reserved for marking the entry as "moved". If
     *        an entry is marked as "moved", the entry may also exist somewhere
     *        else in the filesystem. If the entry exists elsewhere, this entry
     *        must be treated as though it does not exist.
     */
    enum Lfs1EntryTypes
    {
        Lfs1FileEntry = 0x11,
        Lfs1DirectoryEntry = 0x22,
        Lfs1SuperBlockEntry = 0x2E,
    };

    struct Lfs1EntryHeader
    {
        /**
         * @brief Type of the entry, which is one of the Lfs1EntryTypes.
         */
        std::uint8_t EntryType;

        /**
         * @brief Length in bytes of the entry-specific data. This does not
         *        include the entry type size, attributes, or name. The full
         *        size in bytes of the entry is sizeof(Lfs1EntryHeader) +
         *        EntryLength + AttributeLength + NameLength.
         */
        std::uint8_t EntryLength;

        /**
         * @brief Length of system-specific attributes in bytes. Since
         *        attributes are system specific, there is not much
         *        guarantee on the values in this section, and systems
         *        are expected to work even when it is empty.
         */
        std::uint8_t AttributeLength;

        /**
         * @brief Length of the entry name. Entry names are stored as UTF8,
         *        although most systems will probably only support ASCII. Entry
         *        names can not contain '/' and can not be '.' or '..' as these
         *        are a part of the syntax of filesystem paths.
         */
        std::uint8_t NameLength;
    };

    /**
     * @brief The superblock is the anchor for the littlefs. The superblock is
     *        stored as a metadata pair containing a single superblock entry.
     *        It is through the superblock that littlefs can access the rest of
     *        the filesystem.
     *        The superblock can always be found in blocks 0 and 1, however
     *        fetching the superblock requires knowing the block size. The block
     *        size can be guessed by searching the beginning of disk for the
     *        string "littlefs", although currently the filesystems relies on
     *        the user providing the correct block size.
     *        The superblock is the most valuable block in the filesystem. It is
     *        updated very rarely, only during format or when the root directory
     *        must be moved. It is encouraged to always write out both
     *        superblock pairs even though it is not required.
     */
    struct Lfs1SuperBlockEntryContent
    {
        /**
         * @brief Pointer to the root directory's metadata pair.
         */
        Lfs1MetadataPairPointer RootDirectory;

        /**
         * @brief Size of the logical block size used by the filesystem.
         */
        std::uint32_t BlockSize;

        /**
         * @brief Number of blocks in the filesystem.
         */
        std::uint32_t BlockCount;

        /**
         * @brief The littlefs version encoded as a 32 bit value. The upper 16
         *        bits encodes the major version, which is incremented when a
         *        breaking-change is introduced in the filesystem specification.
         *        The lower 16 bits encodes the minor version, which is
         *        incremented when a backwards-compatible change is introduced.
         *        Non-standard Attribute changes do not change the version. This
         *        specification describes version 1.1 (0x00010001), which is the
         *        first version of littlefs.
         */
        std::uint32_t Version;
    };

    /**
     * @brief Directories are stored in entries with a pointer to the first
     *        metadata pair in the directory. Keep in mind that a directory
     *        may be composed of multiple metadata pairs connected by the tail
     *        pointer when the highest bit in the dir size is set.
     */
    struct Lfs1DirectoryEntryContent
    {
        /**
         * @brief Pointer to the first metadata pair in the directory.
         */
        Lfs1MetadataPairPointer Directory;
    };

    /**
     * @brief Files are stored in entries with a pointer to the head of the file
     *        and the size of the file. This is enough information to determine
     *        the state of the CTZ skip-list that is being referenced.
     *        A terribly quick summary: For every nth block where n is divisible
     *        by 2^x, the block contains a pointer to block n-2^x. These
     *        pointers are stored in increasing order of x in each block of the
     *        file preceding the data in the block.
     *        The maximum number of pointers in a block is bounded by the
     *        maximum file size divided by the block size. With 32 bits for file
     *        size, this results in a minimum block size of 104 bytes.
     */
    struct Lfs1FileEntryContent
    {
        /**
         * @brief Pointer to the block that is the head of the file's CTZ
         *        skip-list.
         */
        std::uint32_t FileHead;

        /**
         * @brief Size of file in bytes.
         */
        std::uint32_t FileSize;
    };

    // littlefs v2 definitions (work in progress)

    const std::uint32_t g_LfsInitialTag = 0xffffffff;

    // Version of On-disk data structures
    // Major (top-nibble), incremented on backwards incompatible changes
    // Minor (bottom-nibble), incremented on feature additions

    const std::uint32_t g_LfsDiskVersion = 0x00020001;
    const std::uint16_t g_LfsDiskVersionMajor =
        static_cast<const std::uint16_t>(g_LfsDiskVersion >> 16);
    const std::uint32_t g_LfsDiskVersionMinor =
        static_cast<const std::uint16_t>(g_LfsDiskVersion >> 0);

    // Maximum name size in bytes, may be redefined to reduce the size of the
    // info struct. Limited to <= 1022. Stored in superblock and must be
    // respected by other littlefs drivers.
    const std::uint32_t g_LfsMaximumNameLength = 255;

    // Maximum size of a file in bytes, may be redefined to limit to support
    // other drivers. Limited on disk to <= 2147483647. Stored in superblock
    // and must be respected by other littlefs drivers.
    const std::uint32_t g_LfsMaximumFileLength = 2147483647;

    // Maximum size of custom attributes in bytes, may be redefined, but there
    // is no real benefit to using a smaller g_LfsMaximumAttributeLength.
    // Limited to <= 1022. Stored in superblock and must be respected by other
    // littlefs drivers.
    const std::uint32_t g_LfsMaximumAttributeLength = 1022;

    // File types
    enum LfsTypes
    {
        // file types

        LfsTypeRegular = 0x001,
        LfsTypeDirectory = 0x002,

        // internally used types

        LfsTypeSplice = 0x400,
        LfsTypeName = 0x000,
        LfsTypeStruct = 0x200,
        LfsTypeUserAttribute = 0x300,
        LfsTypeFrom = 0x100,
        LfsTypeTail = 0x600,
        LfsTypeGlobals = 0x700,
        LfsTypeCrc = 0x500,

        // internally used type specializations

        LfsTypeCreate = 0x401,
        LfsTypeDelete = 0x4ff,
        LfsTypeSuperBlock = 0x0ff,
        LfsTypeDirectoryStructure = 0x200,
        LfsTypeCtzStructure = 0x202,
        LfsTypeInlineStructure = 0x201,
        LfsTypeSoftTail = 0x600,
        LfsTypeHardTail = 0x601,
        LfsTypeMoveState = 0x7ff,
        LfsTypeCcrc = 0x500,
        LfsTypeFcrc = 0x5ff,

        // internal chip sources

        LfsFromNoop = 0x000,
        LfsFromMove = 0x101,
        LfsFromUserAttributes = 0x102,
    };

    union LfsMetadataTag
    {
        std::uint32_t AsRaw;
        struct
        {
            std::uint32_t Length : 10;
            std::uint32_t Id : 10;
            std::uint32_t Type : 11;
            std::uint32_t Invalid : 1;
        } Information;

        bool CheckWith(
            std::uint16_t const& Type,
            std::uint16_t const& Id,
            std::uint16_t const& Size)
        {
            return (
                !this->Information.Invalid &&
                Type == this->Information.Type &&
                Id == this->Information.Id &&
                Size == this->Information.Length);
        }
    };

    // The superblock must always be the first entry (id 0) in the metadata
    // pair, and the name tag must always be the first tag in the metadata pair.
    // This makes it so that the magic string "littlefs" will always reside at
    // offset = 8 in a valid littlefs superblock.

    struct LfsSuperBlockInlineStructure
    {
        // The version of littlefs at format time. The version is encoded in a
        // 32-bit value with the upper 16-bits containing the major version, and
        // the lower 16-bits containing the minor version.
        // This specification describes version 2.0 (`0x00020000`).
        std::uint32_t Version;
        // Size of the logical block size used by the filesystem in bytes.
        std::uint32_t BlockSize;
        // Number of blocks in the filesystem.
        std::uint32_t BlockCount;
        // Maximum size of file names in bytes.
        std::uint32_t MaximumNameLength;
        // Maximum size of files in bytes.
        std::uint32_t MaximumFileLength;
        // Maximum size of file attributes in bytes.
        std::uint32_t MaximumAttributeLength;
    };

    struct LfsSuperMetadataHeader
    {
        std::uint32_t RawRevisionCount;
        std::uint32_t RawSuperBlockTag;
        char RawSignature[8];
        std::uint32_t RawStructureTag;
        LfsSuperBlockInlineStructure RawStructure;
    };

    struct LittlefsFilePathInformation
    {
        std::uint32_t Inode = 0;
        // Type of the file, either LfsTypeRegular or LfsTypeDirectory
        std::uint8_t Type = 0;
        std::uint32_t Size = 0;
        std::string Path;
        // TODO: Currently just ultimate raw block extractor for analyzing
        std::uint64_t Offset = 0;
        //std::vector<std::uint32_t> BlockOffsets;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct Littlefs : public Mile::ComObject<Littlefs, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
        LfsSuperMetadataHeader m_SuperMetadataHeader = {};
        std::vector<LittlefsFilePathInformation> m_FilePaths;
        bool m_IsInitialized = false;

    private:

        std::uint8_t ReadUInt8(
            const void* BaseAddress)
        {
            return ::MileReadUInt8(BaseAddress);
        }

        std::uint16_t ReadUInt16(
            const void* BaseAddress)
        {
            ::MileReadUInt16LittleEndian(BaseAddress);
        }

        std::uint32_t ReadUInt32(
            const void* BaseAddress)
        {
            return ::MileReadUInt32LittleEndian(BaseAddress);
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

        std::uint32_t ReadRawMetadataTag(
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
            // Tags stored in commits are actually stored in big-endian (and is
            // the only thing in littlefs stored in big-endian).
            return
                (static_cast<std::uint32_t>(Base[3])) |
                (static_cast<std::uint32_t>(Base[2]) << 8) |
                (static_cast<std::uint32_t>(Base[1]) << 16) |
                (static_cast<std::uint32_t>(Base[0]) << 24);
        }

    public:

        Littlefs()
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

                UINT64 BundleSize = 0;
                if (FAILED(this->m_FileStream->Seek(
                    0,
                    STREAM_SEEK_END,
                    &BundleSize)))
                {
                    break;
                }

                LfsMetadataTag PreviousTag = {};
                PreviousTag.AsRaw = g_LfsInitialTag;

                if (BundleSize < sizeof(LfsSuperMetadataHeader))
                {
                    break;
                }

                if (FAILED(this->ReadFileStream(
                    0,
                    &this->m_SuperMetadataHeader,
                    sizeof(LfsSuperMetadataHeader))))
                {
                    break;
                }

                PreviousTag.AsRaw ^= this->ReadRawMetadataTag(
                    &this->m_SuperMetadataHeader.RawSuperBlockTag);
                if (!PreviousTag.CheckWith(
                    LfsTypeSuperBlock,
                    0,
                    sizeof(::LfsSuperBlockMagic)))
                {
                    break;
                }

                if (0 != std::memcmp(
                    this->m_SuperMetadataHeader.RawSignature,
                    ::LfsSuperBlockMagic,
                    sizeof(::LfsSuperBlockMagic)))
                {
                    break;
                }

                PreviousTag.AsRaw ^= this->ReadRawMetadataTag(
                    &this->m_SuperMetadataHeader.RawStructureTag);
                if (!PreviousTag.CheckWith(
                    LfsTypeInlineStructure,
                    0,
                    sizeof(LfsSuperBlockInlineStructure)))
                {
                    break;
                }

                std::uint32_t DiskVersion = this->ReadUInt32(
                    &this->m_SuperMetadataHeader.RawStructure.Version);
                if (g_LfsDiskVersionMajor != static_cast<const std::uint16_t>(
                    DiskVersion >> 16))
                {
                    break;
                }

                std::uint32_t BlockSize = this->ReadUInt32(
                    &this->m_SuperMetadataHeader.RawStructure.BlockSize);
                std::uint32_t BlockCount = this->ReadUInt32(
                    &this->m_SuperMetadataHeader.RawStructure.BlockCount);

                std::uint64_t TotalFiles = 0;
                std::uint64_t TotalBytes = 0;

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                // TODO: this->GetAllPaths

                // TODO: OpenCallback->SetTotal with new TotalFiles

                this->m_FilePaths.clear();

                // TODO: Currently just ultimate raw block extractor for analyzing
                for (std::uint32_t i = 0; i < BlockCount; ++i)
                {
                    LittlefsFilePathInformation Information;
                    Information.Inode = i;
                    Information.Type = LfsTypeRegular;
                    Information.Size = BlockSize;
                    Information.Path = Mile::FormatString("[%d]", i);
                    Information.Offset = i * BlockSize;
                    this->m_FilePaths.emplace_back(Information);
                }

                hr = S_OK;

            } while (false);

            if (hr != S_OK)
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
            this->m_SuperMetadataHeader = {};
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

            LittlefsFilePathInformation& Information = this->m_FilePaths[Index];

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
                Value->boolVal = LfsTypeDirectory == Information.Type
                    ? VARIANT_TRUE
                    : VARIANT_FALSE;
                Value->vt = VT_BOOL;
                break;
            }
            case SevenZipArchiveSize:
            {
                Value->uhVal.QuadPart = Information.Size;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchivePackSize:
            {
                Value->uhVal.QuadPart = Information.Size;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveInode:
            {
                Value->uhVal.QuadPart = Information.Inode;
                Value->vt = VT_UI8;
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
                LittlefsFilePathInformation& Information =
                    this->m_FilePaths[ActualFileIndex];
                TotalSize += Information.Size;
            }
            ExtractCallback->SetTotal(TotalSize);

            UINT64 Completed = 0;

            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                LittlefsFilePathInformation& Information =
                    this->m_FilePaths[ActualFileIndex];

                Completed += Information.Size;
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

                bool Succeeded = false;

                // TODO: Currently just ultimate raw block extractor for analyzing

                std::vector<std::uint8_t> Buffer(Information.Size);
                if (SUCCEEDED(this->ReadFileStream(
                    Information.Offset,
                    &Buffer[0],
                    Information.Size)))
                {
                    UINT32 ProceededSize = 0;
                    Succeeded = SUCCEEDED(OutputStream->Write(
                        &Buffer[0],
                        static_cast<UINT32>(Information.Size),
                        &ProceededSize));
                }

                OutputStream->Release();

                ExtractCallback->SetOperationResult(
                    Succeeded
                    ? SevenZipExtractOperationResultSuccess
                    : SevenZipExtractOperationResultUnavailable);
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetArchiveProperty(
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value)
        {
            if (!Value)
            {
                return E_INVALIDARG;
            }

            if (!this->m_IsInitialized)
            {
                Value->vt = VT_EMPTY;
                return S_OK;
            }

            switch (PropId)
            {
            case SevenZipArchivePhysicalSize:
            {
                std::uint32_t BlockSize = this->ReadUInt32(
                    &this->m_SuperMetadataHeader.RawStructure.BlockSize);
                std::uint32_t BlockCount = this->ReadUInt32(
                    &this->m_SuperMetadataHeader.RawStructure.BlockCount);

                Value->uhVal.QuadPart = BlockSize * BlockCount;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveFreeSpace:
            {
                // TODO: Unimplemented
                Value->uhVal.QuadPart = 0;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveClusterSize:
            {
                std::uint32_t BlockSize = this->ReadUInt32(
                    &this->m_SuperMetadataHeader.RawStructure.BlockSize);

                Value->ulVal = BlockSize;
                Value->vt = VT_UI4;
                break;
            }
            case SevenZipArchiveUnpackVersion:
            {
                std::uint32_t DiskVersion = this->ReadUInt32(
                    &this->m_SuperMetadataHeader.RawStructure.Version);
                Value->uhVal.QuadPart = static_cast<UINT64>(DiskVersion >> 16);
                Value->vt = VT_UI8;
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

    IInArchive* CreateLittlefs()
    {
        return new Littlefs();
    }
}
