// (C) 2017 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"
#include "BrotliEncoder.h"
#include "BrotliDecoder.h"

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NBROTLI {

CEncoder::CEncoder():
  _processedIn(0),
  _processedOut(0),
  _inputSize(0),
  _numThreads(NWindows::NSystem::GetNumberOfProcessors()),
  _Long(-1),
  _WindowLog(-1),
  _ctx(NULL),
  unpackSize(0)
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
    case NCoderPropID::kLong:
      {
        /* like --long in zstd cli program */
        _Long = 1;
        if (v == 0) {
          if (prop.vt == VT_EMPTY) {
            // m0=brotli:long -> long=default
            _WindowLog = BROTLI_MAX_WINDOW_BITS; //BROTLI_DEFAULT_WINDOW;
          } else {
            // m0=brotli:long=0 -> long=auto
            _WindowLog = 0;
          }
        } else if (v < BROTLI_MIN_WINDOW_BITS) {
          // m0=brotli:long=9 -> long=10
          _WindowLog = BROTLI_MIN_WINDOW_BITS;
        } else if (v > BROTLI_LARGE_MAX_WINDOW_BITS) {
          // m0=brotli:long=33 -> long=max
          _WindowLog = BROTLI_LARGE_MAX_WINDOW_BITS;
        } else {
          // m0=brotli:long=15 -> long=value
          _WindowLog = v;
        }
        break;
      }
    case NCoderPropID::kWindowLog:
      {
        if (v < BROTLI_MIN_WINDOW_BITS) v = BROTLI_MIN_WINDOW_BITS;
        if (v > BROTLI_MAX_WINDOW_BITS) v = BROTLI_MAX_WINDOW_BITS;
        _WindowLog = v;
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

  NANAZIP_CODECS_ZSTDMT_STREAM_CONTEXT Context = { 0 };
  Context.InputStream = inStream;
  Context.OutputStream = outStream;
  Context.Progress = progress;
  Context.ProcessedInputSize = &_processedIn;
  Context.ProcessedOutputSize = &_processedOut;

  /* 1) setup read/write functions */
  rdwr.fn_read = ::NanaZipCodecsBrotliRead;
  rdwr.fn_write = ::NanaZipCodecsBrotliWrite;
  rdwr.arg_read = reinterpret_cast<void*>(&Context);
  rdwr.arg_write = reinterpret_cast<void*>(&Context);

  /* 2) create compression context, if needed */
  if (!_ctx)
    _ctx = BROTLIMT_createCCtx(_numThreads, unpackSize, _props._level, _inputSize,
      _WindowLog >= 0 ? _WindowLog : BROTLI_MAX_WINDOW_BITS);
  if (!_ctx)
    return S_FALSE;

  /* 4) compress */
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
  if (numThreads < 0) numThreads = 0;
  if (numThreads > kNumThreadsMax) numThreads = kNumThreadsMax;
  // if single-threaded, retain artificial number set in BrotliHandler (always prefer .br format):
  if (_numThreads == 0 && numThreads == 1) {
    numThreads = 0;
  }
  _numThreads = numThreads;
  return S_OK;
}

}}
#endif
