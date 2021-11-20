// FastLzma2Register.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "Lzma2Decoder.h"

#ifndef EXTRACT_ONLY
#include "Lzma2Encoder.h"
#endif

REGISTER_CODEC_E(
  FLZMA2,
  NCompress::NLzma2::CDecoder(),
  NCompress::NLzma2::CFastEncoder(),
  0x21,
  "FLZMA2")
