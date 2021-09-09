// Windows/FileDir.h

#ifndef __WINDOWS_FILE_DIR_H
#define __WINDOWS_FILE_DIR_H

#include "../Common/MyString.h"

#include "FileIO.h"

namespace NWindows {
namespace NFile {
namespace NDir {

bool GetWindowsDir(FString &path);
bool GetSystemDir(FString &path);

bool SetDirTime(CFSTR path, const FILETIME *cTime, const FILETIME *aTime, const FILETIME *mTime);


#ifdef _WIN32

bool SetFileAttrib(CFSTR path, DWORD attrib);

/*
  Some programs store posix attributes in high 16 bits of windows attributes field.
  Also some programs use additional flag markers: 0x8000 or 0x4000.
  SetFileAttrib_PosixHighDetect() tries to detect posix field, and it extracts only attribute
  bits that are related to current system only.
*/

#endif

bool SetFileAttrib_PosixHighDetect(CFSTR path, DWORD attrib);


bool MyMoveFile(CFSTR existFileName, CFSTR newFileName);

#ifndef UNDER_CE
bool MyCreateHardLink(CFSTR newFileName, CFSTR existFileName);
#endif

bool RemoveDir(CFSTR path);
bool CreateDir(CFSTR path);

/* CreateComplexDir returns true, if directory can contain files after the call (two cases):
    1) the directory already exists (network shares and drive paths are supported)
    2) the directory was created
  path can be WITH or WITHOUT trailing path separator. */

bool CreateComplexDir(CFSTR path);

bool DeleteFileAlways(CFSTR name);
bool RemoveDirWithSubItems(const FString &path);

bool MyGetFullPathName(CFSTR path, FString &resFullPath);
bool GetFullPathAndSplit(CFSTR path, FString &resDirPrefix, FString &resFileName);
bool GetOnlyDirPrefix(CFSTR path, FString &resDirPrefix);

#ifndef UNDER_CE

bool SetCurrentDir(CFSTR path);
bool GetCurrentDir(FString &resultPath);

#endif

bool MyGetTempPath(FString &resultPath);

class CTempFile  MY_UNCOPYABLE
{
  bool _mustBeDeleted;
  FString _path;
  void DisableDeleting() { _mustBeDeleted = false; }
public:
  CTempFile(): _mustBeDeleted(false) {}
  ~CTempFile() { Remove(); }
  const FString &GetPath() const { return _path; }
  bool Create(CFSTR pathPrefix, NIO::COutFile *outFile); // pathPrefix is not folder prefix
  bool CreateRandomInTempFolder(CFSTR namePrefix, NIO::COutFile *outFile);
  bool Remove();
  bool MoveTo(CFSTR name, bool deleteDestBefore);
};


#ifdef _WIN32
class CTempDir  MY_UNCOPYABLE
{
  bool _mustBeDeleted;
  FString _path;
public:
  CTempDir(): _mustBeDeleted(false) {}
  ~CTempDir() { Remove();  }
  const FString &GetPath() const { return _path; }
  void DisableDeleting() { _mustBeDeleted = false; }
  bool Create(CFSTR namePrefix) ;
  bool Remove();
};
#endif


#if !defined(UNDER_CE)
class CCurrentDirRestorer  MY_UNCOPYABLE
{
  FString _path;
public:
  bool NeedRestore;

  CCurrentDirRestorer(): NeedRestore(true)
  {
    GetCurrentDir(_path);
  }
  ~CCurrentDirRestorer()
  {
    if (!NeedRestore)
      return;
    FString s;
    if (GetCurrentDir(s))
      if (s != _path)
        SetCurrentDir(_path);
  }
};
#endif

}}}

#endif
