// HkdfBlake2sp.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_INC_CRYPTO_HKDF_BLAKE2SP_H
#define ZIP7_INC_CRYPTO_HKDF_BLAKE2SP_H

#include "../../../C/Blake2.h"
#include "../../Common/MyBuffer2.h"

namespace NCrypto {
namespace NHkdfBlake2sp {

void Derive(const Byte *prk, unsigned prkSize,
    const char *info, unsigned infoLen,
    Byte *output, unsigned outSize);

}}

#endif
