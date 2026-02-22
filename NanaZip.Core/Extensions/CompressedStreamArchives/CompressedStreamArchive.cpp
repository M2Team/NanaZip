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
        _In_ const CompressedStreamArchiveInfo* Info) : m_Info(Info)
    {
        if (m_Info->InnerArc)
        {
            m_Inner = m_Info->InnerArc->CreateInArchive();
        }
    }

    void CompressedStreamArchive::DecompressionThread::Execute()
    {
        m_Compressed->Extract(&m_Index, 1, FALSE, m_Callback);
        if (m_Binder)
        {
            m_Binder->CloseWrite();
        }
    }

    HRESULT CompressedStreamArchive::StartThread()
    {
        m_Binder.Create_ReInit();
        m_Binder.CreateStreams2(m_PipeIn, m_PipeOut);

        m_Thread.m_Compressed = m_Compressed;
        m_Thread.m_Callback = this;
        m_Thread.m_OutStream = m_PipeOut;
        m_Thread.m_Binder = &m_Binder;
        m_Thread.m_Index = m_Info->Index;
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

    HRESULT CompressedStreamArchive::ReadSignature(
        _In_ IInStream* SourceStream,
        _Inout_ std::vector<Byte>& Signature) noexcept
    {
        const size_t SignatureBufSize = 1 << 20;
        size_t SignatureSize = SignatureBufSize;
        HRESULT hr;

        try
        {
            Signature.resize(SignatureBufSize);

            hr = ReadStream(SourceStream, Signature.data(), &SignatureSize);
            if (FAILED(hr))
            {
                return hr;
            }

            hr = SourceStream->Seek(0, STREAM_SEEK_SET, nullptr);
            if (FAILED(hr))
            {
                return hr;
            }

            if (0 == SignatureSize)
            {
                hr = S_FALSE;
            }

            Signature.resize(SignatureSize);
            return hr;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
        catch (...)
        {
            return E_FAIL;
        }
    }

    bool CompressedStreamArchive::CheckArc(
        _In_ const CArcInfo* ArcInfo,
        _In_reads_bytes_(SignatureSize) const Byte* Signature,
        _In_ size_t SignatureSize) noexcept
    {
        // If there's IsArc, then it has to match.
        if (ArcInfo->IsArc)
        {
            return k_IsArc_Res_YES == ArcInfo->IsArc(
                Signature,
                SignatureSize);
        }

        if (ArcInfo->Signature)
        {
            bool Result = false;

            if (!(ArcInfo->Flags & NArcInfoFlags::kMultiSignature) &&
                ArcInfo->SignatureOffset + ArcInfo->SignatureSize <= SignatureSize &&
                0 == std::memcmp(Signature + ArcInfo->SignatureOffset,
                    ArcInfo->Signature,
                    ArcInfo->SignatureSize))
            {
                Result = true;
            }
            else if (ArcInfo->Flags & NArcInfoFlags::kMultiSignature)
            {
                Byte SubSignature = 0;

                while (SubSignature < ArcInfo->SignatureSize)
                {
                    Byte SubSignatureSize = ArcInfo->Signature[SubSignature];
                    if (SubSignature + 1 + SubSignatureSize > ArcInfo->SignatureSize)
                    {
                        break;
                    }

                    if (ArcInfo->SignatureOffset + SubSignatureSize <= SignatureSize &&
                        0 == std::memcmp(Signature + ArcInfo->SignatureOffset,
                            ArcInfo->Signature + SubSignature + 1,
                            SubSignatureSize))
                    {
                        Result = true;
                        break;
                    }
                    SubSignature += SubSignatureSize + 1;
                }
            }

            return Result;
        }
        else
        {
            // !ArcInfo->Signature && !ArcInfo->IsArc, need to try Open()
            return true;
        }
    }

    HRESULT CompressedStreamArchive::ScanMetadata() noexcept
    {
        HRESULT hr = StartThread();
        if (FAILED(hr))
        {
            return hr;
        }

        std::vector<Byte> Signature;
        hr = ReadSignature(this, Signature);
        if (S_OK != hr)
        {
            goto OutStop;
        }

        if (!CheckArc(m_Info->InnerArc, Signature.data(), Signature.size()))
        {
            hr = S_FALSE;
            goto OutStop;
        }

        hr = m_Inner->Open(this, 0, m_OpenCallback);
        if (S_OK != hr)
        {
            goto OutStop;
        }

        return S_OK;

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
        hr = ReadSignature(m_SourceStream, Signature);
        if (S_OK != hr)
        {
            goto OutClose;
        }

        for (UInt32 i = 0; i < m_Info->ArcInfoCount; i++)
        {
            bool IsArc = false;
            bool HasSignature = false;
            const CArcInfo* ArcInfo = m_Info->ArcInfos[i];

            if (!ArcInfo)
            {
                continue;
            }

            if (CheckArc(ArcInfo, Signature.data(), Signature.size()))
            {
                m_Compressed = ArcInfo->CreateInArchive();
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
        if (S_OK != hr || m_Info->ItemCount != ItemCount)
        {
            m_Compressed->Close();
            m_Compressed.Release();
            hr = S_FALSE;
            goto OutClose;
        }

        {
            NWindows::NCOM::CPropVariant Prop;
            if (S_OK == m_Compressed->GetProperty(m_Info->Index, kpidSize, &Prop))
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
            if (m_DecompressedSize_Defined)
            {
                TargetPos = m_DecompressedSize + Offset;
            }
            else if (Offset == 0)
            {
                TargetPos = (UInt64)-1;
            }
            else
            {
                return E_NOTIMPL;
            }
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

    HRESULT STDMETHODCALLTYPE CompressedStreamArchive::SetOperationResult(
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
