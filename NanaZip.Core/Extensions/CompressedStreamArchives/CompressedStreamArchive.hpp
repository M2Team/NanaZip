/*
 * PROJECT:    NanaZip
 * FILE:       CompressedStreamArchive.hpp
 * PURPOSE:    Generic declarations for compressed stream archive support
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */

 /*
  * Note: Only actually-compressed archives are supported for now, because
  * a "copy" handler doesn't yet exist, and even if it existed, it wouldn't
  * help us anyway since CompressedStreamArchive doesn't implement seeking.
  */

#include <Windows.h>
#include <K7Base.h>

#include "../../SevenZip/CPP/Common/MyCom.h"

#include "../../SevenZip/CPP/7zip/Common/StreamBinder.h"
#include "../../SevenZip/CPP/7zip/Common/VirtThread.h"

#include "../../SevenZip/CPP/7zip/Archive/IArchive.h"

namespace NanaZip::Core::Archive
{
    struct CodecInfo
    {
        const char* HandlerName;
        IInArchive* (*CreateArcExported)();
        UInt32(WINAPI* IsArc)(const Byte*, size_t);
        const Byte* Signature;
        size_t SignatureSize;
    };

    struct CompressedStreamArchiveInfo
    {
        const char* InnerHandlerName;
        IInArchive* (*CreateArcExported)();
        const CodecInfo* CodecInfos;
        UInt32 CodecInfoCount;
    };

    Z7_class_final(CompressedStreamArchive) :
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
        CompressedStreamArchive(const CompressedStreamArchiveInfo* Info);

    private:
        struct DecompressionThread : public CVirtThread
        {
            IInArchive* Compressed = nullptr;
            IArchiveExtractCallback* Callback = nullptr;
            ISequentialOutStream* OutStream = nullptr;
            CStreamBinder* Binder = nullptr;

            virtual void Execute() override;
        };

        const CompressedStreamArchiveInfo* m_Info;
        CMyComPtr<IInArchive> m_Inner;

        CMyComPtr<IInStream> m_SourceStream;
        CMyComPtr<IInArchive> m_Compressed;

        CStreamBinder m_Binder;
        DecompressionThread m_Thread;
        CMyComPtr<ISequentialInStream> m_PipeIn;
        CMyComPtr<ISequentialOutStream> m_PipeOut;

        CMyComPtr<IArchiveOpenCallback> m_OpenCallback;

        UInt64 m_PipePos = 0;
        bool m_DecompressedSize_Defined = false;
        UInt64 m_DecompressedSize = 0;

        HRESULT ScanMetadata() noexcept;

        HRESULT StartThread();
        void StopThread();

        // IInStream->ISequentialInStream
        HRESULT STDMETHODCALLTYPE Read(
            void* Data,
            UInt32 Size,
            UInt32* ProcessedSize) noexcept;

        // IInStream
        HRESULT STDMETHODCALLTYPE Seek(
            Int64 Offset,
            UInt32 SeekOrigin,
            UInt64* NewPosition) noexcept;

        // IArchiveExtractCallback->IProgress
        HRESULT STDMETHODCALLTYPE SetTotal(
            _In_ UInt64 Total) noexcept;
        HRESULT STDMETHODCALLTYPE SetCompleted(
            _In_ const UInt64* CompleteValue) noexcept;
    };
}
