// InOutTempBuffer.cpp

#include "StdAfx.h"

#include "InOutTempBuffer.h"
#include "StreamUtils.h"

#ifdef USE_InOutTempBuffer_FILE

#include "../../../C/7zCrc.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const size_t kTempBufSize = (1 << 20);

#define kTempFilePrefixString FTEXT("7zt")
CInOutTempBuffer::~CInOutTempBuffer()
{
  delete []_buf;
}
#endif

CInOutTempBuffer::CInOutTempBuffer()
  #ifdef USE_InOutTempBuffer_FILE
  : _buf(NULL)
  #endif
{ }

void CInOutTempBuffer::Create()
{
  #ifdef USE_InOutTempBuffer_FILE
  if (!_buf)
    _buf = new Byte[kTempBufSize];
  #endif
}

void CInOutTempBuffer::InitWriting()
{
  #ifdef USE_InOutTempBuffer_FILE
  _bufPos = 0;
  _crc = CRC_INIT_VAL;
  _tempFileCreated = false;
  #endif
  _size = 0;
}


#ifdef USE_InOutTempBuffer_FILE

static inline HRESULT Get_HRESULT_LastError()
{
  #ifdef _WIN32
  DWORD lastError = ::GetLastError();
  if (lastError != 0)
    return HRESULT_FROM_WIN32(lastError);
  #endif
  return E_FAIL;
}

#endif


HRESULT CInOutTempBuffer::Write_HRESULT(const void *data, UInt32 size)
{
  #ifdef USE_InOutTempBuffer_FILE

  if (size == 0)
    return S_OK;
  size_t cur = kTempBufSize - _bufPos;
  if (cur != 0)
  {
    if (cur > size)
      cur = size;
    memcpy(_buf + _bufPos, data, cur);
    _crc = CrcUpdate(_crc, data, cur);
    _bufPos += cur;
    _size += cur;
    size -= (UInt32)cur;
    data = ((const Byte *)data) + cur;
  }

  if (size == 0)
    return S_OK;

  if (!_tempFileCreated)
  {
    if (!_tempFile.CreateRandomInTempFolder(kTempFilePrefixString, &_outFile))
      return Get_HRESULT_LastError();
    _tempFileCreated = true;
  }
  UInt32 processed;
  if (!_outFile.Write(data, size, processed))
    return Get_HRESULT_LastError();
  _crc = CrcUpdate(_crc, data, processed);
  _size += processed;
  return (processed == size) ? S_OK : E_FAIL;

  #else

  const size_t newSize = _size + size;
  if (newSize < _size)
    return E_OUTOFMEMORY;
  if (!_dynBuffer.EnsureCapacity(newSize))
    return E_OUTOFMEMORY;
  memcpy(((Byte *)_dynBuffer) + _size, data, size);
  _size = newSize;
  return S_OK;

  #endif
}


HRESULT CInOutTempBuffer::WriteToStream(ISequentialOutStream *stream)
{
  #ifdef USE_InOutTempBuffer_FILE

  if (!_outFile.Close())
    return E_FAIL;

  UInt64 size = 0;
  UInt32 crc = CRC_INIT_VAL;

  if (_bufPos != 0)
  {
    RINOK(WriteStream(stream, _buf, _bufPos));
    crc = CrcUpdate(crc, _buf, _bufPos);
    size += _bufPos;
  }
  
  if (_tempFileCreated)
  {
    NIO::CInFile inFile;
    if (!inFile.Open(_tempFile.GetPath()))
      return E_FAIL;
    while (size < _size)
    {
      UInt32 processed;
      if (!inFile.ReadPart(_buf, kTempBufSize, processed))
        return E_FAIL;
      if (processed == 0)
        break;
      RINOK(WriteStream(stream, _buf, processed));
      crc = CrcUpdate(crc, _buf, processed);
      size += processed;
    }
  }
  return (_crc == crc && size == _size) ? S_OK : E_FAIL;

  #else

  return WriteStream(stream, (const Byte *)_dynBuffer, _size);

  #endif
}

/*
STDMETHODIMP CSequentialOutTempBufferImp::Write(const void *data, UInt32 size, UInt32 *processed)
{
  if (!_buf->Write(data, size))
  {
    if (processed)
      *processed = 0;
    return E_FAIL;
  }
  if (processed)
    *processed = size;
  return S_OK;
}
*/
