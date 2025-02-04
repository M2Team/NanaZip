/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.WebAssembly.cpp
 * PURPOSE:   Implementation for WebAssembly (WASM) binary file readonly support
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
        { SevenZipArchiveOffset, VT_UI8 },
    };

    const std::size_t g_PropertyItemsCount =
        sizeof(g_PropertyItems) / sizeof(*g_PropertyItems);

    const char g_WebAssemblySignature[] = { '\0', 'a', 's', 'm' };

    namespace WebAssemblySectionType
    {
        enum
        {
            Custom,
            Type,
            Import,
            Function,
            Table,
            Memory,
            Global,
            Export,
            Start,
            Element,
            Code,
            Data,
            DataCount,
            Unknown
        };
    }

    const char* ToWebAssemblySectionTypeName(
        std::uint8_t const& Type)
    {
        static const char* TypeNames[] =
        {
            "[CUSTOM]",
            ".type",
            ".import",
            ".function",
            ".table",
            ".memory",
            ".global",
            ".export",
            ".start",
            ".element",
            ".code",
            ".data",
            ".data_count",
            "[UNKNOWN]"
        };

        if (Type < WebAssemblySectionType::Unknown)
        {
            return TypeNames[Type];
        }

        return TypeNames[WebAssemblySectionType::Unknown];
    }

    struct WebAssemblySection
    {
        std::uint64_t Offset = 0;
        std::uint64_t Size = 0;
        std::string Name;
    };
}

namespace NanaZip::Codecs::Archive
{
    struct WebAssembly : public Mile::ComObject<WebAssembly, IInArchive>
    {
    private:

        IInStream* m_FileStream = nullptr;
        std::uint64_t m_FullSize = 0;
        std::vector<WebAssemblySection> m_Sections;
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

        std::uint32_t ReadUleb128(
            const void* BaseAddress,
            std::uint8_t* ProcessedBytes = nullptr)
        {
            if (ProcessedBytes)
            {
                *ProcessedBytes = 0;
            }
            const std::uint8_t* Base =
                reinterpret_cast<const std::uint8_t*>(BaseAddress);
            std::uint32_t Result = 0;
            std::uint8_t Shift = 0;
            while (true)
            {
                if (ProcessedBytes)
                {
                    ++(*ProcessedBytes);
                }
                std::uint8_t Byte = *(Base++);
                Result |= (Byte & 0x7F) << Shift;
                if (!(Byte & 0x80))
                {
                    break;
                }
                Shift += 7;
                if (Shift >= 35)
                {
                    return static_cast<std::uint32_t>(-1);
                }
            }

            return Result;
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

        WebAssembly()
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

                const std::size_t HeaderSize = 2 * sizeof(std::uint32_t);

                if (BundleSize < HeaderSize)
                {
                    break;
                }

                std::uint8_t HeaderBuffer[HeaderSize];
                if (FAILED(this->ReadFileStream(
                    0,
                    HeaderBuffer,
                    HeaderSize)))
                {
                    break;
                }

                if (0 != std::memcmp(
                    HeaderBuffer,
                    g_WebAssemblySignature,
                    sizeof(g_WebAssemblySignature)))
                {
                    break;
                }

                std::uint32_t Version = this->ReadUInt32(
                    &HeaderBuffer[sizeof(std::uint32_t)]);
                if (1 != Version)
                {
                    break;
                }

                std::multimap<std::string, WebAssemblySection> Sections;
                for (UINT64 i = HeaderSize; i < BundleSize;)
                {
                    std::uint8_t Type = 0;
                    std::uint32_t Size = 0;
                    {
                        const std::size_t MaximumSize =
                            sizeof(std::uint8_t) * (1 + 5);
                        std::size_t AcquireSize =
                            BundleSize - i < MaximumSize
                            ? BundleSize - i
                            : MaximumSize;
                        std::uint8_t Buffer[MaximumSize] = { 0 };
                        if (FAILED(this->ReadFileStream(
                            i,
                            Buffer,
                            AcquireSize)))
                        {
                            break;
                        }
                        Type = Buffer[0];
                        std::uint8_t ProcessedBytes = 0;
                        Size = this->ReadUleb128(
                            &Buffer[1],
                            &ProcessedBytes);
                        i += sizeof(std::uint8_t) + ProcessedBytes;
                    }
                    if (WebAssemblySectionType::Custom == Type && 0 == Size)
                    {
                        this->m_FullSize = i;
                        break;
                    }

                    std::string CustomName;
                    if (WebAssemblySectionType::Custom == Type)
                    {
                        std::uint32_t NameSize = 0;
                        {
                            const std::size_t MaximumSize =
                                sizeof(std::uint8_t) * 5;
                            std::size_t AcquireSize =
                                BundleSize - i < MaximumSize
                                ? BundleSize - i
                                : MaximumSize;
                            std::uint8_t Buffer[MaximumSize] = { 0 };
                            if (FAILED(this->ReadFileStream(
                                i,
                                Buffer,
                                AcquireSize)))
                            {
                                break;
                            }
                            std::uint8_t ProcessedBytes = 0;
                            NameSize = this->ReadUleb128(
                                Buffer,
                                &ProcessedBytes);
                            i += ProcessedBytes;
                            Size -= ProcessedBytes;
                        }
                        if (NameSize)
                        {
                            CustomName = std::string(NameSize, '\0');
                            if (FAILED(this->ReadFileStream(
                                i,
                                &CustomName[0],
                                NameSize)))
                            {
                                break;
                            }
                            i += sizeof(char) * NameSize;
                            Size -= sizeof(char) * NameSize;
                        }
                    }
                    WebAssemblySection Section;
                    Section.Offset = i;
                    Section.Size = Size;
                    Section.Name = CustomName.empty()
                        ? ::ToWebAssemblySectionTypeName(Type)
                        : std::string(".") + CustomName;
                    Sections.emplace(Section.Name, Section);
                    i += Size;
                }
                if (!this->m_FullSize && !Sections.empty())
                {
                    this->m_FullSize = BundleSize;
                }

                std::uint64_t TotalFiles = Sections.size();
                std::uint64_t TotalBytes = this->m_FullSize;

                if (OpenCallback)
                {
                    OpenCallback->SetTotal(&TotalFiles, &TotalBytes);
                }

                this->m_Sections.clear();

                for (auto const& Item : Sections)
                {
                    this->m_Sections.emplace_back(Item.second);
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
            this->m_Sections.clear();
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

            *NumItems = static_cast<UINT32>(this->m_Sections.size());
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

            if (!(Index < this->m_Sections.size()))
            {
                return E_INVALIDARG;
            }

            WebAssemblySection& Information = this->m_Sections[Index];

            switch (PropId)
            {
            case SevenZipArchivePath:
            {
                Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                    CP_UTF8,
                    this->m_Sections[Index].Name).c_str());
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
            case SevenZipArchiveOffset:
            {
                Value->uhVal.QuadPart = Information.Offset;
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
                NumItems = static_cast<UINT32>(this->m_Sections.size());
            }

            UINT64 TotalSize = 0;
            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                WebAssemblySection& Information =
                    this->m_Sections[ActualFileIndex];
                TotalSize += Information.Size;
            }
            ExtractCallback->SetTotal(TotalSize);

            UINT64 Completed = 0;

            for (UINT32 i = 0; i < NumItems; ++i)
            {
                UINT32 ActualFileIndex = AllFilesMode ? i : Indices[i];
                WebAssemblySection& Information =
                    this->m_Sections[ActualFileIndex];

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

    IInArchive* CreateWebAssembly()
    {
        return new WebAssembly();
    }
}
