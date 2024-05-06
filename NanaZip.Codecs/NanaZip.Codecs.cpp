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
    // Definition of NanaZip interface constants
    // I had mentioned one of the reasons that I call this project NanaZip
    // because Nana is the romaji of なな which means seven in Japanese. But
    // I had not mentioned the way I confirm that: I had recalled one of the
    // Japanese VTubers called Kagura Nana when I waiting my elder sister for
    // dinner at Taiyanggong subway station, Beijing.
    // For playing more puns, NanaZip uses the K7 prefix in some definitions.
    // (K -> Kagura, 7 -> Nana)

    // 0x4823374B is big-endian representation of K7#H (H -> Hash)
    const UINT64 NanaZipHasherIdBase = 0x4823374B00000000;

    std::vector<std::pair<std::string, IHasher*>> g_Hashers =
    {
        { "BLAKE3", NanaZip::Codecs::Hash::CreateBlake3() },
    };
}

struct HasherFactory : public winrt::implements<
    HasherFactory, IHashers>
{
public:

    UINT32 STDMETHODCALLTYPE GetNumHashers()
    {
        return static_cast<UINT32>(g_Hashers.size());
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
            Value->uhVal.QuadPart = NanaZipHasherIdBase | Index;
            Value->vt = VT_UI8;
            break;
        }
        case SevenZipHasherName:
        {
            Value->bstrVal = ::SysAllocString(
                Mile::ToWideString(CP_UTF8, g_Hashers[Index].first).c_str());
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
                NanaZipHasherIdBase | Index;
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
            IHasher* Hasher = g_Hashers[Index].second;
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

        *Hasher = g_Hashers[Index].second;
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
