/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.cpp
 * PURPOSE:   Implementation for NanaZip.Codecs
 *
 * LICENSE:   The MIT License
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
        const char* Name;
        IHasher* (*Create)();
    };

    // Registered Hashers
    // DO NOT CHANGE THE SEQUENCE FOR COMPATIBILITY
    HashProviderItem g_Hashers[] =
    {
        { "MD2", NanaZip::Codecs::Hash::CreateMd2 },
        { "MD4", NanaZip::Codecs::Hash::CreateMd4 },
        { "MD5", NanaZip::Codecs::Hash::CreateMd5 },
        { "SHA1", NanaZip::Codecs::Hash::CreateSha1 },
        { "SHA256", NanaZip::Codecs::Hash::CreateSha256 },
        { "SHA384", NanaZip::Codecs::Hash::CreateSha384 },
        { "SHA512", NanaZip::Codecs::Hash::CreateSha512 },
        { "SHA3-256", NanaZip::Codecs::Hash::CreateSha3256 },
        { "SHA3-384", NanaZip::Codecs::Hash::CreateSha3384 },
        { "SHA3-512", NanaZip::Codecs::Hash::CreateSha3512 },
        { "BLAKE3", NanaZip::Codecs::Hash::CreateBlake3 },
        { "SM3", NanaZip::Codecs::Hash::CreateSm3 },
        { "AICH", NanaZip::Codecs::Hash::CreateAich },
        { "BLAKE2b", NanaZip::Codecs::Hash::CreateBlake2b },
        { "ED2K", NanaZip::Codecs::Hash::CreateEd2k },
        { "EDON-R-224", NanaZip::Codecs::Hash::CreateEdonR224 },
        { "EDON-R-256", NanaZip::Codecs::Hash::CreateEdonR256 },
        { "EDON-R-384", NanaZip::Codecs::Hash::CreateEdonR384 },
        { "EDON-R-512", NanaZip::Codecs::Hash::CreateEdonR512 },
        { "GOST94", NanaZip::Codecs::Hash::CreateGost94 },
        { "GOST94CryptoPro", NanaZip::Codecs::Hash::CreateGost94CryptoPro },
        { "GOST12-256", NanaZip::Codecs::Hash::CreateGost12256 },
        { "GOST12-512", NanaZip::Codecs::Hash::CreateGost12512 },
        { "HAS-160", NanaZip::Codecs::Hash::CreateHas160 },
        { "RIPEMD-160", NanaZip::Codecs::Hash::CreateRipemd160 },
        { "SHA224", NanaZip::Codecs::Hash::CreateSha224 },
        { "SHA3-224", NanaZip::Codecs::Hash::CreateSha3224 },
        { "SNEFRU-128", NanaZip::Codecs::Hash::CreateSnefru128 },
        { "SNEFRU-256", NanaZip::Codecs::Hash::CreateSnefru256 },
        { "TIGER", NanaZip::Codecs::Hash::CreateTiger },
        { "TIGER2", NanaZip::Codecs::Hash::CreateTiger2 },
        { "BTIH", NanaZip::Codecs::Hash::CreateTorrent },
        { "TTH", NanaZip::Codecs::Hash::CreateTth },
        { "WHIRLPOOL", NanaZip::Codecs::Hash::CreateWhirlpool },
        { "XXH32", NanaZip::Codecs::Hash::CreateXxh32 },
        { "XXH64", NanaZip::Codecs::Hash::CreateXxh64 },
        { "XXH3_64bits", NanaZip::Codecs::Hash::CreateXxh364 },
        { "XXH3_128bits", NanaZip::Codecs::Hash::CreateXxh3128 },
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
    ArchiverProviderItem g_Archivers[] =
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
            SevenZipHandlerFlagFindSignature,
            0,
            reinterpret_cast<const std::uint8_t*>("littlefs"),
            8,
            8,
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

        switch (PropId)
        {
        case SevenZipHasherId:
        {
            Value->uhVal.QuadPart =
                NanaZip::Codecs::HashProviderIdBase | Index;
            Value->vt = VT_UI8;
            break;
        }
        case SevenZipHasherName:
        {
            Value->bstrVal = ::SysAllocString(
                Mile::ToWideString(CP_UTF8, g_Hashers[Index].Name).c_str());
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
                NanaZip::Codecs::HashProviderIdBase | Index;
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
            IHasher* Hasher = g_Hashers[Index].Create();
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

    if (Iid == __uuidof(IHasher))
    {
        if (Clsid.Data1 == SevenZipGuidData1 &&
            Clsid.Data2 == SevenZipGuidData2 &&
            Clsid.Data3 == SevenZipGuidData3Hasher)
        {
            std::uint64_t ProviderId =
                *reinterpret_cast<const std::uint64_t*>(Clsid.Data4);
            std::uint64_t ProviderIdBase = ProviderId & 0xFFFFFFFF00000000;
            std::uint32_t ProviderIndex =
                static_cast<std::uint32_t>(ProviderId);
            if (NanaZip::Codecs::HashProviderIdBase == ProviderIdBase)
            {
                if (ProviderIndex < g_HashersCount)
                {
                    *OutObject = g_Hashers[ProviderIndex].Create();
                    return S_OK;
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
                    return S_OK;
                }
            }
        }
    }

    return E_NOINTERFACE;
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
