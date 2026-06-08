/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Fuzz.h
 * PURPOSE:    Shared libFuzzer harness scaffolding for NanaZip-specific
 *             archive handlers (formats not provided by upstream 7-Zip).
 *
 * LICENSE:    The MIT License
 *
 * Each per-format Fuzz.<Format>.cpp defines LLVMFuzzerTestOneInput and calls
 * RunFuzzCase(FormatIndex, Data, Size). The harness loads the already-built
 * NanaZip.Codecs.dll (link with the ASan build under Output\Binaries\...) and
 * uses its exported 7-Zip-style CreateObject to instantiate the handler, so
 * no NanaZip object files need to be compiled into the fuzz binary.
 *
 * Fuzzed handlers (g_Archivers indices in NanaZip.Codecs.cpp):
 *   0 UFS / UFS2
 *   1 .NET single-file application
 *   2 Electron asar
 *   3 ROMFS
 *   4 ZealFS
 *   5 WebAssembly (wasm)
 *   6 littlefs
 *
 * Build (MSVC cl.exe, x64, ASan + libFuzzer; VS 2022 17.0+):
 *   cl /MD /Zi /O1 /EHsc /fsanitize=address /fsanitize=fuzzer ^
 *     /I"%cd%\..\NanaZip.Specification" ^
 *     Fuzz.Ufs.cpp /Fe:Fuzz.Ufs.exe /link /SUBSYSTEM:CONSOLE
 * (MSVC rejects the comma-joined /fsanitize=address,fuzzer form; pass each
 * sanitizer as a separate flag.)
 *
 * Run:
 *   set ASAN_OPTIONS=abort_on_error=1:allocator_may_return_null=1:detect_leaks=0
 *   Fuzz.Ufs.exe -timeout=30 -rss_limit_mb=4096 .\corpus\ufs
 *
 * The fuzz binary must live next to NanaZip.Codecs.dll (and
 * clang_rt.asan_dynamic-x86_64.dll). Easiest is to drop it into
 * Output\Binaries\Release\x64\ and run it from there.
 */

#ifndef NANAZIP_CODECS_FUZZ
#define NANAZIP_CODECS_FUZZ

#include <Windows.h>

