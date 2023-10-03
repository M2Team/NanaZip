﻿// Windows/FileDir.cpp

#include "StdAfx.h"


#ifndef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../Common/StringConvert.h"
#include "../Common/C_FileIO.h"
#endif

#include "FileDir.h"
#include "FileFind.h"
#include "FileName.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

using namespace NWindows;
using namespace NFile;
using namespace NName;

#ifndef _WIN32

static bool FiTime_To_timespec(const CFiTime *ft, timespec &ts)
{
  if (ft)
  {
    ts = *ft;
    return true;
  }
  // else
  {
    ts.tv_sec = 0;
    ts.tv_nsec =
    #ifdef UTIME_OMIT
      UTIME_OMIT; // -2 keep old timesptamp
    #else
      // UTIME_NOW; -1 // set to the current time
      0;
    #endif
    return false;
  }
}
#endif

namespace NWindows {
namespace NFile {
namespace NDir {

#ifdef _WIN32

#ifndef UNDER_CE

bool GetWindowsDir(FString &path)
{
  UINT needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetWindowsDirectory(s, MAX_PATH + 1);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetWindowsDirectoryW(s, MAX_PATH + 1);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}

bool GetSystemDir(FString &path)
{
  UINT needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetSystemDirectory(s, MAX_PATH + 1);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetSystemDirectoryW(s, MAX_PATH + 1);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}
#endif // UNDER_CE


bool SetDirTime(CFSTR path, const CFiTime *cTime, const CFiTime *aTime, const CFiTime *mTime)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
  }
  #endif

  HANDLE hDir = INVALID_HANDLE_VALUE;
  IF_USE_MAIN_PATH
    hDir = ::CreateFileW(fs2us(path), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  #ifdef WIN_LONG_PATH
  if (hDir == INVALID_HANDLE_VALUE && USE_SUPER_PATH)
  {
    UString superPath;
    if (GetSuperPath(path, superPath, USE_MAIN_PATH))
      hDir = ::CreateFileW(superPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
          NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  }
  #endif

  bool res = false;
  if (hDir != INVALID_HANDLE_VALUE)
  {
    res = BOOLToBool(::SetFileTime(hDir, cTime, aTime, mTime));
    ::CloseHandle(hDir);
  }
  return res;
}



bool SetFileAttrib(CFSTR path, DWORD attrib)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::SetFileAttributes(fs2fas(path), attrib))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::SetFileAttributesW(fs2us(path), attrib))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::SetFileAttributesW(superPath, attrib));
    }
    #endif
  }
  return false;
}


bool SetFileAttrib_PosixHighDetect(CFSTR path, DWORD attrib)
{
  #ifdef _WIN32
  if ((attrib & 0xF0000000) != 0)
    attrib &= 0x3FFF;
  #endif
  return SetFileAttrib(path, attrib);
}


bool RemoveDir(CFSTR path)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::RemoveDirectory(fs2fas(path)))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::RemoveDirectoryW(fs2us(path)))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::RemoveDirectoryW(superPath));
    }
    #endif
  }
  return false;
}


bool MyMoveFile(CFSTR oldFile, CFSTR newFile)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::MoveFile(fs2fas(oldFile), fs2fas(newFile)))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH_2(oldFile, newFile)
    {
      if (::MoveFileW(fs2us(oldFile), fs2us(newFile)))
        return true;
    }
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH_2)
    {
      UString d1, d2;
      if (GetSuperPaths(oldFile, newFile, d1, d2, USE_MAIN_PATH_2))
        return BOOLToBool(::MoveFileW(d1, d2));
    }
    #endif
  }
  return false;
}

#ifndef UNDER_CE
EXTERN_C_BEGIN
typedef BOOL (WINAPI *Func_CreateHardLinkW)(
    LPCWSTR lpFileName,
    LPCWSTR lpExistingFileName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes
    );
EXTERN_C_END
#endif // UNDER_CE

