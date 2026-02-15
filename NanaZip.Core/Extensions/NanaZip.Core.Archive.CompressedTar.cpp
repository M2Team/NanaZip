/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Core.Archive.CompressedTar.cpp
 * PURPOSE:    Implementation for compressed Tar support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */


#include <array>

#include <Windows.h>

#include "../SevenZip/CPP/Common/MyCom.h"
#include "../SevenZip/CPP/7zip/Common/RegisterArc.h"

#include "../SevenZip/CPP/7zip/Archive/IArchive.h"

#include "../SevenZip/CPP/7zip/Common/StreamBinder.h"
#include "../SevenZip/CPP/7zip/Common/VirtThread.h"

namespace NArchive
{
    namespace NBROTLI
    {
        EXTERN_C UInt32 WINAPI IsArc_Brotli(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NBz2
    {
        EXTERN_C UInt32 WINAPI IsArc_BZip2(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NGz
    {
        EXTERN_C UInt32 WINAPI IsArc_Gz(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NLIZARD
    {
        EXTERN_C UInt32 WINAPI IsArc_lizard(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NLZ4
    {
        EXTERN_C UInt32 WINAPI IsArc_lz4(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NLZ5
    {
        EXTERN_C UInt32 WINAPI IsArc_lz5(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NLz
    {
        EXTERN_C UInt32 WINAPI IsArc_Lz(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NXz
    {
        IInArchive* CreateArcForTar();
    }

    namespace NZ
    {
        EXTERN_C UInt32 WINAPI IsArc_Z(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NZSTD
    {
        EXTERN_C UInt32 WINAPI IsArc_zstd(const Byte* p, size_t size);
        IInArchive* CreateArcForTar();
    }

    namespace NTar
    {
        IInArchive* CreateArcForTar();
    }

    namespace NCompressedTar
    {
        struct CodecInfo
        {
            UInt32(WINAPI* IsArc)(const Byte*, size_t);
            IInArchive* (*CreateArcForTar)();
        };

        static const std::array<CodecInfo, 10> CodecInfos = {
            CodecInfo {
                NArchive::NBROTLI::IsArc_Brotli,
                NArchive::NBROTLI::CreateArcForTar
            },
            CodecInfo {
                NArchive::NBz2::IsArc_BZip2,
                NArchive::NBz2::CreateArcForTar
            },
            CodecInfo {
                NArchive::NGz::IsArc_Gz,
                NArchive::NGz::CreateArcForTar
            },
            CodecInfo {
                NArchive::NLIZARD::IsArc_lizard,
                NArchive::NLIZARD::CreateArcForTar
            },
            CodecInfo {
                NArchive::NLZ4::IsArc_lz4,
                NArchive::NLZ4::CreateArcForTar
            },
            CodecInfo {
                NArchive::NLZ5::IsArc_lz5,
                NArchive::NLZ5::CreateArcForTar
            },
            CodecInfo {
                NArchive::NLz::IsArc_Lz,
                NArchive::NLz::CreateArcForTar
            },
            CodecInfo {
                nullptr,
                NArchive::NXz::CreateArcForTar
            },
            CodecInfo {
                NArchive::NZ::IsArc_Z,
                NArchive::NZ::CreateArcForTar
            },
            CodecInfo {
                NArchive::NZSTD::IsArc_zstd,
                NArchive::NZSTD::CreateArcForTar
            },
        };

        Z7_class_final(CompressedTar) :
            public IInArchive,
            public IInStream,
            public IArchiveExtractCallback,
            public ISequentialOutStream,
            public IInArchiveGetStream,
            public CMyUnknownImp
        {
            Z7_IFACES_IMP_UNK_5(
                IInArchive,
                IInStream,
                IArchiveExtractCallback,
                ISequentialOutStream,
                IInArchiveGetStream);

        private:

            struct DecompressionThread : public CVirtThread
            {
                IInArchive* Compressed = nullptr;
                IArchiveExtractCallback* Callback = nullptr;
                ISequentialOutStream* OutStream = nullptr;
                CStreamBinder* Binder = nullptr;

                virtual void Execute() override
                {
                    UInt32 indices[] = { 0 };
                    Compressed->Extract(indices, 1, FALSE, Callback);
                    if (Binder)
                    {
                        Binder->CloseWrite();
                    }
                }
            };

            CMyComPtr<IInStream> m_SourceStream;
            CMyComPtr<IInArchive> m_Compressed;
            CMyComPtr<IInArchive> m_Tar;

            CStreamBinder m_Binder;
            DecompressionThread m_Thread;
            CMyComPtr<ISequentialInStream> m_PipeIn;
            CMyComPtr<ISequentialOutStream> m_PipeOut;

            UInt64 m_PipePos = 0;

            HRESULT StartThread()
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

            void StopThread()
            {
                m_Thread.Exit = true;
                m_Binder.CloseRead_CallOnce();
                m_Thread.WaitThreadFinish();
                m_PipeIn.Release();
                m_PipeOut.Release();
            }

        public:
            HRESULT STDMETHODCALLTYPE Read(
                void* Data,
                UInt32 Size,
                UInt32* ProcessedSize) noexcept;
            HRESULT STDMETHODCALLTYPE SetTotal(_In_ UInt64 Total) noexcept;
            HRESULT STDMETHODCALLTYPE SetCompleted(
                _In_ const UInt64* CompleteValue) noexcept;
        };

        HRESULT STDMETHODCALLTYPE CompressedTar::Open(
            _In_ IInStream* Stream,
            _In_opt_ const UInt64* MaxCheckStartPosition,
            _In_opt_ IArchiveOpenCallback* OpenCallback) noexcept
        {
            HRESULT hr;

            if (!Stream)
            {
                return E_INVALIDARG;
            }

            m_Compressed.Release();
            m_Tar.Release();
            m_SourceStream.Release();

            m_SourceStream = Stream;

            // Max common size for all handlers
            std::array<Byte, 16> Signature;
            UInt32 SignatureSize;
            {
                UInt64 currentPos;
                hr = m_SourceStream->Seek(0, STREAM_SEEK_CUR, &currentPos);
                if (S_OK != hr)
                {
                    return hr;
                }

                hr = m_SourceStream->Read(
                    &Signature[0],
                    Signature.size(),
                    &SignatureSize);
                m_SourceStream->Seek(currentPos, STREAM_SEEK_SET, nullptr);

                if (S_OK != hr || SignatureSize != Signature.size())
                {
                    return S_FALSE;
                }
            }

            for (const auto& CodecInfo : CodecInfos)
            {
                if (nullptr != CodecInfo.IsArc &&
                    k_IsArc_Res_YES == CodecInfo.IsArc(
                        Signature.data(),
                        Signature.size()))
                {
                    m_Compressed = CodecInfo.CreateArcForTar();
                    if (!m_Compressed)
                    {
                        return E_OUTOFMEMORY;
                    }
                    break;
                }
            }

            if (m_Compressed)
            {
                hr = m_Compressed->Open(
                    m_SourceStream,
                    MaxCheckStartPosition,
                    OpenCallback);
                if (S_OK != hr)
                {
                    m_Compressed.Release();
                }
            }

            if (!m_Compressed)
            {
                // Nothing detected via signature
                for (const auto& CodecInfo : CodecInfos)
                {
                    if (!CodecInfo.IsArc)
                    {
                        m_Compressed = CodecInfo.CreateArcForTar();
                        if (!m_Compressed)
                        {
                            continue;
                        }

                        hr = m_Compressed->Open(
                            m_SourceStream,
                            MaxCheckStartPosition,
                            OpenCallback);
                        if (S_OK == hr)
                        {
                            break;
                        }

                        m_Compressed.Release();
                    }
                }
            }

            if (!m_Compressed)
            {
                return S_FALSE;
            }

            UInt32 ItemCount = 0;
            hr = m_Compressed->GetNumberOfItems(&ItemCount);
            if (S_OK != hr || 1 != ItemCount)
            {
                m_Compressed->Close();
                m_Compressed.Release();
                return S_FALSE;
            }

            hr = StartThread();
            if (S_OK != hr)
            {
                m_Compressed->Close();
                m_Compressed.Release();
                return hr;
            }

            m_Tar = NArchive::NTar::CreateArcForTar();
            if (!m_Tar)
            {
                StopThread();
                m_Compressed->Close();
                m_Compressed.Release();
                return E_OUTOFMEMORY;
            }

            // We need to pass ourselves as IInStream to m_Tar, because m_Tar
            // might call Seek(0) which we need to handle by restarting the
            // thread.
            CMyComPtr<IArchiveOpenSeq> TarOpenSeq;
            if (S_OK == m_Tar->QueryInterface(
                IID_IArchiveOpenSeq,
                (void**)&TarOpenSeq) && TarOpenSeq)
            {
                hr = TarOpenSeq->OpenSeq(this);
            }
            else
            {
                hr = m_Tar->Open(this, nullptr, OpenCallback);
            }

            if (S_OK != hr)
            {
                m_Tar.Release();
                StopThread();
                m_Compressed->Close();
                m_Compressed.Release();
                return hr;
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::Close() noexcept
        {
            if (m_Tar)
            {
                m_Tar->Close();
                m_Tar.Release();
            }

            if (m_Compressed)
            {
                m_Compressed->Close();
                m_Compressed.Release();
            }

            StopThread();
            m_SourceStream.Release();

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfItems(
            _Out_ UInt32* NumItems) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->GetNumberOfItems(NumItems);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetProperty(
            _In_ UInt32 Index,
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->GetProperty(Index, PropId, Value);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::Extract(
            _In_opt_ const UInt32* Indices,
            _In_ UInt32 NumItems,
            _In_ BOOL TestMode,
            _In_ IArchiveExtractCallback* ExtractCallback) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->Extract(Indices, NumItems, TestMode, ExtractCallback);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetArchiveProperty(
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->GetArchiveProperty(PropId, Value);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfProperties(
            _Out_ UInt32* NumProps) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->GetNumberOfProperties(NumProps);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetPropertyInfo(
            _In_ UInt32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->GetPropertyInfo(Index, Name, PropId, VarType);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfArchiveProperties(
            _Out_ UInt32* NumProps) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->GetNumberOfArchiveProperties(NumProps);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetArchivePropertyInfo(
            _In_ UInt32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            return m_Tar->GetArchivePropertyInfo(Index, Name, PropId, VarType);
        }

        // IInStream::Read
        HRESULT STDMETHODCALLTYPE CompressedTar::Read(
            void* Data,
            UInt32 Size,
            UInt32* ProcessedSize) noexcept
        {
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

        // IInStream::Seek
        HRESULT STDMETHODCALLTYPE CompressedTar::Seek(
            Int64 Offset,
            UInt32 SeekOrigin,
            UInt64* NewPosition) noexcept
        {
            HRESULT hr;

            if (SeekOrigin == STREAM_SEEK_SET && Offset == 0)
            {
                if (m_PipePos != 0)
                {
                    StopThread();
                    m_SourceStream->Seek(0, STREAM_SEEK_SET, nullptr);
                    hr = StartThread();
                    if (S_OK != hr)
                    {
                        return hr;
                    }
                }
                if (NewPosition) *NewPosition = 0;
                return S_OK;
            }

            if (SeekOrigin == STREAM_SEEK_CUR && Offset == 0)
            {
                if (NewPosition)
                {
                    *NewPosition = m_PipePos;
                }

                return S_OK;
            }

            return E_NOTIMPL;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::SetTotal(
            _In_ UInt64 Total) noexcept
        {
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::SetCompleted(
            _In_ const UInt64* CompleteValue) noexcept
        {
            return S_OK;
        }

        // IArchiveExtractCallback::GetStream
        HRESULT STDMETHODCALLTYPE CompressedTar::GetStream(
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

        HRESULT STDMETHODCALLTYPE CompressedTar::PrepareOperation(
            _In_ Int32 AskExtractMode) noexcept
        {
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::SetOperationResult(
            _In_ Int32 OperationResult) noexcept
        {
            return S_OK;
        }

        // ISequentialOutStream::Write
        HRESULT STDMETHODCALLTYPE CompressedTar::Write(
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

        // IInArchiveGetStream::GetStream
        HRESULT STDMETHODCALLTYPE CompressedTar::GetStream(
            _In_ UInt32 Index,
            _Out_ ISequentialInStream** Stream) noexcept
        {
            if (!m_Tar)
            {
                return S_FALSE;
            }

            CMyComPtr<IInArchiveGetStream> GetStream;
            if (S_OK == m_Tar->QueryInterface(
                IID_IInArchiveGetStream,
                (void**)&GetStream) && GetStream)
            {
                return GetStream->GetStream(Index, Stream);
            }

            return E_NOTIMPL;
        }

        static IInArchive* CreateArc()
        {
            return new CompressedTar();
        }

        extern const CArcInfo CompressedTarArcInfo = {
            NArcInfoFlags::kStartOpen |
            NArcInfoFlags::kSymLinks |
            NArcInfoFlags::kHardLinks |
            NArcInfoFlags::kMTime |
            NArcInfoFlags::kMTime_Default,
            0xA,
            0,
            0,
            nullptr,
            "CompressedTar",
            "br brotli tbr "
                "bz2 bzip2 tbz2 tbz "
                "gz gzip tgz tpz apk "
                "liz tliz "
                "lz4 tlz4 "
                "lz5 tlz5 "
                "lz tlz "
                "xz txz "
                "z taz "
                "zst zstd tzst tzstd",
            nullptr,
            ((UInt32)1 << (
                NArcInfoTimeFlags::kTime_Prec_Mask_bit_index +
                (NFileTimeType::kWindows))) |
            ((UInt32)1 << (
                NArcInfoTimeFlags::kTime_Prec_Mask_bit_index +
                (NFileTimeType::kUnix))) |
            ((UInt32)1 << (
                NArcInfoTimeFlags::kTime_Prec_Mask_bit_index +
                (NFileTimeType::k1ns))) |
            ((UInt32)(NFileTimeType::kUnix) <<
                NArcInfoTimeFlags::kTime_Prec_Default_bit_index),
            CreateArc,
            0,
            nullptr,
        };

        struct CRegisterArc
        {
            CRegisterArc()
            {
                RegisterArc(&CompressedTarArcInfo);
            }
        };

        static CRegisterArc g_RegisterArc;
    }
}
