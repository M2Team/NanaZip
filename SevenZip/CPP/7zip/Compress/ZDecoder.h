// ZDecoder.h

#ifndef __COMPRESS_Z_DECODER_H
#define __COMPRESS_Z_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NZ {

// Z decoder decodes Z data stream, including 3 bytes of header.
  
class CDecoder:
  public ICompressCoder,
  public CMyUnknownImp
{
  UInt16 *_parents;
  Byte *_suffixes;
  Byte *_stack;
  unsigned _numMaxBits;

public:
  CDecoder(): _parents(0), _suffixes(0), _stack(0), /* _prop(0), */ _numMaxBits(0) {};
  ~CDecoder();
  void Free();
  UInt64 PackSize;

  MY_UNKNOWN_IMP1(ICompressCoder)

  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
};

/*
  There is no end_of_payload_marker in Z stream.
  Z decoder stops decoding, if it reaches end of input stream.
   
  CheckStream function:
    (size) must be at least 3 bytes (size of Z header).
    if (size) is larger than size of real Z stream in (data), CheckStream can return false.
*/

const unsigned kRecommendedCheckSize = 64;

bool CheckStream(const Byte *data, size_t size);

}}

#endif
