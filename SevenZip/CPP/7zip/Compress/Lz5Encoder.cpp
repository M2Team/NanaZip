// (C) 2016 - 2020 Tino Reichardt

#include "StdAfx.h"
#include "Lz5Encoder.h"
#include "Lz5Decoder.h"

#ifndef EXTRACT_ONLY
namespace NCompress {
namespace NLZ5 {

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
    LZ5MT_freeCCtx(_ctx);
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

        _props._level = static_cast < Byte > (prop.ulVal);
        Byte mylevel = static_cast < Byte > (LZ5MT_LEVEL_MAX);
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
  LZ5MT_RdWr_t rdwr;
  size_t result;
  HRESULT res = S_OK;

  struct Lz5Stream Rd;
  Rd.inStream = inStream;
  Rd.outStream = outStream;
  Rd.processedIn = &_processedIn;
  Rd.processedOut = &_processedOut;

  struct Lz5Stream Wr;
  if (_processedIn == 0)
    Wr.progress = progress;
  else
    Wr.progress = 0;
  Wr.inStream = inStream;
  Wr.outStream = outStream;
  Wr.processedIn = &_processedIn;
  Wr.processedOut = &_processedOut;

  /* 1) setup read/write functions */
  rdwr.fn_read = ::Lz5Read;
  rdwr.fn_write = ::Lz5Write;
  rdwr.arg_read = (void *)&Rd;
  rdwr.arg_write = (void *)&Wr;

  /* 2) create compression context, if needed */
  if (!_ctx)
    _ctx = LZ5MT_createCCtx(_numThreads, _props._level, _inputSize);
  if (!_ctx)
    return S_FALSE;

  /* 3) compress */
  result = LZ5MT_compressCCtx(_ctx, &rdwr);
  if (LZ5MT_isError(result)) {
    if (result == (size_t)-LZ5MT_error_canceled)
      return E_ABORT;
    return E_FAIL;
  }

  return res;
}

STDMETHODIMP CEncoder::SetNumberOfThreads(UInt32 numThreads)
{
  const UInt32 kNumThreadsMax = LZ5MT_THREAD_MAX;
  if (numThreads < 1) numThreads = 1;
  if (numThreads > kNumThreadsMax) numThreads = kNumThreadsMax;
  _numThreads = numThreads;
  return S_OK;
}

}}
#endif
