// FastLzma2Register.cpp

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"

#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include "../../SevenZip/CPP/7zip/Compress/Lzma2Decoder.h"

#ifndef Z7_EXTRACT_ONLY
#include "FastLzma2Encoder.h"
#endif

REGISTER_CODEC_E(
  FLZMA2,
  NCompress::NLzma2::CDecoder(),
  NCompress::NLzma2::CFastEncoder(),
  0x21,
  "FLZMA2")
