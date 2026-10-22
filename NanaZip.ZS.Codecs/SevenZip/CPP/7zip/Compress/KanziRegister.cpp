// KanziRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "KanziDecoder.h"

#ifndef Z7_EXTRACT_ONLY
#include "KanziEncoder.h"
#endif

REGISTER_CODEC_E(
  KANZI,
  NCompress::NKANZI::CDecoder(),
  NCompress::NKANZI::CEncoder(),
  0x4F71107, "KANZI")
