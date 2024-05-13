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

// 7-Zip Interface GUID Format: 23170F69-40C1-278A-0000-00yy00xx0000

const UINT32 SevenZipGuidData1 = 0x23170F69;
const UINT16 SevenZipGuidData2 = 0x40C1;
const UINT16 SevenZipGuidData3Common = 0x278A;
const UINT16 SevenZipGuidData3Decoder = 0x2790;
const UINT16 SevenZipGuidData3Encoder = 0x2791;
const UINT16 SevenZipGuidData3Hasher = 0x2792;

MIDL_INTERFACE("23170F69-40C1-278A-0000-000300010000")
ISequentialOutStream : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE Read(
        _Out_opt_ LPVOID Data,
        _In_ UINT32 Size,
        _Out_ PUINT32 ProcessedSize) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000300020000")
ISequentialInStream : public IUnknown
{
public:

    virtual HRESULT STDMETHODCALLTYPE Write(
        _In_opt_ LPCVOID Data,
        _In_ UINT32 Size,
        _Out_ PUINT32 ProcessedSize) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400040000")
ICompressProgressInfo : public IUnknown
{
public:

     virtual HRESULT STDMETHODCALLTYPE SetRatioInfo(
         _In_opt_ const PUINT64 InSize,
         _In_opt_ const PUINT64 OutSize) = 0;
};

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400050000")
ICompressCoder : public IUnknown
{
public:

     virtual HRESULT STDMETHODCALLTYPE Code(
         _In_ ISequentialInStream* InStream,
         _In_ ISequentialOutStream* OutStream,
         _In_opt_ const PUINT64 InSize,
         _In_opt_ const PUINT64 OutSize,
         _In_opt_ ICompressProgressInfo* Progress) = 0;
};

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

typedef enum _SEVENZIP_HASHER_PROPERTY_TYPE
{
    SevenZipHasherId = 0, // VT_UI8
    SevenZipHasherName = 1, // VT_BSTR
    SevenZipHasherEncoder = 3, // VT_BSTR (Actually GUID structure)
    SevenZipHasherDigestSize = 9, // VT_UI4
} SEVENZIP_HASHER_PROPERTY_TYPE, *PSEVENZIP_HASHER_PROPERTY_TYPE;

MIDL_INTERFACE("23170F69-40C1-278A-0000-000400C10000")
IHashers: public IUnknown
{
public:
    virtual UINT32 STDMETHODCALLTYPE GetNumHashers() = 0;

    virtual HRESULT STDMETHODCALLTYPE GetHasherProp(
        _In_ UINT32 Index,
        _In_ PROPID PropId,
        _Inout_ LPPROPVARIANT Value) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateHasher(
        _In_ UINT32 Index,
        _Out_ IHasher** Hasher) = 0;
};

EXTERN_C HRESULT WINAPI GetHashers(
    _Out_ IHashers** Hashers);

// IArchiveOpenSeq

// ICompressSetCoderMt

// ICompressSetCoderProperties

// ICompressSetDecoderProperties2

// ICompressSetInStream

// ICompressWriteCoderProperties

// IInArchive

// IOutArchive

// ISetProperties

#endif /* !NANAZIP_SPECIFICATION_SEVENZIP */
