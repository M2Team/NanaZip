// Archive/CabItem.h

#ifndef ZIP7_INC_ARCHIVE_CAB_ITEM_H
#define ZIP7_INC_ARCHIVE_CAB_ITEM_H

#include "../../../Common/MyString.h"

#include "CabHeader.h"

namespace NArchive {
namespace NCab {

const unsigned kNumMethodsMax = 16;

struct CFolder
{
  UInt32 DataStart; // offset of the first CFDATA block in this folder
  UInt16 NumDataBlocks; // number of CFDATA blocks in this folder
  Byte MethodMajor;
  Byte MethodMinor;
  
  Byte GetMethod() const { return (Byte)(MethodMajor & 0xF); }
};

struct CItem
{
  AString Name;
  UInt32 Offset;
  UInt32 Size;
  UInt32 Time;
  unsigned FolderIndex;
  UInt16 Flags;
  UInt16 Attributes;
  
  UInt64 GetEndOffset() const { return (UInt64)Offset + Size; }
  /* v26.03: we show all items as files, as original cab extracting software does.
     Also we disable FILE_ATTRIBUTE_DIRECTORY in GetWinAttrib(), because some client
     software of 7zip still can parse FILE_ATTRIBUTE_DIRECTORY flag from returned Attributes. */
  UInt32 GetWinAttrib() const { return (UInt32)Attributes & ~((UInt32)NHeader::kFileNameIsUtf8_Mask | (UInt32)FILE_ATTRIBUTE_DIRECTORY); }
  bool IsNameUTF() const { return (Attributes & NHeader::kFileNameIsUtf8_Mask) != 0; }
  // bool IsDir() const { return (Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }

  bool ContinuedFromPrev() const
  {
    return
      FolderIndex == NHeader::NFolderIndex::kContinuedFromPrev ||
      FolderIndex == NHeader::NFolderIndex::kContinuedPrevAndNext;
  }
  // in:  (numFolders > 0) is expected
  // out: (returned_value >= 0) is expected
  int GetFolderIndex(unsigned numFolders) const
  {
    const unsigned folderIndex = FolderIndex;
    switch (folderIndex)
    {
      case NHeader::NFolderIndex::kContinuedFromPrev:
      case NHeader::NFolderIndex::kContinuedPrevAndNext:
        return 0;
      case NHeader::NFolderIndex::kContinuedToNext:
        return (int)numFolders - 1;
      default:
        return (int)folderIndex;
    }
  }
};

}}

#endif
