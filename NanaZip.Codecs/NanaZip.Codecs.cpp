/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.cpp
 * PURPOSE:    Implementation for NanaZip.Codecs
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <Mile.Helpers.CppBase.h>

#include <string>
#include <vector>
#include <utility>

namespace
{
    struct HashProviderItem
    {
        std::uint32_t Id;
        const char* Name;
        IHasher* (*Create)();
    };

    // Registered Hashers
    // DO NOT CHANGE EXISTING IDS FOR COMPATIBILITY
    const HashProviderItem g_Hashers[] =
    {
        { 0, "MD2", NanaZip::Codecs::Hash::CreateMd2 },
        { 1, "MD4", NanaZip::Codecs::Hash::CreateMd4 },
        { 2, "MD5", NanaZip::Codecs::Hash::CreateMd5 },
        // ID 3 (Originally "SHA1") was previously reserved.
        // ID 4 (Originally "SHA256") was previously reserved.
        // ID 5 (Originally "SHA384") was previously reserved.
        // ID 6 (Originally "SHA512") was previously reserved.
        // ID 7 (Originally "SHA3-256") was previously reserved.
        // ID 8 (Originally "SHA3-384") was previously reserved.
        // ID 9 (Originally "SHA3-512") was previously reserved.
        { 10, "BLAKE3", NanaZip::Codecs::Hash::CreateBlake3 },
        { 11, "SM3", NanaZip::Codecs::Hash::CreateSm3 },
        { 12, "AICH", NanaZip::Codecs::Hash::CreateAich },
        { 13, "BLAKE2b", NanaZip::Codecs::Hash::CreateBlake2b },
        { 14, "ED2K", NanaZip::Codecs::Hash::CreateEd2k },
        { 15, "EDON-R-224", NanaZip::Codecs::Hash::CreateEdonR224 },
        { 16, "EDON-R-256", NanaZip::Codecs::Hash::CreateEdonR256 },
        { 17, "EDON-R-384", NanaZip::Codecs::Hash::CreateEdonR384 },
        { 18, "EDON-R-512", NanaZip::Codecs::Hash::CreateEdonR512 },
        { 19, "GOST94", NanaZip::Codecs::Hash::CreateGost94 },
        { 20, "GOST94CryptoPro", NanaZip::Codecs::Hash::CreateGost94CryptoPro },
        { 21, "GOST12-256", NanaZip::Codecs::Hash::CreateGost12256 },
        { 22, "GOST12-512", NanaZip::Codecs::Hash::CreateGost12512 },
        { 23, "HAS-160", NanaZip::Codecs::Hash::CreateHas160 },
        { 24, "RIPEMD-160", NanaZip::Codecs::Hash::CreateRipemd160 },
        { 25, "SHA224", NanaZip::Codecs::Hash::CreateSha224 },
        // ID 26 (Originally "SHA3-224") was previously reserved.
        { 27, "SNEFRU-128", NanaZip::Codecs::Hash::CreateSnefru128 },
        { 28, "SNEFRU-256", NanaZip::Codecs::Hash::CreateSnefru256 },
        { 29, "TIGER", NanaZip::Codecs::Hash::CreateTiger },
        { 30, "TIGER2", NanaZip::Codecs::Hash::CreateTiger2 },
        { 31, "BTIH", NanaZip::Codecs::Hash::CreateTorrent },
        { 32, "TTH", NanaZip::Codecs::Hash::CreateTth },
        { 33, "WHIRLPOOL", NanaZip::Codecs::Hash::CreateWhirlpool },
        { 34, "XXH32", NanaZip::Codecs::Hash::CreateXxh32 },
        { 35, "XXH64", NanaZip::Codecs::Hash::CreateXxh64 },
        { 36, "XXH3_64bits", NanaZip::Codecs::Hash::CreateXxh364 },
        { 37, "XXH3_128bits", NanaZip::Codecs::Hash::CreateXxh3128 },
    };

    const std::size_t g_HashersCount =
        sizeof(g_Hashers) / sizeof(*g_Hashers);

