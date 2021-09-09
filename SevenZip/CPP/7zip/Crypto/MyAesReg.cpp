// MyAesReg.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "MyAes.h"

namespace NCrypto {

#ifndef _SFX

#define REGISTER_AES_2(name, nameString, keySize, isCtr) \
  REGISTER_FILTER_E(name, \
    CAesCoder(false, keySize, isCtr), \
    CAesCoder(true , keySize, isCtr), \
    0x6F00100 | ((keySize - 16) * 8) | (isCtr ? 4 : 1), \
    nameString) \

#define REGISTER_AES(name, nameString, isCtr) \
  /* REGISTER_AES_2(AES128 ## name, "AES128" nameString, 16, isCtr) */ \
  /* REGISTER_AES_2(AES192 ## name, "AES192" nameString, 24, isCtr) */ \
  REGISTER_AES_2(AES256 ## name, "AES256" nameString, 32, isCtr) \

REGISTER_AES(CBC, "CBC", false)
// REGISTER_AES(CTR, "CTR", true)

#endif

}
