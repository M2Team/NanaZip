// BranchMisc.h

#ifndef ZIP7_INC_COMPRESS_BRANCH_MISC_H
#define ZIP7_INC_COMPRESS_BRANCH_MISC_H
#include "../../../C/Bra.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NBranch {

Z7_CLASS_IMP_COM_1(
  CCoder
  , ICompressFilter
)
  UInt32 _pc;
  z7_Func_BranchConv BraFunc;
public:
  CCoder(z7_Func_BranchConv bra): _pc(0), BraFunc(bra) {}
};

namespace NArm64 {

#ifndef Z7_EXTRACT_ONLY

Z7_CLASS_IMP_COM_3(
  CEncoder
  , ICompressFilter
  , ICompressSetCoderProperties
  , ICompressWriteCoderProperties
)
  UInt32 _pc;
  UInt32 _pc_Init;
public:
  CEncoder(): _pc(0), _pc_Init(0) {}
};

#endif

Z7_CLASS_IMP_COM_2(
  CDecoder
  , ICompressFilter
  , ICompressSetDecoderProperties2
)
  UInt32 _pc;
  UInt32 _pc_Init;
public:
  CDecoder(): _pc(0), _pc_Init(0) {}
};

}

}}

#endif
