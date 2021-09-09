// Windows/FileSystem.cpp

#include "StdAfx.h"

#ifndef UNDER_CE

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "FileSystem.h"
#include "Defs.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NFile {
namespace NSystem {

#ifdef _WIN32

bool MyGetVolumeInformation(
    CFSTR rootPath,
    UString &volumeName,
    LPDWORD volumeSerialNumber,
    LPDWORD maximumComponentLength,
    LPDWORD fileSystemFlags,
    UString &fileSystemName)
{
  BOOL res;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR v[MAX_PATH + 2]; v[0] = 0;
    TCHAR f[MAX_PATH + 2]; f[0] = 0;
    res = GetVolumeInformation(fs2fas(rootPath),
        v, MAX_PATH,
        volumeSerialNumber, maximumComponentLength, fileSystemFlags,
        f, MAX_PATH);
    volumeName = MultiByteToUnicodeString(v);
    fileSystemName = MultiByteToUnicodeString(f);
  }
  else
  #endif
  {
    WCHAR v[MAX_PATH + 2]; v[0] = 0;
    WCHAR f[MAX_PATH + 2]; f[0] = 0;
    res = GetVolumeInformationW(fs2us(rootPath),
        v, MAX_PATH,
        volumeSerialNumber, maximumComponentLength, fileSystemFlags,
        f, MAX_PATH);
    volumeName = v;
    fileSystemName = f;
  }
  return BOOLToBool(res);
}

UINT MyGetDriveType(CFSTR pathName)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    return GetDriveType(fs2fas(pathName));
  }
  else
  #endif
  {
    return GetDriveTypeW(fs2us(pathName));
  }
}

typedef BOOL (WINAPI * GetDiskFreeSpaceExA_Pointer)(
  LPCSTR lpDirectoryName,                  // directory name
  PULARGE_INTEGER lpFreeBytesAvailable,    // bytes available to caller
  PULARGE_INTEGER lpTotalNumberOfBytes,    // bytes on disk
  PULARGE_INTEGER lpTotalNumberOfFreeBytes // free bytes on disk
);

typedef BOOL (WINAPI * GetDiskFreeSpaceExW_Pointer)(
  LPCWSTR lpDirectoryName,                 // directory name
  PULARGE_INTEGER lpFreeBytesAvailable,    // bytes available to caller
  PULARGE_INTEGER lpTotalNumberOfBytes,    // bytes on disk
  PULARGE_INTEGER lpTotalNumberOfFreeBytes // free bytes on disk
);

bool MyGetDiskFreeSpace(CFSTR rootPath, UInt64 &clusterSize, UInt64 &totalSize, UInt64 &freeSize)
{
  DWORD numSectorsPerCluster, bytesPerSector, numFreeClusters, numClusters;
  bool sizeIsDetected = false;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    GetDiskFreeSpaceExA_Pointer pGetDiskFreeSpaceEx = (GetDiskFreeSpaceExA_Pointer)(void *)GetProcAddress(
        GetModuleHandle(TEXT("kernel32.dll")), "GetDiskFreeSpaceExA");
    if (pGetDiskFreeSpaceEx)
    {
      ULARGE_INTEGER freeBytesToCaller2, totalSize2, freeSize2;
      sizeIsDetected = BOOLToBool(pGetDiskFreeSpaceEx(fs2fas(rootPath), &freeBytesToCaller2, &totalSize2, &freeSize2));
      totalSize = totalSize2.QuadPart;
      freeSize = freeSize2.QuadPart;
    }
    if (!::GetDiskFreeSpace(fs2fas(rootPath), &numSectorsPerCluster, &bytesPerSector, &numFreeClusters, &numClusters))
      return false;
  }
  else
  #endif
  {
    GetDiskFreeSpaceExW_Pointer pGetDiskFreeSpaceEx = (GetDiskFreeSpaceExW_Pointer)(void *)GetProcAddress(
        GetModuleHandle(TEXT("kernel32.dll")), "GetDiskFreeSpaceExW");
    if (pGetDiskFreeSpaceEx)
    {
      ULARGE_INTEGER freeBytesToCaller2, totalSize2, freeSize2;
      sizeIsDetected = BOOLToBool(pGetDiskFreeSpaceEx(fs2us(rootPath), &freeBytesToCaller2, &totalSize2, &freeSize2));
      totalSize = totalSize2.QuadPart;
      freeSize = freeSize2.QuadPart;
    }
    if (!::GetDiskFreeSpaceW(fs2us(rootPath), &numSectorsPerCluster, &bytesPerSector, &numFreeClusters, &numClusters))
      return false;
  }
  clusterSize = (UInt64)bytesPerSector * (UInt64)numSectorsPerCluster;
  if (!sizeIsDetected)
  {
    totalSize = clusterSize * (UInt64)numClusters;
    freeSize = clusterSize * (UInt64)numFreeClusters;
  }
  return true;
}

#endif

}}}

#endif
