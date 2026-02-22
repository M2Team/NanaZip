/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Core.Archive.CompressedTar.cpp
 * PURPOSE:    Implementation for compressed tarball support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */

 /*
  * Note: Only actually-compressed tarballs are supported for now, because
  * a "copy" handler doesn't yet exist, and even if it existed, it wouldn't
  * help us anyway since CompressedTar doesn't implement seeking.
  */

#include <array>
#include <vector>
#include <cstring>

#include <Windows.h>

#include <K7Base.h>

#include "../SevenZip/CPP/Common/IntToString.h"
#include "../SevenZip/CPP/Common/MyCom.h"
#include "../SevenZip/CPP/Common/StringConvert.h"

#include "../SevenZip/CPP/Windows/PropVariant.h"
#include "../SevenZip/CPP/Windows/TimeUtils.h"

#include "../SevenZip/CPP/7zip/PropID.h"

#include "../SevenZip/CPP/7zip/Common/RegisterArc.h"
#include "../SevenZip/CPP/7zip/Common/StreamBinder.h"
#include "../SevenZip/CPP/7zip/Common/StreamUtils.h"
#include "../SevenZip/CPP/7zip/Common/VirtThread.h"

#include "../SevenZip/CPP/7zip/Archive/Common/ItemNameUtils.h"
#include "../SevenZip/CPP/7zip/Archive/IArchive.h"

