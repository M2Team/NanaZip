// 7zKeyDerivation.cpp
// Key derivation common module for 7z format

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/Sha256.h"

#include "../../Common/MyBuffer2.h"

#include "7zKeyDerivation.h"
#include "Pbkdf2HmacSha512.h"

namespace NCrypto {
namespace N7zKeyDerivation {

static bool ConstantTimeCompare(const Byte *a, const Byte *b, size_t size)
{
  volatile Byte result = 0;
  for (size_t i = 0; i < size; i++)
    result |= a[i] ^ b[i];
  return result == 0;
}

bool CKeyInfo::IsEqualTo(const CKeyInfo &a) const
{
  if (DerivMode != a.DerivMode)
    return false;
  if (SaltSize != a.SaltSize || NumCyclesPower != a.NumCyclesPower)
    return false;
  if (!ConstantTimeCompare(Salt, a.Salt, SaltSize))
    return false;
  if (Password.Size() != a.Password.Size())
    return false;
  return ConstantTimeCompare(Password, a.Password, Password.Size());
}

void CKeyInfo::CalcKey()
{
  if (DerivMode == kDeriv_Cascade)
  {
    // PBKDF2-HMAC-SHA512, output 96 bytes
    const UInt32 numIterations = (NumCyclesPower == 0x3F) ?
        1 : (UInt32)1 << NumCyclesPower;
    NSha512::Pbkdf2Hmac(
        Password, Password.Size(),
        Salt, SaltSize,
        numIterations,
        CascadeKey, kCascadeKeySize);
  }
  else if (NumCyclesPower == 0x3F)
  {
    unsigned pos;
    for (pos = 0; pos < SaltSize; pos++)
      Key[pos] = Salt[pos];
    for (unsigned i = 0; i < Password.Size() && pos < kKeySize; i++)
      Key[pos++] = Password[i];
    for (; pos < kKeySize; pos++)
      Key[pos] = 0;
  }
  else
  {
    const unsigned kUnrPow = 6;
    const UInt32 numUnroll = (UInt32)1 << (NumCyclesPower <= kUnrPow ? (unsigned)NumCyclesPower : kUnrPow);

    const size_t bufSize = 8 + SaltSize + Password.Size();
    const size_t unrollSize = bufSize * numUnroll;

    const size_t shaAllocSize = sizeof(CSha256) + unrollSize + bufSize * 2;
    CAlignedBuffer1 sha(shaAllocSize);
    Byte *buf = sha + sizeof(CSha256);

    memcpy(buf, Salt, SaltSize);
    memcpy(buf + SaltSize, Password, Password.Size());
    memset(buf + bufSize - 8, 0, 8);
    
    Sha256_Init((CSha256 *)(void *)(Byte *)sha);
    
    {
      {
        Byte *dest = buf;
        for (UInt32 i = 1; i < numUnroll; i++)
        {
          dest += bufSize;
          memcpy(dest, buf, bufSize);
        }
      }

      const UInt32 numRounds = (UInt32)1 << NumCyclesPower;
      UInt32 r = 0;
      do
      {
        Byte *dest = buf + bufSize - 8;
        UInt32 i = r;
        r += numUnroll;
        do
        {
          SetUi32(dest, i);  i++; dest += bufSize;
        }
        while (i < r);
        Sha256_Update((CSha256 *)(void *)(Byte *)sha, buf, unrollSize);
      }
      while (r < numRounds);
    }

    Sha256_Final((CSha256 *)(void *)(Byte *)sha, Key);
    memset(sha, 0, shaAllocSize);
  }
}

bool CKeyInfoCache::GetKey(CKeyInfo &key)
{
  FOR_VECTOR (i, Keys)
  {
    const CKeyInfo &cached = Keys[i];
    if (key.IsEqualTo(cached))
    {
      if (cached.DerivMode == kDeriv_Cascade)
        memcpy(key.CascadeKey, cached.CascadeKey, kCascadeKeySize);
      else
        memcpy(key.Key, cached.Key, kKeySize);
      if (i != 0)
        Keys.MoveToFront(i);
      return true;
    }
  }
  return false;
}

void CKeyInfoCache::FindAndAdd(const CKeyInfo &key)
{
  FOR_VECTOR (i, Keys)
  {
    const CKeyInfo &cached = Keys[i];
    if (key.IsEqualTo(cached))
    {
      if (i != 0)
        Keys.MoveToFront(i);
      return;
    }
  }
  Add(key);
}

void CKeyInfoCache::Add(const CKeyInfo &key)
{
  if (Keys.Size() >= Size)
    Keys.DeleteBack();
  Keys.Insert(0, key);
}

}}
