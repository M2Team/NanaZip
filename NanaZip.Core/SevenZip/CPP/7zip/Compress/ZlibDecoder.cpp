// ZlibDecoder.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../Common/StreamUtils.h"

#include "ZlibDecoder.h"

namespace NCompress {
namespace NZlib {

#define DEFLATE_TRY_BEGIN try {
#define DEFLATE_TRY_END } catch(...) { return S_FALSE; }

#define ADLER_MOD 65521
#define ADLER_LOOP_MAX 5550

UInt32 Adler32_Update(UInt32 adler, const Byte *data, size_t size);
UInt32 Adler32_Update(UInt32 adler, const Byte *data, size_t size)
{
  if (size == 0)
    return adler;
  UInt32 a = adler & 0xffff;
  UInt32 b = adler >> 16;
  do
  {
    size_t cur = size;
    if (cur > ADLER_LOOP_MAX)
        cur = ADLER_LOOP_MAX;
    size -= cur;
    const Byte *lim = data + cur;
    if (cur >= 4)
    {
      lim -= 4 - 1;
      do
      {
        a += data[0];  b += a;
        a += data[1];  b += a;
        a += data[2];  b += a;
        a += data[3];  b += a;
        data += 4;
      }
      while (data < lim);
      lim += 4 - 1;
    }
    if (data != lim) { a += *data++;  b += a;
    if (data != lim) { a += *data++;  b += a;
    if (data != lim) { a += *data++;  b += a; }}}
    a %= ADLER_MOD;
    b %= ADLER_MOD;
  }
  while (size);
  return (b << 16) + a;
}

Z7_COM7F_IMF(COutStreamWithAdler::Write(const void *data, UInt32 size, UInt32 *processedSize))
{
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Write(data, size, &size);
  _adler = Adler32_Update(_adler, (const Byte *)data, size);
  _size += size;
  if (processedSize)
    *processedSize = size;
  return result;
}

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  DEFLATE_TRY_BEGIN
  _inputProcessedSize_Additional = 0;
  AdlerStream.Create_if_Empty();
  DeflateDecoder.Create_if_Empty();
  DeflateDecoder->Set_NeedFinishInput(true);

  if (inSize && *inSize < 2)
    return S_FALSE;
  {
    Byte buf[2];
    RINOK(ReadStream_FALSE(inStream, buf, 2))
    if (!IsZlib(buf))
      return S_FALSE;
  }
  _inputProcessedSize_Additional = 2;
  AdlerStream->SetStream(outStream);
  AdlerStream->Init();
  // NDeflate::NDecoder::Code() ignores inSize
  /*
  UInt64 inSize2 = 0;
  if (inSize)
    inSize2 = *inSize - 2;
  */
  const HRESULT res = DeflateDecoder.Interface()->Code(inStream, AdlerStream,
      /* inSize ? &inSize2 : */ NULL, outSize, progress);
  AdlerStream->ReleaseStream();

  if (res == S_OK)
  {
    UInt32 footer32[1];
    UInt32 processedSize;
    RINOK(DeflateDecoder->ReadUnusedFromInBuf(footer32, 4, &processedSize))
    if (processedSize != 4)
    {
      size_t processedSize2 = 4 - processedSize;
      RINOK(ReadStream(inStream, (Byte *)(void *)footer32 + processedSize, &processedSize2))
      _inputProcessedSize_Additional += (Int32)processedSize2;
      processedSize += (UInt32)processedSize2;
    }
    
    if (processedSize == 4)
    {
      const UInt32 adler = GetBe32a(footer32);
      if (adler != AdlerStream->GetAdler())
        return S_FALSE; // adler error
    }
    else if (!IsAdlerOptional)
      return S_FALSE; // unexpeced end of stream (can't read adler)
    else
    {
      // IsAdlerOptional == true
      if (processedSize != 0)
      {
         // we exclude adler bytes from processed size:
        _inputProcessedSize_Additional -= (Int32)processedSize;
        return S_FALSE;
      }
    }
  }
  return res;
  DEFLATE_TRY_END
}

}}
