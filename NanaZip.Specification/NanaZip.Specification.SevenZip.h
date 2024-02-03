/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Specification.SevenZip.h
 * PURPOSE:   Definition for 7-Zip Codec Interface Specification
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_SPECIFICATION_SEVENZIP
#define NANAZIP_SPECIFICATION_SEVENZIP

#include <Windows.h>

/* 7-Zip Interface GUID Format: 23170F69-40C1-278A-0000-00yy00xx0000 */

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400C00000")
IHasher : public IUnknown
{
public:
    virtual void STDMETHODCALLTYPE Init() = 0;

    virtual void STDMETHODCALLTYPE Update(
        _In_ LPCVOID Data,
        _In_ UINT32 Size) = 0;

    virtual void STDMETHODCALLTYPE Final(
        _Out_ PBYTE Digest) = 0;

    virtual UINT32 STDMETHODCALLTYPE GetDigestSize() = 0;
};

#define SEVENZIP_HASHER_PROPID_ID 0 /* VT_UI8 */
#define SEVENZIP_HASHER_PROPID_NAME 1 /* VT_BSTR */
#define SEVENZIP_HASHER_PROPID_DIGEST_SIZE 9 /* VT_UI4 */

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400C10000")
IHashers: public IUnknown
{
public:
    virtual UINT32 STDMETHODCALLTYPE GetNumHashers() = 0;

    virtual HRESULT STDMETHODCALLTYPE GetHasherProp(
        _In_ UINT32 Index,
        _In_ PROPID PropID,
        _Out_ LPPROPVARIANT Value) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateHasher(
        _In_ UINT32 Index,
        _Out_ IHasher** Hasher) = 0;
};

EXTERN_C HRESULT WINAPI GetHashers(
    _Out_ IHashers** Hashers);

#endif /* !NANAZIP_SPECIFICATION_SEVENZIP */
