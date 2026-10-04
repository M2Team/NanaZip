// CascadeRegister.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "Cascade.h"

namespace NCrypto {
namespace NAXPCascade {

REGISTER_FILTER_E(AXP,
    CAXPDecoder,
    CAXPEncoder,
    0x6F10704, "AES+XChaCha20-Poly1305")

}}

namespace NCrypto {
namespace NAXACascade {

REGISTER_FILTER_E(AXA,
    CDecoder,
    CEncoder,
    0x6F10705, "AES+XChaCha20+Ascon")

}}

namespace NCrypto {
namespace NAXECascade {

REGISTER_FILTER_E(XAE,
    CDecoder,
    CEncoder,
    0x6F10706, "XChaCha20+AES+AEGIS")

}}
