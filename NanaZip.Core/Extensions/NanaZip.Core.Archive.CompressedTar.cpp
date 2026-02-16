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
        static const std::array<CodecInfo, 10> CodecInfos = {
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

        private:

            struct DecompressionThread : public CVirtThread
            {
                IInArchive* Compressed = nullptr;
                IArchiveExtractCallback* Callback = nullptr;
                ISequentialOutStream* OutStream = nullptr;
                CStreamBinder* Binder = nullptr;

                virtual void Execute() override
                {
                    UInt32 Indices[] = { 0 };

                    Compressed->Extract(Indices, 1, FALSE, Callback);
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
            bool m_DecompressedSize_Defined = false;
            UInt64 m_DecompressedSize = 0;

            std::vector<NArchive::NTar::CItemEx> m_Items;
            UInt32 m_OpenCodePage = CP_UTF8;
            NArchive::NTar::CEncodingCharacts m_EncodingCharacts;
            bool m_MetadataScanned = false;

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

        private:
            void TarStringToUnicode(
                const AString& String,
                NWindows::NCOM::CPropVariant& Prop,
                bool ToOS = false) const;
            static void PaxTimeToProp(
                const NArchive::NTar::CPaxTime& PaxTime,
                NWindows::NCOM::CPropVariant& Prop);
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

            // Technically we're a separate handler, and is also blockable using
            // our policy mechanism, but we should also respect restrictions on
            // the tar handler as well.
            if (MO_TRUE != K7BaseGetAllowedHandlerPolicy(TarHandlerName))
            {
                // disabled
                return S_FALSE;
            }

            m_Compressed.Release();
            m_Tar.Release();
            m_SourceStream.Release();

            m_SourceStream = Stream;

            m_DecompressedSize_Defined = false;
            m_DecompressedSize = 0;

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
                    return hr;
                }
                m_SourceStream->Seek(0, STREAM_SEEK_SET, nullptr);
                if (0 == SignatureSize)
                {
                    return S_FALSE;
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
                        return E_OUTOFMEMORY;
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
                }
            }

            return ScanMetadata();
        }

        HRESULT CompressedTar::ScanMetadata() noexcept
        {
            if (m_MetadataScanned)
            {
                return S_OK;
            }

            if (!m_Compressed)
            {
                return S_FALSE;
            }

            HRESULT hr = StartThread();
            if (FAILED(hr))
            {
                return hr;
            }

            m_Items.clear();
            m_EncodingCharacts.Clear();

            {
                NArchive::NTar::CArchive Archive;
                Archive.Clear();
                Archive.SeqStream = this;

                for (;;)
                {
                    NArchive::NTar::CItemEx Item;
                    hr = Archive.ReadItem(Item);
                    if (FAILED(hr))
                    {
                        break;
                    }
                    if (!Archive.filled)
                    {
                        break;
                    }

                    Item.EncodingCharacts.Check(Item.Name);
                    m_EncodingCharacts.Update(Item.EncodingCharacts);

                    m_Items.push_back(Item);

                    hr = Seek(
                        Item.Get_PackSize_Aligned(),
                        STREAM_SEEK_CUR,
                        nullptr);
                    if (FAILED(hr))
                    {
                        break;
                    }
                }
            }

            hr = Seek(0, STREAM_SEEK_SET, nullptr);
            if (FAILED(hr))
            {
                StopThread();
                return hr;
            }

            m_MetadataScanned = true;

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::Close() noexcept
        {
            m_Tar.Release();

            if (m_Compressed)
            {
                m_Compressed->Close();
                m_Compressed.Release();
            }

            StopThread();
            m_SourceStream.Release();

            m_DecompressedSize_Defined = false;
            m_DecompressedSize = 0;

            m_Items.clear();
            m_MetadataScanned = false;

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfItems(
            _Out_ UInt32* NumItems) noexcept
        {
            if (!m_MetadataScanned)
            {
                ScanMetadata();
            }

            *NumItems = (UInt32)m_Items.size();
            return S_OK;
        }

        // Copies of TarHandler functions below.

        static void AddSpecCharToString(const char Char, AString& String)
        {
            if ((Byte)Char <= 0x20 || (Byte)Char > 127)
            {
                String.Add_Char('[');
                String.Add_Char(GET_HEX_CHAR_LOWER((Byte)Char >> 4));
                String.Add_Char(GET_HEX_CHAR_LOWER(Char & 15));
                String.Add_Char(']');
            }
            else
            {
                String.Add_Char(Char);
            }
        }

        static void AddSpecUInt64(
            AString& String,
            const char* Name,
            UInt64 Value)
        {
            if (Value != 0)
            {
                String.Add_OptSpaced(Name);
                if (Value > 1)
                {
                    String.Add_Colon();
                    String.Add_UInt64(Value);
                }
            }
        }

        static void AddSpecBools(
            AString& String,
            const char* Name,
            bool Bool1,
            bool Bool2)
        {
            if (Bool1)
            {
                String.Add_OptSpaced(Name);
                if (Bool2)
                {
                    String.Add_Char('*');
                }
            }
        }

        void CompressedTar::TarStringToUnicode(
            const AString& String,
            NWindows::NCOM::CPropVariant& Prop,
            bool ToOS) const
        {
            UString Dest;
            if (m_OpenCodePage == CP_UTF8)
            {
                ConvertUTF8ToUnicode(String, Dest);
            }
            else
            {
                MultiByteToUnicodeString2(Dest, String, m_OpenCodePage);
            }
            if (ToOS)
            {
                NArchive::NItemName::ReplaceToOsSlashes_Remove_TailSlash(
                    Dest,
                    true);
            }
            Prop = Dest;
        }

        void CompressedTar::PaxTimeToProp(
            const NArchive::NTar::CPaxTime& PaxTime,
            NWindows::NCOM::CPropVariant& Prop)
        {
            UInt64 Value;

            if (!NWindows::NTime::UnixTime64_To_FileTime64(PaxTime.Sec, Value))
            {
                return;
            }

            if (0 != PaxTime.Ns)
            {
                Value += PaxTime.Ns / 100;
            }

            FILETIME FileTime;
            FileTime.dwLowDateTime = (DWORD)Value;
            FileTime.dwHighDateTime = (DWORD)(Value >> 32);
            Prop.SetAsTimeFrom_FT_Prec_Ns100(
                FileTime,
                k_PropVar_TimePrec_Base + (unsigned)PaxTime.NumDigits,
                PaxTime.Ns % 100);
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetProperty(
            _In_ UInt32 Index,
            _In_ PROPID PropId,
            _Inout_ LPPROPVARIANT Value) noexcept
        {
            if (Index >= (UInt32)m_Items.size())
            {
                return E_INVALIDARG;
            }

            NWindows::NCOM::CPropVariant Prop;
            const NArchive::NTar::CItemEx& Item = m_Items[Index];

            switch (PropId)
            {
            case kpidPath:
                TarStringToUnicode(Item.Name, Prop, true);
                break;

            case kpidIsDir:
                Prop = Item.IsDir();
                break;

            case kpidSize:
                Prop = Item.Get_UnpackSize();
                break;

            case kpidPackSize:
                Prop = Item.Get_PackSize_Aligned();
                break;

            case kpidMTime:
            {
                if (Item.PaxTimes.MTime.IsDefined())
                {
                    PaxTimeToProp(Item.PaxTimes.MTime, Prop);
                }
                else
                {
                    FILETIME FileTime;
                    if (NWindows::NTime::UnixTime64_To_FileTime(
                        Item.MTime,
                        FileTime))
                    {
                        unsigned Precision = k_PropVar_TimePrec_Unix;
                        if (Item.MTime_IsBin)
                        {
                            Precision = k_PropVar_TimePrec_Base;
                        }
                        Prop.SetAsTimeFrom_FT_Prec(FileTime, Precision);
                    }
                }
                break;
            }

            case kpidATime:
                if (Item.PaxTimes.ATime.IsDefined())
                {
                    PaxTimeToProp(Item.PaxTimes.ATime, Prop);
                }
                break;

            case kpidCTime:
                if (Item.PaxTimes.CTime.IsDefined())
                {
                    PaxTimeToProp(Item.PaxTimes.CTime, Prop);
                }
                break;

            case kpidPosixAttrib:
                Prop = Item.Get_Combined_Mode();
                break;

            case kpidUser:
                if (!Item.User.IsEmpty())
                {
                    TarStringToUnicode(Item.User, Prop);
                }
                break;

            case kpidGroup:
                if (!Item.Group.IsEmpty())
                {
                    TarStringToUnicode(Item.Group, Prop);
                }
                break;

            case kpidUserId:
                Prop = (UInt32)Item.UID;
                break;

            case kpidGroupId:
                Prop = (UInt32)Item.GID;
                break;

            case kpidDeviceMajor:
                if (Item.DeviceMajor_Defined)
                    Prop = (UInt32)Item.DeviceMajor;
                break;

            case kpidDeviceMinor:
                if (Item.DeviceMinor_Defined)
                    Prop = (UInt32)Item.DeviceMinor;
                break;

            case kpidSymLink:
                if (Item.Is_SymLink() && !Item.LinkName.IsEmpty())
                {
                    TarStringToUnicode(Item.LinkName, Prop);
                }
                break;

            case kpidHardLink:
                if (Item.Is_HardLink() && !Item.LinkName.IsEmpty())
                {
                    TarStringToUnicode(Item.LinkName, Prop);
                }
                break;

            case kpidCharacts:
            {
                AString String;

                {
                    String.Add_Space_if_NotEmpty();
                    AddSpecCharToString(Item.LinkFlag, String);
                }
                if (Item.IsMagic_GNU())
                {
                    String.Add_OptSpaced("GNU");
                }
                else if (Item.IsMagic_Posix_ustar_00())
                {
                    String.Add_OptSpaced("POSIX");
                }
                else
                {
                    String.Add_Space_if_NotEmpty();
                    for (unsigned i = 0; i < sizeof(Item.Magic); i++)
                    {
                        AddSpecCharToString(Item.Magic[i], String);
                    }
                }

                if (Item.IsSignedChecksum)
                {
                    String.Add_OptSpaced("SignedChecksum");
                }

                if (Item.Prefix_WasUsed)
                {
                    String.Add_OptSpaced("PREFIX");
                }

                String.Add_OptSpaced(Item.EncodingCharacts.GetCharactsString());

                AddSpecBools(
                    String,
                    "LongName",
                    Item.LongName_WasUsed,
                    Item.LongName_WasUsed_2);
                AddSpecBools(
                    String,
                    "LongLink",
                    Item.LongLink_WasUsed,
                    Item.LongLink_WasUsed_2);

                if (Item.MTime_IsBin)
                {
                    String.Add_OptSpaced("bin_mtime");
                }
                if (Item.PackSize_IsBin)
                {
                    String.Add_OptSpaced("bin_psize");
                }
                if (Item.PackSize_IsBin)
                {
                    // bin_size is same as bin_psize for Tar
                    String.Add_OptSpaced("bin_size");
                }

                AddSpecUInt64(String, "PAX", Item.Num_Pax_Records);

                if (Item.PaxTimes.MTime.IsDefined())
                {
                    String.Add_OptSpaced("mtime");
                }
                if (Item.PaxTimes.ATime.IsDefined())
                {
                    String.Add_OptSpaced("atime");
                }
                if (Item.PaxTimes.CTime.IsDefined())
                {
                    String.Add_OptSpaced("ctime");
                }

                if (Item.pax_path_WasUsed)
                {
                    String.Add_OptSpaced("pax_path");
                }
                if (Item.pax_link_WasUsed)
                {
                    String.Add_OptSpaced("pax_linkpath");
                }
                if (Item.pax_size_WasUsed)
                {
                    String.Add_OptSpaced("pax_size");
                }
                if (!Item.SCHILY_fflags.IsEmpty())
                {
                    String.Add_OptSpaced("SCHILY.fflags=");
                    String += Item.SCHILY_fflags;
                }
                if (Item.Is_Sparse())
                {
                    String.Add_OptSpaced("SPARSE");
                }
                if (Item.IsThereWarning())
                {
                    String.Add_OptSpaced("WARNING");
                }
                if (Item.HeaderError)
                {
                    String.Add_OptSpaced("ERROR");
                }
                if (Item.Method_Error)
                {
                    String.Add_OptSpaced("METHOD_ERROR");
                }
                if (Item.Pax_Error)
                {
                    String.Add_OptSpaced("PAX_error");
                }
                if (!Item.PaxExtra.RawLines.IsEmpty())
                {
                    String.Add_OptSpaced("PAX_unsupported_line");
                }
                if (Item.Pax_Overflow)
                {
                    String.Add_OptSpaced("PAX_overflow");
                }
                if (!String.IsEmpty())
                {
                    Prop = String;
                }
                break;
            }

            case kpidComment:
            {
                AString String;
                Item.PaxExtra.Print_To_String(String);
                if (!String.IsEmpty())
                {
                    Prop = String;
                }
                break;
            }

            default:
                if (m_Tar)
                {
                    return m_Tar->GetProperty(Index, PropId, Value);
                }
                break;
            }

            Prop.Detach(Value);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::Extract(
            _In_opt_ const UInt32* Indices,
            _In_ UInt32 NumItems,
            _In_ BOOL TestMode,
            _In_ IArchiveExtractCallback* ExtractCallback) noexcept
        {
            if (!m_Tar)
            {
                m_Tar = NArchive::NTar::CreateArcForTar();
                if (!m_Tar)
                {
                    return E_OUTOFMEMORY;
                }
            }

            auto Handler = reinterpret_cast<NArchive::NTar::CHandler*>(
                m_Tar.Interface());
            if (0 == Handler->_items.Size() && !m_Items.empty())
            {
                // Luckily, NTar::CHandler::_items is public, so we can inject
                // all of our cached item metadata here.
                Handler->_items.ClearAndReserve(m_Items.size());
                for (const auto& Item : m_Items)
                {
                    Handler->_items.AddInReserved(Item);
                }
                Handler->_stream = this;
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
            NWindows::NCOM::CPropVariant Prop;
            HRESULT hr;

            switch (PropId)
            {
            case kpidPhySize:
                // This prop has to come from the compression handler, or we'll
                // get an "unexpected end of archive" error.
                if (!m_Compressed)
                {
                    return S_OK;
                }

                return m_Compressed->GetArchiveProperty(PropId, Value);

            case kpidErrorFlags:
            case kpidWarningFlags:
            {
                // The error flags and messages need to be combined.

                UInt32 Flags = 0;

                if (!m_Compressed)
                {
                    return S_OK;
                }

                hr = m_Compressed->GetArchiveProperty(PropId, &Prop);
                if (S_OK != hr)
                {
                    return hr;
                }
                if (VT_UI4 == Prop.vt)
                {
                    Flags = Prop.ulVal;
                }

                if (m_Tar &&
                    S_OK == m_Tar->GetArchiveProperty(PropId, &Prop) &&
                    VT_UI4 == Prop.vt)
                {
                    Flags |= Prop.ulVal;
                }

                Prop = Flags;
                Prop.Detach(Value);
                return S_OK;
            }

            case kpidError:
            case kpidWarning:
            {
                UString Message;

                if (!m_Compressed)
                {
                    return S_OK;
                }

                hr = m_Compressed->GetArchiveProperty(PropId, &Prop);
                if (S_OK != hr)
                {
                    return hr;
                }
                if (Prop.vt == VT_BSTR)
                {
                    Message = Prop.bstrVal;
                }

                if (m_Tar &&
                    S_OK == m_Tar->GetArchiveProperty(PropId, &Prop) &&
                    Prop.vt == VT_BSTR)
                {
                    if (!Message.IsEmpty())
                    {
                        Message += ". ";
                    }
                    Message += Prop.bstrVal;
                }

                Prop = Message;
                Prop.Detach(Value);
                return S_OK;
            }

            default:
                if (!m_Tar)
                {
                    return S_OK;
                }

                return m_Tar->GetArchiveProperty(PropId, Value);
            }
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfProperties(
            _Out_ UInt32* NumProps) noexcept
        {
            *NumProps = NArchive::NTar::PropCount;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetPropertyInfo(
            _In_ UInt32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType) noexcept
        {
            if (Index >= NArchive::NTar::PropCount)
            {
                return E_INVALIDARG;
            }

            *Name = NULL;
            *PropId = NArchive::NTar::kProps[Index];
            *VarType = k7z_PROPID_To_VARTYPE[(unsigned)*PropId];
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetNumberOfArchiveProperties(
            _Out_ UInt32* NumProps) noexcept
        {
            *NumProps = NArchive::NTar::ArcPropCount;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::GetArchivePropertyInfo(
            _In_ UInt32 Index,
            _Out_ BSTR* Name,
            _Out_ PROPID* PropId,
            _Out_ VARTYPE* VarType) noexcept
        {
            if (Index >= NArchive::NTar::ArcPropCount)
            {
                return E_INVALIDARG;
            }

            *Name = NULL;
            *PropId = NArchive::NTar::kArcProps[Index];
            *VarType = k7z_PROPID_To_VARTYPE[(unsigned)*PropId];
            return S_OK;
        }

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
                const UInt32 BufSize = 65536;
                std::array<Byte, BufSize> Buffer;

                while (m_PipePos < TargetPos)
                {
                    UInt64 SkipSize = TargetPos - m_PipePos;
                    UInt32 ToRead = (SkipSize > BufSize) ?
                        BufSize :
                        (UInt32)SkipSize;
                    UInt32 Processed;

                    HRESULT hr = Read(Buffer.data(), ToRead, &Processed);
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
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE CompressedTar::SetCompleted(
            _In_ const UInt64* CompleteValue) noexcept
        {
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
