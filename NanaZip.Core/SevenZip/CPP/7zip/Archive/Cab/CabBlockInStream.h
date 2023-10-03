﻿// CabBlockInStream.h

#ifndef ZIP7_INC_CAB_BLOCK_IN_STREAM_H
#define ZIP7_INC_CAB_BLOCK_IN_STREAM_H

#include "../../../Common/MyCom.h"
#include "../../IStream.h"

namespace NArchive {
namespace NCab {

Z7_CLASS_IMP_NOQIB_1(
  CCabBlockInStream
  , ISequentialInStream
)
  Byte *_buf;
  UInt32 _size;
  UInt32 _pos;

public:
  UInt32 ReservedSize; // < 256
  bool MsZip;

  CCabBlockInStream(): _buf(NULL), ReservedSize(0), MsZip(false) {}
  ~CCabBlockInStream();
  
  bool Create();
  
  void InitForNewBlock() { _size = 0; _pos = 0; }
  
  HRESULT PreRead(ISequentialInStream *stream, UInt32 &packSize, UInt32 &unpackSize);

  UInt32 GetPackSizeAvail() const { return _size - _pos; }
  const Byte *GetData() const { return _buf + _pos; }
};

}}

#endif
