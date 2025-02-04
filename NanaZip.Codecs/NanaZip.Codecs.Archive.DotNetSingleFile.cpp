/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.DotNetSingleFile.cpp
 * PURPOSE:   Implementation for .NET Single File Application readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

#include <map>

namespace
{
    struct PropertyItem
    {
        PROPID Property;
        VARTYPE Type;
    };

    const PropertyItem g_PropertyItems[] =
    {
        { SevenZipArchivePath, VT_BSTR },
        { SevenZipArchiveSize, VT_UI8 },
        { SevenZipArchivePackSize, VT_UI8 },
    };

    const std::size_t g_PropertyItemsCount =
        sizeof(g_PropertyItems) / sizeof(*g_PropertyItems);

    // Reference in https://github.com/dotnet/runtime/tree/v9.0.1.
    // - src/native/corehost/apphost/bundle_marker.cpp
    // - src/native/corehost/bundle/file_entry.h
    // - src/native/corehost/bundle/file_type.h
    // - src/native/corehost/bundle/header.h
    // - src/installer/managed/Microsoft.NET.HostModel/Bundle/FileType.cs
    // - src/installer/managed/Microsoft.NET.HostModel/Bundle/Manifest.cs
    //
    // FileEntry: Records information about embedded files.
    // 
    // The bundle manifest records the following meta-data for each 
    // file embedded in the bundle:
    // Fixed size portion (file_entry_fixed_t)
    //   - Offset     
    //   - Size       
    //   - CompressedSize  - only in bundleVersion 6+
    //   - File Entry Type       
    // Variable Size portion
    //   - relative path (7-bit extension encoded length prefixed string)
    //
    // The Bundle Header (v1)
    // Fixed size thunk (header_fixed_t)
    //   - Major Version
    //   - Minor Version
    //   - Number of embedded files
    // Variable size portion:
    //   - Bundle ID (7-bit extension encoded length prefixed string)
    // The Bundle Header (v2) [additional content]
    // Fixed size thunk (header_fixed_v2_t)
    //   - DepsJson Location (Offset, Size)
    //   - RuntimeConfig Location (Offset, Size)
    //   - Flags

    const std::uint8_t g_BundleSignature[] =
    {
        // Represent the bundle signature: SHA-256 for ".net core bundle"
        0x8b, 0x12, 0x02, 0xb9, 0x6a, 0x61, 0x20, 0x38,
        0x72, 0x7b, 0x93, 0x02, 0x14, 0xd7, 0xa0, 0x32,
        0x13, 0xf5, 0xb9, 0xe6, 0xef, 0xae, 0x33, 0x18,
        0xee, 0x3b, 0x2d, 0xce, 0x24, 0xb3, 0x6a, 0xae
    };

    // Set the signature search buffer size to 20 MiB because .NET AppHost
    // which contained the signature should smaller than 20 MiB in the current
    // implementations.
    const std::size_t g_SignatureSearchBufferSize = 20 << 20;

    /**
     * @brief Identifies the type of file embedded into the bundle. The bundler
     *        differentiates a few kinds of files via the manifest, with respect
     *        to the way in which they'll be used by the runtime.
     */
    enum BundleFileType : std::uint8_t
    {
        /**
         * @brief Type not determined.
         */
        Unknown,
        /**
         * @brief IL and R2R Assemblies
         */
        Assembly,
        /**
         * @brief Native Binaries
         */
        NativeBinary,
        /**
         * @brief .deps.json configuration file
         */
        DepsJson,
        /**
         * @brief .runtimeconfig.json configuration file
         */
        RuntimeConfigJson,
        /**
         * @brief PDB Files
         */
        Symbols,
    };

    struct BundleFileEntry
    {
        std::int64_t Offset = 0;
        std::int64_t Size = 0;
        std::int64_t CompressedSize = 0; // Only in bundleVersion 6+
        BundleFileType Type = BundleFileType::Unknown;
        std::string RelativePath;
    };

