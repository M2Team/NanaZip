// InOutTempBuffer.h

#ifndef __IN_OUT_TEMP_BUFFER_H
#define __IN_OUT_TEMP_BUFFER_H

#ifdef _WIN32
// #define USE_InOutTempBuffer_FILE
#endif

#ifdef USE_InOutTempBuffer_FILE
#include "../../Windows/FileDir.h"
#else
#include "StreamObjects.h"
#endif

#include "../IStream.h"

class CInOutTempBuffer
{
  #ifdef USE_InOutTempBuffer_FILE
  
  NWindows::NFile::NDir::CTempFile _tempFile;
  NWindows::NFile::NIO::COutFile _outFile;
  bool _tempFileCreated;
  Byte *_buf;
  size_t _bufPos;
  UInt64 _size;
  UInt32 _crc;

  #else
  
  CByteDynBuffer _dynBuffer;
  size_t _size;
  
  #endif

  CLASS_NO_COPY(CInOutTempBuffer);
public:
  CInOutTempBuffer();
  void Create();

  #ifdef USE_InOutTempBuffer_FILE
  ~CInOutTempBuffer();
  #endif

  void InitWriting();
  HRESULT Write_HRESULT(const void *data, UInt32 size);
  HRESULT WriteToStream(ISequentialOutStream *stream);
  UInt64 GetDataSize() const { return _size; }
};

/*
class CSequentialOutTempBufferImp:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CInOutTempBuffer *_buf;
public:
  void Init(CInOutTempBuffer *buffer)  { _buf = buffer; }
  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};
*/

#endif
