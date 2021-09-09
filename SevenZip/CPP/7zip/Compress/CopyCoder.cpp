// Compress/CopyCoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "CopyCoder.h"

namespace NCompress {

static const UInt32 kBufSize = 1 << 17;

CCopyCoder::~CCopyCoder()
{
  ::MidFree(_buf);
}

STDMETHODIMP CCopyCoder::SetFinishMode(UInt32 /* finishMode */)
{
  return S_OK;
}

STDMETHODIMP CCopyCoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize,
    ICompressProgressInfo *progress)
{
  if (!_buf)
  {
    _buf = (Byte *)::MidAlloc(kBufSize);
    if (!_buf)
      return E_OUTOFMEMORY;
  }

  TotalSize = 0;
  
  for (;;)
  {
    UInt32 size = kBufSize;
    if (outSize && size > *outSize - TotalSize)
      size = (UInt32)(*outSize - TotalSize);
    if (size == 0)
      return S_OK;
    
    HRESULT readRes = inStream->Read(_buf, size, &size);

    if (size == 0)
      return readRes;

    if (outStream)
    {
      UInt32 pos = 0;
      do
      {
        UInt32 curSize = size - pos;
        HRESULT res = outStream->Write(_buf + pos, curSize, &curSize);
        pos += curSize;
        TotalSize += curSize;
        RINOK(res);
        if (curSize == 0)
          return E_FAIL;
      }
      while (pos < size);
    }
    else
      TotalSize += size;

    RINOK(readRes);

    if (progress)
    {
      RINOK(progress->SetRatioInfo(&TotalSize, &TotalSize));
    }
  }
}

STDMETHODIMP CCopyCoder::SetInStream(ISequentialInStream *inStream)
{
  _inStream = inStream;
  TotalSize = 0;
  return S_OK;
}

STDMETHODIMP CCopyCoder::ReleaseInStream()
{
  _inStream.Release();
  return S_OK;
}

STDMETHODIMP CCopyCoder::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  UInt32 realProcessedSize = 0;
  HRESULT res = _inStream->Read(data, size, &realProcessedSize);
  TotalSize += realProcessedSize;
  if (processedSize)
    *processedSize = realProcessedSize;
  return res;
}

STDMETHODIMP CCopyCoder::GetInStreamProcessedSize(UInt64 *value)
{
  *value = TotalSize;
  return S_OK;
}

HRESULT CopyStream(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress)
{
  CMyComPtr<ICompressCoder> copyCoder = new CCopyCoder;
  return copyCoder->Code(inStream, outStream, NULL, NULL, progress);
}

HRESULT CopyStream_ExactSize(ISequentialInStream *inStream, ISequentialOutStream *outStream, UInt64 size, ICompressProgressInfo *progress)
{
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder;
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;
  RINOK(copyCoder->Code(inStream, outStream, NULL, &size, progress));
  return copyCoderSpec->TotalSize == size ? S_OK : E_FAIL;
}

}
