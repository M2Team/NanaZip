// OutStreamWithCRC.h

#ifndef __OUT_STREAM_WITH_CRC_H
#define __OUT_STREAM_WITH_CRC_H

#include "../../../../C/7zCrc.h"

#include "../../../Common/MyCom.h"

#include "../../IStream.h"

class COutStreamWithCRC:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
  UInt32 _crc;
  bool _calculate;
public:
  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init(bool calculate = true)
  {
    _size = 0;
    _calculate = calculate;
    _crc = CRC_INIT_VAL;
  }
  void EnableCalc(bool calculate) { _calculate = calculate; }
  void InitCRC() { _crc = CRC_INIT_VAL; }
  UInt64 GetSize() const { return _size; }
  UInt32 GetCRC() const { return CRC_GET_DIGEST(_crc); }
};

#endif
