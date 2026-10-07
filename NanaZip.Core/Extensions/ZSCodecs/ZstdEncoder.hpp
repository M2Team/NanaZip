/*
 * PROJECT:    NanaZip
 * FILE:       ZstdEncoder.hpp
 * PURPOSE:    Definitions for Zstandard encoder
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */

#include <memory>
#include <vector>

#include <Windows.h>
#include <K7Base.h>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#include "../../SevenZip/CPP/Common/MyCom.h"
#include "../../SevenZip/CPP/7zip/ICoder.h"

namespace NanaZip::Core::Extensions
{
    Z7_class_final(ZstdEncoder) :
        public ICompressCoder,
        public ICompressSetCoderMt,
        public ICompressSetCoderProperties,
        public ICompressSetCoderPropertiesOpt,
        public ICompressWriteCoderProperties,
        public CMyUnknownImp
    {
        Z7_IFACES_IMP_UNK_5(
            ICompressCoder,
            ICompressSetCoderMt,
            ICompressSetCoderProperties,
            ICompressSetCoderPropertiesOpt,
            ICompressWriteCoderProperties);

    public:
        ZstdEncoder();

    private:
        struct ZstdDeleter
        {
            void operator()(_In_opt_ ZSTD_CCtx *Context) const
            {
                ZSTD_freeCCtx(Context);
            }
        };

        HRESULT SetCoderProperty(
            const PROPID PropID,
            const PROPVARIANT *Prop) noexcept;

        std::unique_ptr<ZSTD_CCtx, ZstdDeleter> m_Context;
        std::vector<BYTE> m_InBuffer;
        std::vector<BYTE> m_OutBuffer;
        ULONGLONG m_ExpectedDataSize = 0;
    };
}
