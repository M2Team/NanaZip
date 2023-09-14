// MyMap.cpp

#include "StdAfx.h"

#include "MyMap.h"

static const unsigned kNumBitsMax = sizeof(UInt32) * 8;

static UInt32 GetSubBits(UInt32 value, unsigned startPos, unsigned numBits) throw()
{
  if (startPos == sizeof(value) * 8)
    return 0;
  value >>= startPos;
  if (numBits == sizeof(value) * 8)
    return value;
  return value & (((UInt32)1 << numBits) - 1);
}

static inline unsigned GetSubBit(UInt32 v, unsigned n) { return (unsigned)(v >> n) & 1; }

bool CMap32::Find(UInt32 key, UInt32 &valueRes) const throw()
{
  valueRes = (UInt32)(Int32)-1;
  if (Nodes.Size() == 0)
    return false;
  if (Nodes.Size() == 1)
  {
    const CNode &n = Nodes[0];
    if (n.Len == kNumBitsMax)
    {
      valueRes = n.Values[0];
      return (key == n.Key);
    }
  }

  unsigned cur = 0;
  unsigned bitPos = kNumBitsMax;
  for (;;)
  {
    const CNode &n = Nodes[cur];
    bitPos -= n.Len;
    if (GetSubBits(key, bitPos, n.Len) != GetSubBits(n.Key, bitPos, n.Len))
      return false;
    unsigned bit = GetSubBit(key, --bitPos);
    if (n.IsLeaf[bit])
    {
      valueRes = n.Values[bit];
      return (key == n.Keys[bit]);
    }
    cur = (unsigned)n.Keys[bit];
  }
}

bool CMap32::Set(UInt32 key, UInt32 value)
{
  if (Nodes.Size() == 0)
  {
    CNode n;
    n.Key = n.Keys[0] = n.Keys[1] = key;
    n.Values[0] = n.Values[1] = value;
    n.IsLeaf[0] = n.IsLeaf[1] = 1;
    n.Len = kNumBitsMax;
    Nodes.Add(n);
    return false;
  }
  if (Nodes.Size() == 1)
  {
    CNode &n = Nodes[0];
    if (n.Len == kNumBitsMax)
    {
      if (key == n.Key)
      {
        n.Values[0] = n.Values[1] = value;
        return true;
      }
      unsigned i = kNumBitsMax - 1;
      for (; GetSubBit(key, i) == GetSubBit(n.Key, i); i--);
      n.Len = (UInt16)(kNumBitsMax - (1 + i));
      const unsigned newBit = GetSubBit(key, i);
      n.Values[newBit] = value;
      n.Keys[newBit] = key;
      return false;
    }
  }

  unsigned cur = 0;
  unsigned bitPos = kNumBitsMax;
  for (;;)
  {
    CNode &n = Nodes[cur];
    bitPos -= n.Len;
    if (GetSubBits(key, bitPos, n.Len) != GetSubBits(n.Key, bitPos, n.Len))
    {
      unsigned i = (unsigned)n.Len - 1;
      for (; GetSubBit(key, bitPos + i) == GetSubBit(n.Key, bitPos + i); i--);
      
      CNode e2(n);
      e2.Len = (UInt16)i;

      n.Len = (UInt16)(n.Len - (1 + i));
      unsigned newBit = GetSubBit(key, bitPos + i);
      n.Values[newBit] = value;
      n.IsLeaf[newBit] = 1;
      n.IsLeaf[1 - newBit] = 0;
      n.Keys[newBit] = key;
      n.Keys[1 - newBit] = Nodes.Size();
      Nodes.Add(e2);
      return false;
    }
    const unsigned bit = GetSubBit(key, --bitPos);

    if (n.IsLeaf[bit])
    {
      if (key == n.Keys[bit])
      {
        n.Values[bit] = value;
        return true;
      }
      unsigned i = bitPos - 1;
      for (; GetSubBit(key, i) == GetSubBit(n.Keys[bit], i); i--);
     
      CNode e2;
      
      const unsigned newBit = GetSubBit(key, i);
      e2.Values[newBit] = value;
      e2.Values[1 - newBit] = n.Values[bit];
      e2.IsLeaf[newBit] = e2.IsLeaf[1 - newBit] = 1;
      e2.Keys[newBit] = key;
      e2.Keys[1 - newBit] = e2.Key = n.Keys[bit];
      e2.Len = (UInt16)(bitPos - (1 + i));

      n.IsLeaf[bit] = 0;
      n.Keys[bit] = Nodes.Size();

      Nodes.Add(e2);
      return false;
    }
    cur = (unsigned)n.Keys[bit];
  }
}
