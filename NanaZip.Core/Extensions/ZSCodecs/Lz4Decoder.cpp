// (C) 2016 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"
#include "Lz4Decoder.h"

namespace NCompress {
namespace NLZ4 {

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

STDMETHODIMP CDecoder::SetDecoderProperties2(const Byte * prop, UInt32 size)
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

STDMETHODIMP CDecoder::SetNumberOfThreads(UInt32 numThreads)
{
  const UInt32 kNumThreadsMax = LZ4MT_THREAD_MAX;
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

STDMETHODIMP CDecoder::SetOutStreamSize(const UInt64 * outSize)
{
  _processedIn = 0;
  RINOK(SetOutStreamSizeResume(outSize));
  return S_OK;
}

HRESULT CDecoder::CodeSpec(ISequentialInStream * inStream,
  ISequentialOutStream * outStream, ICompressProgressInfo * progress)
{
  LZ4MT_RdWr_t rdwr;
  size_t result;
  HRESULT res = S_OK;

  NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT ReadContext = { 0 };
  ReadContext.InputStream = inStream;
  ReadContext.ProcessedInputSize = &_processedIn;

  NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT WriteContext = { 0 };
  WriteContext.Progress = progress;
  WriteContext.OutputStream = outStream;
  WriteContext.ProcessedInputSize = &_processedIn;
  WriteContext.ProcessedOutputSize = &_processedOut;

  /* 1) setup read/write functions */
  rdwr.fn_read = ::NanaZipCodecsLz4Read;
  rdwr.fn_write = ::NanaZipCodecsLz4Write;
  rdwr.arg_read = reinterpret_cast<void*>(&ReadContext);
  rdwr.arg_write = reinterpret_cast<void*>(&WriteContext);

  /* 2) create decompression context */
  LZ4MT_DCtx *ctx = LZ4MT_createDCtx(_numThreads, _inputSize);
  if (!ctx)
      return S_FALSE;

  /* 3) decompress */
  result = LZ4MT_decompressDCtx(ctx, &rdwr);
  if (LZ4MT_isError(result)) {
    if (result == (size_t)-LZ4MT_error_canceled)
      return E_ABORT;
    return E_FAIL;
  }

  /* 4) free resources */
  LZ4MT_freeDCtx(ctx);
  return res;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream * inStream, ISequentialOutStream * outStream,
  const UInt64 * /*inSize */, const UInt64 *outSize, ICompressProgressInfo * progress)
{
  SetOutStreamSize(outSize);
  return CodeSpec(inStream, outStream, progress);
}

#ifndef Z7_NO_READ_FROM_CODER
STDMETHODIMP CDecoder::SetInStream(ISequentialInStream * inStream)
{
  _inStream = inStream;
  return S_OK;
}

STDMETHODIMP CDecoder::ReleaseInStream()
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
