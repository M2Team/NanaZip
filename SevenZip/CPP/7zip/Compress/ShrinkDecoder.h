// ShrinkDecoder.h

#ifndef __COMPRESS_SHRINK_DECODER_H
#define __COMPRESS_SHRINK_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NShrink {

const unsigned kNumMaxBits = 13;
const unsigned kNumItems = 1 << kNumMaxBits;

class CDecoder :
  public ICompressCoder,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public CMyUnknownImp
{
  bool _fullStreamMode;
  UInt64 _inProcessed;

  UInt16 _parents[kNumItems];
  Byte _suffixes[kNumItems];
  Byte _stack[kNumItems];

  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

public:
  MY_UNKNOWN_IMP2(
      ICompressSetFinishMode,
      ICompressGetInStreamProcessedSize)

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);
};

}}

#endif
