// 7zSpecStream.cpp

#include "StdAfx.h"

#include "7zSpecStream.h"

STDMETHODIMP CSequentialInStreamSizeCount2::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  UInt32 realProcessedSize;
  HRESULT result = _stream->Read(data, size, &realProcessedSize);
  _size += realProcessedSize;
  if (processedSize)
    *processedSize = realProcessedSize;
  return result;
}

STDMETHODIMP CSequentialInStreamSizeCount2::GetSubStreamSize(UInt64 subStream, UInt64 *value)
{
  if (!_getSubStreamSize)
    return E_NOTIMPL;
  return _getSubStreamSize->GetSubStreamSize(subStream, value);
}
