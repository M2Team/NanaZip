// 7zAes.h

#ifndef __CRYPTO_7Z_AES_H
#define __CRYPTO_7Z_AES_H

#include "../../Common/MyBuffer.h"
#include "../../Common/MyCom.h"
#include "../../Common/MyVector.h"

#include "../ICoder.h"
#include "../IPassword.h"

namespace NCrypto {
namespace N7z {

const unsigned kKeySize = 32;
const unsigned kSaltSizeMax = 16;
const unsigned kIvSizeMax = 16; // AES_BLOCK_SIZE;

class CKeyInfo
{
public:
  unsigned NumCyclesPower;
  unsigned SaltSize;
  Byte Salt[kSaltSizeMax];
  CByteBuffer Password;
  Byte Key[kKeySize];

  bool IsEqualTo(const CKeyInfo &a) const;
  void CalcKey();

  CKeyInfo() { ClearProps(); }
  void ClearProps()
  {
    NumCyclesPower = 0;
    SaltSize = 0;
    for (unsigned i = 0; i < sizeof(Salt); i++)
      Salt[i] = 0;
  }

  void Wipe()
  {
    Password.Wipe();
    NumCyclesPower = 0;
    SaltSize = 0;
    MY_memset_0_ARRAY(Salt);
    MY_memset_0_ARRAY(Key);
  }

  ~CKeyInfo() { Wipe(); }
};

class CKeyInfoCache
{
  unsigned Size;
  CObjectVector<CKeyInfo> Keys;
public:
  CKeyInfoCache(unsigned size): Size(size) {}
  bool GetKey(CKeyInfo &key);
  void Add(const CKeyInfo &key);
  void FindAndAdd(const CKeyInfo &key);
};

class CBase
{
  CKeyInfoCache _cachedKeys;
protected:
  CKeyInfo _key;
  Byte _iv[kIvSizeMax];
  unsigned _ivSize;
  
  void PrepareKey();
  CBase();
};

class CBaseCoder:
  public ICompressFilter,
  public ICryptoSetPassword,
  public CMyUnknownImp,
  public CBase
{
protected:
  CMyComPtr<ICompressFilter> _aesFilter;

public:
  INTERFACE_ICompressFilter(;)
  
  STDMETHOD(CryptoSetPassword)(const Byte *data, UInt32 size);
};

#ifndef EXTRACT_ONLY

class CEncoder:
  public CBaseCoder,
  public ICompressWriteCoderProperties,
  // public ICryptoResetSalt,
  public ICryptoResetInitVector
{
public:
  MY_UNKNOWN_IMP4(
      ICompressFilter,
      ICryptoSetPassword,
      ICompressWriteCoderProperties,
      // ICryptoResetSalt,
      ICryptoResetInitVector)
  STDMETHOD(WriteCoderProperties)(ISequentialOutStream *outStream);
  // STDMETHOD(ResetSalt)();
  STDMETHOD(ResetInitVector)();
  CEncoder();
};

#endif

class CDecoder:
  public CBaseCoder,
  public ICompressSetDecoderProperties2
{
public:
  MY_UNKNOWN_IMP3(
      ICompressFilter,
      ICryptoSetPassword,
      ICompressSetDecoderProperties2)
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);
  CDecoder();
};

}}

#endif
