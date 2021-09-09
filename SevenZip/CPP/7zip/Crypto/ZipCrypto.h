// Crypto/ZipCrypto.h

#ifndef __CRYPTO_ZIP_CRYPTO_H
#define __CRYPTO_ZIP_CRYPTO_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"
#include "../IPassword.h"

namespace NCrypto {
namespace NZip {

const unsigned kHeaderSize = 12;

/* ICompressFilter::Init() does nothing for this filter.
  Call to init:
    Encoder:
      CryptoSetPassword();
      WriteHeader();
    Decoder:
      [CryptoSetPassword();]
      ReadHeader();
      [CryptoSetPassword();] Init_and_GetCrcByte();
      [CryptoSetPassword();] Init_and_GetCrcByte();
*/

class CCipher:
  public ICompressFilter,
  public ICryptoSetPassword,
  public CMyUnknownImp
{
protected:
  UInt32 Key0;
  UInt32 Key1;
  UInt32 Key2;
  
  UInt32 KeyMem0;
  UInt32 KeyMem1;
  UInt32 KeyMem2;

  void RestoreKeys()
  {
    Key0 = KeyMem0;
    Key1 = KeyMem1;
    Key2 = KeyMem2;
  }

public:
  MY_UNKNOWN_IMP1(ICryptoSetPassword)
  STDMETHOD(Init)();
  STDMETHOD(CryptoSetPassword)(const Byte *data, UInt32 size);
  
  virtual ~CCipher()
  {
    Key0 = KeyMem0 =
    Key1 = KeyMem1 =
    Key2 = KeyMem2 = 0;
  }
};

class CEncoder: public CCipher
{
public:
  STDMETHOD_(UInt32, Filter)(Byte *data, UInt32 size);
  HRESULT WriteHeader_Check16(ISequentialOutStream *outStream, UInt16 crc);
};

class CDecoder: public CCipher
{
public:
  Byte _header[kHeaderSize];
  STDMETHOD_(UInt32, Filter)(Byte *data, UInt32 size);
  HRESULT ReadHeader(ISequentialInStream *inStream);
  void Init_BeforeDecode();
};

}}

#endif
