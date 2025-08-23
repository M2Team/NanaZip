﻿// (C) 2016 - 2020 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"
#include "Lz4Encoder.h"
#include "Lz4Decoder.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NLZ4 {

CEncoder::CEncoder():
  _processedIn(0),
  _processedOut(0),
  _inputSize(0),
  _ctx(NULL),
  _numThreads(1)
{
  _props.clear();
}

CEncoder::~CEncoder()
{
  if (_ctx)
    LZ4MT_freeCCtx(_ctx);
}

Z7_COM7F_IMF(CEncoder::SetCoderProperties(const PROPID * propIDs, const PROPVARIANT * coderProps, UInt32 numProps))
{
  _props.clear();

  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT & prop = coderProps[i];
    PROPID propID = propIDs[i];
    UInt32 v = (UInt32)prop.ulVal;
    switch (propID)
    {
    case NCoderPropID::kLevel:
      {
        if (prop.vt != VT_UI4)
          return E_INVALIDARG;

        _props._level = static_cast < Byte > (prop.ulVal);
        Byte mylevel = static_cast < Byte > (LZ4MT_LEVEL_MAX);
        if (_props._level > mylevel)
          _props._level = mylevel;

        break;
      }
    case NCoderPropID::kNumThreads:
      {
        SetNumberOfThreads(v);
        break;
      }
    default:
      {
        break;
      }
    }
  }

  return S_OK;
}

Z7_COM7F_IMF(CEncoder::WriteCoderProperties(ISequentialOutStream * outStream))
{
  return WriteStream(outStream, &_props, sizeof (_props));
}

Z7_COM7F_IMF(CEncoder::Code(ISequentialInStream *inStream,
  ISequentialOutStream *outStream, const UInt64 * /*inSize*/ ,
  const UInt64 * /*outSize */, ICompressProgressInfo *progress))
{
  LZ4MT_RdWr_t rdwr;
  size_t result;
  HRESULT res = S_OK;

  NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context = {};
  Context.InputStream = inStream;
  Context.OutputStream = outStream;
  Context.Progress = progress;
  Context.ProcessedInputSize = &_processedIn;
  Context.ProcessedOutputSize = &_processedOut;

  /* 1) setup read/write functions */
  rdwr.fn_read = ::NanaZipCodecsLz4Read;
  rdwr.fn_write = ::NanaZipCodecsLz4Write;
  rdwr.arg_read = reinterpret_cast<void*>(&Context);
  rdwr.arg_write = reinterpret_cast<void*>(&Context);

  /* 2) create compression context, if needed */
  if (!_ctx)
    _ctx = LZ4MT_createCCtx(_numThreads, _props._level, _inputSize);
  if (!_ctx)
    return S_FALSE;

  /* 3) compress */
  result = LZ4MT_compressCCtx(_ctx, &rdwr);
  if (LZ4MT_isError(result)) {
    if (result == (size_t)-LZ4MT_error_canceled)
      return E_ABORT;
    return E_FAIL;
  }

  return res;
}

Z7_COM7F_IMF(CEncoder::SetNumberOfThreads(UInt32 numThreads))
{
  const UInt32 kNumThreadsMax = LZ4MT_THREAD_MAX;
  if (numThreads < 1) numThreads = 1;
  if (numThreads > kNumThreadsMax) numThreads = kNumThreadsMax;
  _numThreads = numThreads;
  return S_OK;
}

}}
#endif
