// ShrinkDecoder.h

#ifndef ZIP7_INC_COMPRESS_SHRINK_DECODER_H
#define ZIP7_INC_COMPRESS_SHRINK_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NShrink {

#define Z7_SHRINK_NUM_MAX_BITS  13
#define Z7_SHRINK_NUM_ITEMS     (1 << Z7_SHRINK_NUM_MAX_BITS)
const size_t kNumItems = Z7_SHRINK_NUM_ITEMS;

Z7_CLASS_IMP_NOQIB_3(
  CDecoder
  , ICompressCoder
  , ICompressSetFinishMode
  , ICompressGetInStreamProcessedSize
)
  UInt32 _fullStreamMode;
  UInt64 _inProcessed;

  UInt16 _parents[kNumItems];
  Byte _suffixes[kNumItems];
  Byte _stack[kNumItems];

  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
public:
  CDecoder(): _fullStreamMode(0) {}
};

}}

#endif
