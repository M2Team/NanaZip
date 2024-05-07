/*
 * PROJECT:   Mile.Samples.DynamicLibrary
 * FILE:      NanaZip.Codecs.cpp
 * PURPOSE:   Implementation for NanaZip.Codecs
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#include <Mile.Helpers.CppBase.h>

#include <winrt/Windows.Foundation.h>

#include <string>
#include <vector>
#include <utility>

namespace
{
    struct HashProviderItem
    {
        const char* Name;
        IHasher* Interface;
    };

    HashProviderItem g_Hashers[] =
    {
        { "BLAKE3", NanaZip::Codecs::Hash::CreateBlake3() },
        { "SM3", NanaZip::Codecs::Hash::CreateSm3() },
        { "MD2", NanaZip::Codecs::Hash::CreateMd2() },
        { "AICH", NanaZip::Codecs::Hash::CreateAich() },
        { "BLAKE2b", NanaZip::Codecs::Hash::CreateBlake2b() },
        { "ED2K", NanaZip::Codecs::Hash::CreateEd2k() },
        { "EDON-R-224", NanaZip::Codecs::Hash::CreateEdonR224() },
        { "EDON-R-256", NanaZip::Codecs::Hash::CreateEdonR256() },
        { "EDON-R-384", NanaZip::Codecs::Hash::CreateEdonR384() },
        { "EDON-R-512", NanaZip::Codecs::Hash::CreateEdonR512() },
        { "GOST94", NanaZip::Codecs::Hash::CreateGost94() },
        { "GOST94CryptoPro", NanaZip::Codecs::Hash::CreateGost94CryptoPro() },
        { "GOST12-256", NanaZip::Codecs::Hash::CreateGost12256() },
        { "GOST12-512", NanaZip::Codecs::Hash::CreateGost12512() },
        { "HAS-160", NanaZip::Codecs::Hash::CreateHas160() },
        { "MD4", NanaZip::Codecs::Hash::CreateMd4() },
        { "MD5", NanaZip::Codecs::Hash::CreateMd5() },
        { "RIPEMD-160", NanaZip::Codecs::Hash::CreateRipemd160() },
        { "SHA224", NanaZip::Codecs::Hash::CreateSha224() },
        { "SHA384", NanaZip::Codecs::Hash::CreateSha384() },
        { "SHA512", NanaZip::Codecs::Hash::CreateSha512() },
        { "SHA3-224", NanaZip::Codecs::Hash::CreateSha3224() },
        { "SHA3-256", NanaZip::Codecs::Hash::CreateSha3256() },
        { "SHA3-384", NanaZip::Codecs::Hash::CreateSha3384() },
        { "SHA3-512", NanaZip::Codecs::Hash::CreateSha3512() },
        { "SNEFRU-128", NanaZip::Codecs::Hash::CreateSnefru128() },
        { "SNEFRU-256", NanaZip::Codecs::Hash::CreateSnefru256() },
        { "TIGER", NanaZip::Codecs::Hash::CreateTiger() },
        { "TIGER2", NanaZip::Codecs::Hash::CreateTiger2() },
        { "BTIH", NanaZip::Codecs::Hash::CreateTorrent() },
        { "TTH", NanaZip::Codecs::Hash::CreateTth() },
        { "WHIRLPOOL", NanaZip::Codecs::Hash::CreateWhirlpool() },
    };

    const size_t g_HashersCount = sizeof(g_Hashers) / sizeof(*g_Hashers);
}

struct HasherFactory : public winrt::implements<
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
            IHasher* Hasher = g_Hashers[Index].Interface;
            if (Hasher)
            {
                Value->ulVal = Hasher->GetDigestSize();
                Value->vt = VT_UI4;
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

        *Hasher = g_Hashers[Index].Interface;
        if (*Hasher)
        {
            (*Hasher)->AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }
};

EXTERN_C HRESULT WINAPI GetHashers(
    _Out_ IHashers** Hashers)
{
    if (!Hashers)
    {
        return E_INVALIDARG;
    }

    try
    {
        *Hashers = winrt::make<HasherFactory>().detach();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }

    return S_OK;
}
