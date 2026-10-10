#include "../../Common/RegisterArc.h"
#include "Header.h"
#include "StdAfx.h"
#include "PyInstallerRegister.h"

namespace NArchive {
namespace NPyin {

REGISTER_ARC_I(
  "exe", "exe", NULL, 0xAA,
  kSignature,
  0,
  NArcInfoFlags::kFindSignature |
  NArcInfoFlags::kUseGlobalOffset,
  NULL)

}
}
