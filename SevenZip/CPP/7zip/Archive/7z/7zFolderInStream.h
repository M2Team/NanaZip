// 7zFolderInStream.h

#ifndef __7Z_FOLDER_IN_STREAM_H
#define __7Z_FOLDER_IN_STREAM_H

#include "../../../../C/7zCrc.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/MyVector.h"

#include "../../ICoder.h"
#include "../IArchive.h"

namespace NArchive {
namespace N7z {

class CFolderInStream:
  public ISequentialInStream,
  public ICompressGetSubStreamSize,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialInStream> _stream;
  UInt64 _pos;
  UInt32 _crc;
  bool _size_Defined;
  bool _times_Defined;
  UInt64 _size;
  FILETIME _cTime;
  FILETIME _aTime;
  FILETIME _mTime;
  UInt32 _attrib;

  unsigned _numFiles;
  const UInt32 *_indexes;

  CMyComPtr<IArchiveUpdateCallback> _updateCallback;

  HRESULT OpenStream();
  HRESULT AddFileInfo(bool isProcessed);

public:
  CRecordVector<bool> Processed;
  CRecordVector<UInt32> CRCs;
  CRecordVector<UInt64> Sizes;
  CRecordVector<UInt64> CTimes;
  CRecordVector<UInt64> ATimes;
  CRecordVector<UInt64> MTimes;
  CRecordVector<UInt32> Attribs;
  CRecordVector<bool> TimesDefined;

  bool Need_CTime;
  bool Need_ATime;
  bool Need_MTime;
  bool Need_Attrib;

  // CMyComPtr<IArchiveUpdateCallbackArcProp> _reportArcProp;

  MY_UNKNOWN_IMP2(ISequentialInStream, ICompressGetSubStreamSize)
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(GetSubStreamSize)(UInt64 subStream, UInt64 *value);

  void Init(IArchiveUpdateCallback *updateCallback, const UInt32 *indexes, unsigned numFiles);

  bool WasFinished() const { return Processed.Size() == _numFiles; }

  UInt64 GetFullSize() const
  {
    UInt64 size = 0;
    FOR_VECTOR (i, Sizes)
      size += Sizes[i];
    return size;
  }

  CFolderInStream():
      Need_CTime(false),
      Need_ATime(false),
      Need_MTime(false),
      Need_Attrib(false)
      {}
};

}}

#endif
