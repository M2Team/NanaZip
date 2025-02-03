/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.Littlefs.cpp
 * PURPOSE:   Implementation for littlefs file system image readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

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
    // - https://github.com/littlefs-project/littlefs/blob/master/SPEC.md
    // - https://github.com/littlefs-project/littlefs/blob/master/lfs.h

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

    const char g_LfsSignature[] = { 'l', 'i', 't', 't', 'l', 'e', 'f', 's' };

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
        LfsSuperMetadataHeader m_SuperMetadataHeader = { 0 };
        std::vector<LittlefsFilePathInformation> m_FilePaths;
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
            return
                (static_cast<std::uint16_t>(Base[0])) |
                (static_cast<std::uint16_t>(Base[1]) << 8);
        }

        std::uint32_t ReadUInt32(
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
            return
                (static_cast<std::uint32_t>(Base[0])) |
                (static_cast<std::uint32_t>(Base[1]) << 8) |
                (static_cast<std::uint32_t>(Base[2]) << 16) |
                (static_cast<std::uint32_t>(Base[3]) << 24);
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

                LfsMetadataTag PreviousTag = { 0 };
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
                    sizeof(g_LfsSignature)))
                {
                    break;
                }

                if (0 != std::memcmp(
                    this->m_SuperMetadataHeader.RawSignature,
                    g_LfsSignature,
                    sizeof(g_LfsSignature)))
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
            this->m_SuperMetadataHeader = { 0 };
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
