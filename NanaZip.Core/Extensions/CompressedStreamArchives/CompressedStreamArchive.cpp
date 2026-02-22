/*
 * PROJECT:    NanaZip
 * FILE:       CompressedStreamArchive.cpp
 * PURPOSE:    Generic implementation for compressed stream archive support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */

#include <array>
#include <vector>
#include <cstring>

#include "../../SevenZip/CPP/Common/IntToString.h"
#include "../../SevenZip/CPP/Common/StringConvert.h"

#include "../../SevenZip/CPP/Windows/PropVariant.h"
#include "../../SevenZip/CPP/Windows/TimeUtils.h"

#include "../../SevenZip/CPP/7zip/PropID.h"

#include "../../SevenZip/CPP/7zip/Common/StreamUtils.h"

#include "../../SevenZip/CPP/7zip/Archive/Common/ItemNameUtils.h"

#include "CompressedStreamArchive.hpp"

namespace NanaZip::Core::Archive
{
    CompressedStreamArchive::CompressedStreamArchive(
        const CompressedStreamArchiveInfo* Info) : m_Info(Info)
    {
        // Technically we're a separate handler, and is also blockable
        // using our policy mechanism, but we should also respect
        // restrictions on the inner handler as well.
        if (MO_TRUE == K7BaseGetAllowedHandlerPolicy(m_Info->InnerHandlerName))
        {
            m_Inner = m_Info->CreateArcExported();
        }
    }

    void CompressedStreamArchive::DecompressionThread::Execute()
    {
        UInt32 Index = 0;

        Compressed->Extract(&Index, 1, FALSE, Callback);
        if (Binder)
        {
            Binder->CloseWrite();
        }
    }

    HRESULT CompressedStreamArchive::StartThread()
    {
        m_Binder.Create_ReInit();
        m_Binder.CreateStreams2(m_PipeIn, m_PipeOut);

        m_Thread.Compressed = m_Compressed;
        m_Thread.Callback = this;
        m_Thread.OutStream = m_PipeOut;
        m_Thread.Binder = &m_Binder;
        m_Thread.Exit = false;

        WRes Res = m_Thread.Create();
        if (Res != 0)
        {
            return HRESULT_FROM_WIN32(Res);
        }

        Res = m_Thread.Start();
        if (Res != 0)
        {
            return HRESULT_FROM_WIN32(Res);
        }

        m_PipePos = 0;
        return S_OK;
    }

    void CompressedStreamArchive::StopThread()
    {
        m_Thread.Exit = true;
        m_Binder.CloseRead_CallOnce();
        m_Thread.WaitThreadFinish();
        m_PipeIn.Release();
        m_PipeOut.Release();
    }

