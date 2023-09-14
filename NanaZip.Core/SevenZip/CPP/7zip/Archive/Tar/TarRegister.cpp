// TarRegister.cpp

#include "StdAfx.h"

#include "../../Common/RegisterArc.h"

#include "TarHandler.h"

namespace NArchive {
namespace NTar {

static const Byte k_Signature[] = { 'u', 's', 't', 'a', 'r' };

REGISTER_ARC_IO(
  "tar", "tar ova", NULL, 0xEE,
  k_Signature,
  NFileHeader::kUstarMagic_Offset,
    NArcInfoFlags::kStartOpen
  | NArcInfoFlags::kSymLinks
  | NArcInfoFlags::kHardLinks
  | NArcInfoFlags::kMTime
  | NArcInfoFlags::kMTime_Default
  // | NArcInfoTimeFlags::kCTime
  // | NArcInfoTimeFlags::kATime
  , TIME_PREC_TO_ARC_FLAGS_MASK (NFileTimeType::kWindows)
  | TIME_PREC_TO_ARC_FLAGS_MASK (NFileTimeType::kUnix)
  | TIME_PREC_TO_ARC_FLAGS_MASK (NFileTimeType::k1ns)
  | TIME_PREC_TO_ARC_FLAGS_TIME_DEFAULT (NFileTimeType::kUnix)
  , IsArc_Tar)
 
}}
