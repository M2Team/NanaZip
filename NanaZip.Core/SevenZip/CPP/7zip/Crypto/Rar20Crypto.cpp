// Crypto/Rar20Crypto.cpp

#include "StdAfx.h"

#include "../../../C/7zCrc.h"
#include "../../../C/CpuArch.h"
#include "../../../C/RotateDefs.h"

#include "Rar20Crypto.h"

namespace NCrypto {
namespace NRar2 {

static const unsigned kNumRounds = 32;

static const Byte g_InitSubstTable[256] = {
  215, 19,149, 35, 73,197,192,205,249, 28, 16,119, 48,221,  2, 42,
  232,  1,177,233, 14, 88,219, 25,223,195,244, 90, 87,239,153,137,
  255,199,147, 70, 92, 66,246, 13,216, 40, 62, 29,217,230, 86,  6,
   71, 24,171,196,101,113,218,123, 93, 91,163,178,202, 67, 44,235,
  107,250, 75,234, 49,167,125,211, 83,114,157,144, 32,193,143, 36,
  158,124,247,187, 89,214,141, 47,121,228, 61,130,213,194,174,251,
   97,110, 54,229,115, 57,152, 94,105,243,212, 55,209,245, 63, 11,
  164,200, 31,156, 81,176,227, 21, 76, 99,139,188,127, 17,248, 51,
  207,120,189,210,  8,226, 41, 72,183,203,135,165,166, 60, 98,  7,
  122, 38,155,170, 69,172,252,238, 39,134, 59,128,236, 27,240, 80,
  131,  3, 85,206,145, 79,154,142,159,220,201,133, 74, 64, 20,129,
  224,185,138,103,173,182, 43, 34,254, 82,198,151,231,180, 58, 10,
  118, 26,102, 12, 50,132, 22,191,136,111,162,179, 45,  4,148,108,
  161, 56, 78,126,242,222, 15,175,146, 23, 33,241,181,190, 77,225,
    0, 46,169,186, 68, 95,237, 65, 53,208,253,168,  9, 18,100, 52,
  116,184,160, 96,109, 37, 30,106,140,104,150,  5,204,117,112, 84
};

void CData::UpdateKeys(const Byte *data)
{
  for (unsigned i = 0; i < 16; i += 4)
    for (unsigned j = 0; j < 4; j++)
      Keys[j] ^= g_CrcTable[data[i + j]];
}

static inline void Swap(Byte &b1, Byte &b2)
{
  Byte b = b1;
  b1 = b2;
  b2 = b;
}

void CData::SetPassword(const Byte *data, unsigned size)
{
  Keys[0] = 0xD3A3B879L;
  Keys[1] = 0x3F6D12F7L;
  Keys[2] = 0x7515A235L;
  Keys[3] = 0xA4E7F123L;
  
  Byte psw[128];
  Z7_memset_0_ARRAY(psw);
  if (size != 0)
  {
    if (size >= sizeof(psw))
      size = sizeof(psw) - 1;
    memcpy(psw, data, size);
  }

  memcpy(SubstTable, g_InitSubstTable, sizeof(SubstTable));

  for (unsigned j = 0; j < 256; j++)
    for (unsigned i = 0; i < size; i += 2)
    {
      unsigned n1 = (Byte)g_CrcTable[(psw[i] - j) & 0xFF];
      unsigned n2 = (Byte)g_CrcTable[(psw[(size_t)i + 1] + j) & 0xFF];
      for (unsigned k = 1; (n1 & 0xFF) != n2; n1++, k++)
        Swap(SubstTable[n1 & 0xFF], SubstTable[(n1 + i + k) & 0xFF]);
    }
  
  for (unsigned i = 0; i < size; i += 16)
    EncryptBlock(psw + i);
}

void CData::CryptBlock(Byte *buf, bool encrypt)
{
  Byte inBuf[16];
  UInt32 A, B, C, D;

  A = GetUi32(buf +  0) ^ Keys[0];
  B = GetUi32(buf +  4) ^ Keys[1];
  C = GetUi32(buf +  8) ^ Keys[2];
  D = GetUi32(buf + 12) ^ Keys[3];

  if (!encrypt)
    memcpy(inBuf, buf, sizeof(inBuf));
  
  for (unsigned i = 0; i < kNumRounds; i++)
  {
    UInt32 key = Keys[(encrypt ? i : (kNumRounds - 1 - i)) & 3];
    UInt32 TA = A ^ SubstLong((C + rotlFixed(D, 11)) ^ key);
    UInt32 TB = B ^ SubstLong((D ^ rotlFixed(C, 17)) + key);
    A = C; C = TA;
    B = D; D = TB;
  }

  SetUi32(buf +  0, C ^ Keys[0])
  SetUi32(buf +  4, D ^ Keys[1])
  SetUi32(buf +  8, A ^ Keys[2])
  SetUi32(buf + 12, B ^ Keys[3])

  UpdateKeys(encrypt ? buf : inBuf);
}

Z7_COM7F_IMF(CDecoder::Init())
{
  return S_OK;
}

static const UInt32 kBlockSize = 16;

Z7_COM7F_IMF2(UInt32, CDecoder::Filter(Byte *data, UInt32 size))
{
  if (size == 0)
    return 0;
  if (size < kBlockSize)
    return kBlockSize;
  size -= kBlockSize;
  UInt32 i;
  for (i = 0; i <= size; i += kBlockSize)
    DecryptBlock(data + i);
  return i;
}

}}
