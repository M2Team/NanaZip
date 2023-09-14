// Archive/IsoHeader.h

#ifndef ZIP7_INC_ARCHIVE_ISO_HEADER_H
#define ZIP7_INC_ARCHIVE_ISO_HEADER_H

#include "../../../Common/MyTypes.h"

namespace NArchive {
namespace NIso {

namespace NVolDescType
{
  const Byte kBootRecord = 0;
  const Byte kPrimaryVol = 1;
  const Byte kSupplementaryVol = 2;
  const Byte kVolParttition = 3;
  const Byte kTerminator = 255;
}

const Byte kVersion = 1;

namespace NFileFlags
{
  const Byte kDirectory = 1 << 1;
  const Byte kNonFinalExtent = 1 << 7;
}

extern const char * const kElToritoSpec;

const UInt32 kStartPos = 0x8000;

namespace NBootEntryId
{
  const Byte kValidationEntry = 1;
  const Byte kInitialEntryNotBootable = 0;
  const Byte kInitialEntryBootable = 0x88;

  const Byte kMoreHeaders = 0x90;
  const Byte kFinalHeader = 0x91;
  
  const Byte kExtensionIndicator = 0x44;
}

namespace NBootPlatformId
{
  const Byte kX86 = 0;
  const Byte kPowerPC = 1;
  const Byte kMac = 2;
}

const Byte kBootMediaTypeMask = 0xF;

namespace NBootMediaType
{
  const Byte kNoEmulation = 0;
  const Byte k1d2Floppy = 1;
  const Byte k1d44Floppy = 2;
  const Byte k2d88Floppy = 3;
  const Byte kHardDisk = 4;
}

}}

#endif
