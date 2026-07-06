/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Core.Fuzz.h
 * PURPOSE:    libFuzzer scaffolding for upstream-7-Zip handlers shipped by
 *             NanaZip but NOT bundled into 7-Zip's official 7z.dll. Currently
 *             AvbHandler (Android Verified Boot vbmeta) and LvmHandler (Linux
 *             LVM2 physical volume metadata).
 *
 * LICENSE:    The MIT License
 *
 * Loads NanaZip.Core.dll (the 7-Zip-fork DLL inside the NanaZip install) and
 * uses its exported 7-Zip codec ABI (CreateObject + GetNumberOfFormats +
 * GetHandlerProperty2) to look up a handler by its registered "Name" string
 * (e.g. "AVB", "LVM"). Reuses the COM stream/callback stubs from the sibling
 * header NanaZip.Codecs.Fuzz.h.
 */

#ifndef NANAZIP_CORE_FUZZ
#define NANAZIP_CORE_FUZZ

#include "NanaZip.Codecs.Fuzz.h"

#include <atomic>
#include <cstring>
#include <string>

namespace NanaZip::Fuzz::Core
{
    using GetNumberOfFormatsFn = HRESULT(WINAPI*)(UINT32*);
    using GetHandlerProperty2Fn = HRESULT(WINAPI*)(UINT32, PROPID, PROPVARIANT*);

    // 7-Zip NHandlerPropID enum values from IArchive.h.
    constexpr PROPID HandlerPropName    = 0;
    constexpr PROPID HandlerPropClassId = 1;

    struct CoreApi
    {
        HMODULE Module;
        CreateObjectFn CreateObject;
        GetNumberOfFormatsFn GetNumberOfFormats;
        GetHandlerProperty2Fn GetHandlerProperty2;
    };

    inline CoreApi const& LoadCoreApi()
    {
        static CoreApi Api = []() -> CoreApi {
            CoreApi A{};
            A.Module = ::LoadLibraryW(L"NanaZip.Core.dll");
            if (!A.Module) std::abort();
            A.CreateObject = reinterpret_cast<CreateObjectFn>(
                ::GetProcAddress(A.Module, "CreateObject"));
            A.GetNumberOfFormats = reinterpret_cast<GetNumberOfFormatsFn>(
                ::GetProcAddress(A.Module, "GetNumberOfFormats"));
            A.GetHandlerProperty2 = reinterpret_cast<GetHandlerProperty2Fn>(
                ::GetProcAddress(A.Module, "GetHandlerProperty2"));
            if (!A.CreateObject || !A.GetNumberOfFormats || !A.GetHandlerProperty2)
            {
                std::abort();
            }
            return A;
        }();
        return Api;
    }

    // Returns true and fills OutClsid if a handler whose registered name
    // matches `Name` (case-sensitive) is found. The CLSID lives in a static
    // cache so subsequent calls are free.
    inline bool ResolveHandlerClsid(const wchar_t* Name, GUID& OutClsid)
    {
        CoreApi const& Api = LoadCoreApi();
        UINT32 Count = 0;
        if (FAILED(Api.GetNumberOfFormats(&Count))) return false;

        for (UINT32 I = 0; I < Count; ++I)
        {
            PROPVARIANT VName{};
            if (FAILED(Api.GetHandlerProperty2(I, HandlerPropName, &VName)))
            {
                continue;
            }
            bool Match = (VName.vt == VT_BSTR && VName.bstrVal &&
                std::wcscmp(VName.bstrVal, Name) == 0);
            ::PropVariantClear(&VName);
            if (!Match) continue;

            PROPVARIANT VClsid{};
            if (FAILED(Api.GetHandlerProperty2(I, HandlerPropClassId, &VClsid)))
            {
                return false;
            }
            // ClassID is delivered as a 16-byte binary blob in a BSTR.
            bool Ok = false;
            if (VClsid.vt == VT_BSTR && VClsid.bstrVal &&
                ::SysStringByteLen(VClsid.bstrVal) >= sizeof(GUID))
            {
                std::memcpy(&OutClsid, VClsid.bstrVal, sizeof(GUID));
                Ok = true;
            }
            ::PropVariantClear(&VClsid);
            return Ok;
        }
        return false;
    }

    inline IInArchive* CreateHandlerByName(const wchar_t* Name)
    {
        // Cache the resolved CLSID per name across iterations.
        static std::atomic<bool> Resolved{ false };
        static GUID CachedClsid{};
        static const wchar_t* CachedName = nullptr;

        if (!Resolved.load(std::memory_order_acquire) || CachedName != Name)
        {
            GUID Guid{};
            if (!ResolveHandlerClsid(Name, Guid)) return nullptr;
            CachedClsid = Guid;
            CachedName = Name;
            Resolved.store(true, std::memory_order_release);
        }

        CoreApi const& Api = LoadCoreApi();
        GUID Iid = __uuidof(IInArchive);
        void* Object = nullptr;
        if (FAILED(Api.CreateObject(&CachedClsid, &Iid, &Object)) || !Object)
        {
            return nullptr;
        }
        return static_cast<IInArchive*>(Object);
    }

    inline void RunFuzzCaseByName(
        const wchar_t* HandlerName,
        const std::uint8_t* Data,
        std::size_t Size)
    {
        IInArchive* Archive = CreateHandlerByName(HandlerName);
        if (!Archive) return;

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

#endif // !NANAZIP_CORE_FUZZ