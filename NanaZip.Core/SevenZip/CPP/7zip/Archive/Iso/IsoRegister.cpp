// IsoRegister.cpp

#include "StdAfx.h"

#include "../../Common/RegisterArc.h"

#include "IsoHandler.h"

namespace NArchive {
namespace NIso {

static const Byte k_Signature[] = { 'C', 'D', '0', '0', '1' };

REGISTER_ARC_I(
  "Iso", "iso img", NULL, 0xE7,
  k_Signature,
  NArchive::NIso::kStartPos + 1,
  0,
  NULL)

}}
