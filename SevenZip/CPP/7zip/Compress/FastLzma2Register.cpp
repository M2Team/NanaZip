// FastLzma2Register.cpp

#include "../../../../ThirdParty/LZMA/CPP/7zip/Compress/StdAfx.h"

#include "../../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "../../../../ThirdParty/LZMA/CPP/7zip/Compress/Lzma2Decoder.h"

#ifndef EXTRACT_ONLY
#include "../../../../ThirdParty/LZMA/CPP/7zip/Compress/Lzma2Encoder.h"
#endif

REGISTER_CODEC_E(
  FLZMA2,
  NCompress::NLzma2::CDecoder(),
  NCompress::NLzma2::CFastEncoder(),
  0x21,
  "FLZMA2")
