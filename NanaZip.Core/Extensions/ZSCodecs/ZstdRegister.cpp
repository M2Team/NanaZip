// (C) 2016 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"

#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include "../../SevenZip/CPP/7zip/Compress/ZstdDecoder.h"

#ifndef Z7_EXTRACT_ONLY
#include "ZstdEncoder.h"
#endif

REGISTER_CODEC_E(
  Zstd,
  NCompress::NZstd::CDecoder(),
  NCompress::NZSTD::CEncoder(),
  0x4F71101,
  "Zstd")
