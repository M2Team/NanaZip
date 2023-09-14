// CabRegister.cpp

#include "StdAfx.h"

#include "../../Common/RegisterArc.h"

#include "CabHandler.h"

namespace NArchive {
namespace NCab {

REGISTER_ARC_I(
  "Cab", "cab", NULL, 8,
  NHeader::kMarker,
  0,
  NArcInfoFlags::kFindSignature,
  NULL)

}}
