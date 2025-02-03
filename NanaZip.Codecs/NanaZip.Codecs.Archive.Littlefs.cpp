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

    struct LittlefsFilePathInformation
    {
        std::uint32_t Inode = 0;
        // Type of the file, either LfsTypeRegular or LfsTypeDirectory
        std::uint8_t Type = 0;
        std::uint32_t Size = 0;
        std::string Path;
        std::vector<std::uint32_t> BlockOffsets;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct Littlefs : public Mile::ComObject<Littlefs, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
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

                const std::uint32_t MinimumMetadataHeaderSize =
                    sizeof(std::uint32_t) + // Revision Count
                    sizeof(std::uint32_t) + // Super Block Tag
                    (8 * sizeof(std::uint8_t)) + // Super Block Signature
                    sizeof(std::uint32_t) + // Super Block Inline Structure Tag
                    sizeof(LfsSuperBlockInlineStructure);

                if (BundleSize < MinimumMetadataHeaderSize)
                {
                    break;
                }

                std::uint8_t MetadataHeader[MinimumMetadataHeaderSize];
                if (FAILED(this->ReadFileStream(
                    0,
                    MetadataHeader,
                    MinimumMetadataHeaderSize)))
                {
                    break;
                }

                if (0 != std::memcmp(
                    &MetadataHeader[8],
                    g_LfsSignature,
                    sizeof(g_LfsSignature)))
                {
                    break;
                }

                std::uint64_t TotalFiles = 0;
                std::uint64_t TotalBytes = 0;

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                // TODO: this->GetAllPaths

                // TODO: OpenCallback->SetTotal with new TotalFiles

                this->m_FilePaths.clear();

                // TODO: Add this->GetAllPaths contents to this->m_FilePaths

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

                bool Failed = false;

                // TODO: Extract Files

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
            case SevenZipArchivePhysicalSize:
            {
                // TODO: Unimplemented
                Value->uhVal.QuadPart = 0;
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
                // TODO: Unimplemented
                Value->ulVal = 0;
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
