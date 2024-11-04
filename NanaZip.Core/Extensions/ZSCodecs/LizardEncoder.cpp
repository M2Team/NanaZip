// (C) 2017 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"
#include "LizardEncoder.h"
#include "LizardDecoder.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NLIZARD {

CEncoder::CEncoder():
  _processedIn(0),
  _processedOut(0),
  _inputSize(0),
  _ctx(NULL),
  _numThreads(NWindows::NSystem::GetNumberOfProcessors())
{
  _props.clear();
}

CEncoder::~CEncoder()
{
  if (_ctx)
    LIZARDMT_freeCCtx(_ctx);
}

STDMETHODIMP CEncoder::SetCoderProperties(const PROPID * propIDs, const PROPVARIANT * coderProps, UInt32 numProps)
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

        /* level 1..22 */
        _props._level = static_cast < Byte > (prop.ulVal);
        Byte mylevel = static_cast < Byte > (LIZARDMT_LEVEL_MAX);
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

STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream * outStream)
{
  return WriteStream(outStream, &_props, sizeof (_props));
}

STDMETHODIMP CEncoder::Code(ISequentialInStream *inStream,
  ISequentialOutStream *outStream, const UInt64 * /*inSize*/ ,
  const UInt64 * /*outSize */, ICompressProgressInfo *progress)
{
  LIZARDMT_RdWr_t rdwr;
  size_t result;
  HRESULT res = S_OK;

  NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context = { 0 };
  Context.InputStream = inStream;
  Context.OutputStream = outStream;
  Context.Progress = progress;
  Context.ProcessedInputSize = &_processedIn;
  Context.ProcessedOutputSize = &_processedOut;

  /* 1) setup read/write functions */
  rdwr.fn_read = ::NanaZipCodecsLizardRead;
  rdwr.fn_write = ::NanaZipCodecsLizardWrite;
  rdwr.arg_read = reinterpret_cast<void*>(&Context);
  rdwr.arg_write = reinterpret_cast<void*>(&Context);

  /* 2) create compression context, if needed */
  if (!_ctx)
    _ctx = LIZARDMT_createCCtx(_numThreads, _props._level, _inputSize);
  if (!_ctx)
    return S_FALSE;

  /* 3) compress */
  result = LIZARDMT_compressCCtx(_ctx, &rdwr);
  if (LIZARDMT_isError(result)) {
    if (result == (size_t)-LIZARDMT_error_canceled)
      return E_ABORT;
    return E_FAIL;
  }

  return res;
}

STDMETHODIMP CEncoder::SetNumberOfThreads(UInt32 numThreads)
{
  const UInt32 kNumThreadsMax = LIZARDMT_THREAD_MAX;
  if (numThreads < 1) numThreads = 1;
  if (numThreads > kNumThreadsMax) numThreads = kNumThreadsMax;
  _numThreads = numThreads;
  return S_OK;
}

}}
#endif
