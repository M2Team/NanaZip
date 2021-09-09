// Crypto/MyAes.h

#ifndef __CRYPTO_MY_AES_H
#define __CRYPTO_MY_AES_H

#include "../../../C/Aes.h"

#include "../../Common/MyBuffer2.h"
#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCrypto {

class CAesCoder:
  public ICompressFilter,
  public ICryptoProperties,
  #ifndef _SFX
  public ICompressSetCoderProperties,
  #endif
  public CMyUnknownImp
{
  AES_CODE_FUNC _codeFunc;
  // unsigned _offset;
  unsigned _keySize;
  bool _keyIsSet;
  bool _encodeMode;
  bool _ctrMode;

  // UInt32 _aes[AES_NUM_IVMRK_WORDS + 3];
  CAlignedBuffer _aes;

  Byte _iv[AES_BLOCK_SIZE];

  // UInt32 *Aes() { return _aes + _offset; }
  UInt32 *Aes() { return (UInt32 *)(void *)(Byte *)_aes; }

  bool SetFunctions(UInt32 algo);

public:
  CAesCoder(bool encodeMode, unsigned keySize, bool ctrMode);
  
  virtual ~CAesCoder() {};   // we need virtual destructor for derived classes
  
  MY_QUERYINTERFACE_BEGIN2(ICompressFilter)
  MY_QUERYINTERFACE_ENTRY(ICryptoProperties)
  #ifndef _SFX
  MY_QUERYINTERFACE_ENTRY(ICompressSetCoderProperties)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE
  
  INTERFACE_ICompressFilter(;)

  void SetKeySize(unsigned size) { _keySize = size; }
  
  STDMETHOD(SetKey)(const Byte *data, UInt32 size);
  STDMETHOD(SetInitVector)(const Byte *data, UInt32 size);
  
  #ifndef _SFX
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  #endif
};

#ifndef _SFX
struct CAesCbcEncoder: public CAesCoder
{
  CAesCbcEncoder(unsigned keySize = 0): CAesCoder(true, keySize, false) {}
};
#endif

struct CAesCbcDecoder: public CAesCoder
{
  CAesCbcDecoder(unsigned keySize = 0): CAesCoder(false, keySize, false) {}
};

}

#endif
