// NsisRegister.cpp

#include "StdAfx.h"

#include "../../Common/RegisterArc.h"

#include "NsisHandler.h"

namespace NArchive {
namespace NNsis {

REGISTER_ARC_I(
  "Nsis", "nsis", NULL, 0x9,
  kSignature,
  4,
  NArcInfoFlags::kFindSignature |
  NArcInfoFlags::kUseGlobalOffset,
  NULL)

}}