bool MyCreateHardLink(CFSTR newFileName, CFSTR existFileName)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
    /*
    if (::CreateHardLink(fs2fas(newFileName), fs2fas(existFileName), NULL))
      return true;
    */
  }
  else
  #endif
  {
    Func_CreateHardLinkW my_CreateHardLinkW = (Func_CreateHardLinkW)
        (void *)::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "CreateHardLinkW");
    if (!my_CreateHardLinkW)
      return false;
    IF_USE_MAIN_PATH_2(newFileName, existFileName)
    {
      if (my_CreateHardLinkW(fs2us(newFileName), fs2us(existFileName), NULL))
        return true;
    }
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH_2)
    {
      UString d1, d2;
      if (GetSuperPaths(newFileName, existFileName, d1, d2, USE_MAIN_PATH_2))
        return BOOLToBool(my_CreateHardLinkW(d1, d2, NULL));
    }
    #endif
  }
  return false;
}


/*
WinXP-64 CreateDir():
  ""                  - ERROR_PATH_NOT_FOUND
  \                   - ERROR_ACCESS_DENIED
  C:\                 - ERROR_ACCESS_DENIED, if there is such drive,

  D:\folder             - ERROR_PATH_NOT_FOUND, if there is no such drive,
  C:\nonExistent\folder - ERROR_PATH_NOT_FOUND

  C:\existFolder      - ERROR_ALREADY_EXISTS
  C:\existFolder\     - ERROR_ALREADY_EXISTS

  C:\folder   - OK
  C:\folder\  - OK

  \\Server\nonExistent    - ERROR_BAD_NETPATH
  \\Server\Share_Readonly - ERROR_ACCESS_DENIED
  \\Server\Share          - ERROR_ALREADY_EXISTS

  \\Server\Share_NTFS_drive - ERROR_ACCESS_DENIED
  \\Server\Share_FAT_drive  - ERROR_ALREADY_EXISTS
*/

bool CreateDir(CFSTR path)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::CreateDirectory(fs2fas(path), NULL))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::CreateDirectoryW(fs2us(path), NULL))
        return true;
    #ifdef WIN_LONG_PATH
    if ((!USE_MAIN_PATH || ::GetLastError() != ERROR_ALREADY_EXISTS) && USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::CreateDirectoryW(superPath, NULL));
    }
    #endif
  }
  return false;
}

/*
  CreateDir2 returns true, if directory can contain files after the call (two cases):
    1) the directory already exists
    2) the directory was created
  path must be WITHOUT trailing path separator.

  We need CreateDir2, since fileInfo.Find() for reserved names like "com8"
   returns FILE instead of DIRECTORY. And we need to use SuperPath */

static bool CreateDir2(CFSTR path)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::CreateDirectory(fs2fas(path), NULL))
      return true;
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH
      if (::CreateDirectoryW(fs2us(path), NULL))
        return true;
    #ifdef WIN_LONG_PATH
    if ((!USE_MAIN_PATH || ::GetLastError() != ERROR_ALREADY_EXISTS) && USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
      {
        if (::CreateDirectoryW(superPath, NULL))
          return true;
        if (::GetLastError() != ERROR_ALREADY_EXISTS)
          return false;
        NFind::CFileInfo fi;
        if (!fi.Find(us2fs(superPath)))
          return false;
        return fi.IsDir();
      }
    }
    #endif
  }
  if (::GetLastError() != ERROR_ALREADY_EXISTS)
    return false;
  NFind::CFileInfo fi;
  if (!fi.Find(path))
    return false;
  return fi.IsDir();
}

#endif // _WIN32

static bool CreateDir2(CFSTR path);

bool CreateComplexDir(CFSTR _path)
{
  #ifdef _WIN32

  {
    DWORD attrib = NFind::GetFileAttrib(_path);
    if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
      return true;
  }

  #ifndef UNDER_CE

  if (IsDriveRootPath_SuperAllowed(_path))
    return false;

  const unsigned prefixSize = GetRootPrefixSize(_path);

  #endif // UNDER_CE

  #else // _WIN32

  // Posix
  NFind::CFileInfo fi;
  if (fi.Find(_path))
  {
    if (fi.IsDir())
      return true;
  }

  #endif // _WIN32

  FString path (_path);

  int pos = path.ReverseFind_PathSepar();
  if (pos >= 0 && (unsigned)pos == path.Len() - 1)
  {
    if (path.Len() == 1)
      return true;
    path.DeleteBack();
  }

  const FString path2 (path);
  pos = (int)path.Len();

  for (;;)
  {
    if (CreateDir2(path))
      break;
    if (::GetLastError() == ERROR_ALREADY_EXISTS)
      return false;
    pos = path.ReverseFind_PathSepar();
    if (pos < 0 || pos == 0)
      return false;

    #if defined(_WIN32) && !defined(UNDER_CE)
    if (pos == 1 && IS_PATH_SEPAR(path[0]))
      return false;
    if (prefixSize >= (unsigned)pos + 1)
      return false;
    #endif

    path.DeleteFrom((unsigned)pos);
  }

  while (pos < (int)path2.Len())
  {
    int pos2 = NName::FindSepar(path2.Ptr((unsigned)pos + 1));
    if (pos2 < 0)
      pos = (int)path2.Len();
    else
      pos += 1 + pos2;
    path.SetFrom(path2, (unsigned)pos);
    if (!CreateDir(path))
      return false;
  }

  return true;
}


