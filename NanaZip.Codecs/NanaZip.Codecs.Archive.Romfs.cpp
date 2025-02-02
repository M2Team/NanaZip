/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.Romfs.cpp
 * PURPOSE:   Implementation for ROMFS file system image readonly support
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

    const PropertyItem g_ArchivePropertyItems[] =
    {
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
        { SevenZipArchiveSymbolicLink, VT_BSTR },
        { SevenZipArchiveHardLink, VT_BSTR },
        { SevenZipArchiveInode, VT_UI8 },
    };

    const std::size_t g_PropertyItemsCount =
        sizeof(g_PropertyItems) / sizeof(*g_PropertyItems);

    // Reference: https://www.kernel.org/doc/html/v6.12/filesystems/romfs.html
    // Every multi byte value (32 bit words, I’ll use the longwords term from
    // now on) must be in big endian order.

    const char g_RomfsSignature[] = { '-', 'r', 'o', 'm', '1', 'f', 's', '-' };

    // The Windows MAX_PATH as the maximum path length should enough for most
    // ROMFS cases.
    const std::size_t g_RomfsMaximumPathLength = MAX_PATH;

    struct RomfsHeader
    {
        // The ASCII representation of those bytes (i.e. "-rom1fs-")
        char Signature[8];
        // The number of accessible bytes in this fs
        std::uint32_t FullSize;
        // The checksum of the first 512 bytes (or the number of bytes
        // accessible, whichever is smaller)
        std::uint32_t CheckSum;
        // The zero terminated name of the volume, padded to 16 byte boundary
        char VolumeName[1];
    };

    enum RomfsFileType
    {
        // link destination [file header]
        HardLink,
        // first file's header
        Directory,
        // unused, must be zero [MBZ]
        RegularFile,
        // unused, MBZ (file data is the link content)
        SymbolicLink,
        // 16/16 bits major/minor number
        BlockDevice,
        // - " -
        CharDevice,
        // unused, MBZ
        Socket,
        // unused, MBZ
        Fifo,
        Mask = 0x7,
    };

    struct RomfsFileHeader
    {
        // The offset of the next file header (zero if no more files)
        std::uint32_t NextOffsetAndType;
        // Info for directories/hard links/devices
        std::uint32_t SpecInfo;
        // The size of this file in bytes
        std::uint32_t Size;
        // Covering the meta data, including the file name, and padding
        std::uint32_t CheckSum;
        // The zero terminated name of the file, padded to 16 byte boundary
        char FileName[1];
    };

    struct RomfsFilePathInformation
    {
        std::uint32_t Inode = 0;
        RomfsFileType Type = RomfsFileType::HardLink;
        std::uint32_t Size = 0;
        std::uint32_t Offset = 0;
        std::string Path;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct Romfs : public Mile::ComObject<Romfs, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
        std::uint32_t m_FullSize = 0;
        std::string m_VolumeName;
        std::map<std::string, RomfsFilePathInformation> m_TemporaryFilePaths;
        std::vector<RomfsFilePathInformation> m_FilePaths;
        bool m_IsInitialized = false;

    private:

        std::uint32_t ReadUInt32(
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
            return
                (static_cast<std::uint32_t>(Base[0]) << 24) |
                (static_cast<std::uint32_t>(Base[1]) << 16) |
                (static_cast<std::uint32_t>(Base[2]) << 8) |
                (static_cast<std::uint32_t>(Base[3]));
        }

        std::uint32_t GetAlignedSize(
            int Size,
            int Alignment)
        {
            return (Size + Alignment - 1) & ~(Alignment - 1);
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

        void GetAllPaths(
            std::uint32_t Offset,
            std::string const& RootPath)
        {
            while (Offset)
            {
                RomfsFilePathInformation Information;
                Information.Inode = Offset;

                std::uint8_t FileHeaderBuffer[
                    offsetof(RomfsFileHeader, FileName)] = { 0 };
                if (FAILED(this->ReadFileStream(
                    Offset,
                    &FileHeaderBuffer[0],
                    sizeof(FileHeaderBuffer))))
                {
                    break;
                }
                Offset += offsetof(RomfsFileHeader, FileName);

                std::uint32_t NextOffsetAndType = this->ReadUInt32(
                    &FileHeaderBuffer[
                        offsetof(RomfsFileHeader, NextOffsetAndType)]);
                std::uint32_t NextOffset = NextOffsetAndType & 0xFFFFFFF0;

                Information.Type = static_cast<RomfsFileType>(
                    NextOffsetAndType & RomfsFileType::Mask);
                Information.Size = this->ReadUInt32(
                    &FileHeaderBuffer[offsetof(RomfsFileHeader, Size)]);
                std::string FileName = std::string(
                    m_FullSize - Offset < g_RomfsMaximumPathLength
                    ? m_FullSize - Offset
                    : g_RomfsMaximumPathLength,
                    '\0');
                if (FAILED(this->ReadFileStream(
                    Offset,
                    &FileName[0],
                    FileName.size())))
                {
                    break;
                }
                FileName.resize(std::strlen(FileName.c_str()));
                Offset += this->GetAlignedSize(
                    static_cast<std::uint32_t>(FileName.size()) + 1,
                    16);

                if (FileName != "." && FileName != "..")
                {
                    Information.Offset = Offset;
                    Information.Path = RootPath + FileName;

                    if (RomfsFileType::Directory == Information.Type)
                    {
                        this->GetAllPaths(Offset, Information.Path + "/");
                    }
                    else if (RomfsFileType::HardLink == Information.Type)
                    {
                        Information.Inode = this->ReadUInt32(&FileHeaderBuffer[
                            offsetof(RomfsFileHeader, SpecInfo)]);
                    }

                    this->m_TemporaryFilePaths.emplace(
                        Information.Path,
                        Information);
                }

                Offset = NextOffset;
            }
        }

    public:

        Romfs()
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

                const std::size_t MinimumHeaderSize =
                    this->GetAlignedSize(sizeof(RomfsHeader), 16);

                if (BundleSize < MinimumHeaderSize)
                {
                    break;
                }

                std::uint32_t Offset = 0;

                std::uint8_t HeaderBuffer[
                    offsetof(RomfsHeader, VolumeName)] = { 0 };
                if (FAILED(this->ReadFileStream(
                    0,
                    &HeaderBuffer[0],
                    sizeof(HeaderBuffer))))
                {
                    break;
                }
                Offset += offsetof(RomfsHeader, VolumeName);

                if (0 != std::memcmp(
                    HeaderBuffer,
                    g_RomfsSignature,
                    sizeof(g_RomfsSignature)))
                {
                    break;
                }

                this->m_FullSize = this->ReadUInt32(
                    &HeaderBuffer[offsetof(RomfsHeader, FullSize)]);
                this->m_VolumeName = std::string(
                    BundleSize - Offset < g_RomfsMaximumPathLength
                    ? BundleSize - Offset
                    : g_RomfsMaximumPathLength,
                    '\0');
                if (FAILED(this->ReadFileStream(
                    Offset,
                    &this->m_VolumeName[0],
                    this->m_VolumeName.size())))
                {
                    break;
                }
                this->m_VolumeName.resize(
                    std::strlen(this->m_VolumeName.c_str()));
                Offset += this->GetAlignedSize(
                    static_cast<std::uint32_t>(this->m_VolumeName.size()) + 1,
                    16);

                std::uint64_t TotalFiles = 0;
                std::uint64_t TotalBytes = BundleSize;

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                this->GetAllPaths(Offset, "");

                TotalFiles = this->m_TemporaryFilePaths.size();
                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                this->m_FilePaths.clear();

                for (auto const& Item : this->m_TemporaryFilePaths)
                {
                    this->m_FilePaths.emplace_back(Item.second);
                }
                this->m_TemporaryFilePaths.clear();

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
            this->m_VolumeName.clear();
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

            RomfsFilePathInformation& Information = this->m_FilePaths[Index];

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
                    RomfsFileType::Directory == Information.Type
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
            case SevenZipArchiveSymbolicLink:
            {
                if (RomfsFileType::SymbolicLink == Information.Type)
                {
                    std::string SymbolicLink(Information.Size, '\0');
                    if (FAILED(this->ReadFileStream(
                        Information.Offset,
                        &SymbolicLink[0],
                        SymbolicLink.size())))
                    {
                        SymbolicLink.clear();
                    }

                    if (!SymbolicLink.empty())
                    {
                        Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                            CP_UTF8,
                            SymbolicLink).c_str());
                        if (Value->bstrVal)
                        {
                            Value->vt = VT_BSTR;
                        }
                    }
                }
                break;
            }
            case SevenZipArchiveHardLink:
            {
                if (RomfsFileType::HardLink == Information.Type)
                {
                    std::string HardLink;
                    for (RomfsFilePathInformation& Item : this->m_FilePaths)
                    {
                        if (Item.Inode == Information.Inode &&
                            Item.Path != Information.Path)
                        {
                            HardLink = Item.Path;
                            break;
                        }
                    }

                    if (!HardLink.empty())
                    {
                        Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                            CP_UTF8,
                            HardLink).c_str());
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
                RomfsFilePathInformation& Information =
                    this->m_FilePaths[ActualFileIndex];
                TotalSize += Information.Size;
            }
            ExtractCallback->SetTotal(TotalSize);

            UINT64 Completed = 0;

            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                RomfsFilePathInformation& Information =
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
                Value->uhVal.QuadPart = this->m_FullSize;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveVolumeName:
            {
                Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                    CP_UTF8,
                    this->m_VolumeName).c_str());
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

    IInArchive* CreateRomfs()
    {
        return new Romfs();
    }
}
