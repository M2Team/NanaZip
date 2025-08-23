// (C) 2017 Tino Reichardt

#include "../../SevenZip/CPP/7zip/Compress/StdAfx.h"

#include "../../SevenZip/CPP/7zip/Common/RegisterCodec.h"

#include "LizardDecoder.h"

#ifndef Z7_EXTRACT_ONLY
#include "LizardEncoder.h"
#endif

REGISTER_CODEC_E(
  LIZARD,
  NCompress::NLIZARD::CDecoder(),
  NCompress::NLIZARD::CEncoder(),
  0x4F71106, "LIZARD")
