// UniqBlocks.h

#ifndef __UNIQ_BLOCKS_H
#define __UNIQ_BLOCKS_H

#include "../../Common/MyBuffer.h"
#include "../../Common/MyString.h"

struct C_UInt32_UString_Map
{
  CRecordVector<UInt32> Numbers;
  UStringVector Strings;

  void Add_UInt32(const UInt32 n)
  {
    Numbers.AddToUniqueSorted(n);
  }
  int Find(const UInt32 n)
  {
    return Numbers.FindInSorted(n);
  }
};


struct CUniqBlocks
{
  CObjectVector<CByteBuffer> Bufs;
  CUIntVector Sorted;
  CUIntVector BufIndexToSortedIndex;

  unsigned AddUniq(const Byte *data, size_t size);
  UInt64 GetTotalSizeInBytes() const;
  void GetReverseMap();

  bool IsOnlyEmpty() const
  {
    return (Bufs.Size() == 0 || (Bufs.Size() == 1 && Bufs[0].Size() == 0));
  }
};

#endif