#include "../SevenZip/CPP/7zip/Archive/Tar/TarIn.h"
#include "../SevenZip/CPP/7zip/Archive/Tar/TarHandler.h"

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
        extern const Byte kProps[];
        extern const UInt32 PropCount;
        extern const Byte kArcProps[];
        extern const UInt32 ArcPropCount;
    }

    namespace NCompressedTar
    {
        struct CodecInfo
        {
            const char* HandlerName;
            IInArchive* (*CreateArcForTar)();
            UInt32(WINAPI* IsArc)(const Byte*, size_t);
            const Byte* Signature;
            size_t SignatureSize;
        };

        static const Byte XzSignature[] = { 0xFD, '7', 'z', 'X', 'Z', 0x00 };

        // All handler names below must be synced with 7-Zip and 7-Zip ZS.
        static const char* TarHandlerName = "tar";
        static const CodecInfo CodecInfos[] = {
            CodecInfo {
                "brotli",
                NArchive::NBROTLI::CreateArcForTar,
                NArchive::NBROTLI::IsArc_Brotli,
                nullptr,
                0
            },
            CodecInfo {
                "bzip2",
                NArchive::NBz2::CreateArcForTar,
                NArchive::NBz2::IsArc_BZip2,
                nullptr,
                0
            },
            CodecInfo {
                "gzip",
                NArchive::NGz::CreateArcForTar,
                NArchive::NGz::IsArc_Gz,
                nullptr,
                0
            },
            CodecInfo {
                "lizard",
                NArchive::NLIZARD::CreateArcForTar,
                NArchive::NLIZARD::IsArc_lizard,
                nullptr,
                0
            },
            CodecInfo {
                "lz4",
                NArchive::NLZ4::CreateArcForTar,
                NArchive::NLZ4::IsArc_lz4,
                nullptr,
                0
            },
            CodecInfo {
                "lz5",
                NArchive::NLZ5::CreateArcForTar,
                NArchive::NLZ5::IsArc_lz5,
                nullptr,
                0
            },
            CodecInfo {
                "lzip",
                NArchive::NLz::CreateArcForTar,
                NArchive::NLz::IsArc_Lz,
                nullptr,
                0
            },
            CodecInfo {
                "xz",
                NArchive::NXz::CreateArcForTar,
                nullptr,
                XzSignature,
                sizeof(XzSignature)
            },
            CodecInfo {
                "Z",
                NArchive::NZ::CreateArcForTar,
                NArchive::NZ::IsArc_Z,
                nullptr,
                0
            },
            CodecInfo {
                "zstd",
                NArchive::NZSTD::CreateArcForTar,
                NArchive::NZSTD::IsArc_zstd,
                nullptr,
                0
            },
            // Handlers with neither IsArc nor Signature must be at the end.
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

        public:
            CompressedTar()
            {
                // Technically we're a separate handler, and is also blockable
                // using our policy mechanism, but we should also respect
                // restrictions on the tar handler as well.
                if (MO_TRUE == K7BaseGetAllowedHandlerPolicy(TarHandlerName))
                {
                    m_Tar = NArchive::NTar::CreateArcForTar();
                }
            }

        private:
            struct DecompressionThread : public CVirtThread
            {
                IInArchive* Compressed = nullptr;
                IArchiveExtractCallback* Callback = nullptr;
                ISequentialOutStream* OutStream = nullptr;
                CStreamBinder* Binder = nullptr;

                virtual void Execute() override
                {
                    UInt32 Index = 0;

                    Compressed->Extract(&Index, 1, FALSE, Callback);
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

            CMyComPtr<IArchiveOpenCallback> m_OpenCallback;

            UInt64 m_PipePos = 0;
            bool m_DecompressedSize_Defined = false;
            UInt64 m_DecompressedSize = 0;

            HRESULT ScanMetadata() noexcept;

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
            UInt32 ItemCount = 0;
            HRESULT hr;

            if (!Stream)
            {
                return E_INVALIDARG;
            }

            if (!m_Tar)
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

            for (const auto& CodecInfo : CodecInfos)
            {
                bool IsArc = false;
                bool HasSignature = false;

                if (MO_TRUE != K7BaseGetAllowedHandlerPolicy(
                    CodecInfo.HandlerName))
                {
                    // disabled
                    continue;
                }

                // If there's IsArc, then it has to match.
                if (!CodecInfo.IsArc || k_IsArc_Res_YES == CodecInfo.IsArc(
                    Signature.data(),
                    SignatureSize))
                {
                    IsArc = true;
                }

                // If there's Signature, then it has to match.
                if (!CodecInfo.Signature || (
                    CodecInfo.SignatureSize <= SignatureSize &&
                    0 == std::memcmp(Signature.data(),
                        CodecInfo.Signature,
                        CodecInfo.SignatureSize)))
                {
                    HasSignature = true;
                }

                if (IsArc && HasSignature)
                {
                    m_Compressed = CodecInfo.CreateArcForTar();
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

        HRESULT CompressedTar::ScanMetadata() noexcept
        {
            HRESULT hr = StartThread();
            if (FAILED(hr))
            {
                return hr;
            }

            hr = m_Tar->Open(this, 0, m_OpenCallback);
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
            m_Tar->Close();

        OutStop:
            StopThread();
            return hr;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::Close() noexcept
        {
            m_Tar->Close();

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

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfItems(
            _Out_ UInt32* NumItems) noexcept
        {
            if (!m_Tar)
            {
                *NumItems = 0;
                return S_OK;
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
                Value->vt = VT_EMPTY;
                return S_OK;
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
                return E_FAIL;
            }

            return m_Tar->Extract(
                Indices,
                NumItems,
                TestMode,
                ExtractCallback);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetArchiveProperty(
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value) noexcept
        {
            if (!m_Tar)
            {
                Value->vt = VT_EMPTY;
                return S_OK;
            }

            return m_Tar->GetArchiveProperty(PropId, Value);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfProperties(
            _Out_ UInt32* NumProps) noexcept
        {
            if (!m_Tar)
            {
                *NumProps = 0;
                return S_OK;
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
                return E_FAIL;
            }

            return m_Tar->GetPropertyInfo(Index, Name, PropId, VarType);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfArchiveProperties(
            _Out_ UInt32* NumProps) noexcept
        {
            if (!m_Tar)
            {
                *NumProps = 0;
                return S_OK;
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
                return E_FAIL;
            }

            return m_Tar->GetArchivePropertyInfo(Index, Name, PropId, VarType);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::Read(
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

        HRESULT STDMETHODCALLTYPE CompressedTar::Seek(
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

        HRESULT STDMETHODCALLTYPE CompressedTar::SetTotal(
            _In_ UInt64 Total) noexcept
        {
            if (m_Thread.Exit)
            {
                return E_ABORT;
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::SetCompleted(
            _In_ const UInt64* CompleteValue) noexcept
        {
            if (m_Thread.Exit)
            {
                return E_ABORT;
            }

            return S_OK;
        }

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
