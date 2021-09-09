// StreamUtils.cpp

#include "StdAfx.h"

#include "StreamUtils.h"

static const UInt32 kBlockSize = ((UInt32)1 << 31);

HRESULT ReadStream(ISequentialInStream *stream, void *data, size_t *processedSize) throw()
{
  size_t size = *processedSize;
  *processedSize = 0;
  while (size != 0)
  {
    UInt32 curSize = (size < kBlockSize) ? (UInt32)size : kBlockSize;
    UInt32 processedSizeLoc;
    HRESULT res = stream->Read(data, curSize, &processedSizeLoc);
    *processedSize += processedSizeLoc;
    data = (void *)((Byte *)data + processedSizeLoc);
    size -= processedSizeLoc;
    RINOK(res);
    if (processedSizeLoc == 0)
      return S_OK;
  }
  return S_OK;
}

HRESULT ReadStream_FALSE(ISequentialInStream *stream, void *data, size_t size) throw()
{
  size_t processedSize = size;
  RINOK(ReadStream(stream, data, &processedSize));
  return (size == processedSize) ? S_OK : S_FALSE;
}

HRESULT ReadStream_FAIL(ISequentialInStream *stream, void *data, size_t size) throw()
{
  size_t processedSize = size;
  RINOK(ReadStream(stream, data, &processedSize));
  return (size == processedSize) ? S_OK : E_FAIL;
}

HRESULT WriteStream(ISequentialOutStream *stream, const void *data, size_t size) throw()
{
  while (size != 0)
  {
    UInt32 curSize = (size < kBlockSize) ? (UInt32)size : kBlockSize;
    UInt32 processedSizeLoc;
    HRESULT res = stream->Write(data, curSize, &processedSizeLoc);
    data = (const void *)((const Byte *)data + processedSizeLoc);
    size -= processedSizeLoc;
    RINOK(res);
    if (processedSizeLoc == 0)
      return E_FAIL;
  }
  return S_OK;
}