    enum BundleFileHeaderFlags : std::uint64_t
    {
        None = 0,
        /**
         * @brief NetcoreApp3CompatMode flag is set on a .net5+ app, which
         *        chooses to build single-file apps in .netcore3.x compat mode.
         *        This indicates that:
         *        - All published files are bundled into the app; some of them
         *          will be extracted to disk.
         *        - AppContext.BaseDirectory is set to the extraction directory
         *          (and not the AppHost directory).
         *        This mode is expected to be deprecated in future versions of
         *        .NET.
         */
        NetcoreApp3CompatMode = 1
    };

    struct BundleFileLocation
    {
        std::int64_t Offset = 0;
        std::int64_t Size = 0;
    };

    struct BundleFileHeader
    {
        std::uint32_t MajorVersion = 0;
        std::uint32_t MinorVersion = 0;
        std::int32_t NumberOfEmbeddedFiles = 0;
        /**
         * @brief Bundle ID is a string that is used to uniquely identify this
         *        bundle. It is chosen to be compatible with path-names so that
         *        the AppHost can use it in extraction path.
         */
        std::string BundleId;
        BundleFileLocation DepsJsonLocation;
        BundleFileLocation RuntimeConfigLocation;
        BundleFileHeaderFlags Flags = BundleFileHeaderFlags::None;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct DotNetSingleFile :
        public Mile::ComObject<DotNetSingleFile, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
        std::uint64_t m_FullSize = 0;
        std::vector<BundleFileEntry> m_FilePaths;
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

