// RarItem.h

#ifndef ZIP7_INC_ARCHIVE_RAR_ITEM_H
#define ZIP7_INC_ARCHIVE_RAR_ITEM_H

#include "../../../Common/StringConvert.h"

#include "RarHeader.h"

namespace NArchive {
namespace NRar {

struct CRarTime
{
  UInt32 DosTime;
  Byte LowSecond;
  Byte SubTime[3];
};

struct CItem
{
  UInt64 Size;
  UInt64 PackSize;
  
  CRarTime CTime;
  CRarTime ATime;
  CRarTime MTime;

  UInt32 FileCRC;
  UInt32 Attrib;

  UInt16 Flags;
  Byte HostOS;
  Byte UnPackVersion;
  Byte Method;

  bool CTimeDefined;
  bool ATimeDefined;

  AString Name;
  UString UnicodeName;

  Byte Salt[8];
  
  bool Is_Size_Defined() const { return Size != (UInt64)(Int64)-1; }

  bool IsEncrypted()   const { return (Flags & NHeader::NFile::kEncrypted) != 0; }
  bool IsSolid()       const { return (Flags & NHeader::NFile::kSolid) != 0; }
  bool IsCommented()   const { return (Flags & NHeader::NFile::kComment) != 0; }
  bool IsSplitBefore() const { return (Flags & NHeader::NFile::kSplitBefore) != 0; }
  bool IsSplitAfter()  const { return (Flags & NHeader::NFile::kSplitAfter) != 0; }
  bool HasSalt()       const { return (Flags & NHeader::NFile::kSalt) != 0; }
  bool HasExtTime()    const { return (Flags & NHeader::NFile::kExtTime) != 0; }
  bool HasUnicodeName()const { return (Flags & NHeader::NFile::kUnicodeName) != 0; }
  bool IsOldVersion()  const { return (Flags & NHeader::NFile::kOldVersion) != 0; }
  
  UInt32 GetDictSize() const { return (Flags >> NHeader::NFile::kDictBitStart) & NHeader::NFile::kDictMask; }
  bool IsDir() const;
  bool IgnoreItem() const;
  UInt32 GetWinAttrib() const;

  UInt64 Position;
  unsigned MainPartSize;
  UInt16 CommentSize;
  UInt16 AlignSize;

  // int BaseFileIndex;
  // bool IsAltStream;

  UString GetName() const
  {
    if (( /* IsAltStream || */ HasUnicodeName()) && !UnicodeName.IsEmpty())
      return UnicodeName;
    return MultiByteToUnicodeString(Name, CP_OEMCP);
  }

  void Clear()
  {
    CTimeDefined = false;
    ATimeDefined = false;
    Name.Empty();
    UnicodeName.Empty();
    // IsAltStream = false;
    // BaseFileIndex = -1;
  }

  CItem() { Clear(); }

  UInt64 GetFullSize()  const { return MainPartSize + CommentSize + AlignSize + PackSize; }
  //  DWORD GetHeaderWithCommentSize()  const { return MainPartSize + CommentSize; }
  UInt64 GetCommentPosition() const { return Position + MainPartSize; }
  UInt64 GetDataPosition()    const { return GetCommentPosition() + CommentSize + AlignSize; }
};

}}

#endif
