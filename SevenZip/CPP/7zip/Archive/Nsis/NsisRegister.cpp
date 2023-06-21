// NsisRegister.cpp

#include "StdAfx.h"

#include "../../../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterArc.h"

#include "NsisHandler.h"

namespace NArchive {
namespace NNsis {

REGISTER_ARC_I(
  "Nsis", "nsis", 0, 0x9,
  kSignature,
  4,
  NArcInfoFlags::kFindSignature |
  NArcInfoFlags::kUseGlobalOffset,
  NULL)

}}
