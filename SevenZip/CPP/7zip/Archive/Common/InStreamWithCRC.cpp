// InStreamWithCRC.cpp

#include "StdAfx.h"

#include "InStreamWithCRC.h"

STDMETHODIMP CSequentialInStreamWithCRC::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  UInt32 realProcessed = 0;
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Read(data, size, &realProcessed);
  _size += realProcessed;
  if (size != 0 && realProcessed == 0)
    _wasFinished = true;
  _crc = CrcUpdate(_crc, data, realProcessed);
  if (processedSize)
    *processedSize = realProcessed;
  return result;
}

STDMETHODIMP CInStreamWithCRC::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  UInt32 realProcessed = 0;
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Read(data, size, &realProcessed);
  _size += realProcessed;
  /*
  if (size != 0 && realProcessed == 0)
    _wasFinished = true;
  */
  _crc = CrcUpdate(_crc, data, realProcessed);
  if (processedSize)
    *processedSize = realProcessed;
  return result;
}

STDMETHODIMP CInStreamWithCRC::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  if (seekOrigin != STREAM_SEEK_SET || offset != 0)
    return E_FAIL;
  _size = 0;
  _crc = CRC_INIT_VAL;
  return _stream->Seek(offset, seekOrigin, newPosition);
}
