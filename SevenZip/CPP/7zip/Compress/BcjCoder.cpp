// BcjCoder.cpp

#include "StdAfx.h"

#include "BcjCoder.h"

namespace NCompress {
namespace NBcj {

STDMETHODIMP CCoder::Init()
{
  _bufferPos = 0;
  x86_Convert_Init(_prevMask);
  return S_OK;
}

STDMETHODIMP_(UInt32) CCoder::Filter(Byte *data, UInt32 size)
{
  UInt32 processed = (UInt32)::x86_Convert(data, size, _bufferPos, &_prevMask, _encode);
  _bufferPos += processed;
  return processed;
}

}}
