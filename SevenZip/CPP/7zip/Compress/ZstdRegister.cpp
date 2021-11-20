// (C) 2016 Tino Reichardt

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "ZstdDecoder.h"

#ifndef EXTRACT_ONLY
#include "ZstdEncoder.h"
#endif

REGISTER_CODEC_E(
  ZSTD,
  NCompress::NZSTD::CDecoder(),
  NCompress::NZSTD::CEncoder(),
  0x4F71101, "ZSTD")
