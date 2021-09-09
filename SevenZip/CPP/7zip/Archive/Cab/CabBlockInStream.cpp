// CabBlockInStream.cpp

#include "StdAfx.h"

#include "../../../../C/Alloc.h"
#include "../../../../C/CpuArch.h"

#include "../../Common/StreamUtils.h"

#include "CabBlockInStream.h"

namespace NArchive {
namespace NCab {

static const UInt32 kBlockSize = (1 << 16);

bool CCabBlockInStream::Create()
{
  if (!_buf)
    _buf = (Byte *)::MyAlloc(kBlockSize);
  return _buf != 0;
}

CCabBlockInStream::~CCabBlockInStream()
{
  ::MyFree(_buf);
}

static UInt32 CheckSum(const Byte *p, UInt32 size)
{
  UInt32 sum = 0;
  
  for (; size >= 8; size -= 8)
  {
    sum ^= GetUi32(p) ^ GetUi32(p + 4);
    p += 8;
  }
  
  if (size >= 4)
  {
    sum ^= GetUi32(p);
    p += 4;
  }
  
  size &= 3;
  if (size > 2) sum ^= (UInt32)(*p++) << 16;
  if (size > 1) sum ^= (UInt32)(*p++) << 8;
  if (size > 0) sum ^= (UInt32)(*p++);
  
  return sum;
}

HRESULT CCabBlockInStream::PreRead(ISequentialInStream *stream, UInt32 &packSize, UInt32 &unpackSize)
{
  const UInt32 kHeaderSize = 8;
  const UInt32 kReservedMax = 256;
  Byte header[kHeaderSize + kReservedMax];
  RINOK(ReadStream_FALSE(stream, header, kHeaderSize + ReservedSize))
  packSize = GetUi16(header + 4);
  unpackSize = GetUi16(header + 6);
  if (packSize > kBlockSize - _size)
    return S_FALSE;
  RINOK(ReadStream_FALSE(stream, _buf + _size, packSize));
  
  if (MsZip)
  {
    if (_size == 0)
    {
      if (packSize < 2 || _buf[0] != 0x43 || _buf[1] != 0x4B)
        return S_FALSE;
      _pos = 2;
    }
    if (_size + packSize > ((UInt32)1 << 15) + 12) /* v9.31 fix. MSZIP specification */
      return S_FALSE;
  }

  if (GetUi32(header) != 0) // checkSum
    if (CheckSum(header, kHeaderSize + ReservedSize) != CheckSum(_buf + _size, packSize))
      return S_FALSE;

  _size += packSize;
  return S_OK;
}

STDMETHODIMP CCabBlockInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (size != 0)
  {
    UInt32 rem = _size - _pos;
    if (size > rem)
      size = rem;
    memcpy(data, _buf + _pos, size);
    _pos += size;
  }
  if (processedSize)
    *processedSize = size;
  return S_OK;
}

}}