    HRESULT CompressedStreamArchive::ScanMetadata() noexcept
    {
        HRESULT hr = StartThread();
        if (FAILED(hr))
        {
            return hr;
        }

        hr = m_Inner->Open(this, 0, m_OpenCallback);
        if (S_OK != hr)
        {
            goto OutStop;
        }

        hr = Seek(0, STREAM_SEEK_SET, nullptr);
        if (FAILED(hr))
        {
            goto OutClose;
        }

        return S_OK;

    OutClose:
        m_Inner->Close();

    OutStop:
        StopThread();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::Open(
        _In_ IInStream* Stream,
        _In_opt_ const UInt64* MaxCheckStartPosition,
        _In_opt_ IArchiveOpenCallback* OpenCallback) noexcept
    {
        UInt32 ItemCount = 0;
        HRESULT hr;

        if (!Stream)
        {
            return E_INVALIDARG;
        }

        if (!m_Inner)
        {
            // disabled
            return S_FALSE;
        }

        m_Compressed.Release();
        m_SourceStream.Release();

        m_SourceStream = Stream;

        m_DecompressedSize_Defined = false;
        m_DecompressedSize = 0;

        m_OpenCallback = OpenCallback;

        std::vector<Byte> Signature;
        size_t SignatureSize;
        {
            const size_t BufSize = 1 << 20;
            Signature.resize(BufSize);
            SignatureSize = BufSize;
            hr = ReadStream(m_SourceStream,
                Signature.data(),
                &SignatureSize);
            if (FAILED(hr))
            {
                goto OutClose;
            }
            m_SourceStream->Seek(0, STREAM_SEEK_SET, nullptr);
            if (0 == SignatureSize)
            {
                hr = S_FALSE;
                goto OutClose;
            }
        }

        for (UInt32 i = 0; i < m_Info->CodecInfoCount; i++)
        {
            bool IsArc = false;
            bool HasSignature = false;
            const CodecInfo* CodecInfo = &m_Info->CodecInfos[i];

            if (MO_TRUE != K7BaseGetAllowedHandlerPolicy(
                CodecInfo->HandlerName))
            {
                // disabled
                continue;
            }

            // If there's IsArc, then it has to match.
            if (!CodecInfo->IsArc || k_IsArc_Res_YES == CodecInfo->IsArc(
                Signature.data(),
                SignatureSize))
            {
                IsArc = true;
            }

            // If there's Signature, then it has to match.
            if (!CodecInfo->Signature || (
                CodecInfo->SignatureSize <= SignatureSize &&
                0 == std::memcmp(Signature.data(),
                    CodecInfo->Signature,
                    CodecInfo->SignatureSize)))
            {
                HasSignature = true;
            }

            if (IsArc && HasSignature)
            {
                m_Compressed = CodecInfo->CreateArcExported();
                if (!m_Compressed)
                {
                    hr = E_OUTOFMEMORY;
                    goto OutClose;
                }

                hr = m_Compressed->Open(
                    m_SourceStream,
                    MaxCheckStartPosition,
                    nullptr);

                if (S_OK == hr)
                {
                    break;
                }

                m_Compressed.Release();
            }
        }

        if (!m_Compressed)
        {
            hr = S_FALSE;
            goto OutClose;
        }

        hr = m_Compressed->GetNumberOfItems(&ItemCount);
        if (S_OK != hr || 1 != ItemCount)
        {
            m_Compressed->Close();
            m_Compressed.Release();
            hr = S_FALSE;
            goto OutClose;
        }

        {
            NWindows::NCOM::CPropVariant Prop;
            if (m_Compressed->GetProperty(0, kpidSize, &Prop) == S_OK)
            {
                if (Prop.vt == VT_UI8)
                {
                    m_DecompressedSize = Prop.uhVal.QuadPart;
                    m_DecompressedSize_Defined = true;
                }
                else if (Prop.vt == VT_UI4)
                {
                    m_DecompressedSize = Prop.ulVal;
                    m_DecompressedSize_Defined = true;
                }

                if (m_OpenCallback)
                {
                    UInt64 Files = 0;
                    hr = m_OpenCallback->SetTotal(&Files, &m_DecompressedSize);
                    if (S_OK != hr)
                    {
                        goto OutClose;
                    }
                }
            }
        }

        hr = ScanMetadata();
        m_OpenCallback.Release();
        if (S_OK != hr)
        {
            goto OutClose;
        }
        return hr;

    OutClose:
        this->Close();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::Close() noexcept
    {
        m_Inner->Close();

        if (m_Compressed)
        {
            m_Compressed->Close();
            m_Compressed.Release();
        }

        StopThread();
        m_SourceStream.Release();

        m_OpenCallback.Release();

        m_DecompressedSize_Defined = false;
        m_DecompressedSize = 0;

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetNumberOfItems(
        _Out_ UInt32* NumItems) noexcept
    {
        if (!m_Inner)
        {
            *NumItems = 0;
            return S_OK;
        }

        return m_Inner->GetNumberOfItems(NumItems);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetProperty(
        _In_ UInt32 Index,
        _In_ PROPID PropId,
        _Inout_ LPPROPVARIANT Value) noexcept
    {
        if (!m_Inner)
        {
            Value->vt = VT_EMPTY;
            return S_OK;
        }

        return m_Inner->GetProperty(Index, PropId, Value);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::Extract(
        _In_opt_ const UInt32* Indices,
        _In_ UInt32 NumItems,
        _In_ BOOL TestMode,
        _In_ IArchiveExtractCallback* ExtractCallback) noexcept
    {
        if (!m_Inner)
        {
            return E_FAIL;
        }

        return m_Inner->Extract(
            Indices,
            NumItems,
            TestMode,
            ExtractCallback);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetArchiveProperty(
        _In_ PROPID PropId,
        _Inout_ LPPROPVARIANT Value) noexcept
    {
        if (!m_Inner)
        {
            Value->vt = VT_EMPTY;
            return S_OK;
        }

        return m_Inner->GetArchiveProperty(PropId, Value);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetNumberOfProperties(
        _Out_ UInt32* NumProps) noexcept
    {
        if (!m_Inner)
        {
            *NumProps = 0;
            return S_OK;
        }

        return m_Inner->GetNumberOfProperties(NumProps);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetPropertyInfo(
        _In_ UInt32 Index,
        _Out_ BSTR* Name,
        _Out_ PROPID* PropId,
        _Out_ VARTYPE* VarType) noexcept
    {
        if (!m_Inner)
        {
            return E_FAIL;
        }

        return m_Inner->GetPropertyInfo(Index, Name, PropId, VarType);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetNumberOfArchiveProperties(
        _Out_ UInt32* NumProps) noexcept
    {
        if (!m_Inner)
        {
            *NumProps = 0;
            return S_OK;
        }

        return m_Inner->GetNumberOfArchiveProperties(NumProps);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetArchivePropertyInfo(
        _In_ UInt32 Index,
        _Out_ BSTR* Name,
        _Out_ PROPID* PropId,
        _Out_ VARTYPE* VarType) noexcept
    {
        if (!m_Inner)
        {
            return E_FAIL;
        }

        return m_Inner->GetArchivePropertyInfo(Index, Name, PropId, VarType);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::Read(
        void* Data,
        UInt32 Size,
        UInt32* ProcessedSize) noexcept
    {
        if (m_Thread.Exit)
        {
            return E_ABORT;
        }

        if (!m_PipeIn)
        {
            return E_FAIL;
        }

        HRESULT hr = m_PipeIn->Read(Data, Size, ProcessedSize);
        if (SUCCEEDED(hr) && ProcessedSize)
        {
            m_PipePos += *ProcessedSize;
        }

        return hr;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::Seek(
        Int64 Offset,
        UInt32 SeekOrigin,
        UInt64* NewPosition) noexcept
    {
        UInt64 TargetPos;

        switch (SeekOrigin)
        {
        case STREAM_SEEK_SET:
            TargetPos = Offset;
            break;
        case STREAM_SEEK_CUR:
            TargetPos = m_PipePos + Offset;
            break;
        case STREAM_SEEK_END:
            if (!m_DecompressedSize_Defined)
            {
                return E_NOTIMPL;
            }
            TargetPos = m_DecompressedSize + Offset;
            break;
        default:
            return STG_E_INVALIDFUNCTION;
        }

        if (TargetPos < m_PipePos)
        {
            // Backward seek: restart
            StopThread();
            m_SourceStream->Seek(0, STREAM_SEEK_SET, nullptr);
            HRESULT hr = StartThread();
            if (FAILED(hr))
            {
                return hr;
            }
            m_PipePos = 0;
        }

        if (m_PipePos < TargetPos)
        {
            Byte Buffer[65536];

            while (m_PipePos < TargetPos)
            {
                UInt64 SkipSize = TargetPos - m_PipePos;
                UInt32 ToRead = static_cast<UInt32>(
                    (std::min)(SkipSize, sizeof(Buffer)));
                UInt32 Processed;

                HRESULT hr = Read(&Buffer[0], ToRead, &Processed);
                if (FAILED(hr))
                {
                    return hr;
                }
                if (0 == Processed)
                {
                    break; // EOF
                }
            }
        }

        if (NewPosition)
        {
            *NewPosition = m_PipePos;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::SetTotal(
        _In_ UInt64 Total) noexcept
    {
        if (m_Thread.Exit)
        {
            return E_ABORT;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::SetCompleted(
        _In_ const UInt64* CompleteValue) noexcept
    {
        if (m_Thread.Exit)
        {
            return E_ABORT;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetStream(
        _In_ UInt32 Index,
        _Out_ ISequentialOutStream** Stream,
        _In_ Int32 AskExtractMode) noexcept
    {
        if (AskExtractMode != NArchive::NExtract::NAskMode::kExtract)
        {
            *Stream = nullptr;
            return S_OK;
        }

        return this->QueryInterface(
            IID_ISequentialOutStream,
            (void**)Stream);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::PrepareOperation(
        _In_ Int32 AskExtractMode) noexcept
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetOperationResult(
        _In_ Int32 OperationResult) noexcept
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::Write(
        const void* Data,
        UInt32 Size,
        UInt32* ProcessedSize) noexcept
    {
        if (!m_PipeOut)
        {
            return E_FAIL;
        }

        return m_PipeOut->Write(Data, Size, ProcessedSize);
    }

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::GetStream(
        _In_ UInt32 Index,
        _Out_ ISequentialInStream** Stream) noexcept
    {
        if (!m_Inner)
        {
            return S_FALSE;
        }

        CMyComPtr<IInArchiveGetStream> GetStream;
        if (S_OK == m_Inner->QueryInterface(
            IID_IInArchiveGetStream,
            (void**)&GetStream) && GetStream)
        {
            return GetStream->GetStream(Index, Stream);
        }

        return E_NOTIMPL;
    }
}
