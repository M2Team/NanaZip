// Windows/FileFind.h

#ifndef __WINDOWS_FILE_FIND_H
#define __WINDOWS_FILE_FIND_H

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

#include "../Common/MyString.h"
#include "../Common/MyWindows.h"
#include "Defs.h"

namespace NWindows {
namespace NFile {
namespace NFind {

// bool DoesFileExist(CFSTR name, bool followLink);
bool DoesFileExist_Raw(CFSTR name);
bool DoesFileExist_FollowLink(CFSTR name);
bool DoesDirExist(CFSTR name, bool followLink);

inline bool DoesDirExist(CFSTR name)
  { return DoesDirExist(name, false); }
inline bool DoesDirExist_FollowLink(CFSTR name)
  { return DoesDirExist(name, true); }

// it's always _Raw
bool DoesFileOrDirExist(CFSTR name);

DWORD GetFileAttrib(CFSTR path);


namespace NAttributes
{
  inline bool IsReadOnly(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_READONLY) != 0; }
  inline bool IsHidden(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_HIDDEN) != 0; }
  inline bool IsSystem(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_SYSTEM) != 0; }
  inline bool IsDir(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0; }
  inline bool IsArchived(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_ARCHIVE) != 0; }
  inline bool IsCompressed(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_COMPRESSED) != 0; }
  inline bool IsEncrypted(DWORD attrib) { return (attrib & FILE_ATTRIBUTE_ENCRYPTED) != 0; }
}

class CFileInfoBase
{
  bool MatchesMask(UINT32 mask) const { return ((Attrib & mask) != 0); }
public:
  UInt64 Size;
  FILETIME CTime;
  FILETIME ATime;
  FILETIME MTime;
  DWORD Attrib;
  bool IsAltStream;
  bool IsDevice;

  #ifdef _WIN32
  /*
  #ifdef UNDER_CE
  DWORD ObjectID;
  #else
  UINT32 ReparseTag;
  #endif
  */
  #else
  dev_t dev;
  ino_t ino;
  nlink_t nlink;
  mode_t mode;
  // bool Use_lstat;
  #endif

  CFileInfoBase() { ClearBase(); }
  void ClearBase() throw();
  
  void SetAsDir()
  {
    Attrib = FILE_ATTRIBUTE_DIRECTORY;
    #ifndef _WIN32
    Attrib |= (FILE_ATTRIBUTE_UNIX_EXTENSION + (S_IFDIR << 16));
    #endif
  }

  bool IsArchived() const { return MatchesMask(FILE_ATTRIBUTE_ARCHIVE); }
  bool IsCompressed() const { return MatchesMask(FILE_ATTRIBUTE_COMPRESSED); }
  bool IsDir() const { return MatchesMask(FILE_ATTRIBUTE_DIRECTORY); }
  bool IsEncrypted() const { return MatchesMask(FILE_ATTRIBUTE_ENCRYPTED); }
  bool IsHidden() const { return MatchesMask(FILE_ATTRIBUTE_HIDDEN); }
  bool IsNormal() const { return MatchesMask(FILE_ATTRIBUTE_NORMAL); }
  bool IsOffline() const { return MatchesMask(FILE_ATTRIBUTE_OFFLINE); }
  bool IsReadOnly() const { return MatchesMask(FILE_ATTRIBUTE_READONLY); }
  bool HasReparsePoint() const { return MatchesMask(FILE_ATTRIBUTE_REPARSE_POINT); }
  bool IsSparse() const { return MatchesMask(FILE_ATTRIBUTE_SPARSE_FILE); }
  bool IsSystem() const { return MatchesMask(FILE_ATTRIBUTE_SYSTEM); }
  bool IsTemporary() const { return MatchesMask(FILE_ATTRIBUTE_TEMPORARY); }

  #ifndef _WIN32
  bool IsPosixLink() const
  {
    const UInt32 mod = Attrib >> 16;
    return S_ISLNK(mod);
  }
  #endif

  bool IsOsSymLink() const
  {
    #ifdef _WIN32
      return HasReparsePoint();
    #else
      return IsPosixLink();
    #endif
  }
};

struct CFileInfo: public CFileInfoBase
{
  FString Name;
  #if defined(_WIN32) && !defined(UNDER_CE)
  // FString ShortName;
  #endif

  bool IsDots() const throw();
  bool Find(CFSTR path, bool followLink = false);
  bool Find_FollowLink(CFSTR path) { return Find(path, true); }

  #ifdef _WIN32
  bool Fill_From_ByHandleFileInfo(CFSTR path);
  // bool FollowReparse(CFSTR path, bool isDir);
  #else
  bool Find_DontFill_Name(CFSTR path, bool followLink = false);
  void SetFrom_stat(const struct stat &st);
  #endif
};


#ifdef _WIN32

