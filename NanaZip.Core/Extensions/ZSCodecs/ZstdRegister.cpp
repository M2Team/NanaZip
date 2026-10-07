// (C) 2016 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"

#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include "ZstdDecoder.h"

// **************** NanaZip Modification Start ****************
// NanaZip uses our own ZS-compatible ZstdEncoder implementation.
#if 0 // ******** Annotated 7-Zip ZS Source Code snippet Start ********
#ifndef Z7_EXTRACT_ONLY
#include "ZstdEncoder.h"
#endif

REGISTER_CODEC_E(
  ZSTD,
  NCompress::NZSTD::CDecoder(),
  NCompress::NZSTD::CEncoder(),
  0x4F71101, "ZSTD")
#endif // ******** Annotated 7-Zip ZS Source Code snippet End ********
#ifndef Z7_EXTRACT_ONLY
#include "ZstdEncoder.hpp"
#endif

REGISTER_CODEC_E(
    ZSTD,
    NCompress::NZSTD::CDecoder(),
    NanaZip::Core::Extensions::ZstdEncoder(),
    0x4F71101, "ZSTD")
// **************** NanaZip Modification End ****************