#ifdef _WIN32

bool DeleteFileAlways(CFSTR path)
{
  /* If alt stream, we also need to clear READ-ONLY attribute of main file before delete.
     SetFileAttrib("name:stream", ) changes attributes of main file. */
  {
    DWORD attrib = NFind::GetFileAttrib(path);
    if (attrib != INVALID_FILE_ATTRIBUTES
        && (attrib & FILE_ATTRIBUTE_DIRECTORY) == 0
        && (attrib & FILE_ATTRIBUTE_READONLY) != 0)
    {
      if (!SetFileAttrib(path, attrib & ~(DWORD)FILE_ATTRIBUTE_READONLY))
        return false;
    }
  }

  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (::DeleteFile(fs2fas(path)))
      return true;
  }
  else
  #endif
  {
    /* DeleteFile("name::$DATA") deletes all alt streams (same as delete DeleteFile("name")).
       Maybe it's better to open "name::$DATA" and clear data for unnamed stream? */
    IF_USE_MAIN_PATH
      if (::DeleteFileW(fs2us(path)))
        return true;
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH)
    {
      UString superPath;
      if (GetSuperPath(path, superPath, USE_MAIN_PATH))
        return BOOLToBool(::DeleteFileW(superPath));
    }
    #endif
  }
  return false;
}



bool RemoveDirWithSubItems(const FString &path)
{
  bool needRemoveSubItems = true;
  {
    NFind::CFileInfo fi;
    if (!fi.Find(path))
      return false;
    if (!fi.IsDir())
    {
      ::SetLastError(ERROR_DIRECTORY);
      return false;
    }
    if (fi.HasReparsePoint())
      needRemoveSubItems = false;
  }

  if (needRemoveSubItems)
  {
    FString s (path);
    s.Add_PathSepar();
    const unsigned prefixSize = s.Len();
    NFind::CEnumerator enumerator;
    enumerator.SetDirPrefix(s);
    NFind::CDirEntry fi;
    bool isError = false;
    DWORD lastError = 0;
    while (enumerator.Next(fi))
    {
      s.DeleteFrom(prefixSize);
      s += fi.Name;
      if (fi.IsDir())
      {
        if (!RemoveDirWithSubItems(s))
        {
          lastError = GetLastError();
          isError = true;
        }
      }
      else if (!DeleteFileAlways(s))
      {
        lastError = GetLastError();
        isError = false;
      }
    }
    if (isError)
    {
      SetLastError(lastError);
      return false;
    }
  }

  // we clear read-only attrib to remove read-only dir
  if (!SetFileAttrib(path, 0))
    return false;
  return RemoveDir(path);
}

#endif // _WIN32

#ifdef UNDER_CE

bool MyGetFullPathName(CFSTR path, FString &resFullPath)
{
  resFullPath = path;
  return true;
}

#else

bool MyGetFullPathName(CFSTR path, FString &resFullPath)
{
  return GetFullPath(path, resFullPath);
}

#ifdef _WIN32

bool SetCurrentDir(CFSTR path)
{
  // SetCurrentDirectory doesn't support \\?\ prefix
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    return BOOLToBool(::SetCurrentDirectory(fs2fas(path)));
  }
  else
  #endif
  {
    return BOOLToBool(::SetCurrentDirectoryW(fs2us(path)));
  }
}


bool GetCurrentDir(FString &path)
{
  path.Empty();

  DWORD needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetCurrentDirectory(MAX_PATH + 1, s);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetCurrentDirectoryW(MAX_PATH + 1, s);
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);
}

