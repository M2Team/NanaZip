// InStreamWithCRC.h

#ifndef __IN_STREAM_WITH_CRC_H
#define __IN_STREAM_WITH_CRC_H

#include "../../../../C/7zCrc.h"

#include "../../../Common/MyCom.h"

#include "../../IStream.h"

class CSequentialInStreamWithCRC:
  public ISequentialInStream,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
private:
  CMyComPtr<ISequentialInStream> _stream;
  UInt64 _size;
  UInt32 _crc;
  bool _wasFinished;
public:
  void SetStream(ISequentialInStream *stream) { _stream = stream;  }
  void Init()
  {
    _size = 0;
    _wasFinished = false;
    _crc = CRC_INIT_VAL;
  }
  void ReleaseStream() { _stream.Release(); }
  UInt32 GetCRC() const { return CRC_GET_DIGEST(_crc); }
  UInt64 GetSize() const { return _size; }
  bool WasFinished() const { return _wasFinished; }
};

class CInStreamWithCRC:
  public IInStream,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
private:
  CMyComPtr<IInStream> _stream;
  UInt64 _size;
  UInt32 _crc;
  // bool _wasFinished;
public:
  void SetStream(IInStream *stream) { _stream = stream;  }
  void Init()
  {
    _size = 0;
    // _wasFinished = false;
    _crc = CRC_INIT_VAL;
  }
  void ReleaseStream() { _stream.Release(); }
  UInt32 GetCRC() const { return CRC_GET_DIGEST(_crc); }
  UInt64 GetSize() const { return _size; }
  // bool WasFinished() const { return _wasFinished; }
};

#endif
