// CabBlockInStream.h

#ifndef __CAB_BLOCK_IN_STREAM_H
#define __CAB_BLOCK_IN_STREAM_H

#include "../../../Common/MyCom.h"
#include "../../IStream.h"

namespace NArchive {
namespace NCab {

class CCabBlockInStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
  Byte *_buf;
  UInt32 _size;
  UInt32 _pos;

public:
  UInt32 ReservedSize; // < 256
  bool MsZip;

  MY_UNKNOWN_IMP

  CCabBlockInStream(): _buf(0), ReservedSize(0), MsZip(false) {}
  ~CCabBlockInStream();
  
  bool Create();
  
  void InitForNewBlock() { _size = 0; _pos = 0; }
  
  HRESULT PreRead(ISequentialInStream *stream, UInt32 &packSize, UInt32 &unpackSize);

  UInt32 GetPackSizeAvail() const { return _size - _pos; }
  const Byte *GetData() const { return _buf + _pos; }

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
};

}}

#endif