#endif // _WIN32
#endif // UNDER_CE


bool GetFullPathAndSplit(CFSTR path, FString &resDirPrefix, FString &resFileName)
{
  bool res = MyGetFullPathName(path, resDirPrefix);
  if (!res)
    resDirPrefix = path;
  int pos = resDirPrefix.ReverseFind_PathSepar();
  pos++;
  resFileName = resDirPrefix.Ptr((unsigned)pos);
  resDirPrefix.DeleteFrom((unsigned)pos);
  return res;
}

bool GetOnlyDirPrefix(CFSTR path, FString &resDirPrefix)
{
  FString resFileName;
  return GetFullPathAndSplit(path, resDirPrefix, resFileName);
}

bool MyGetTempPath(FString &path)
{
  #ifdef _WIN32
  path.Empty();
  DWORD needLength;
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    TCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetTempPath(MAX_PATH + 1, s);
    path = fas2fs(s);
  }
  else
  #endif
  {
    WCHAR s[MAX_PATH + 2];
    s[0] = 0;
    needLength = ::GetTempPathW(MAX_PATH + 1, s);;
    path = us2fs(s);
  }
  return (needLength > 0 && needLength <= MAX_PATH);

  #else

  // FIXME: improve that code
  path = "/tmp/";
  if (!NFind::DoesDirExist_FollowLink(path))
    path = "./";
  return true;
  #endif
}


static bool CreateTempFile(CFSTR prefix, bool addRandom, FString &path, NIO::COutFile *outFile)
{
  UInt32 d =
    #ifdef _WIN32
      (GetTickCount() << 12) ^ (GetCurrentThreadId() << 14) ^ GetCurrentProcessId();
    #else
      (UInt32)(time(NULL) << 12) ^  ((UInt32)getppid() << 14) ^ (UInt32)(getpid());
    #endif

  for (unsigned i = 0; i < 100; i++)
  {
    path = prefix;
    if (addRandom)
    {
      char s[16];
      UInt32 val = d;
      unsigned k;
      for (k = 0; k < 8; k++)
      {
        unsigned t = val & 0xF;
        val >>= 4;
        s[k] = (char)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
      }
      s[k] = '\0';
      if (outFile)
        path += '.';
      path += s;
      UInt32 step = GetTickCount() + 2;
      if (step == 0)
        step = 1;
      d += step;
    }
    addRandom = true;
    if (outFile)
      path += ".tmp";
    if (NFind::DoesFileOrDirExist(path))
    {
      SetLastError(ERROR_ALREADY_EXISTS);
      continue;
    }
    if (outFile)
    {
      if (outFile->Create(path, false))
        return true;
    }
    else
    {
      if (CreateDir(path))
        return true;
    }
    DWORD error = GetLastError();
    if (error != ERROR_FILE_EXISTS &&
        error != ERROR_ALREADY_EXISTS)
      break;
  }
  path.Empty();
  return false;
}

bool CTempFile::Create(CFSTR prefix, NIO::COutFile *outFile)
{
  if (!Remove())
    return false;
  if (!CreateTempFile(prefix, false, _path, outFile))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempFile::CreateRandomInTempFolder(CFSTR namePrefix, NIO::COutFile *outFile)
{
  if (!Remove())
    return false;
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (!CreateTempFile(tempPath + namePrefix, true, _path, outFile))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempFile::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !DeleteFileAlways(_path);
  return !_mustBeDeleted;
}

bool CTempFile::MoveTo(CFSTR name, bool deleteDestBefore)
{
  // DWORD attrib = 0;
  if (deleteDestBefore)
  {
    if (NFind::DoesFileExist_Raw(name))
    {
      // attrib = NFind::GetFileAttrib(name);
      if (!DeleteFileAlways(name))
        return false;
    }
  }
  DisableDeleting();
  return MyMoveFile(_path, name);

  /*
  if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_READONLY))
  {
    DWORD attrib2 = NFind::GetFileAttrib(name);
    if (attrib2 != INVALID_FILE_ATTRIBUTES)
      SetFileAttrib(name, attrib2 | FILE_ATTRIBUTE_READONLY);
  }
  */
}

#ifdef _WIN32
bool CTempDir::Create(CFSTR prefix)
{
  if (!Remove())
    return false;
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (!CreateTempFile(tempPath + prefix, true, _path, NULL))
    return false;
  _mustBeDeleted = true;
  return true;
}

bool CTempDir::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !RemoveDirWithSubItems(_path);
  return !_mustBeDeleted;
}
#endif



