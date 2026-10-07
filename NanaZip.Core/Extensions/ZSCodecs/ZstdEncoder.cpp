/*
 * PROJECT:    NanaZip.Core
 * FILE:       ZstdEncoder.cpp
 * PURPOSE:    Implementation for Zstandard encoder
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: Tu Dinh <contact@tudinh.xyz>
 */

#ifndef Z7_EXTRACT_ONLY

#include <intrin.h>

#include "ZstdEncoder.hpp"

#include <zstd_errors.h>

#include "../../SevenZip/CPP/7zip/Common/StreamUtils.h"

namespace
{
    static HRESULT ZstdErrorCodeMap[ZSTD_error_maxCode];

    static HRESULT ZstdResultToHRESULT(std::size_t Result) noexcept
    {
        static const HRESULT *CachedMap = ([]() -> const HRESULT *
        {
            for (std::size_t Index = 0; Index < ZSTD_error_maxCode; Index++)
            {
                ZstdErrorCodeMap[Index] = E_FAIL;
            }
            // Inspired by SResToHRESULT in CWrappers.cpp
            ZstdErrorCodeMap[ZSTD_error_no_error] = S_OK;
            ZstdErrorCodeMap[ZSTD_error_prefix_unknown] = S_FALSE;
            ZstdErrorCodeMap[ZSTD_error_version_unsupported] = E_NOTIMPL;
            ZstdErrorCodeMap[ZSTD_error_frameParameter_unsupported] = E_NOTIMPL;
            ZstdErrorCodeMap[ZSTD_error_frameParameter_windowTooLarge] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_corruption_detected] = S_FALSE;
            ZstdErrorCodeMap[ZSTD_error_checksum_wrong] = S_FALSE;
            ZstdErrorCodeMap[ZSTD_error_literals_headerWrong] = S_FALSE;
            ZstdErrorCodeMap[ZSTD_error_dictionary_corrupted] = S_FALSE;
            ZstdErrorCodeMap[ZSTD_error_dictionary_wrong] = S_FALSE;
            ZstdErrorCodeMap[ZSTD_error_dictionaryCreation_failed] = E_FAIL;
            ZstdErrorCodeMap[ZSTD_error_parameter_unsupported] = E_NOTIMPL;
            ZstdErrorCodeMap[ZSTD_error_parameter_combination_unsupported] = E_NOTIMPL;
            ZstdErrorCodeMap[ZSTD_error_parameter_outOfBound] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_tableLog_tooLarge] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_maxSymbolValue_tooLarge] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_maxSymbolValue_tooSmall] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_cannotProduce_uncompressedBlock] = E_FAIL;
            ZstdErrorCodeMap[ZSTD_error_stabilityCondition_notRespected] = E_FAIL;
            ZstdErrorCodeMap[ZSTD_error_stage_wrong] = E_FAIL;
            ZstdErrorCodeMap[ZSTD_error_init_missing] = E_FAIL;
            ZstdErrorCodeMap[ZSTD_error_memory_allocation] = E_OUTOFMEMORY;
            ZstdErrorCodeMap[ZSTD_error_workSpace_tooSmall] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_dstSize_tooSmall] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_srcSize_wrong] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_dstBuffer_null] = E_INVALIDARG;
            ZstdErrorCodeMap[ZSTD_error_noForwardProgress_destFull] = E_FAIL;
            ZstdErrorCodeMap[ZSTD_error_noForwardProgress_inputEmpty] = E_FAIL;

            return ZstdErrorCodeMap;
        })();

        if (!ZSTD_isError(Result))
        {
            return S_OK;
        }

        ZSTD_ErrorCode Error = ZSTD_getErrorCode(Result);
        if (Error >= ZSTD_error_maxCode)
        {
            return E_FAIL;
        }
        return CachedMap[Error];
    }

    static int LevelToZstdLevel(ULONG Level)
    {
        switch (Level)
        {
            // Just an arbitrary level mapping.
            case 0:
            case 1:
                return -3;
            case 2:
                return -1;
            case 3:
                return 1;
            case 4:
                return 3;
            case 5:
                return 6;
            case 6:
                return 10;
            case 7:
                return 16;
            case 8:
                return 19;
            case 9:
            default:
                return ZSTD_maxCLevel();
        }
    }

    static int ClampValueToInt(ULONG Value)
    {
        if (Value <= static_cast<ULONG>(INT_MAX))
        {
            return static_cast<int>(Value);
        }
        else
        {
            return INT_MAX;
        }
    }

    static int ClampValueToInt(ULONGLONG Value)
    {
        if (Value <= static_cast<ULONGLONG>(INT_MAX))
        {
            return static_cast<int>(Value);
        }
        else
        {
            return INT_MAX;
        }
    }

    static int ClampZstdParameter(const ZSTD_bounds &Bounds, int Value)
    {
        if (ZSTD_isError(Bounds.error))
        {
            return 0;
        }
        else if (Value != 0 && Value < Bounds.lowerBound)
        {
            return Bounds.lowerBound;
        }
        else if (Value > Bounds.upperBound)
        {
            return Bounds.upperBound;
        }
        else
        {
            return Value;
        }
    }
}

