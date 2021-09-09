// Windows/FileSystem.h

#ifndef __WINDOWS_FILE_SYSTEM_H
#define __WINDOWS_FILE_SYSTEM_H

#include "../Common/MyString.h"
#include "../Common/MyTypes.h"

namespace NWindows {
namespace NFile {
namespace NSystem {

#ifdef _WIN32

bool MyGetVolumeInformation(
    CFSTR rootPath  ,
    UString &volumeName,
    LPDWORD volumeSerialNumber,
    LPDWORD maximumComponentLength,
    LPDWORD fileSystemFlags,
    UString &fileSystemName);

UINT MyGetDriveType(CFSTR pathName);

bool MyGetDiskFreeSpace(CFSTR rootPath, UInt64 &clusterSize, UInt64 &totalSize, UInt64 &freeSize);

#endif

}}}

#endif
