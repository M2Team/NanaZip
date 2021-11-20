// (C) 2017 Tino Reichardt

#include "StdAfx.h"
#include "BrotliEncoder.h"
#include "BrotliDecoder.h"

#ifndef EXTRACT_ONLY
namespace NCompress {
namespace NBROTLI {

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
    BROTLIMT_freeCCtx(_ctx);
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
        Byte mylevel = static_cast < Byte > (BROTLIMT_LEVEL_MAX);
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
  BROTLIMT_RdWr_t rdwr;
  size_t result;
  HRESULT res = S_OK;
  
  _processedIn = 0;
  _processedOut = 0;

  struct BrotliStream Rd;
  Rd.inStream = inStream;
  Rd.outStream = outStream;
  Rd.processedIn = &_processedIn;
  Rd.processedOut = &_processedOut;

  struct BrotliStream Wr;
//  if (_processedIn == 0)
    Wr.progress = progress;
//  else
//    Wr.progress = 0;
  Wr.inStream = inStream;
  Wr.outStream = outStream;
  Wr.processedIn = &_processedIn;
  Wr.processedOut = &_processedOut;

  /* 1) setup read/write functions */
  rdwr.fn_read = ::BrotliRead;
  rdwr.fn_write = ::BrotliWrite;
  rdwr.arg_read = (void *)&Rd;
  rdwr.arg_write = (void *)&Wr;

  /* 2) create compression context, if needed */
  if (!_ctx)
    _ctx = BROTLIMT_createCCtx(_numThreads, _props._level, _inputSize);
  if (!_ctx)
    return S_FALSE;

  /* 3) compress */
  result = BROTLIMT_compressCCtx(_ctx, &rdwr);
  if (BROTLIMT_isError(result)) {
    if (result == (size_t)-BROTLIMT_error_canceled)
      return E_ABORT;
    return E_FAIL;
  }

  return res;
}

STDMETHODIMP CEncoder::SetNumberOfThreads(UInt32 numThreads)
{
  const UInt32 kNumThreadsMax = BROTLIMT_THREAD_MAX;
  if (numThreads < 1) numThreads = 1;
  if (numThreads > kNumThreadsMax) numThreads = kNumThreadsMax;
  _numThreads = numThreads;
  return S_OK;
}

}}
#endif
