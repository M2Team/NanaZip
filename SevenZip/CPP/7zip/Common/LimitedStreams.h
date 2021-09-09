// LimitedStreams.h

#ifndef __LIMITED_STREAMS_H
#define __LIMITED_STREAMS_H

#include "../../Common/MyBuffer.h"
#include "../../Common/MyCom.h"
#include "../../Common/MyVector.h"
#include "../IStream.h"

class CLimitedSequentialInStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialInStream> _stream;
  UInt64 _size;
  UInt64 _pos;
  bool _wasFinished;
public:
  void SetStream(ISequentialInStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init(UInt64 streamSize)
  {
    _size = streamSize;
    _pos = 0;
    _wasFinished = false;
  }
 
  MY_UNKNOWN_IMP1(ISequentialInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  UInt64 GetSize() const { return _pos; }
  UInt64 GetRem() const { return _size - _pos; }
  bool WasFinished() const { return _wasFinished; }
};

class CLimitedInStream:
  public IInStream,
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _stream;
  UInt64 _virtPos;
  UInt64 _physPos;
  UInt64 _size;
  UInt64 _startOffset;

  HRESULT SeekToPhys() { return _stream->Seek((Int64)_physPos, STREAM_SEEK_SET, NULL); }
public:
  void SetStream(IInStream *stream) { _stream = stream; }
  HRESULT InitAndSeek(UInt64 startOffset, UInt64 size)
  {
    _startOffset = startOffset;
    _physPos = startOffset;
    _virtPos = 0;
    _size = size;
    return SeekToPhys();
  }
 
  MY_UNKNOWN_IMP2(ISequentialInStream, IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);

  HRESULT SeekToStart() { return Seek(0, STREAM_SEEK_SET, NULL); }
};

HRESULT CreateLimitedInStream(IInStream *inStream, UInt64 pos, UInt64 size, ISequentialInStream **resStream);

class CClusterInStream:
  public IInStream,
  public CMyUnknownImp
{
  UInt64 _virtPos;
  UInt64 _physPos;
  UInt32 _curRem;
public:
  unsigned BlockSizeLog;
  UInt64 Size;
  CMyComPtr<IInStream> Stream;
  CRecordVector<UInt32> Vector;
  UInt64 StartOffset;

  HRESULT SeekToPhys() { return Stream->Seek((Int64)_physPos, STREAM_SEEK_SET, NULL); }

  HRESULT InitAndSeek()
  {
    _curRem = 0;
    _virtPos = 0;
    _physPos = StartOffset;
    if (Vector.Size() > 0)
    {
      _physPos = StartOffset + (Vector[0] << BlockSizeLog);
      return SeekToPhys();
    }
    return S_OK;
  }

  MY_UNKNOWN_IMP2(ISequentialInStream, IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};

struct CSeekExtent
{
  UInt64 Phy;
  UInt64 Virt;
};

class CExtentsStream:
  public IInStream,
  public CMyUnknownImp
{
  UInt64 _phyPos;
  UInt64 _virtPos;
  bool _needStartSeek;

  HRESULT SeekToPhys() { return Stream->Seek((Int64)_phyPos, STREAM_SEEK_SET, NULL); }

public:
  CMyComPtr<IInStream> Stream;
  CRecordVector<CSeekExtent> Extents;

  MY_UNKNOWN_IMP2(ISequentialInStream, IInStream)
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
  void ReleaseStream() { Stream.Release(); }

  void Init()
  {
    _virtPos = 0;
    _phyPos = 0;
    _needStartSeek = true;
  }
};

class CLimitedSequentialOutStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
  bool _overflow;
  bool _overflowIsAllowed;
public:
  MY_UNKNOWN_IMP1(ISequentialOutStream)
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init(UInt64 size, bool overflowIsAllowed = false)
  {
    _size = size;
    _overflow = false;
    _overflowIsAllowed = overflowIsAllowed;
  }
  bool IsFinishedOK() const { return (_size == 0 && !_overflow); }
  UInt64 GetRem() const { return _size; }
};


class CTailInStream:
  public IInStream,
  public CMyUnknownImp
{
  UInt64 _virtPos;
public:
  CMyComPtr<IInStream> Stream;
  UInt64 Offset;

  void Init()
  {
    _virtPos = 0;
  }
 
  MY_UNKNOWN_IMP2(ISequentialInStream, IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);

  HRESULT SeekToStart() { return Stream->Seek((Int64)Offset, STREAM_SEEK_SET, NULL); }
};

class CLimitedCachedInStream:
  public IInStream,
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _stream;
  UInt64 _virtPos;
  UInt64 _physPos;
  UInt64 _size;
  UInt64 _startOffset;
  
  const Byte *_cache;
  size_t _cacheSize;
  size_t _cachePhyPos;


  HRESULT SeekToPhys() { return _stream->Seek((Int64)_physPos, STREAM_SEEK_SET, NULL); }
public:
  CByteBuffer Buffer;

  void SetStream(IInStream *stream) { _stream = stream; }
  void SetCache(size_t cacheSize, size_t cachePos)
  {
    _cache = Buffer;
    _cacheSize = cacheSize;
    _cachePhyPos = cachePos;
  }

  HRESULT InitAndSeek(UInt64 startOffset, UInt64 size)
  {
    _startOffset = startOffset;
    _physPos = startOffset;
    _virtPos = 0;
    _size = size;
    return SeekToPhys();
  }
 
  MY_UNKNOWN_IMP2(ISequentialInStream, IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);

  HRESULT SeekToStart() { return Seek(0, STREAM_SEEK_SET, NULL); }
};

class CTailOutStream:
  public IOutStream,
  public CMyUnknownImp
{
  UInt64 _virtPos;
  UInt64 _virtSize;
public:
  CMyComPtr<IOutStream> Stream;
  UInt64 Offset;
  
  virtual ~CTailOutStream() {}

  MY_UNKNOWN_IMP2(ISequentialOutStream, IOutStream)

  void Init()
  {
    _virtPos = 0;
    _virtSize = 0;
  }

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
  STDMETHOD(SetSize)(UInt64 newSize);
};

#endif
