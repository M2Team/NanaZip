﻿/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Archive.Zealfs.cpp
 * PURPOSE:    Implementation for ZealFS file system image readonly support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

#include <map>

#include "Mile.Helpers.Portable.Base.Unstaged.h"

#include "NanaZip.Codecs.Specification.Zealfs.h"

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
        { SevenZipArchiveCreationTime, VT_FILETIME },
        { SevenZipArchiveInode, VT_UI8 },
    };

    const std::size_t g_PropertyItemsCount =
        sizeof(g_PropertyItems) / sizeof(*g_PropertyItems);

    // Reference: https://github.com/Zeal8bit/ZealFS

    const std::uint16_t g_ZealfsMaximumNameLength = 16;

    const std::uint8_t ZealfsFileFlagDirectory = 0x01;
    const std::uint8_t ZealfsFileFlagOccupied = 0x80;

    struct ZealfsFileEntry
    {
        // Bit 0: 1 = directory, 0 = file
        // bit n: reserved
        // Bit 7: 1 = occupied, 0 = free
        // IS_DIR, IS_FILE, etc...
        std::uint8_t Flags;
        char Name[g_ZealfsMaximumNameLength];
        std::uint8_t StartPage;
        // Size of the file in bytes, little-endian!
        std::uint16_t Size;
        // Date of creation.
        ZEALOS_TIME CreationTime;
        // Reserved for future use
        std::uint8_t Reserved[4];
    };

    // According to zealfs_fuse.c's implementation
    const std::uint8_t g_ZealfsCurrentVersion = 1;

    struct ZealfsFilePathInformation
    {
        std::string Path;
        ZealfsFileEntry Information;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct Zealfs : public Mile::ComObject<Zealfs, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
        std::uint32_t m_PhysicalSize = 0;
        std::uint32_t m_FreeSpace = 0;
        std::string m_VolumeName;
        std::map<std::string, ZealfsFileEntry> m_TemporaryFilePaths;
        std::vector<ZealfsFilePathInformation> m_FilePaths;
        bool m_IsInitialized = false;

    private:

        std::uint16_t ReadUInt16(
            const void* BaseAddress)
        {
            return ::MileReadUInt16LittleEndian(BaseAddress);
        }

        std::uint32_t GetAlignedSize(
            int Size,
            int Alignment)
        {
            return (Size + Alignment - 1) & ~(Alignment - 1);
        }

        std::uint8_t ReadBcd(
            const void* BaseAddress)
        {
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
            return (*Base >> 4) * 10 + (*Base & 0xF);
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

        FILETIME GetFileTime(
            ZEALOS_TIME const& Value)
        {
            SYSTEMTIME LocalTime = {};
            LocalTime.wYear = this->ReadBcd(&Value.Year[0]) * 100;
            LocalTime.wYear += this->ReadBcd(&Value.Year[1]);
            LocalTime.wMonth = this->ReadBcd(&Value.Month);
            LocalTime.wDayOfWeek = this->ReadBcd(&Value.Date);
            LocalTime.wDay = this->ReadBcd(&Value.Day);
            LocalTime.wHour = this->ReadBcd(&Value.Hours);
            LocalTime.wMinute = this->ReadBcd(&Value.Minutes);
            LocalTime.wSecond = this->ReadBcd(&Value.Seconds);
            SYSTEMTIME SystemTime = {};
            ::TzSpecificLocalTimeToSystemTime(nullptr, &LocalTime, &SystemTime);
            FILETIME Result = {};
            ::SystemTimeToFileTime(&SystemTime, &Result);
            return Result;
        }

        void GetAllPaths(
            std::uint32_t Offset,
            std::string const& RootPath)
        {
            std::uint32_t MaximumOffset =
                this->GetAlignedSize(Offset, ZEALFS_V1_PAGE_SIZE);

            std::uint8_t MaximumCount = static_cast<std::uint8_t>(
                (MaximumOffset - Offset) / sizeof(ZealfsFileEntry));

            for (std::uint8_t i = 0; i < MaximumCount; ++i)
            {
                ZealfsFileEntry Information = {};
                if (FAILED(this->ReadFileStream(
                    Offset + (i * sizeof(ZealfsFileEntry)),
                    &Information,
                    sizeof(ZealfsFileEntry))))
                {
                    break;
                }

                if (!(Information.Flags & ZealfsFileFlagOccupied))
                {
                    continue;
                }

                std::string FileName =
                    std::string(Information.Name, g_ZealfsMaximumNameLength);
                FileName.resize(std::strlen(FileName.c_str()));

                std::string Path = RootPath + FileName;

                if (Information.Flags & ZealfsFileFlagDirectory)
                {
                    this->GetAllPaths(
                        Information.StartPage * ZEALFS_V1_PAGE_SIZE,
                        Path + "/");
                }

                this->m_TemporaryFilePaths.emplace(Path, Information);
            }
        }

    public:

        Zealfs()
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

                if (BundleSize < ZEALFS_V1_MINIMUM_PARTITION_SIZE)
                {
                    break;
                }

                ZEALFS_V1_HEADER Header = {};
                if (FAILED(this->ReadFileStream(
                    0,
                    &Header,
                    sizeof(ZEALFS_V1_HEADER))))
                {
                    break;
                }

                if (ZEALFS_MAGIC != Header.Common.Magic)
                {
                    break;
                }

                if (g_ZealfsCurrentVersion != Header.Common.Version)
                {
                    break;
                }

                if (!Header.BitmapSize)
                {
                    break;
                }

                this->m_PhysicalSize = ZEALFS_V1_PAGE_SIZE * Header.BitmapSize * 8;
                this->m_FreeSpace = 0;
                for (std::uint8_t i = 0; i < Header.BitmapSize; ++i)
                {
                    this->m_FreeSpace += !(0x01 & Header.PagesBitmap[i]);
                    this->m_FreeSpace += !(0x02 & Header.PagesBitmap[i]);
                    this->m_FreeSpace += !(0x04 & Header.PagesBitmap[i]);
                    this->m_FreeSpace += !(0x08 & Header.PagesBitmap[i]);
                    this->m_FreeSpace += !(0x10 & Header.PagesBitmap[i]);
                    this->m_FreeSpace += !(0x20 & Header.PagesBitmap[i]);
                    this->m_FreeSpace += !(0x40 & Header.PagesBitmap[i]);
                    this->m_FreeSpace += !(0x80 & Header.PagesBitmap[i]);
                }
                this->m_FreeSpace *= ZEALFS_V1_PAGE_SIZE;

                std::uint64_t TotalFiles = 0;
                std::uint64_t TotalBytes = this->m_PhysicalSize;

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                this->GetAllPaths(sizeof(ZEALFS_V1_HEADER), "");

                TotalFiles = this->m_TemporaryFilePaths.size();
                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                this->m_FilePaths.clear();

                for (auto const& Item : this->m_TemporaryFilePaths)
                {
                    ZealfsFilePathInformation Current;
                    Current.Path = Item.first;
                    Current.Information = Item.second;
                    this->m_FilePaths.emplace_back(Current);
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
            this->m_FreeSpace = 0;
            this->m_PhysicalSize = 0;
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

            ZealfsFileEntry& Information = this->m_FilePaths[Index].Information;

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
                Value->boolVal = Information.Flags & ZealfsFileFlagDirectory
                    ? VARIANT_TRUE
                    : VARIANT_FALSE;
                Value->vt = VT_BOOL;
                break;
            }
            case SevenZipArchiveSize:
            {
                Value->uhVal.QuadPart = this->ReadUInt16(&Information.Size);
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchivePackSize:
            {
                Value->uhVal.QuadPart = this->ReadUInt16(&Information.Size);
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveCreationTime:
            {
                Value->filetime = this->GetFileTime(Information.CreationTime);
                Value->vt = VT_FILETIME;
                break;
            }
            case SevenZipArchiveInode:
            {
                Value->uhVal.QuadPart = Information.StartPage;
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
                ZealfsFileEntry& Information =
                    this->m_FilePaths[ActualFileIndex].Information;
                TotalSize += Information.Size;
            }
            ExtractCallback->SetTotal(TotalSize);

            UINT64 Completed = 0;

            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                ZealfsFileEntry& Information =
                    this->m_FilePaths[ActualFileIndex].Information;

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

                std::uint16_t Todo = this->ReadUInt16(&Information.Size);
                std::uint16_t Done = 0;
                std::uint16_t CurrentOffset =
                    Information.StartPage * ZEALFS_V1_PAGE_SIZE;

                bool Failed = false;
                while (Todo && CurrentOffset)
                {
                    std::uint16_t CurrentDo = Todo > ZEALFS_V1_PAGE_SIZE - 1
                        ? ZEALFS_V1_PAGE_SIZE - 1
                        : Todo;

                    std::uint8_t Buffer[ZEALFS_V1_PAGE_SIZE];
                    if (FAILED(this->ReadFileStream(
                        CurrentOffset,
                        Buffer,
                        ZEALFS_V1_PAGE_SIZE)))
                    {
                        Failed = true;
                        break;
                    }

                    UINT32 ProceededSize = 0;
                    hr = OutputStream->Write(
                        &Buffer[1],
                        CurrentDo,
                        &ProceededSize);
                    if (FAILED(hr))
                    {
                        Failed = true;
                        break;
                    }

                    Todo -= CurrentDo;
                    Done += CurrentDo;
                    CurrentOffset = Buffer[0] * ZEALFS_V1_PAGE_SIZE;
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
            case SevenZipArchivePhysicalSize:
            {
                Value->uhVal.QuadPart = this->m_PhysicalSize;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveFreeSpace:
            {
                Value->uhVal.QuadPart = this->m_FreeSpace;
                Value->vt = VT_UI8;
                break;
            }
            case SevenZipArchiveClusterSize:
            {
                Value->ulVal = ZEALFS_V1_PAGE_SIZE;
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

    IInArchive* CreateZealfs()
    {
        return new Zealfs();
    }
}
