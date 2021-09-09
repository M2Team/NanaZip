// Crypto/MyAes.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "MyAes.h"

namespace NCrypto {

static struct CAesTabInit { CAesTabInit() { AesGenTables();} } g_AesTabInit;

CAesCoder::CAesCoder(bool encodeMode, unsigned keySize, bool ctrMode):
  _keySize(keySize),
  _keyIsSet(false),
  _encodeMode(encodeMode),
  _ctrMode(ctrMode),
  _aes(AES_NUM_IVMRK_WORDS * 4 + AES_BLOCK_SIZE * 2)
{
  // _offset = ((0 - (unsigned)(ptrdiff_t)_aes) & 0xF) / sizeof(UInt32);
  memset(_iv, 0, AES_BLOCK_SIZE);
  /*
  // we can use the following code to test 32-bit overflow case for AES-CTR
  for (unsigned i = 0; i < 16; i++) _iv[i] = (Byte)(i + 1);
  _iv[0] = 0xFE; _iv[1] = _iv[2] = _iv[3] = 0xFF;
  */
  SetFunctions(0);
}

STDMETHODIMP CAesCoder::Init()
{
  AesCbc_Init(Aes(), _iv);
  return _keyIsSet ? S_OK : E_NOTIMPL; // E_FAIL
}

STDMETHODIMP_(UInt32) CAesCoder::Filter(Byte *data, UInt32 size)
{
  if (!_keyIsSet)
    return 0;
  if (size == 0)
    return 0;
  if (size < AES_BLOCK_SIZE)
  {
    #ifndef _SFX
    if (_ctrMode)
    {
      // use that code only for last block !!!
      Byte *ctr = (Byte *)(Aes() + AES_NUM_IVMRK_WORDS);
      memset(ctr, 0, AES_BLOCK_SIZE);
      memcpy(ctr, data, size);
      _codeFunc(Aes(), ctr, 1);
      memcpy(data, ctr, size);
      return size;
    }
    #endif
    return AES_BLOCK_SIZE;
  }
  size >>= 4;
  _codeFunc(Aes(), data, size);
  return size << 4;
}

STDMETHODIMP CAesCoder::SetKey(const Byte *data, UInt32 size)
{
  if ((size & 0x7) != 0 || size < 16 || size > 32)
    return E_INVALIDARG;
  if (_keySize != 0 && size != _keySize)
    return E_INVALIDARG;
  AES_SET_KEY_FUNC setKeyFunc = (_ctrMode | _encodeMode) ? Aes_SetKey_Enc : Aes_SetKey_Dec;
  setKeyFunc(Aes() + 4, data, size);
  _keyIsSet = true;
  return S_OK;
}

STDMETHODIMP CAesCoder::SetInitVector(const Byte *data, UInt32 size)
{
  if (size != AES_BLOCK_SIZE)
    return E_INVALIDARG;
  memcpy(_iv, data, size);
  CAesCoder::Init(); // don't call virtual function here !!!
  return S_OK;
}

#ifndef _SFX

#ifdef MY_CPU_X86_OR_AMD64
  #define USE_HW_AES
#elif defined(MY_CPU_ARM_OR_ARM64) && defined(MY_CPU_LE)
  #if defined(__clang__)
    #if (__clang_major__ >= 8) // fix that check
      #define USE_HW_AES
    #endif
  #elif defined(__GNUC__)
    #if (__GNUC__ >= 6) // fix that check
      #define USE_HW_AES
    #endif
  #elif defined(_MSC_VER)
    #if _MSC_VER >= 1910
      #define USE_HW_AES
    #endif
  #endif
#endif

#endif


bool CAesCoder::SetFunctions(UInt32
    #ifndef _SFX
    algo
    #endif
    )
{
  _codeFunc = g_AesCbc_Decode;

  #ifdef _SFX

  return true;

  #else
  
  if (_ctrMode)
    _codeFunc = g_AesCtr_Code;
  else if (_encodeMode)
    _codeFunc = g_AesCbc_Encode;

  if (algo < 1)
    return true;

  if (algo == 1)
  {
    _codeFunc = AesCbc_Decode;
  
    #ifndef _SFX
    if (_ctrMode)
      _codeFunc = AesCtr_Code;
    else if (_encodeMode)
      _codeFunc = AesCbc_Encode;
    #endif
    return true;
  }

  #ifdef USE_HW_AES
  // if (CPU_IsSupported_AES())
  {
    if (algo == 2)
    if (g_Aes_SupportedFunctions_Flags & k_Aes_SupportedFunctions_HW)
    {
      _codeFunc = AesCbc_Decode_HW;
      #ifndef _SFX
      if (_ctrMode)
        _codeFunc = AesCtr_Code_HW;
      else if (_encodeMode)
        _codeFunc = AesCbc_Encode_HW;
      #endif
      return true;
    }

    #if defined(MY_CPU_X86_OR_AMD64)
    if (algo == 3)
    if (g_Aes_SupportedFunctions_Flags & k_Aes_SupportedFunctions_HW_256)
    {
      _codeFunc = AesCbc_Decode_HW_256;
      #ifndef _SFX
      if (_ctrMode)
        _codeFunc = AesCtr_Code_HW_256;
      else if (_encodeMode)
        _codeFunc = AesCbc_Encode_HW;
      #endif
      return true;
    }
    #endif
  }
  #endif

  return false;

  #endif
}


#ifndef _SFX

STDMETHODIMP CAesCoder::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps)
{
  UInt32 algo = 0;
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    if (propIDs[i] == NCoderPropID::kDefaultProp)
    {
      if (prop.vt != VT_UI4)
        return E_INVALIDARG;
      if (prop.ulVal > 3)
        return E_NOTIMPL;
      algo = prop.ulVal;
    }
  }
  if (!SetFunctions(algo))
    return E_NOTIMPL;
  return S_OK;
}

#endif

}
