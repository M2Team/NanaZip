/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Archive.Ufs.cpp
 * PURPOSE:   Implementation for UFS/UFS2 file system image readonly support
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Codecs.h"

#ifdef _MSC_VER
#if _MSC_VER > 1000
#pragma once
#endif
#if (_MSC_VER >= 1200)
#pragma warning(push)
#endif
#pragma warning(disable:4201) // nameless struct/union
#endif

#include "FreeBSD/fs.h"
#include "FreeBSD/dir.h"

#ifdef _MSC_VER
#if (_MSC_VER >= 1200)
#pragma warning(pop)
#else
#pragma warning(default:4201) // nameless struct/union
#endif
#endif

namespace NanaZip::Codecs::Archive
{
    struct Ufs : public Mile::ComObject<Ufs, IInArchive>
    {
    public:

        Ufs()
        {

        }

        HRESULT STDMETHODCALLTYPE Open(
            _In_ IInStream* Stream,
            _In_opt_ const PUINT64 MaxCheckStartPosition,
            _In_opt_ IArchiveOpenCallback* OpenCallback)
        {
            UNREFERENCED_PARAMETER(Stream);
            UNREFERENCED_PARAMETER(MaxCheckStartPosition);
            UNREFERENCED_PARAMETER(OpenCallback);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE Close()
        {
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfItems(
            _Out_ PUINT32 NumItems)
        {
            UNREFERENCED_PARAMETER(NumItems);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetProperty(
            _In_ UINT32 Index,
            _In_ PROPID PropID,
            _Inout_ LPPROPVARIANT Value)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(Value);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE Extract(
            _In_ const PUINT32 Indices,
            _In_ UINT32 NumItems,
            _In_ BOOL TestMode,
            _In_ IArchiveExtractCallback* ExtractCallback)
        {
            UNREFERENCED_PARAMETER(Indices);
            UNREFERENCED_PARAMETER(NumItems);
            UNREFERENCED_PARAMETER(TestMode);
            UNREFERENCED_PARAMETER(ExtractCallback);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetArchiveProperty(
            _In_ PROPID PropID,
            _Inout_ LPPROPVARIANT Value)
        {
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(Value);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfProperties(
            _Out_ PUINT32 NumProps)
        {
            UNREFERENCED_PARAMETER(NumProps);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetPropertyInfo(
            _In_ UINT32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropID,
            _Out_ VARTYPE* VarType)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(Name);
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(VarType);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetNumberOfArchiveProperties(
            _Out_ PUINT32 NumProps)
        {
            UNREFERENCED_PARAMETER(NumProps);
            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE GetArchivePropertyInfo(
            _In_ UINT32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropID,
            _Out_ VARTYPE* VarType)
        {
            UNREFERENCED_PARAMETER(Index);
            UNREFERENCED_PARAMETER(Name);
            UNREFERENCED_PARAMETER(PropID);
            UNREFERENCED_PARAMETER(VarType);
            return E_NOTIMPL;
        }
    };

    IInArchive* CreateUfs()
    {
        return new Ufs();
    }
}