#ifndef _WIN32

bool RemoveDir(CFSTR path)
{
  return (rmdir(path) == 0);
}


static BOOL My__CopyFile(CFSTR oldFile, CFSTR newFile)
{
  NWindows::NFile::NIO::COutFile outFile;
  if (!outFile.Create(newFile, false))
    return FALSE;

  NWindows::NFile::NIO::CInFile inFile;
  if (!inFile.Open(oldFile))
    return FALSE;

  char buf[1 << 14];

  for (;;)
  {
    const ssize_t num = inFile.read_part(buf, sizeof(buf));
    if (num == 0)
      return TRUE;
    if (num < 0)
      return FALSE;
    size_t processed;
    const ssize_t num2 = outFile.write_full(buf, (size_t)num, processed);
    if (num2 != num || processed != (size_t)num)
      return FALSE;
  }
}


bool MyMoveFile(CFSTR oldFile, CFSTR newFile)
{
  int res = rename(oldFile, newFile);
  if (res == 0)
    return true;
  if (errno != EXDEV) // (oldFile and newFile are not on the same mounted filesystem)
    return false;

  if (My__CopyFile(oldFile, newFile) == FALSE)
    return false;

  struct stat info_file;
  res = stat(oldFile, &info_file);
  if (res != 0)
    return false;

  /*
  ret = chmod(dst,info_file.st_mode & g_umask.mask);
  */
  return (unlink(oldFile) == 0);
}


bool CreateDir(CFSTR path)
{
  return (mkdir(path, 0777) == 0); // change it
}

static bool CreateDir2(CFSTR path)
{
  return (mkdir(path, 0777) == 0); // change it
}


bool DeleteFileAlways(CFSTR path)
{
  return (remove(path) == 0);
}

bool SetCurrentDir(CFSTR path)
{
  return (chdir(path) == 0);
}


bool GetCurrentDir(FString &path)
{
  path.Empty();

  #define MY__PATH_MAX  PATH_MAX
  // #define MY__PATH_MAX  1024

  char s[MY__PATH_MAX + 1];
  char *res = getcwd(s, MY__PATH_MAX);
  if (res)
  {
    path = fas2fs(s);
    return true;
  }
  {
    // if (errno != ERANGE) return false;
    #if defined(__GLIBC__) || defined(__APPLE__)
    /* As an extension to the POSIX.1-2001 standard, glibc's getcwd()
       allocates the buffer dynamically using malloc(3) if buf is NULL. */
    res = getcwd(NULL, 0);
    if (res)
    {
      path = fas2fs(res);
      ::free(res);
      return true;
    }
    #endif
    return false;
  }
}



// #undef UTIME_OMIT // to debug

#ifndef UTIME_OMIT
  /* we can define UTIME_OMIT for debian and another systems.
     Is it OK to define UTIME_OMIT to -2 here, if UTIME_OMIT is not defined? */
  // #define UTIME_OMIT -2
#endif





bool SetDirTime(CFSTR path, const CFiTime *cTime, const CFiTime *aTime, const CFiTime *mTime)
{
  // need testing
  /*
  struct utimbuf buf;
  struct stat st;
  UNUSED_VAR(cTime)

  printf("\nstat = %s\n", path);
  int ret = stat(path, &st);

  if (ret == 0)
  {
    buf.actime  = st.st_atime;
    buf.modtime = st.st_mtime;
  }
  else
  {
    time_t cur_time = time(0);
    buf.actime  = cur_time;
    buf.modtime = cur_time;
  }

  if (aTime)
  {
    UInt32 ut;
    if (NTime::FileTimeToUnixTime(*aTime, ut))
      buf.actime = ut;
  }

  if (mTime)
  {
    UInt32 ut;
    if (NTime::FileTimeToUnixTime(*mTime, ut))
      buf.modtime = ut;
  }

  return utime(path, &buf) == 0;
  */

  // if (!aTime && !mTime) return true;

  struct timespec times[2];
  UNUSED_VAR(cTime)

  bool needChange;
  needChange  = FiTime_To_timespec(aTime, times[0]);
  needChange |= FiTime_To_timespec(mTime, times[1]);

  /*
  if (mTime)
  {
    printf("\n time = %ld.%9ld\n", mTime->tv_sec, mTime->tv_nsec);
  }
  */

  if (!needChange)
    return true;
  const int flags = 0; // follow link
    // = AT_SYMLINK_NOFOLLOW; // don't follow link
  return utimensat(AT_FDCWD, path, times, flags) == 0;
}