    struct ArchiverProviderItem
    {
        const char* Name;
        const char* Extension;
        const char* AddExtension;
        std::uint32_t Flags;
        std::uint32_t TimeFlags;
        const uint8_t* Signature;
        std::uint16_t SignatureOffset;
        std::uint8_t SignatureSize;
        bool Update;
        IInArchive* (*CreateIn)();
    };

    // Registered Archivers
    // DO NOT CHANGE THE SEQUENCE FOR COMPATIBILITY
    const ArchiverProviderItem g_Archivers[] =
    {
        {
            "UFS",
            "ufs ufs2 img",
            nullptr,
            SevenZipHandlerFlagBackwardOpen,
            0,
            nullptr,
            0,
            0,
            false,
            NanaZip::Codecs::Archive::CreateUfs
        },
        {
            ".NET Single File Application",
            "coreclrapphost",
            nullptr,
            SevenZipHandlerFlagBackwardOpen,
            0,
            nullptr,
            0,
            0,
            false,
            NanaZip::Codecs::Archive::CreateDotNetSingleFile
        },
        {
            ".Electron Archive (asar)",
            "asar",
            nullptr,
            SevenZipHandlerFlagBackwardOpen,
            0,
            nullptr,
            0,
            0,
            false,
            NanaZip::Codecs::Archive::CreateElectronAsar
        },
        {
            "ROMFS",
            "romfs",
            nullptr,
            SevenZipHandlerFlagFindSignature,
            0,
            reinterpret_cast<const std::uint8_t*>("-rom1fs-"),
            0,
            8,
            false,
            NanaZip::Codecs::Archive::CreateRomfs
        },
        {
            "ZealFS",
            "zealfs",
            nullptr,
            SevenZipHandlerFlagFindSignature,
            0,
            reinterpret_cast<const std::uint8_t*>("Z"),
            0,
            1,
            false,
            NanaZip::Codecs::Archive::CreateZealfs
        },
        {
            "WebAssembly (WASM)",
            "wasm",
            nullptr,
            SevenZipHandlerFlagFindSignature,
            0,
            reinterpret_cast<const std::uint8_t*>("\0asm"),
            0,
            4,
            false,
            NanaZip::Codecs::Archive::CreateWebAssembly
        },
        {
            "littlefs",
            "littlefs",
            nullptr,
            SevenZipHandlerFlagBackwardOpen,
            0,
            nullptr,
            0,
            0,
            false,
            NanaZip::Codecs::Archive::CreateLittlefs
        },
    };

    const std::size_t g_ArchiversCount =
        sizeof(g_Archivers) / sizeof(*g_Archivers);
}

