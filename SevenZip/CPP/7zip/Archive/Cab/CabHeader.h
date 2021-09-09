// Archive/CabHeader.h

#ifndef __ARCHIVE_CAB_HEADER_H
#define __ARCHIVE_CAB_HEADER_H

#include "../../../Common/MyTypes.h"

namespace NArchive {
namespace NCab {
namespace NHeader {

const unsigned kMarkerSize = 8;
extern const Byte kMarker[kMarkerSize];

namespace NArcFlags
{
  const unsigned kPrevCabinet = 1;
  const unsigned kNextCabinet = 2;
  const unsigned kReservePresent = 4;
}

namespace NMethod
{
  const Byte kNone = 0;
  const Byte kMSZip = 1;
  const Byte kQuantum = 2;
  const Byte kLZX = 3;
}

const unsigned kFileNameIsUtf8_Mask = 0x80;

namespace NFolderIndex
{
  const unsigned kContinuedFromPrev    = 0xFFFD;
  const unsigned kContinuedToNext      = 0xFFFE;
  const unsigned kContinuedPrevAndNext = 0xFFFF;
}

}}}

#endif
