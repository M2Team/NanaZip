// Archive/TarHeader.cpp

#include "StdAfx.h"

#include "TarHeader.h"

namespace NArchive {
namespace NTar {
namespace NFileHeader {

  const char * const kLongLink = "././@LongLink";
  const char * const kLongLink2 = "@LongLink";

  // The magic field is filled with this if uname and gname are valid.
  namespace NMagic
  {
    // const char * const kUsTar  = "ustar";   // 5 chars
    // const char * const kGNUTar = "GNUtar "; // 7 chars and a null
    // const char * const kEmpty = "\0\0\0\0\0\0\0\0";
    // 7-Zip used kUsTar_00 before 21.07:
    const char k_Posix_ustar_00[8]  = { 'u', 's', 't', 'a', 'r', 0, '0', '0' } ;
    // GNU TAR uses such header:
    const char k_GNU_ustar[8] = { 'u', 's', 't', 'a', 'r', ' ', ' ', 0 } ;
  }

/*
pre-POSIX.1-1988 (i.e. v7) tar header:
-----
Link indicator:
'0' or 0 : Normal file
'1' : Hard link
'2' : Symbolic link
Some pre-POSIX.1-1988 tar implementations indicated a directory by having
a trailing slash (/) in the name.

Numeric values : octal with leading zeroes.
For historical reasons, a final NUL or space character should also be used.
Thus only 11 octal digits can be stored from 12 bytes field.

2001 star : introduced a base-256 coding that is indicated by
setting the high-order bit of the leftmost byte of a numeric field.
GNU-tar and BSD-tar followed this idea.
 
versions of tar from before the first POSIX standard from 1988
pad the values with spaces instead of zeroes.

UStar
-----
UStar (Unix Standard TAR) : POSIX IEEE P1003.1 : 1988.
    257 signature: "ustar", 0, "00"
    265 32  Owner user name
    297 32  Owner group name
    329 8 Device major number
    337 8 Device minor number
    345 155 Filename prefix

POSIX.1-2001/pax
----
format is known as extended tar format or pax format
vendor-tagged vendor-specific enhancements.
tags Defined by the POSIX standard:
  atime, mtime, path, linkpath, uname, gname, size, uid, gid, ...


PAX EXTENSION
-----------
Hard links
A further difference from the ustar header block is that data blocks
for files of typeflag 1 (hard link) may be included,
which means that the size field may be greater than zero.
Archives created by pax -o linkdata shall include these data
blocks with the hard links.
*

compatiblity
------------
  7-Zip 16.03 supports "PaxHeader/"
  7-Zip 20.01 supports "PaxHeaders.X/" with optional "./"
  7-Zip 21.02 supports "@PaxHeader" with optional "./" "./"

  GNU tar --format=posix uses "PaxHeaders/" in folder of file
  

GNU TAR format
==============
v7      - Unix V7
oldgnu  - GNU tar <=1.12  : writes zero in last character in name
gnu     - GNU tar 1.13    : doesn't write  zero in last character in name
                            as 7-zip 21.07
ustar       - POSIX.1-1988
posix (pax) - POSIX.1-2001

  gnu tar:
  if (S_ISCHR (st->stat.st_mode) || S_ISBLK (st->stat.st_mode)) {
      major_t devmajor = major (st->stat.st_rdev);
      minor_t devminor = minor (st->stat.st_rdev); }
*/

}}}