namespace NanaZip::Core::Extensions
{
    ZstdEncoder::ZstdEncoder()
    {
        std::size_t Result;

        this->m_Context = {ZSTD_createCCtx(), {}};
        if (!this->m_Context)
        {
            return;
        }

        Result = ZSTD_CCtx_setParameter(
            this->m_Context.get(),
            ZSTD_c_checksumFlag,
            1);
        if (ZSTD_isError(Result))
        {
            this->m_Context.reset();
            return;
        }
        Result = ZSTD_CCtx_setParameter(
            this->m_Context.get(),
            ZSTD_c_dictIDFlag,
            1);
        if (ZSTD_isError(Result))
        {
            this->m_Context.reset();
            return;
        }

        try
        {
            auto InBufferSize = ZSTD_CStreamInSize();
            if (InBufferSize > UINT32_MAX)
            {
                InBufferSize = UINT32_MAX;
            }
            m_InBuffer.resize(InBufferSize);

            auto OutBufferSize = ZSTD_CStreamOutSize();
            if (OutBufferSize > UINT32_MAX)
            {
                OutBufferSize = UINT32_MAX;
            }
            m_OutBuffer.resize(OutBufferSize);
        }
        catch (...)
        {
        }
    }

    HRESULT STDMETHODCALLTYPE ZstdEncoder::Code(
        ISequentialInStream *InStream,
        ISequentialOutStream *OutStream,
        const UInt64 *InSize,
        const UInt64 *OutSize,
        ICompressProgressInfo *Progress) noexcept
    {
        HRESULT hr;
        std::size_t Result;

        if (!this->m_Context ||
            0 == this->m_InBuffer.size() ||
            0 == this->m_OutBuffer.size())
        {
            return E_OUTOFMEMORY;
        }

        Result = ZSTD_CCtx_reset(this->m_Context.get(), ZSTD_reset_session_only);
        if (ZSTD_isError(Result))
        {
            return ZstdResultToHRESULT(Result);
        }

        if (this->m_ExpectedDataSize <
            static_cast<ULONGLONG>(ZSTD_SRCSIZEHINT_MAX))
        {
            ZSTD_CCtx_setParameter(
                this->m_Context.get(),
                ZSTD_c_srcSizeHint,
                this->m_ExpectedDataSize);
        }
        else
        {
            ZSTD_CCtx_setParameter(this->m_Context.get(), ZSTD_c_srcSizeHint, 0);
        }
        // we ignore ZSTD_c_srcSizeHint error since it doesn't block compression

        UInt64 Read = 0, Written = 0;

        while (true)
        {
            UInt32 ReadSize;
            hr = InStream->Read(m_InBuffer.data(), m_InBuffer.size(), &ReadSize);
            if (S_OK != hr)
            {
                return hr;
            }

            ZSTD_inBuffer Input{m_InBuffer.data(), ReadSize, 0};
            while (true)
            {
                ZSTD_outBuffer Output{m_OutBuffer.data(), m_OutBuffer.size(), 0};
                std::size_t LastInPosition = Input.pos;

                Result = ZSTD_compressStream2(
                    this->m_Context.get(),
                    &Output,
                    &Input,
                    0 == ReadSize ? ZSTD_e_end : ZSTD_e_continue);
                if (ZSTD_isError(Result))
                {
                    return ZstdResultToHRESULT(Result);
                }

                hr = WriteStream(
                    OutStream,
                    this->m_OutBuffer.data(),
                    Output.pos);
                if (S_OK != hr)
                {
                    return hr;
                }

                Read += Input.pos - LastInPosition;
                Written += Output.pos;

                if (Progress)
                {
                    hr = Progress->SetRatioInfo(
                        &Read,
                        &Written);
                    if (S_OK != hr)
                    {
                        return hr;
                    }
                }

                if (0 == ReadSize)
                {
                    if (0 == Result)
                    {
                        // Everything has been drained.
                        return S_OK;
                    }
                    // Still draining.
                }
                else if (Input.pos == Input.size)
                {
                    // Need more data.
                    break;
                }
            }
        }
    }

