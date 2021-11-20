// (C) 2016 Tino Reichardt

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "Lz4Decoder.h"

#ifndef EXTRACT_ONLY
#include "Lz4Encoder.h"
#endif

REGISTER_CODEC_E(
  LZ4,
  NCompress::NLZ4::CDecoder(),
  NCompress::NLZ4::CEncoder(),
  0x4F71104, "LZ4")
