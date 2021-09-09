// ZlibEncoder.h

#ifndef __ZLIB_ENCODER_H
#define __ZLIB_ENCODER_H

#include "DeflateEncoder.h"

namespace NCompress {
namespace NZlib {

class CInStreamWithAdler:
  public ISequentialInStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialInStream> _stream;
  UInt32 _adler;
  UInt64 _size;
public:
  MY_UNKNOWN_IMP
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  void SetStream(ISequentialInStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init() { _adler = 1; _size = 0; } // ADLER_INIT_VAL
  UInt32 GetAdler() const { return _adler; }
  UInt64 GetSize() const { return _size; }
};

class CEncoder:
  public ICompressCoder,
  public CMyUnknownImp
{
  CInStreamWithAdler *AdlerSpec;
  CMyComPtr<ISequentialInStream> AdlerStream;
  CMyComPtr<ICompressCoder> DeflateEncoder;
public:
  NCompress::NDeflate::NEncoder::CCOMCoder *DeflateEncoderSpec;
  
  void Create();
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  UInt64 GetInputProcessedSize() const { return AdlerSpec->GetSize(); }

  MY_UNKNOWN_IMP
};

}}

#endif
