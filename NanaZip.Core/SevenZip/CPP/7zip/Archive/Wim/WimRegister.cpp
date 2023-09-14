// WimRegister.cpp

#include "StdAfx.h"

#include "../../Common/RegisterArc.h"

#include "WimHandler.h"

namespace NArchive {
namespace NWim {

REGISTER_ARC_IO(
    "wim", "wim swm esd ppkg", NULL, 0xE6
  , kSignature, 0
  , NArcInfoFlags::kAltStreams
  | NArcInfoFlags::kNtSecure
  | NArcInfoFlags::kSymLinks
  | NArcInfoFlags::kHardLinks
  | NArcInfoFlags::kCTime
  // | NArcInfoFlags::kCTime_Default
  | NArcInfoFlags::kATime
  // | NArcInfoFlags::kATime_Default
  | NArcInfoFlags::kMTime
  | NArcInfoFlags::kMTime_Default
  , TIME_PREC_TO_ARC_FLAGS_MASK (NFileTimeType::kWindows)
  | TIME_PREC_TO_ARC_FLAGS_TIME_DEFAULT (NFileTimeType::kWindows)
  , NULL)

}}
