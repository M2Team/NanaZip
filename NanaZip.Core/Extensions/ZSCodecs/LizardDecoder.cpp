// (C) 2017 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"
#include "LizardDecoder.h"

namespace NCompress {
namespace NLIZARD {

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

  if (size != sizeof(DProps))
    return E_NOTIMPL;

  memcpy(&_props, pProps, sizeof (DProps));

  return S_OK;
}

STDMETHODIMP CDecoder::SetNumberOfThreads(UInt32 numThreads)
{
  const UInt32 kNumThreadsMax = LIZARDMT_THREAD_MAX;
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
  NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context = { 0 };
  Context.InputStream = inStream;
  Context.OutputStream = outStream;
  Context.Progress = progress;
  Context.ProcessedInputSize = &_processedIn;
  Context.ProcessedOutputSize = &_processedOut;
  return ::NanaZipCodecsLizardDecode(&Context, _numThreads, _inputSize);
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
