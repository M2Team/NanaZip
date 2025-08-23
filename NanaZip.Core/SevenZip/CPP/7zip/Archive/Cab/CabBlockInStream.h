// CabBlockInStream.h

#ifndef ZIP7_INC_CAB_BLOCK_IN_STREAM_H
#define ZIP7_INC_CAB_BLOCK_IN_STREAM_H

#include "../../IStream.h"

namespace NArchive {
namespace NCab {

class CBlockPackData
{
  Byte *_buf;
  UInt32 _size;
public:
  CBlockPackData(): _buf(NULL), _size(0) {}
  ~CBlockPackData() throw();
  bool Create() throw();
  void InitForNewBlock() { _size = 0; }
  HRESULT Read(ISequentialInStream *stream, Byte ReservedSize, UInt32 &packSize, UInt32 &unpackSize) throw();
  UInt32 GetPackSize() const { return _size; }
  // 32 bytes of overread zone is available after PackSize:
  const Byte *GetData() const { return _buf; }
};

}}

#endif
