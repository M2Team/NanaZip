/*
 * PROJECT:   Mile.Samples.DynamicLibrary
 * FILE:      NanaZip.Codecs.cpp
 * PURPOSE:   Implementation for NanaZip.Codecs
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Windows.h>

#include <NanaZip.Specification.SevenZip.h>

#include <winrt/Windows.Foundation.h>

struct HasherFactory : public winrt::implements<
    HasherFactory, IHashers>
{
public:

    UINT32 STDMETHODCALLTYPE GetNumHashers()
    {
        return 0;
    }

    HRESULT STDMETHODCALLTYPE GetHasherProp(
        _In_ UINT32 Index,
        _In_ PROPID PropID,
        _Inout_ LPPROPVARIANT Value)
    {
        UNREFERENCED_PARAMETER(Index);
        UNREFERENCED_PARAMETER(PropID);
        UNREFERENCED_PARAMETER(Value);
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CreateHasher(
        _In_ UINT32 Index,
        _Out_ IHasher** Hasher)
    {
        UNREFERENCED_PARAMETER(Index);
        UNREFERENCED_PARAMETER(Hasher);
        return E_NOTIMPL;
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
