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
    // const char kUsTar_00[8] = { 'u', 's', 't', 'a', 'r', 0, '0', '0' } ;
    // GNU TAR uses such header:
    const char kUsTar_GNU[8] = { 'u', 's', 't', 'a', 'r', ' ', ' ', 0 } ;
  }

}}}
