﻿// (C) 2016 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"
#include "Lz5Decoder.h"

namespace NCompress {
namespace NLZ5 {

CDecoder::CDecoder():
  _processedIn(0),
  _processedOut(0),
  _inputSize(0),
  _numThreads(NWindows::NSystem::GetNumberOfProcessors())
{
  _props.clear();
}

CDecoder::~CDecoder()
{
}

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte * prop, UInt32 size))
{
  DProps *pProps = (DProps *)prop;

  switch (size) {
  case 3:
    memcpy(&_props, pProps, 3);
    return S_OK;
  case 5:
    memcpy(&_props, pProps, 5);
    return S_OK;
  default:
    return E_NOTIMPL;
  }
}

Z7_COM7F_IMF(CDecoder::SetNumberOfThreads(UInt32 numThreads))
{
  const UInt32 kNumThreadsMax = LZ5MT_THREAD_MAX;
  if (numThreads < 1) numThreads = 1;
  if (numThreads > kNumThreadsMax) numThreads = kNumThreadsMax;
  _numThreads = numThreads;
  return S_OK;
}

HRESULT CDecoder::SetOutStreamSizeResume(const UInt64 * /*outSize*/)
{
  _processedOut = 0;
  return S_OK;
}

Z7_COM7F_IMF(CDecoder::SetOutStreamSize(const UInt64 * outSize))
{
  _processedIn = 0;
  RINOK(SetOutStreamSizeResume(outSize));
  return S_OK;
}

HRESULT CDecoder::CodeSpec(ISequentialInStream * inStream,
  ISequentialOutStream * outStream, ICompressProgressInfo * progress)
{
  NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context = {};
  Context.InputStream = inStream;
  Context.OutputStream = outStream;
  Context.Progress = progress;
  Context.ProcessedInputSize = &_processedIn;
  Context.ProcessedOutputSize = &_processedOut;
  return ::NanaZipCodecsLz5Decode(&Context, _numThreads, _inputSize);
}

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream * inStream, ISequentialOutStream * outStream,
  const UInt64 * /*inSize */, const UInt64 *outSize, ICompressProgressInfo * progress))
{
  SetOutStreamSize(outSize);
  return CodeSpec(inStream, outStream, progress);
}

#ifndef Z7_NO_READ_FROM_CODER
Z7_COM7F_IMF(CDecoder::SetInStream(ISequentialInStream * inStream))
{
  _inStream = inStream;
  return S_OK;
}

Z7_COM7F_IMF(CDecoder::ReleaseInStream())
{
  _inStream.Release();
  return S_OK;
}
#endif

HRESULT CDecoder::CodeResume(ISequentialOutStream * outStream, const UInt64 * outSize, ICompressProgressInfo * progress)
{
  RINOK(SetOutStreamSizeResume(outSize));
  return CodeSpec(_inStream, outStream, progress);
}

}}