#include <NanaZip.Specification.SevenZip.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace NanaZip::Fuzz
{
    using CreateObjectFn = HRESULT(WINAPI*)(const GUID*, const GUID*, void**);

    inline CreateObjectFn LoadCreateObject()
    {
        // The DLL is expected to live next to the fuzz binary (same directory
        // we are launched from in the typical case).
        HMODULE Module = ::LoadLibraryW(L"NanaZip.Codecs.dll");
        if (!Module)
        {
            std::abort();
        }
        auto Fn = reinterpret_cast<CreateObjectFn>(
            ::GetProcAddress(Module, "CreateObject"));
        if (!Fn)
        {
            std::abort();
        }
        return Fn;
    }

    inline GUID MakeArchiveClsid(std::uint32_t ProviderIndex)
    {
        // Mirrors the decoding in NanaZip.Codecs.cpp::CreateObject:
        //   Data4 reinterpreted as u64 LE = ArchiverProviderIdBase | Index
        //   ArchiverProviderIdBase = 0x4123374B00000000
        const std::uint64_t Id =
            0x4123374B00000000ULL | std::uint64_t(ProviderIndex);
        GUID Guid;
        Guid.Data1 = SevenZipGuidData1;
        Guid.Data2 = SevenZipGuidData2;
        Guid.Data3 = SevenZipGuidData3Common;
        for (int I = 0; I < 8; ++I)
        {
            Guid.Data4[I] = static_cast<std::uint8_t>(Id >> (I * 8));
        }
        return Guid;
    }

    template <typename Interface>
    class FuzzComBase : public Interface
    {
    public:
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return ++m_Ref;
        }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG Value = --m_Ref;
            if (Value == 0) delete this;
            return Value;
        }
    protected:
        virtual ~FuzzComBase() = default;
    private:
        std::atomic<ULONG> m_Ref{ 1 };
    };

    class InMemoryInStream final : public FuzzComBase<IInStream>
    {
    public:
        InMemoryInStream(
            const std::uint8_t* Data,
            std::size_t Size) noexcept
            : m_Data(Data), m_Size(Size), m_Pos(0) {}

        HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID Riid, void** Out) override
        {
            if (!Out) return E_POINTER;
            if (Riid == IID_IUnknown ||
                Riid == __uuidof(ISequentialInStream) ||
                Riid == __uuidof(IInStream))
            {
                *Out = static_cast<IInStream*>(this);
                this->AddRef();
                return S_OK;
            }
            *Out = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE Read(
            void* Data, UINT32 Size, UINT32* ProcessedSize) override
        {
            std::uint64_t Remaining =
                (m_Pos < m_Size) ? (m_Size - m_Pos) : 0;
            UINT32 ToRead = (Size < Remaining)
                ? Size
                : static_cast<UINT32>(Remaining);
            if (ToRead && Data)
            {
                std::memcpy(Data, m_Data + m_Pos, ToRead);
            }
            m_Pos += ToRead;
            if (ProcessedSize) *ProcessedSize = ToRead;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Seek(
            INT64 Offset, UINT32 Origin, UINT64* NewPosition) override
        {
            std::int64_t Base = 0;
            switch (Origin)
            {
            case STREAM_SEEK_SET: Base = 0; break;
            case STREAM_SEEK_CUR: Base = static_cast<std::int64_t>(m_Pos); break;
            case STREAM_SEEK_END: Base = static_cast<std::int64_t>(m_Size); break;
            default: return E_INVALIDARG;
            }
            std::int64_t Target = Base + Offset;
            if (Target < 0)
            {
                return HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK);
            }
            // Seeking past EOF is allowed; subsequent reads return zero bytes.
            m_Pos = static_cast<std::uint64_t>(Target);
            if (NewPosition) *NewPosition = m_Pos;
            return S_OK;
        }

    private:
        const std::uint8_t* m_Data;
        std::size_t m_Size;
        std::uint64_t m_Pos;
    };

    class NullOutStream final : public FuzzComBase<ISequentialOutStream>
    {
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID Riid, void** Out) override
        {
            if (!Out) return E_POINTER;
            if (Riid == IID_IUnknown ||
                Riid == __uuidof(ISequentialOutStream))
            {
                *Out = static_cast<ISequentialOutStream*>(this);
                this->AddRef();
                return S_OK;
            }
            *Out = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE Write(
            const void* /*Data*/, UINT32 Size, UINT32* ProcessedSize) override
        {
            // Discard. We only care that the decode path executes under ASan.
            if (ProcessedSize) *ProcessedSize = Size;
            return S_OK;
        }
    };

    class NullExtractCallback final : public FuzzComBase<IArchiveExtractCallback>
    {
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID Riid, void** Out) override
        {
            if (!Out) return E_POINTER;
            if (Riid == IID_IUnknown ||
                Riid == __uuidof(IProgress) ||
                Riid == __uuidof(IArchiveExtractCallback))
            {
                *Out = static_cast<IArchiveExtractCallback*>(this);
                this->AddRef();
                return S_OK;
            }
            *Out = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE SetTotal(UINT64) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE SetCompleted(const PUINT64) override { return S_OK; }

        HRESULT STDMETHODCALLTYPE GetStream(
            UINT32, ISequentialOutStream** OutStream, INT32 AskMode) override
        {
            if (!OutStream) return E_POINTER;
            if (AskMode == SevenZipExtractAskModeSkip)
            {
                *OutStream = nullptr;
                return S_OK;
            }
            *OutStream = new NullOutStream();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE PrepareOperation(INT32) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE SetOperationResult(INT32) override { return S_OK; }
    };

    inline IInArchive* CreateHandler(std::uint32_t FormatIndex)
    {
        static CreateObjectFn CreateFn = LoadCreateObject();
        GUID Clsid = MakeArchiveClsid(FormatIndex);
        GUID Iid = __uuidof(IInArchive);
        void* Object = nullptr;
        if (FAILED(CreateFn(&Clsid, &Iid, &Object)) || !Object)
        {
            return nullptr;
        }
        return static_cast<IInArchive*>(Object);
    }

    inline void RunFuzzCase(
        std::uint32_t FormatIndex,
        const std::uint8_t* Data,
        std::size_t Size)
    {
        IInArchive* Archive = CreateHandler(FormatIndex);
        if (!Archive) return;

        // The InMemoryInStream and NullExtractCallback start at refcount 1; the
        // handler will AddRef/Release as needed. Match that by holding our own
        // ref via stack ownership and Release() at the end.
        InMemoryInStream* Stream = new InMemoryInStream(Data, Size);
        UINT64 MaxCheck = 1ULL << 24; // 16 MiB header search window cap
        if (Archive->Open(Stream, &MaxCheck, nullptr) == S_OK)
        {
            // Query archive-level property schema and values.
            UINT32 NumArchiveProps = 0;
            Archive->GetNumberOfArchiveProperties(&NumArchiveProps);
            for (UINT32 I = 0; I < NumArchiveProps; ++I)
            {
                BSTR Name = nullptr;
                PROPID PropId = 0;
                VARTYPE VarType = VT_EMPTY;
                Archive->GetArchivePropertyInfo(I, &Name, &PropId, &VarType);
                ::SysFreeString(Name);

                PROPVARIANT V{};
                Archive->GetArchiveProperty(PropId, &V);
                ::PropVariantClear(&V);
            }

            // Discover item-level property IDs from the handler.
            UINT32 NumItemProps = 0;
            Archive->GetNumberOfProperties(&NumItemProps);
            std::vector<PROPID> ItemPropIds(NumItemProps);
            for (UINT32 I = 0; I < NumItemProps; ++I)
            {
                BSTR Name = nullptr;
                VARTYPE VarType = VT_EMPTY;
                Archive->GetPropertyInfo(I, &Name, &ItemPropIds[I], &VarType);
                ::SysFreeString(Name);
            }

            UINT32 Num = 0;
            Archive->GetNumberOfItems(&Num);
            // Cap iterations so adversarial inputs claiming billions of entries
            // don't waste fuzzing budget.
            if (Num > 4096) Num = 4096;

            for (UINT32 I = 0; I < Num; ++I)
            {
                for (UINT32 J = 0; J < NumItemProps; ++J)
                {
                    PROPVARIANT V{};
                    Archive->GetProperty(I, ItemPropIds[J], &V);
                    ::PropVariantClear(&V);
                }
            }

            // Build an explicit index array instead of passing
            // (nullptr, 0xFFFFFFFF) which triggers a null-deref bug in
            // the NanaZip handlers (Indices[i] instead of ActualFileIndex
            // at the GetStream call).
            std::vector<UINT32> Indices(Num);
            for (UINT32 I = 0; I < Num; ++I)
                Indices[I] = I;

            NullExtractCallback* Callback = new NullExtractCallback();
            Archive->Extract(Indices.data(), Num, TRUE, Callback);
            Callback->Release();

            Archive->Close();
        }
        Stream->Release();
        Archive->Release();
    }
}

#endif // !NANAZIP_CODECS_FUZZ