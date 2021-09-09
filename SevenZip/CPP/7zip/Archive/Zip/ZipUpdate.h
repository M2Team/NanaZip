// ZipUpdate.h

#ifndef __ZIP_UPDATE_H
#define __ZIP_UPDATE_H

#include "../../ICoder.h"
#include "../IArchive.h"

#include "../../Common/CreateCoder.h"

#include "ZipCompressionMode.h"
#include "ZipIn.h"

namespace NArchive {
namespace NZip {

/*
struct CUpdateRange
{
  UInt64 Position;
  UInt64 Size;
  
  // CUpdateRange() {};
  CUpdateRange(UInt64 position, UInt64 size): Position(position), Size(size) {};
};
*/

struct CUpdateItem
{
  bool NewData;
  bool NewProps;
  bool IsDir;
  bool NtfsTimeIsDefined;
  bool IsUtf8;
  // bool IsAltStream;
  int IndexInArc;
  unsigned IndexInClient;
  UInt32 Attrib;
  UInt32 Time;
  UInt64 Size;
  AString Name;
  CByteBuffer Name_Utf;    // for Info-Zip (kIzUnicodeName) Extra
  CByteBuffer Comment;
  // bool Commented;
  // CUpdateRange CommentRange;
  FILETIME Ntfs_MTime;
  FILETIME Ntfs_ATime;
  FILETIME Ntfs_CTime;

  void Clear()
  {
    IsDir = false;
    NtfsTimeIsDefined = false;
    IsUtf8 = false;
    // IsAltStream = false;
    Size = 0;
    Name.Empty();
    Name_Utf.Free();
    Comment.Free();
  }

  CUpdateItem():
    IsDir(false),
    NtfsTimeIsDefined(false),
    IsUtf8(false),
    // IsAltStream(false),
    Size(0)
    {}
};

HRESULT Update(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CItemEx> &inputItems,
    CObjectVector<CUpdateItem> &updateItems,
    ISequentialOutStream *seqOutStream,
    CInArchive *inArchive, bool removeSfx,
    const CCompressionMethodMode &compressionMethodMode,
    IArchiveUpdateCallback *updateCallback);

}}

#endif
