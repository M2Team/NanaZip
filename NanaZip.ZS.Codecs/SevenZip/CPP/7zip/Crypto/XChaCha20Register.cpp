// XChaCha20Register.cpp
// Copyright (C) fzxx   Contributor: https://github.com/fzxx
// License: GNU LGPL v2.1+

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "XChaCha20.h"

namespace NCrypto {
namespace NXChaCha20 {

REGISTER_FILTER_E(XChaCha20,
    CDecoder,
    CEncoder,
    0x6F10702, "XChaCha20")

}}
