// XChaCha20Poly1305Register.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "XChaCha20Poly1305.h"

namespace NCrypto {
namespace NXChaCha20Poly1305 {

REGISTER_FILTER_E(XChaCha20Poly1305,
    CDecoder,
    CEncoder,
    0x6F10703, "XChaCha20-Poly1305")

}}