struct C_umask
{
  mode_t mask;

  C_umask()
  {
    /* by security reasons we restrict attributes according
       with process's file mode creation mask (umask) */
    const mode_t um = umask(0); // octal :0022 is expected
    mask = 0777 & (~um);        // octal: 0755 is expected
    umask(um);  // restore the umask
    // printf("\n umask = 0%03o mask = 0%03o\n", um, mask);

    // mask = 0777; // debug we can disable the restriction:
  }
};

static C_umask g_umask;

// #define PRF(x) x;
#define PRF(x)

#define TRACE_SetFileAttrib(msg) \
  PRF(printf("\nSetFileAttrib(%s, %x) : %s\n", (const char *)path, attrib, msg));

#define TRACE_chmod(s, mode) \
  PRF(printf("\n chmod(%s, %o)\n", (const char *)path, (unsigned)(mode)));

int my_chown(CFSTR path, uid_t owner, gid_t group)
{
  return chown(path, owner, group);
}

bool SetFileAttrib_PosixHighDetect(CFSTR path, DWORD attrib)
{
  TRACE_SetFileAttrib("");

  struct stat st;

  bool use_lstat = true;
  if (use_lstat)
  {
    if (lstat(path, &st) != 0)
    {
      TRACE_SetFileAttrib("bad lstat()");
      return false;
    }
    // TRACE_chmod("lstat", st.st_mode);
  }
  else
  {
    if (stat(path, &st) != 0)
    {
      TRACE_SetFileAttrib("bad stat()");
      return false;
    }
  }

  if (attrib & FILE_ATTRIBUTE_UNIX_EXTENSION)
  {
    TRACE_SetFileAttrib("attrib & FILE_ATTRIBUTE_UNIX_EXTENSION");
    st.st_mode = attrib >> 16;
    if (S_ISDIR(st.st_mode))
    {
      // user/7z must be able to create files in this directory
      st.st_mode |= (S_IRUSR | S_IWUSR | S_IXUSR);
    }
    else if (!S_ISREG(st.st_mode))
      return true;
  }
  else if (S_ISLNK(st.st_mode))
  {
    /* for most systems: permissions for symlinks are fixed to rwxrwxrwx.
       so we don't need chmod() for symlinks. */
    return true;
    // SetLastError(ENOSYS);
    // return false;
  }
  else
  {
    TRACE_SetFileAttrib("Only Windows Attributes");
    // Only Windows Attributes
    if (S_ISDIR(st.st_mode)
        || (attrib & FILE_ATTRIBUTE_READONLY) == 0)
      return true;
    st.st_mode &= ~(mode_t)(S_IWUSR | S_IWGRP | S_IWOTH); // octal: ~0222; // disable write permissions
  }

  int res;
  /*
  if (S_ISLNK(st.st_mode))
  {
    printf("\nfchmodat()\n");
    TRACE_chmod(path, (st.st_mode) & g_umask.mask);
    // AT_SYMLINK_NOFOLLOW is not implemted still in Linux.
    res = fchmodat(AT_FDCWD, path, (st.st_mode) & g_umask.mask,
        S_ISLNK(st.st_mode) ? AT_SYMLINK_NOFOLLOW : 0);
  }
  else
  */
  {
    TRACE_chmod(path, (st.st_mode) & g_umask.mask);
    res = chmod(path, (st.st_mode) & g_umask.mask);
  }
  // TRACE_SetFileAttrib("End")
  return (res == 0);
}


bool MyCreateHardLink(CFSTR newFileName, CFSTR existFileName)
{
  PRF(printf("\nhard link() %s -> %s\n", newFileName, existFileName));
  return (link(existFileName, newFileName) == 0);
}

#endif // !_WIN32

// #endif

}}}
