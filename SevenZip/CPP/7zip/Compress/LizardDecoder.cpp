// (C) 2017 Tino Reichardt

#include "StdAfx.h"
#include "LizardDecoder.h"

int LizardRead(void *arg, LIZARDMT_Buffer * in)
{
  struct LizardStream *x = (struct LizardStream*)arg;
  size_t size = in->size;

  HRESULT res = ReadStream(x->inStream, in->buf, &size);

  /* catch errors */
  switch (res) {
  case E_ABORT:
    return -2;
  case E_OUTOFMEMORY:
    return -3;
  }

  /* some other error -> read_fail */
  if (res != S_OK)
    return -1;

  in->size = size;
  *x->processedIn += size;

  return 0;
}

int LizardWrite(void *arg, LIZARDMT_Buffer * out)
{
  struct LizardStream *x = (struct LizardStream*)arg;
  UInt32 todo = (UInt32)out->size;
  UInt32 done = 0;

  while (todo != 0)
  {
    UInt32 block;
    HRESULT res = x->outStream->Write((char*)out->buf + done, todo, &block);

    /* catch errors */
    switch (res) {
    case E_ABORT:
      return -2;
    case E_OUTOFMEMORY:
      return -3;
    }

    done += block;
    if (res == k_My_HRESULT_WritingWasCut)
      break;
    /* some other error -> write_fail */
    if (res != S_OK)
      return -1;

    if (block == 0)
      return -1;
    todo -= block;
  }

  *x->processedOut += done;
  /* we need no lock here, cause only one thread can write... */
  if (x->progress)
    x->progress->SetRatioInfo(x->processedIn, x->processedOut);

  return 0;
}

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
  LIZARDMT_RdWr_t rdwr;
  size_t result;
  HRESULT res = S_OK;

  struct LizardStream Rd;
  Rd.inStream = inStream;
  Rd.processedIn = &_processedIn;

  struct LizardStream Wr;
  Wr.progress = progress;
  Wr.outStream = outStream;
  Wr.processedIn = &_processedIn;
  Wr.processedOut = &_processedOut;

  /* 1) setup read/write functions */
  rdwr.fn_read = ::LizardRead;
  rdwr.fn_write = ::LizardWrite;
  rdwr.arg_read = (void *)&Rd;
  rdwr.arg_write = (void *)&Wr;

  /* 2) create decompression context */
  LIZARDMT_DCtx *ctx = LIZARDMT_createDCtx(_numThreads, _inputSize);
  if (!ctx)
      return S_FALSE;

  /* 3) decompress */
  result = LIZARDMT_decompressDCtx(ctx, &rdwr);
  if (LIZARDMT_isError(result)) {
    if (result == (size_t)-LIZARDMT_error_canceled)
      return E_ABORT;
    return E_FAIL;
  }

  /* 4) free resources */
  LIZARDMT_freeDCtx(ctx);
  return res;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream * inStream, ISequentialOutStream * outStream,
  const UInt64 * /*inSize */, const UInt64 *outSize, ICompressProgressInfo * progress)
{
  SetOutStreamSize(outSize);
  return CodeSpec(inStream, outStream, progress);
}

#ifndef NO_READ_FROM_CODER
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
