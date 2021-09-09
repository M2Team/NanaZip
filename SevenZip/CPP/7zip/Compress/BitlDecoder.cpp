// BitlDecoder.cpp

#include "StdAfx.h"

#include "BitlDecoder.h"

namespace NBitl {

Byte kInvertTable[256];

static
struct CInverterTableInitializer
{
  CInverterTableInitializer()
  {
    for (unsigned i = 0; i < 256; i++)
    {
      unsigned x = ((i & 0x55) << 1) | ((i & 0xAA) >> 1);
      x = ((x & 0x33) << 2) | ((x & 0xCC) >> 2);
      kInvertTable[i] = (Byte)(((x & 0x0F) << 4) | ((x & 0xF0) >> 4));
    }
  }
} g_InverterTableInitializer;

}
