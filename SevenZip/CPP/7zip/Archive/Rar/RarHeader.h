// Archive/RarHeader.h

#ifndef __ARCHIVE_RAR_HEADER_H
#define __ARCHIVE_RAR_HEADER_H

#include "../../../Common/MyTypes.h"

namespace NArchive {
namespace NRar {
namespace NHeader {

const unsigned kMarkerSize = 7;
  
const unsigned kArchiveSolid = 0x1;

namespace NBlockType
{
  enum EBlockType
  {
    kMarker = 0x72,
    kArchiveHeader,
    kFileHeader,
    kCommentHeader,
    kOldAuthenticity,
    kOldSubBlock,
    kRecoveryRecord,
    kAuthenticity,
    kSubBlock,
    kEndOfArchive
  };
}

namespace NArchive
{
  const UInt16 kVolume  = 1;
  const UInt16 kComment = 2;
  const UInt16 kLock    = 4;
  const UInt16 kSolid   = 8;
  const UInt16 kNewVolName = 0x10; // ('volname.partN.rar')
  const UInt16 kAuthenticity  = 0x20;
  const UInt16 kRecovery = 0x40;
  const UInt16 kBlockEncryption  = 0x80;
  const UInt16 kFirstVolume = 0x100; // (set only by RAR 3.0 and later)

  // const UInt16 kEncryptVer = 0x200; // RAR 3.6 : that feature was discarded by origial RAR

  const UInt16 kEndOfArc_Flags_NextVol   = 1;
  const UInt16 kEndOfArc_Flags_DataCRC   = 2;
  const UInt16 kEndOfArc_Flags_RevSpace  = 4;
  const UInt16 kEndOfArc_Flags_VolNumber = 8;

  const unsigned kHeaderSizeMin = 7;
  
  const unsigned kArchiveHeaderSize = 13;

  const unsigned kBlockHeadersAreEncrypted = 0x80;
}

namespace NFile
{
  const unsigned kSplitBefore = 1 << 0;
  const unsigned kSplitAfter  = 1 << 1;
  const unsigned kEncrypted   = 1 << 2;
  const unsigned kComment     = 1 << 3;
  const unsigned kSolid       = 1 << 4;
  
  const unsigned kDictBitStart     = 5;
  const unsigned kNumDictBits  = 3;
  const unsigned kDictMask         = (1 << kNumDictBits) - 1;
  const unsigned kDictDirectoryValue  = 0x7;
  
  const unsigned kSize64Bits    = 1 << 8;
  const unsigned kUnicodeName   = 1 << 9;
  const unsigned kSalt          = 1 << 10;
  const unsigned kOldVersion    = 1 << 11;
  const unsigned kExtTime       = 1 << 12;
  // const unsigned kExtFlags      = 1 << 13;
  // const unsigned kSkipIfUnknown = 1 << 14;

  const unsigned kLongBlock    = 1 << 15;
  
  /*
  struct CBlock
  {
    // UInt16 HeadCRC;
    // Byte Type;
    // UInt16 Flags;
    // UInt16 HeadSize;
    UInt32 PackSize;
    UInt32 UnPackSize;
    Byte HostOS;
    UInt32 FileCRC;
    UInt32 Time;
    Byte UnPackVersion;
    Byte Method;
    UInt16 NameSize;
    UInt32 Attributes;
  };
  */

  /*
  struct CBlock32
  {
    UInt16 HeadCRC;
    Byte Type;
    UInt16 Flags;
    UInt16 HeadSize;
    UInt32 PackSize;
    UInt32 UnPackSize;
    Byte HostOS;
    UInt32 FileCRC;
    UInt32 Time;
    Byte UnPackVersion;
    Byte Method;
    UInt16 NameSize;
    UInt32 Attributes;
    UInt16 GetRealCRC(const void *aName, UInt32 aNameSize,
        bool anExtraDataDefined = false, Byte *anExtraData = 0) const;
  };
  struct CBlock64
  {
    UInt16 HeadCRC;
    Byte Type;
    UInt16 Flags;
    UInt16 HeadSize;
    UInt32 PackSizeLow;
    UInt32 UnPackSizeLow;
    Byte HostOS;
    UInt32 FileCRC;
    UInt32 Time;
    Byte UnPackVersion;
    Byte Method;
    UInt16 NameSize;
    UInt32 Attributes;
    UInt32 PackSizeHigh;
    UInt32 UnPackSizeHigh;
    UInt16 GetRealCRC(const void *aName, UInt32 aNameSize) const;
  };
  */
  
  const unsigned kLabelFileAttribute            = 0x08;
  const unsigned kWinFileDirectoryAttributeMask = 0x10;
  
  enum CHostOS
  {
    kHostMSDOS = 0,
    kHostOS2   = 1,
    kHostWin32 = 2,
    kHostUnix  = 3,
    kHostMacOS = 4,
    kHostBeOS  = 5
  };
}

namespace NBlock
{
  const UInt16 kLongBlock = 1 << 15;
  struct CBlock
  {
    UInt16 CRC;
    Byte Type;
    UInt16 Flags;
    UInt16 HeadSize;
    //  UInt32 DataSize;
  };
}

/*
struct CSubBlock
{
  UInt16 HeadCRC;
  Byte HeadType;
  UInt16 Flags;
  UInt16 HeadSize;
  UInt32 DataSize;
  UInt16 SubType;
  Byte Level; // Reserved : Must be 0
};

struct CCommentBlock
{
  UInt16 HeadCRC;
  Byte HeadType;
  UInt16 Flags;
  UInt16 HeadSize;
  UInt16 UnpSize;
  Byte UnpVer;
  Byte Method;
  UInt16 CommCRC;
};


struct CProtectHeader
{
  UInt16 HeadCRC;
  Byte HeadType;
  UInt16 Flags;
  UInt16 HeadSize;
  UInt32 DataSize;
  Byte Version;
  UInt16 RecSectors;
  UInt32 TotalBlocks;
  Byte Mark[8];
};
*/

}}}

#endif