class CFindFileBase  MY_UNCOPYABLE
{
protected:
  HANDLE _handle;
public:
  bool IsHandleAllocated() const { return _handle != INVALID_HANDLE_VALUE; }
  CFindFileBase(): _handle(INVALID_HANDLE_VALUE) {}
  ~CFindFileBase() { Close(); }
  bool Close() throw();
};

class CFindFile: public CFindFileBase
{
public:
  bool FindFirst(CFSTR wildcard, CFileInfo &fileInfo);
  bool FindNext(CFileInfo &fileInfo);
};

#if defined(_WIN32) && !defined(UNDER_CE)

struct CStreamInfo
{
  UString Name;
  UInt64 Size;

  UString GetReducedName() const; // returns ":Name"
  // UString GetReducedName2() const; // returns "Name"
  bool IsMainStream() const throw();
};

class CFindStream: public CFindFileBase
{
public:
  bool FindFirst(CFSTR filePath, CStreamInfo &streamInfo);
  bool FindNext(CStreamInfo &streamInfo);
};

class CStreamEnumerator  MY_UNCOPYABLE
{
  CFindStream _find;
  FString _filePath;

  bool NextAny(CFileInfo &fileInfo, bool &found);
public:
  CStreamEnumerator(const FString &filePath): _filePath(filePath) {}
  bool Next(CStreamInfo &streamInfo, bool &found);
};

#endif // defined(_WIN32) && !defined(UNDER_CE)


class CEnumerator  MY_UNCOPYABLE
{
  CFindFile _findFile;
  FString _wildcard;

  bool NextAny(CFileInfo &fileInfo);
public:
  void SetDirPrefix(const FString &dirPrefix);
  bool Next(CFileInfo &fileInfo);
  bool Next(CFileInfo &fileInfo, bool &found);
};


class CFindChangeNotification  MY_UNCOPYABLE
{
  HANDLE _handle;
public:
  operator HANDLE () { return _handle; }
  bool IsHandleAllocated() const { return _handle != INVALID_HANDLE_VALUE && _handle != 0; }
  CFindChangeNotification(): _handle(INVALID_HANDLE_VALUE) {}
  ~CFindChangeNotification() { Close(); }
  bool Close() throw();
  HANDLE FindFirst(CFSTR pathName, bool watchSubtree, DWORD notifyFilter);
  bool FindNext() { return BOOLToBool(::FindNextChangeNotification(_handle)); }
};

#ifndef UNDER_CE
bool MyGetLogicalDriveStrings(CObjectVector<FString> &driveStrings);
#endif

typedef CFileInfo CDirEntry;


#else // WIN32


struct CDirEntry
{
  ino_t iNode;
  #if !defined(_AIX)
  Byte Type;
  #endif
  FString Name;

  /*
  #if !defined(_AIX)
  bool IsDir() const
  {
    // (Type == DT_UNKNOWN) on some systems
    return Type == DT_DIR;
  }
  #endif
  */

  bool IsDots() const throw();
};

class CEnumerator  MY_UNCOPYABLE
{
  DIR *_dir;
  FString _wildcard;

  bool NextAny(CDirEntry &fileInfo, bool &found);
public:
  CEnumerator(): _dir(NULL) {}
  ~CEnumerator();
  void SetDirPrefix(const FString &dirPrefix);

  bool Next(CDirEntry &fileInfo, bool &found);
  bool Fill_FileInfo(const CDirEntry &de, CFileInfo &fileInfo, bool followLink) const;
  bool DirEntry_IsDir(const CDirEntry &de, bool followLink) const
  {
    #if !defined(_AIX)
    if (de.Type == DT_DIR)
      return true;
    if (de.Type != DT_UNKNOWN)
      return false;
    #endif
    CFileInfo fileInfo;
    if (Fill_FileInfo(de, fileInfo, followLink))
    {
      return fileInfo.IsDir();
    }
    return false; // change it
  }
};

/*
inline UInt32 Get_WinAttrib_From_PosixMode(UInt32 mode)
{
  UInt32 attrib = S_ISDIR(mode) ?
    FILE_ATTRIBUTE_DIRECTORY :
    FILE_ATTRIBUTE_ARCHIVE;
  if ((st.st_mode & 0222) == 0) // check it !!!
    attrib |= FILE_ATTRIBUTE_READONLY;
  return attrib;
}
*/

UInt32 Get_WinAttribPosix_From_PosixMode(UInt32 mode);

// UInt32 Get_WinAttrib_From_stat(const struct stat &st);
#if defined(_AIX)
  #define MY_ST_TIMESPEC st_timespec
#else
  #define MY_ST_TIMESPEC timespec
#endif

void timespec_To_FILETIME(const MY_ST_TIMESPEC &ts, FILETIME &ft);

#endif // WIN32

}}}

#endif
