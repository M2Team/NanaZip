// (C) 2017 Tino Reichardt

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "LizardDecoder.h"

#ifndef EXTRACT_ONLY
#include "LizardEncoder.h"
#endif

REGISTER_CODEC_E(
  LIZARD,
  NCompress::NLIZARD::CDecoder(),
  NCompress::NLIZARD::CEncoder(),
  0x4F71106, "LIZARD")
