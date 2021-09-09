// MultiStream.h

#ifndef __MULTI_STREAM_H
#define __MULTI_STREAM_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyVector.h"

#include "../../IStream.h"

class CMultiStream:
  public IInStream,
  public CMyUnknownImp
{
  UInt64 _pos;
  UInt64 _totalLength;
  unsigned _streamIndex;

public:

  struct CSubStreamInfo
  {
    CMyComPtr<IInStream> Stream;
    UInt64 Size;
    UInt64 GlobalOffset;
    UInt64 LocalPos;

    CSubStreamInfo(): Size(0), GlobalOffset(0), LocalPos(0) {}
  };
  
  CObjectVector<CSubStreamInfo> Streams;
  
  HRESULT Init()
  {
    UInt64 total = 0;
    FOR_VECTOR (i, Streams)
    {
      CSubStreamInfo &s = Streams[i];
      s.GlobalOffset = total;
      total += Streams[i].Size;
      RINOK(s.Stream->Seek(0, STREAM_SEEK_CUR, &s.LocalPos));
    }
    _totalLength = total;
    _pos = 0;
    _streamIndex = 0;
    return S_OK;
  }

  MY_UNKNOWN_IMP1(IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};

/*
class COutMultiStream:
  public IOutStream,
  public CMyUnknownImp
{
  unsigned _streamIndex; // required stream
  UInt64 _offsetPos; // offset from start of _streamIndex index
  UInt64 _absPos;
  UInt64 _length;

  struct CSubStreamInfo
  {
    CMyComPtr<ISequentialOutStream> Stream;
    UInt64 Size;
    UInt64 Pos;
 };
  CObjectVector<CSubStreamInfo> Streams;
public:
  CMyComPtr<IArchiveUpdateCallback2> VolumeCallback;
  void Init()
  {
    _streamIndex = 0;
    _offsetPos = 0;
    _absPos = 0;
    _length = 0;
  }

  MY_UNKNOWN_IMP1(IOutStream)

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};
*/

#endif
