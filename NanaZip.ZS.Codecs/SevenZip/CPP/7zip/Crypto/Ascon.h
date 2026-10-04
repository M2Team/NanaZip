// Ascon.h
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#ifndef ZIP7_INC_CRYPTO_ASCON_H
#define ZIP7_INC_CRYPTO_ASCON_H

#include "../../Common/MyCom.h"

namespace NCrypto {
namespace NAscon {

const unsigned kKeySize_Ascon = 16;
const unsigned kNonceSize = 16;
const unsigned kTagSize = 16;
const unsigned kRateSize = 16;

static const UInt64 kIV_Ascon = 0x00001000808C0001ULL;

#ifdef _MSC_VER
#include <stdlib.h>
#define ASCON_ROR64(v, n) _rotr64((v), (n))
#else
#define ASCON_ROR64(v, n) (((v) >> (n)) | ((v) << (64 - (n))))
#endif

void AsconP12(UInt64 state[5]);
void AsconP8(UInt64 state[5]);

}}
#endif