        std::uint64_t ReadUInt64(
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
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

    public:

        DotNetSingleFile()
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

                const std::size_t MinimumOffset =
                    (2 * sizeof(std::uint8_t)) + sizeof(std::uint64_t);

                if (BundleSize < sizeof(g_BundleSignature) + MinimumOffset)
                {
                    break;
                }

                std::size_t SearchBufferSize =
                    BundleSize < g_SignatureSearchBufferSize
                    ? BundleSize
                    : g_SignatureSearchBufferSize;

                std::vector<std::uint8_t> SearchBuffer(SearchBufferSize);
                if (FAILED(this->ReadFileStream(
                    0,
                    &SearchBuffer[0],
                    SearchBufferSize)))
                {
                    break;
                }

                if (SearchBuffer[0] != 'M' || SearchBuffer[1] != 'Z')
                {
                    break;
                }

                std::int64_t BundleHeaderOffset = 0;

                // Initial value is skip 'M', 'Z' and the bundle header offset.
                for (size_t i = 10; i < SearchBufferSize; ++i)
                {
                    if (g_BundleSignature[0] != SearchBuffer[i])
                    {
                        continue;
                    }

                    if (0 != std::memcmp(
                        &SearchBuffer[i],
                        g_BundleSignature,
                        sizeof(g_BundleSignature)))
                    {
                        continue;
                    }

                    BundleHeaderOffset = this->ReadInt64(
                        &SearchBuffer[i - sizeof(std::int64_t)]);
                }
                SearchBuffer.clear();
                if (!BundleHeaderOffset)
                {
                    break;
                }

                std::size_t HeaderBufferSize = BundleSize - BundleHeaderOffset;

                std::vector<std::uint8_t> HeaderBuffer(HeaderBufferSize);
                if (FAILED(this->ReadFileStream(
                    BundleHeaderOffset,
                    &HeaderBuffer[0],
                    HeaderBufferSize)))
                {
                    break;
                }

                BundleFileHeader Header;
                std::map<std::string, BundleFileEntry> Files;

                try
                {
                    std::size_t Current = 0;
                    Header.MajorVersion =
                        this->ReadUInt32(&HeaderBuffer[Current]);
                    Current += sizeof(std::uint32_t);
                    Header.MinorVersion =
                        this->ReadUInt32(&HeaderBuffer[Current]);
                    Current += sizeof(std::uint32_t);
                    Header.NumberOfEmbeddedFiles =
                        this->ReadInt32(&HeaderBuffer[Current]);
                    Current += sizeof(std::int32_t);
                    std::uint8_t BundleIdLength =
                        this->ReadUInt8(&HeaderBuffer[Current]);
                    Current += sizeof(std::uint8_t);
                    Header.BundleId = std::string(
                        reinterpret_cast<char*>(&HeaderBuffer[Current]),
                        BundleIdLength);
                    Current += BundleIdLength;
                    if (Header.MajorVersion >= 2)
                    {
                        Header.DepsJsonLocation.Offset =
                            this->ReadInt64(&HeaderBuffer[Current]);
                        Current += sizeof(std::int64_t);
                        Header.DepsJsonLocation.Size =
                            this->ReadInt64(&HeaderBuffer[Current]);
                        Current += sizeof(std::int64_t);
                        Header.RuntimeConfigLocation.Offset =
                            this->ReadInt64(&HeaderBuffer[Current]);
                        Current += sizeof(std::int64_t);
                        Header.RuntimeConfigLocation.Size =
                            this->ReadInt64(&HeaderBuffer[Current]);
                        Current += sizeof(std::int64_t);
                        Header.Flags = static_cast<BundleFileHeaderFlags>(
                            this->ReadUInt64(&HeaderBuffer[Current]));
                        Current += sizeof(std::uint64_t);
                    }

                    for (size_t i = 0; i < Header.NumberOfEmbeddedFiles; ++i)
                    {
                        BundleFileEntry Entry;
                        Entry.Offset =
                            this->ReadInt64(&HeaderBuffer[Current]);
                        Current += sizeof(std::int64_t);
                        Entry.Size =
                            this->ReadInt64(&HeaderBuffer[Current]);
                        Current += sizeof(std::int64_t);
                        if (Header.MajorVersion >= 6)
                        {
                            Entry.CompressedSize =
                                this->ReadInt64(&HeaderBuffer[Current]);
                            Current += sizeof(std::int64_t);
                        }
                        else
                        {
                            Entry.CompressedSize = Entry.Size;
                        }
                        if (0 == Entry.CompressedSize)
                        {
                            Entry.CompressedSize = Entry.Size;
                        }
                        Entry.Type = static_cast<BundleFileType>(
                            this->ReadUInt8(&HeaderBuffer[Current]));
                        Current += sizeof(std::uint8_t);
                        std::uint8_t RelativePathLength =
                            this->ReadUInt8(&HeaderBuffer[Current]);
                        Current += sizeof(std::uint8_t);
                        Entry.RelativePath = std::string(
                            reinterpret_cast<char*>(&HeaderBuffer[Current]),
                            RelativePathLength);
                        Current += RelativePathLength;
                        Files.emplace(Entry.RelativePath, Entry);
                    }
                    this->m_FullSize = BundleHeaderOffset + Current;
                }
                catch (...)
                {
                    break;
                }

                HeaderBuffer.clear();

                std::uint64_t TotalFiles = Files.size();
                std::uint64_t TotalBytes = this->m_FullSize;

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                for (auto const& File : Files)
                {
                    this->m_FilePaths.emplace_back(File.second);
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
            this->m_FullSize = 0;
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

            BundleFileEntry& Information = this->m_FilePaths[Index];

            switch (PropId)
            {
            case SevenZipArchivePath:
            {
                Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                    CP_UTF8,
                    this->m_FilePaths[Index].RelativePath).c_str());
                if (Value->bstrVal)
                {
                    Value->vt = VT_BSTR;
                }
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
                Value->uhVal.QuadPart = Information.CompressedSize;
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
                BundleFileEntry& Information =
                    this->m_FilePaths[ActualFileIndex];
                TotalSize += Information.Size;
            }
            ExtractCallback->SetTotal(TotalSize);

            UINT64 Completed = 0;

            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                BundleFileEntry& Information =
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
                if (Information.Size == Information.CompressedSize)
                {
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
                Value->uhVal.QuadPart = this->m_FullSize;
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
            *NumProps = 0;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetArchivePropertyInfo(
            _In_ UINT32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(Name);
            UNREFERENCED_PARAMETER(PropId);
            UNREFERENCED_PARAMETER(VarType);
            return E_INVALIDARG;
        }
    };

    IInArchive* CreateDotNetSingleFile()
    {
        return new DotNetSingleFile();
    }
}
