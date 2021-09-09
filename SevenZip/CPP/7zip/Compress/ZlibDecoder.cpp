// ZlibDecoder.cpp

#include "StdAfx.h"

#include "../Common/StreamUtils.h"

#include "ZlibDecoder.h"

namespace NCompress {
namespace NZlib {

#define DEFLATE_TRY_BEGIN try {
#define DEFLATE_TRY_END } catch(...) { return S_FALSE; }

#define ADLER_MOD 65521
#define ADLER_LOOP_MAX 5550

UInt32 Adler32_Update(UInt32 adler, const Byte *buf, size_t size);
UInt32 Adler32_Update(UInt32 adler, const Byte *buf, size_t size)
{
  UInt32 a = adler & 0xFFFF;
  UInt32 b = (adler >> 16) & 0xFFFF;
  while (size > 0)
  {
    unsigned curSize = (size > ADLER_LOOP_MAX) ? ADLER_LOOP_MAX : (unsigned )size;
    unsigned i;
    for (i = 0; i < curSize; i++)
    {
      a += buf[i];
      b += a;
    }
    buf += curSize;
    size -= curSize;
    a %= ADLER_MOD;
    b %= ADLER_MOD;
  }
  return (b << 16) + a;
}

STDMETHODIMP COutStreamWithAdler::Write(const void *data, UInt32 size, UInt32 *processedSize)
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

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  DEFLATE_TRY_BEGIN
  if (!AdlerStream)
    AdlerStream = AdlerSpec = new COutStreamWithAdler;
  if (!DeflateDecoder)
  {
    DeflateDecoderSpec = new NDeflate::NDecoder::CCOMCoder;
    DeflateDecoderSpec->ZlibMode = true;
    DeflateDecoder = DeflateDecoderSpec;
  }

  if (inSize && *inSize < 2)
    return S_FALSE;
  Byte buf[2];
  RINOK(ReadStream_FALSE(inStream, buf, 2));
  if (!IsZlib(buf))
    return S_FALSE;

  AdlerSpec->SetStream(outStream);
  AdlerSpec->Init();
  
  UInt64 inSize2 = 0;
  if (inSize)
    inSize2 = *inSize - 2;

  HRESULT res = DeflateDecoder->Code(inStream, AdlerStream, inSize ? &inSize2 : NULL, outSize, progress);
  AdlerSpec->ReleaseStream();

  if (res == S_OK)
  {
    const Byte *p = DeflateDecoderSpec->ZlibFooter;
    UInt32 adler = ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) | ((UInt32)p[2] << 8) | p[3];
    if (adler != AdlerSpec->GetAdler())
      return S_FALSE;
  }
  return res;
  DEFLATE_TRY_END
}

}}
