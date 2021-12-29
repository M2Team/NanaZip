// Archive/TarHeader.h

#ifndef __ARCHIVE_TAR_HEADER_H
#define __ARCHIVE_TAR_HEADER_H

#include "../../../Common/MyTypes.h"

namespace NArchive {
namespace NTar {

namespace NFileHeader
{
  const unsigned kRecordSize = 512;
  const unsigned kNameSize = 100;
  const unsigned kUserNameSize = 32;
  const unsigned kGroupNameSize = 32;
  const unsigned kPrefixSize = 155;

  const unsigned kUstarMagic_Offset = 257;

  /*
  struct CHeader
  {
    char Name[kNameSize];
    char Mode[8];
    char UID[8];
    char GID[8];
    char Size[12];
    char ModificationTime[12];
    char CheckSum[8];
    char LinkFlag;
    char LinkName[kNameSize];
    char Magic[8];
    char UserName[kUserNameSize];
    char GroupName[kGroupNameSize];
    char DeviceMajor[8];
    char DeviceMinor[8];
    char Prefix[155];
  };
  union CRecord
  {
    CHeader Header;
    Byte Padding[kRecordSize];
  };
  */

  namespace NLinkFlag
  {
    const char kOldNormal    = 0;   // Normal disk file, Unix compatible
    const char kNormal       = '0'; // Normal disk file
    const char kHardLink     = '1'; // Link to previously dumped file
    const char kSymLink      = '2'; // Symbolic link
    const char kCharacter    = '3'; // Character special file
    const char kBlock        = '4'; // Block special file
    const char kDirectory    = '5'; // Directory
    const char kFIFO         = '6'; // FIFO special file
    const char kContiguous   = '7'; // Contiguous file
    const char kGnu_LongLink = 'K';
    const char kGnu_LongName = 'L';
    const char kSparse       = 'S';
    const char kLabel        = 'V';
    const char kDumpDir      = 'D'; /* GNUTYPE_DUMPDIR.
      data: list of files created by the --incremental (-G) option
      Each file name is preceded by either
        - 'Y' (file should be in this archive)
        - 'N' (file is a directory, or is not stored in the archive.)
        Each file name is terminated by a null + an additional null after
        the last file name. */
  }

  extern const char * const kLongLink;  //   = "././@LongLink";
  extern const char * const kLongLink2; //   = "@LongLink";

  namespace NMagic
  {
    // extern const char * const kUsTar;  //  = "ustar"; // 5 chars
    // extern const char * const kGNUTar; //  = "GNUtar "; // 7 chars and a null
    // extern const char * const kEmpty;  //  = "\0\0\0\0\0\0\0\0"
    // extern const char kUsTar_00[8];
    extern const char kUsTar_GNU[8];
  }
}

}}

#endif
