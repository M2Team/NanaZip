// (C) 2017 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"

#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include "BrotliDecoder.h"

#ifndef Z7_EXTRACT_ONLY
#include "BrotliEncoder.h"
#endif

REGISTER_CODEC_E(
  BROTLI,
  NCompress::NBROTLI::CDecoder(),
  NCompress::NBROTLI::CEncoder(),
  0x4F71102, "BROTLI")