struct HasherFactory : public Mile::ComObject<
    HasherFactory, IHashers>
{
public:

    UINT32 STDMETHODCALLTYPE GetNumHashers()
    {
        return static_cast<UINT32>(g_HashersCount);
    }

    HRESULT STDMETHODCALLTYPE GetHasherProp(
        _In_ UINT32 Index,
        _In_ PROPID PropId,
        _Inout_ LPPROPVARIANT Value)
    {
        if (!(Index < this->GetNumHashers()))
        {
            return E_INVALIDARG;
        }

        if (!Value)
        {
            return E_INVALIDARG;
        }
        ::PropVariantClear(Value);

        HashProviderItem const& CurrentProvider = g_Hashers[Index];

        switch (PropId)
        {
        case SevenZipHasherId:
        {
            Value->uhVal.QuadPart =
                NanaZip::Codecs::HashProviderIdBase | CurrentProvider.Id;
            Value->vt = VT_UI8;
            break;
        }
        case SevenZipHasherName:
        {
            Value->bstrVal = ::SysAllocString(
                Mile::ToWideString(CP_UTF8, CurrentProvider.Name).c_str());
            if (Value->bstrVal)
            {
                Value->vt = VT_BSTR;
            }
            break;
        }
        case SevenZipHasherEncoder:
        {
            GUID EncoderGuid;
            EncoderGuid.Data1 = SevenZipGuidData1;
            EncoderGuid.Data2 = SevenZipGuidData2;
            EncoderGuid.Data3 = SevenZipGuidData3Hasher;
            *reinterpret_cast<PUINT64>(EncoderGuid.Data4) =
                NanaZip::Codecs::HashProviderIdBase | CurrentProvider.Id;
            Value->bstrVal = ::SysAllocStringByteLen(
                reinterpret_cast<LPCSTR>(&EncoderGuid),
                sizeof(EncoderGuid));
            if (Value->bstrVal)
            {
                Value->vt = VT_BSTR;
            }
            break;
        }
        case SevenZipHasherDigestSize:
        {
            IHasher* Hasher = CurrentProvider.Create();
            if (Hasher)
            {
                Value->ulVal = Hasher->GetDigestSize();
                Value->vt = VT_UI4;
                Hasher->Release();
            }
            break;
        }
        default:
            return E_INVALIDARG;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CreateHasher(
        _In_ UINT32 Index,
        _Out_ IHasher** Hasher)
    {
        if (!(Index < this->GetNumHashers()))
        {
            return E_INVALIDARG;
        }

        if (!Hasher)
        {
            return E_INVALIDARG;
        }

        *Hasher = g_Hashers[Index].Create();
        return *Hasher ? S_OK : E_NOINTERFACE;
    }
};

EXTERN_C HRESULT WINAPI GetHashers(
    _Out_ IHashers** Hashers)
{
    if (!Hashers)
    {
        return E_INVALIDARG;
    }

    *Hashers = new HasherFactory();
    return S_OK;
}

EXTERN_C HRESULT WINAPI CreateObject(
    _In_ REFCLSID Clsid,
    _In_ REFIID Iid,
    _Out_ LPVOID* OutObject)
{
    if (!OutObject)
    {
        return E_INVALIDARG;
    }
    *OutObject = nullptr;

    if (Iid == __uuidof(IHasher))
    {
        if (Clsid.Data1 == SevenZipGuidData1 &&
            Clsid.Data2 == SevenZipGuidData2 &&
            Clsid.Data3 == SevenZipGuidData3Hasher)
        {
            std::uint64_t ProviderId =
                *reinterpret_cast<const std::uint64_t*>(Clsid.Data4);
            std::uint64_t ProviderIdBase = ProviderId & 0xFFFFFFFF00000000;
            std::uint32_t ProviderIdValue =
                static_cast<std::uint32_t>(ProviderId);
            if (NanaZip::Codecs::HashProviderIdBase == ProviderIdBase)
            {
                for (HashProviderItem const& CurrentProvider : g_Hashers)
                {
                    if (CurrentProvider.Id == ProviderIdValue)
                    {
                        *OutObject = CurrentProvider.Create();
                        break;
                    }
                }
            }
        }
    }
    else if (Iid == __uuidof(IInArchive))
    {
        if (Clsid.Data1 == SevenZipGuidData1 &&
            Clsid.Data2 == SevenZipGuidData2 &&
            Clsid.Data3 == SevenZipGuidData3Common)
        {
            std::uint64_t ProviderId =
                *reinterpret_cast<const std::uint64_t*>(Clsid.Data4);
            std::uint64_t ProviderIdBase = ProviderId & 0xFFFFFFFF00000000;
            std::uint32_t ProviderIndex =
                static_cast<std::uint32_t>(ProviderId);
            if (NanaZip::Codecs::ArchiverProviderIdBase == ProviderIdBase)
            {
                if (ProviderIndex < g_ArchiversCount)
                {
                    *OutObject = g_Archivers[ProviderIndex].CreateIn();
                }
            }
        }
    }

    return *OutObject ? S_OK : E_NOINTERFACE;
}

EXTERN_C HRESULT WINAPI GetNumberOfFormats(
    _Out_ PUINT32 NumFormats)
{
    if (!NumFormats)
    {
        return E_INVALIDARG;
    }

    *NumFormats = g_ArchiversCount;
    return S_OK;
}

EXTERN_C HRESULT WINAPI GetHandlerProperty2(
    _In_ UINT32 Index,
    _In_ PROPID PropId,
    _Inout_ LPPROPVARIANT Value)
{
    if (!(Index < g_ArchiversCount))
    {
        return E_INVALIDARG;
    }

    if (!Value)
    {
        return E_INVALIDARG;
    }

    switch (PropId)
    {
    case SevenZipHandlerName:
    {
        Value->bstrVal = ::SysAllocString(Mile::ToWideString(
            CP_UTF8,
            g_Archivers[Index].Name).c_str());
        if (Value->bstrVal)
        {
            Value->vt = VT_BSTR;
        }
        break;
    }
    case SevenZipHandlerClassId:
    {
        GUID ClassId;
        ClassId.Data1 = SevenZipGuidData1;
        ClassId.Data2 = SevenZipGuidData2;
        ClassId.Data3 = SevenZipGuidData3Common;
        *reinterpret_cast<PUINT64>(ClassId.Data4) =
            NanaZip::Codecs::ArchiverProviderIdBase | Index;
        Value->bstrVal = ::SysAllocStringByteLen(
            reinterpret_cast<LPCSTR>(&ClassId),
            sizeof(ClassId));
        if (Value->bstrVal)
        {
            Value->vt = VT_BSTR;
        }
        break;
    }
    case SevenZipHandlerExtension:
    {
        if (g_Archivers[Index].Extension)
        {
            Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                CP_UTF8,
                g_Archivers[Index].Extension).c_str());
            if (Value->bstrVal)
            {
                Value->vt = VT_BSTR;
            }
        }
        break;
    }
    case SevenZipHandlerAddExtension:
    {
        if (g_Archivers[Index].AddExtension)
        {
            Value->bstrVal = ::SysAllocString(Mile::ToWideString(
                CP_UTF8,
                g_Archivers[Index].AddExtension).c_str());
            if (Value->bstrVal)
            {
                Value->vt = VT_BSTR;
            }
        }
        break;
    }
    case SevenZipHandlerUpdate:
    {
        Value->boolVal =
            g_Archivers[Index].Update
            ? VARIANT_TRUE
            : VARIANT_FALSE;
        Value->vt = VT_BOOL;
        break;
    }
    case SevenZipHandlerKeepName:
    {
        Value->boolVal =
            g_Archivers[Index].Flags & SevenZipHandlerFlagKeepName
            ? VARIANT_TRUE
            : VARIANT_FALSE;
        Value->vt = VT_BOOL;
        break;
    }
    case SevenZipHandlerSignature:
    {
        if (g_Archivers[Index].SignatureSize &&
            !(g_Archivers[Index].Flags & SevenZipHandlerFlagMultiSignature))
        {
            Value->bstrVal = ::SysAllocStringByteLen(
                reinterpret_cast<LPCSTR>(g_Archivers[Index].Signature),
                g_Archivers[Index].SignatureSize);
            if (Value->bstrVal)
            {
                Value->vt = VT_BSTR;
            }
        }
        break;
    }
    case SevenZipHandlerMultiSignature:
    {
        if (g_Archivers[Index].SignatureSize &&
            g_Archivers[Index].Flags & SevenZipHandlerFlagMultiSignature)
        {
            Value->bstrVal = ::SysAllocStringByteLen(
                reinterpret_cast<LPCSTR>(g_Archivers[Index].Signature),
                g_Archivers[Index].SignatureSize);
            if (Value->bstrVal)
            {
                Value->vt = VT_BSTR;
            }
        }
        break;
    }
    case SevenZipHandlerSignatureOffset:
    {
        Value->ulVal = g_Archivers[Index].SignatureOffset;
        Value->vt = VT_UI4;
        break;
    }
    case SevenZipHandlerAlternateStream:
    {
        Value->boolVal =
            g_Archivers[Index].Flags & SevenZipHandlerFlagAlternateStreams
            ? VARIANT_TRUE
            : VARIANT_FALSE;
        Value->vt = VT_BOOL;
        break;
    }
    case SevenZipHandlerNtSecurity:
    {
        Value->boolVal =
            g_Archivers[Index].Flags & SevenZipHandlerFlagNtSecurity
            ? VARIANT_TRUE
            : VARIANT_FALSE;
        Value->vt = VT_BOOL;
        break;
    }
    case SevenZipHandlerFlags:
    {
        Value->ulVal = g_Archivers[Index].Flags;
        Value->vt = VT_UI4;
        break;
    }
    case SevenZipHandlerTimeFlags:
    {
        Value->ulVal = g_Archivers[Index].TimeFlags;
        Value->vt = VT_UI4;
        break;
    }
    default:
        return E_INVALIDARG;
    }

    return S_OK;
}
