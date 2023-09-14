// ZlibEncoder.cpp

#include "StdAfx.h"

#include "../Common/StreamUtils.h"

#include "ZlibEncoder.h"

namespace NCompress {
namespace NZlib {

#define DEFLATE_TRY_BEGIN try {
#define DEFLATE_TRY_END } catch(...) { return S_FALSE; }

UInt32 Adler32_Update(UInt32 adler, const Byte *buf, size_t size);

Z7_COM7F_IMF(CInStreamWithAdler::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  const HRESULT result = _stream->Read(data, size, &size);
  _adler = Adler32_Update(_adler, (const Byte *)data, size);
  _size += size;
  if (processedSize)
    *processedSize = size;
  return result;
}

void CEncoder::Create()
{
  if (!DeflateEncoder)
    DeflateEncoder = DeflateEncoderSpec = new NDeflate::NEncoder::CCOMCoder;
}

Z7_COM7F_IMF(CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 * /* outSize */, ICompressProgressInfo *progress))
{
  DEFLATE_TRY_BEGIN
  if (!AdlerStream)
    AdlerStream = AdlerSpec = new CInStreamWithAdler;
  Create();

  {
    Byte buf[2] = { 0x78, 0xDA };
    RINOK(WriteStream(outStream, buf, 2))
  }

  AdlerSpec->SetStream(inStream);
  AdlerSpec->Init();
  const HRESULT res = DeflateEncoder->Code(AdlerStream, outStream, inSize, NULL, progress);
  AdlerSpec->ReleaseStream();

  RINOK(res)

  {
    const UInt32 a = AdlerSpec->GetAdler();
    const Byte buf[4] = { (Byte)(a >> 24), (Byte)(a >> 16), (Byte)(a >> 8), (Byte)(a) };
    return WriteStream(outStream, buf, 4);
  }
  DEFLATE_TRY_END
}

}}
