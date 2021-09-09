// FileStreams.h

#ifndef __FILE_STREAMS_H
#define __FILE_STREAMS_H

#ifdef _WIN32
#define USE_WIN_FILE
#endif

#include "../../Common/MyCom.h"
#include "../../Common/MyString.h"

#include "../../Windows/FileIO.h"

#include "../IStream.h"

struct IInFileStream_Callback
{
  virtual HRESULT InFileStream_On_Error(UINT_PTR val, DWORD error) = 0;
  virtual void InFileStream_On_Destroy(UINT_PTR val) = 0;
};

class CInFileStream:
  public IInStream,
  public IStreamGetSize,
  public IStreamGetProps,
  public IStreamGetProps2,
  public CMyUnknownImp
{
public:
  NWindows::NFile::NIO::CInFile File;

  #ifdef USE_WIN_FILE
  
  #ifdef SUPPORT_DEVICE_FILE
  UInt64 VirtPos;
  UInt64 PhyPos;
  UInt64 BufStartPos;
  Byte *Buf;
  UInt32 BufSize;
  #endif

  #endif

  bool SupportHardLinks;

  IInFileStream_Callback *Callback;
  UINT_PTR CallbackRef;

  virtual ~CInFileStream();

  CInFileStream();
  
  bool Open(CFSTR fileName)
  {
    return File.Open(fileName);
  }
  
  bool OpenShared(CFSTR fileName, bool shareForWrite)
  {
    return File.OpenShared(fileName, shareForWrite);
  }

  MY_QUERYINTERFACE_BEGIN2(IInStream)
  MY_QUERYINTERFACE_ENTRY(IStreamGetSize)
  MY_QUERYINTERFACE_ENTRY(IStreamGetProps)
  MY_QUERYINTERFACE_ENTRY(IStreamGetProps2)
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);

  STDMETHOD(GetSize)(UInt64 *size);
  STDMETHOD(GetProps)(UInt64 *size, FILETIME *cTime, FILETIME *aTime, FILETIME *mTime, UInt32 *attrib);
  STDMETHOD(GetProps2)(CStreamFileProps *props);
};

class CStdInFileStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP

  virtual ~CStdInFileStream() {}
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
};

class COutFileStream:
  public IOutStream,
  public CMyUnknownImp
{
public:
  NWindows::NFile::NIO::COutFile File;

  virtual ~COutFileStream() {}
  bool Create(CFSTR fileName, bool createAlways)
  {
    ProcessedSize = 0;
    return File.Create(fileName, createAlways);
  }
  bool Open(CFSTR fileName, DWORD creationDisposition)
  {
    ProcessedSize = 0;
    return File.Open(fileName, creationDisposition);
  }

  HRESULT Close();
  
  UInt64 ProcessedSize;

  bool SetTime(const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime)
  {
    return File.SetTime(cTime, aTime, mTime);
  }
  bool SetMTime(const FILETIME *mTime) {  return File.SetMTime(mTime); }

  MY_UNKNOWN_IMP1(IOutStream)

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
  STDMETHOD(SetSize)(UInt64 newSize);

  bool SeekToBegin_bool()
  {
    #ifdef USE_WIN_FILE
    return File.SeekToBegin();
    #else
    return File.seekToBegin() == 0;
    #endif
  }

  HRESULT GetSize(UInt64 *size);
};

class CStdOutFileStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  UInt64 _size;
public:
  MY_UNKNOWN_IMP

  UInt64 GetSize() const { return _size; }
  CStdOutFileStream(): _size(0) {}
  virtual ~CStdOutFileStream() {}
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

#endif
