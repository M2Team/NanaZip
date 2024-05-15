// CabBlockInStream.cpp

#include "StdAfx.h"

#include "../../../../C/Alloc.h"
#include "../../../../C/CpuArch.h"

#include "../../Common/StreamUtils.h"

#include "CabBlockInStream.h"

namespace NArchive {
namespace NCab {

static const UInt32 kBlockSize = 1 << 16;
static const unsigned k_OverReadPadZone_Size = 32;
static const unsigned kHeaderSize = 8;
static const unsigned kReservedMax = 256;
static const unsigned kHeaderOffset = kBlockSize + k_OverReadPadZone_Size;

bool CBlockPackData::Create() throw()
{
  if (!_buf)
    _buf = (Byte *)z7_AlignedAlloc(kBlockSize + k_OverReadPadZone_Size + kHeaderSize + kReservedMax);
  return _buf != NULL;
}

CBlockPackData::~CBlockPackData() throw()
{
  z7_AlignedFree(_buf);
}

static UInt32 CheckSum(const Byte *p, UInt32 size) throw()
{
#ifdef MY_CPU_64BIT
 
  UInt64 sum64 = 0;
  if (size >= 16)
  {
    const Byte *lim = p + (size_t)size - 16;
    do
    {
      sum64 ^= GetUi64(p) ^ GetUi64(p + 8);
      p += 16;
    }
    while (p <= lim);
    size = (UInt32)(lim + 16 - p);
  }
  if (size >= 8)
  {
    sum64 ^= GetUi64(p);
    p += 8;
    size -= 8;
  }

  UInt32 sum = (UInt32)(sum64 >> 32) ^ (UInt32)sum64;

#else

  UInt32 sum = 0;
  if (size >= 16)
  {
    const Byte *lim = p + (size_t)size - 16;
    do
    {
      sum ^= GetUi32(p)
           ^ GetUi32(p + 4)
           ^ GetUi32(p + 8)
           ^ GetUi32(p + 12);
      p += 16;
    }
    while (p <= lim);
    size = (UInt32)(lim + 16 - p);
  }
  if (size >= 8)
  {
    sum ^= GetUi32(p + 0) ^ GetUi32(p + 4);
    p += 8;
    size -= 8;
  }

#endif
  
  if (size >= 4)
  {
    sum ^= GetUi32(p);
    p += 4;
  }
  if (size &= 3)
  {
    if (size >= 2)
    {
      if (size > 2)
        sum ^= (UInt32)(*p++) << 16;
      sum ^= (UInt32)(*p++) << 8;
    }
    sum ^= (UInt32)(*p++);
  }
  return sum;
}


HRESULT CBlockPackData::Read(ISequentialInStream *stream, Byte ReservedSize, UInt32 &packSizeRes, UInt32 &unpackSize) throw()
{
  const UInt32 reserved8 = kHeaderSize + ReservedSize;
  const Byte *header = _buf + kHeaderOffset;
  RINOK(ReadStream_FALSE(stream, (void *)header, reserved8))
  unpackSize = GetUi16a(header + 6);
  const UInt32 packSize = GetUi16a(header + 4);
  packSizeRes = packSize;
  if (packSize > kBlockSize - _size)
    return S_FALSE;
  RINOK(ReadStream_FALSE(stream, _buf + _size, packSize))
  memset(_buf + _size + packSize, 0xff, k_OverReadPadZone_Size);
  if (*(const UInt32 *)(const void *)header != 0) // checkSum
    if (CheckSum(header, reserved8) != CheckSum(_buf + _size, packSize))
      return S_FALSE;
  _size += packSize;
  return S_OK;
}

}}
