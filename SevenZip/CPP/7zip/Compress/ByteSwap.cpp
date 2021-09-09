// ByteSwap.cpp

#include "StdAfx.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/RegisterCodec.h"

namespace NCompress {
namespace NByteSwap {

class CByteSwap2:
  public ICompressFilter,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(ICompressFilter);
  INTERFACE_ICompressFilter(;)
};

class CByteSwap4:
  public ICompressFilter,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(ICompressFilter);
  INTERFACE_ICompressFilter(;)
};

STDMETHODIMP CByteSwap2::Init() { return S_OK; }

STDMETHODIMP_(UInt32) CByteSwap2::Filter(Byte *data, UInt32 size)
{
  const UInt32 kStep = 2;
  if (size < kStep)
    return 0;
  size &= ~(kStep - 1);
  
  const Byte *end = data + (size_t)size;
  
  do
  {
    Byte b0 = data[0];
    data[0] = data[1];
    data[1] = b0;
    data += kStep;
  }
  while (data != end);

  return size;
}

STDMETHODIMP CByteSwap4::Init() { return S_OK; }

STDMETHODIMP_(UInt32) CByteSwap4::Filter(Byte *data, UInt32 size)
{
  const UInt32 kStep = 4;
  if (size < kStep)
    return 0;
  size &= ~(kStep - 1);
  
  const Byte *end = data + (size_t)size;
  
  do
  {
    Byte b0 = data[0];
    Byte b1 = data[1];
    data[0] = data[3];
    data[1] = data[2];
    data[2] = b1;
    data[3] = b0;
    data += kStep;
  }
  while (data != end);

  return size;
}

REGISTER_FILTER_CREATE(CreateFilter2, CByteSwap2())
REGISTER_FILTER_CREATE(CreateFilter4, CByteSwap4())

REGISTER_CODECS_VAR
{
  REGISTER_FILTER_ITEM(CreateFilter2, CreateFilter2, 0x20302, "Swap2"),
  REGISTER_FILTER_ITEM(CreateFilter4, CreateFilter4, 0x20304, "Swap4")
};

REGISTER_CODECS(ByteSwap)

}}
