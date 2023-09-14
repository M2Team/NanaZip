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
  UInt32 FolderIndex;
  UInt16 Flags;
  UInt16 Attributes;
  
  UInt64 GetEndOffset() const { return (UInt64)Offset + Size; }
  UInt32 GetWinAttrib() const { return (UInt32)Attributes & ~(UInt32)NHeader::kFileNameIsUtf8_Mask; }
  bool IsNameUTF() const { return (Attributes & NHeader::kFileNameIsUtf8_Mask) != 0; }
  bool IsDir() const { return (Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }

  bool ContinuedFromPrev() const
  {
    return
      FolderIndex == NHeader::NFolderIndex::kContinuedFromPrev ||
      FolderIndex == NHeader::NFolderIndex::kContinuedPrevAndNext;
  }
  
  bool ContinuedToNext() const
  {
    return
      FolderIndex == NHeader::NFolderIndex::kContinuedToNext ||
      FolderIndex == NHeader::NFolderIndex::kContinuedPrevAndNext;
  }

  int GetFolderIndex(unsigned numFolders) const
  {
    if (ContinuedFromPrev())
      return 0;
    if (ContinuedToNext())
      return (int)numFolders - 1;
    return (int)FolderIndex;
  }
};

}}

#endif
