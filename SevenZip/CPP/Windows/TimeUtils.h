// Windows/TimeUtils.h

#ifndef __WINDOWS_TIME_UTILS_H
#define __WINDOWS_TIME_UTILS_H

#include "../Common/MyTypes.h"
#include "../Common/MyWindows.h"

namespace NWindows {
namespace NTime {

bool DosTimeToFileTime(UInt32 dosTime, FILETIME &fileTime) throw();
bool FileTimeToDosTime(const FILETIME &fileTime, UInt32 &dosTime) throw();

// UInt32 Unix Time : for dates 1970-2106
UInt64 UnixTimeToFileTime64(UInt32 unixTime) throw();
void UnixTimeToFileTime(UInt32 unixTime, FILETIME &fileTime) throw();

// Int64 Unix Time : negative values for dates before 1970
UInt64 UnixTime64ToFileTime64(Int64 unixTime) throw();
bool UnixTime64ToFileTime(Int64 unixTime, FILETIME &fileTime) throw();

bool FileTimeToUnixTime(const FILETIME &fileTime, UInt32 &unixTime) throw();
Int64 FileTimeToUnixTime64(const FILETIME &ft) throw();

bool GetSecondsSince1601(unsigned year, unsigned month, unsigned day,
  unsigned hour, unsigned min, unsigned sec, UInt64 &resSeconds) throw();
void GetCurUtcFileTime(FILETIME &ft) throw();

}}

#endif
