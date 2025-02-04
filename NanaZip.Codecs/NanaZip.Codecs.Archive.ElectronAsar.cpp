/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.ElectronAsar.cpp
 * PURPOSE:   Implementation for Electron Archive (asar) readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include "NanaZip.Codecs.SevenZipWrapper.h"

#include <map>
#include <Mile.Json.h>

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

    struct BundleFileEntry
    {
        std::uint64_t Offset = 0;
        std::uint64_t Size = 0;
        std::string RelativePath;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct ElectronAsar : public Mile::ComObject<ElectronAsar, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
        std::uint64_t m_FullSize = 0;
        std::uint64_t m_GlobalOffset = 0;
        std::map<std::string, BundleFileEntry> m_TemporaryFilePaths;
        std::vector<BundleFileEntry> m_FilePaths;
        bool m_IsInitialized = false;

    private:

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
            nlohmann::json const& RootJson,
            std::string const& RootPath)
        {
            nlohmann::json Files = Mile::Json::ToObject(
                Mile::Json::GetSubKey(RootJson, "files"));
            if (!Files.empty())
            {
                for (auto const& File : Files.items())
                {
                    this->GetAllPaths(
                        File.value(),
                        RootPath + File.key() + "/");
                }
            }
            else
            {
                BundleFileEntry Entry;
                Entry.RelativePath = RootPath;
                Entry.RelativePath.resize(Entry.RelativePath.size() - 1);
                Entry.Offset = Mile::ToUInt64(Mile::Json::ToString(
                    Mile::Json::GetSubKey(RootJson, "offset"),
                    "-1"));
                Entry.Size = Mile::Json::ToUInt64(
                    Mile::Json::GetSubKey(RootJson, "size"));
                if (static_cast<std::uint64_t>(-1) != Entry.Offset)
                {
                    this->m_TemporaryFilePaths.emplace(
                        Entry.RelativePath,
                        Entry);
                }
            }
        }

    public:

        ElectronAsar()
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

                const std::size_t BinaryHeaderSize = 4 * sizeof(std::uint32_t);

                if (BundleSize < BinaryHeaderSize)
                {
                    break;
                }

                std::uint8_t BinaryHeaderBuffer[BinaryHeaderSize];
                if (FAILED(this->ReadFileStream(
                    0,
                    &BinaryHeaderBuffer[0],
                    BinaryHeaderSize)))
                {
                    break;
                }

                std::uint32_t HeaderSizeVariableSize = this->ReadUInt32(
                    &BinaryHeaderBuffer[0 * sizeof(std::uint32_t)]);
                std::uint32_t HeaderSize = this->ReadUInt32(
                    &BinaryHeaderBuffer[1 * sizeof(std::uint32_t)]);
                std::uint32_t HeaderBufferSize = this->ReadUInt32(
                    &BinaryHeaderBuffer[2 * sizeof(std::uint32_t)]);
                std::uint32_t HeaderStringSize = this->ReadUInt32(
                    &BinaryHeaderBuffer[3 * sizeof(std::uint32_t)]);

                if (sizeof(std::uint32_t) != HeaderSizeVariableSize)
                {
                    break;
                }
                if (HeaderSize != sizeof(std::uint32_t) + HeaderBufferSize)
                {
                    break;
                }
                if (HeaderStringSize > HeaderBufferSize)
                {
                    break;
                }
                this->m_GlobalOffset = BinaryHeaderSize + HeaderStringSize + 1;

                std::string HeaderString(HeaderStringSize, '\0');
                if (FAILED(this->ReadFileStream(
                    BinaryHeaderSize,
                    &HeaderString[0],
                    HeaderStringSize)))
                {
                    break;
                }

                try
                {
                    nlohmann::json HeaderObject =
                        nlohmann::json::parse(HeaderString);
                    this->GetAllPaths(HeaderObject, "");
                }
                catch (...)
                {
                    break;
                }

                this->m_FullSize = this->m_GlobalOffset;
                for (auto const& Item : this->m_TemporaryFilePaths)
                {
                    this->m_FullSize += Item.second.Size;
                }

                std::uint64_t TotalFiles = this->m_TemporaryFilePaths.size();
                std::uint64_t TotalBytes = this->m_FullSize;
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
            this->m_GlobalOffset = 0;
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
                Value->uhVal.QuadPart = Information.Size;
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
                std::vector<std::uint8_t> Buffer(Information.Size);
                if (SUCCEEDED(this->ReadFileStream(
                    this->m_GlobalOffset + Information.Offset,
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

    IInArchive* CreateElectronAsar()
    {
        return new ElectronAsar();
    }
}
