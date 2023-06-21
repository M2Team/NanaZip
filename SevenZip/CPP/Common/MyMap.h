// MyMap.h

#ifndef __COMMON_MYMAP_H
#define __COMMON_MYMAP_H

#include "../../../ThirdParty/LZMA/CPP/Common/MyTypes.h"
#include "../../../ThirdParty/LZMA/CPP/Common/MyVector.h"

class CMap32
{
  struct CNode
  {
    UInt32 Key;
    UInt32 Keys[2];
    UInt32 Values[2];
    UInt16 Len;
    Byte IsLeaf[2];
  };
  CRecordVector<CNode> Nodes;

public:

  void Clear() { Nodes.Clear(); }
  bool Find(UInt32 key, UInt32 &valueRes) const throw();
  bool Set(UInt32 key, UInt32 value); // returns true, if there is such key already
};

#endif
