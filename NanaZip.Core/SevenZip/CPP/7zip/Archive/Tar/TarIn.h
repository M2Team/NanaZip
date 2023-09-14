// TarIn.h

#ifndef ZIP7_INC_ARCHIVE_TAR_IN_H
#define ZIP7_INC_ARCHIVE_TAR_IN_H

#include "../IArchive.h"

#include "TarItem.h"

namespace NArchive {
namespace NTar {
  
enum EErrorType
{
  k_ErrorType_OK,
  k_ErrorType_Corrupted,
  k_ErrorType_UnexpectedEnd
  // , k_ErrorType_Warning
};


struct CTempBuffer
{
  CByteBuffer Buffer;
  size_t StringSize; // num characters before zero Byte (StringSize <= item.PackSize)
  bool IsNonZeroTail;
  bool StringSize_IsConfirmed;

  void CopyToString(AString &s)
  {
    s.Empty();
    if (StringSize != 0)
      s.SetFrom((const char *)(const void *)(const Byte *)Buffer, (unsigned)StringSize);
  }

  void Init()
  {
    StringSize = 0;
    IsNonZeroTail = false;
    StringSize_IsConfirmed = false;
  }
};


class CArchive
{
public:
  bool _phySize_Defined;
  bool _is_Warning;
  bool PaxGlobal_Defined;
  bool _is_PaxGlobal_Error;
  bool _are_Pax_Items;
  bool _are_Gnu;
  bool _are_Posix;
  bool _are_Pax;
  bool _are_mtime;
  bool _are_atime;
  bool _are_ctime;
  bool _are_pax_path;
  bool _are_pax_link;
  bool _are_LongName;
  bool _are_LongLink;
  bool _pathPrefix_WasUsed;
  // bool _isSparse;

  // temp internal vars for ReadItem():
  bool filled;
private:
  EErrorType error;

public:
  UInt64 _phySize;
  UInt64 _headersSize;
  EErrorType _error;

  ISequentialInStream *SeqStream;
  IInStream *InStream;
  IArchiveOpenCallback *OpenCallback;
  UInt64 NumFiles;
  UInt64 NumFiles_Prev;
  UInt64 Pos_Prev;
  // UInt64 NumRecords;
  // UInt64 NumRecords_Prev;

  CPaxExtra PaxGlobal;

  void Clear()
  {
    SeqStream = NULL;
    InStream = NULL;
    OpenCallback = NULL;
    NumFiles = 0;
    NumFiles_Prev = 0;
    Pos_Prev = 0;
    // NumRecords = 0;
    // NumRecords_Prev = 0;

    PaxGlobal.Clear();
    PaxGlobal_Defined = false;
    _is_PaxGlobal_Error = false;
    _are_Pax_Items = false; // if there are final paxItems
    _are_Gnu = false;
    _are_Posix = false;
    _are_Pax = false;
    _are_mtime = false;
    _are_atime = false;
    _are_ctime = false;
    _are_pax_path = false;
    _are_pax_link = false;
    _are_LongName = false;
    _are_LongLink = false;
    _pathPrefix_WasUsed = false;
    // _isSparse = false;

    _is_Warning = false;
    _error = k_ErrorType_OK;

    _phySize_Defined = false;
    _phySize = 0;
    _headersSize = 0;
  }

private:
  CTempBuffer NameBuf;
  CTempBuffer LinkBuf;
  CTempBuffer PaxBuf;
  CTempBuffer PaxBuf_global;

  CByteBuffer Buffer;

  HRESULT ReadDataToBuffer(const CItemEx &item, CTempBuffer &tb, size_t stringLimit);
  HRESULT Progress(const CItemEx &item, UInt64 posOffset);
  HRESULT GetNextItemReal(CItemEx &item);
  HRESULT ReadItem2(CItemEx &itemInfo);
public:
  CArchive()
  {
    // we will call Clear() in CHandler::Close().
    // Clear(); // it's not required here
  }
  HRESULT ReadItem(CItemEx &itemInfo);
};


API_FUNC_IsArc IsArc_Tar(const Byte *p, size_t size);

}}
  
#endif
