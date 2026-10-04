// Pbkdf2HmacSha512.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_INC_CRYPTO_PBKDF2_HMAC_SHA512_H
#define ZIP7_INC_CRYPTO_PBKDF2_HMAC_SHA512_H

#include <stddef.h>

#include "../../Common/MyTypes.h"

namespace NCrypto {
namespace NSha512 {

void Pbkdf2Hmac(const Byte *pwd, size_t pwdSize, const Byte *salt, size_t saltSize,
    UInt32 numIterations, Byte *key, size_t keySize);

}}

#endif