    HRESULT STDMETHODCALLTYPE ZstdEncoder::SetNumberOfThreads(
        UInt32 NumThreads) noexcept
    {
        if (!this->m_Context)
        {
            return E_FAIL;
        }

        int Value = ClampValueToInt(static_cast<ULONG>(NumThreads));

        static const ZSTD_bounds Bounds = ZSTD_cParam_getBounds(
            ZSTD_c_nbWorkers);
        ClampZstdParameter(Bounds, Value);

        std::size_t Result = ZSTD_CCtx_setParameter(
            this->m_Context.get(),
            ZSTD_c_nbWorkers,
            Value);
        return ZstdResultToHRESULT(Result);
    }

    HRESULT ZstdEncoder::SetCoderProperty(
        const PROPID PropID,
        const PROPVARIANT *Prop) noexcept
    {
        int Value;
        std::size_t Result;

        if (!this->m_Context)
        {
            return E_FAIL;
        }

        switch (PropID)
        {
            case NCoderPropID::kDictionarySize:
            {
                unsigned long Bit;

                if (VT_UI4 != Prop->vt)
                {
                    return E_INVALIDARG;
                }

                if (_BitScanReverse(&Bit, Prop->ulVal))
                {
                    Value = static_cast<int>(Bit);
                }
                else
                {
                    Value = 0;
                }

                if (Value < ZSTD_WINDOWLOG_MIN)
                {
                    Value = ZSTD_WINDOWLOG_MIN;
                }
                else if (Value > ZSTD_WINDOWLOG_LIMIT_DEFAULT)
                {
                    // Limit to avoid creating incompatible archives.
                    Value = ZSTD_WINDOWLOG_LIMIT_DEFAULT;
                }

                Result = ZSTD_CCtx_setParameter(
                    this->m_Context.get(),
                    ZSTD_c_windowLog,
                    Value);
                return ZstdResultToHRESULT(Result);
            }

            case NCoderPropID::kBlockSize:
            {
                switch (Prop->vt)
                {
                    case VT_UI4:
                        Value = ClampValueToInt(Prop->ulVal);
                        break;
                    case VT_UI8:
                        Value = ClampValueToInt(Prop->uhVal.QuadPart);
                        break;
                    default:
                        return E_INVALIDARG;
                }

                static const ZSTD_bounds Bounds = ZSTD_cParam_getBounds(
                    ZSTD_c_jobSize);
                ClampZstdParameter(Bounds, Value);

                Result = ZSTD_CCtx_setParameter(
                    this->m_Context.get(),
                    ZSTD_c_jobSize,
                    Value);
                return ZstdResultToHRESULT(Result);
            }

            case NCoderPropID::kNumThreads:
            {
                if (VT_UI4 != Prop->vt)
                {
                    return E_INVALIDARG;
                }

                return this->SetNumberOfThreads(Prop->ulVal);
            }

            case NCoderPropID::kLevel:
            {
                if (VT_UI4 != Prop->vt)
                {
                    return E_INVALIDARG;
                }

                Value = LevelToZstdLevel(Prop->ulVal);

                Result = ZSTD_CCtx_setParameter(
                    this->m_Context.get(),
                    ZSTD_c_compressionLevel,
                    Value);
                return ZstdResultToHRESULT(Result);
            }

            case NCoderPropID::kReduceSize:
            case NCoderPropID::kAffinity:
            case NCoderPropID::kNumThreadGroups:
            case NCoderPropID::kThreadGroup:
            case NCoderPropID::kAffinityInGroup:
                // Fatal props that we need to accept.
                return S_OK;

            default:
                return E_INVALIDARG;
        }
    }

    HRESULT STDMETHODCALLTYPE ZstdEncoder::SetCoderProperties(
        const PROPID *PropIDs,
        const PROPVARIANT *Props,
        UInt32 NumProps) noexcept
    {
        for (UInt32 Index = 0; Index < NumProps; Index++)
        {
            HRESULT hr = SetCoderProperty(PropIDs[Index], &Props[Index]);

            if (S_OK != hr)
            {
                return hr;
            }
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ZstdEncoder::SetCoderPropertiesOpt(
        const PROPID *PropIDs,
        const PROPVARIANT *Props,
        UInt32 NumProps) noexcept
    {
        for (UInt32 Index = 0; Index < NumProps; Index++)
        {
            switch (PropIDs[Index])
            {
                case NCoderPropID::kExpectedDataSize:
                    if (VT_UI8 != Props[Index].vt)
                    {
                        return S_OK;
                    }
                    this->m_ExpectedDataSize = Props[Index].uhVal.QuadPart;
                    break;

                default:
                    return S_OK;
            }
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ZstdEncoder::WriteCoderProperties(
        ISequentialOutStream *OutStream) noexcept
    {
        const Byte Properties = 0;
        return ::WriteStream(OutStream, &Properties, sizeof(Properties));
    }
}

#endif
