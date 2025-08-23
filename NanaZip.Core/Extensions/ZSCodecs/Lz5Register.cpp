// (C) 2016 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"

#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include "Lz5Decoder.h"

#ifndef Z7_EXTRACT_ONLY
#include "Lz5Encoder.h"
#endif

REGISTER_CODEC_E(
  LZ5,
  NCompress::NLZ5::CDecoder(),
  NCompress::NLZ5::CEncoder(),
  0x4F71105, "LZ5")
