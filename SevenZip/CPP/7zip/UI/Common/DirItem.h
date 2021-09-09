// DirItem.h

#ifndef __DIR_ITEM_H
#define __DIR_ITEM_H

#ifdef _WIN32
#include "../../../Common/MyLinux.h"
#endif

#include "../../../Common/MyString.h"

#include "../../../Windows/FileFind.h"

#include "../../Common/UniqBlocks.h"

#include "../../Archive/IArchive.h"

struct CDirItemsStat
{
  UInt64 NumDirs;
  UInt64 NumFiles;
  UInt64 NumAltStreams;
  UInt64 FilesSize;
  UInt64 AltStreamsSize;
  
  UInt64 NumErrors;
  
  // UInt64 Get_NumItems() const { return NumDirs + NumFiles + NumAltStreams; }
  UInt64 Get_NumDataItems() const { return NumFiles + NumAltStreams; }
  UInt64 GetTotalBytes() const { return FilesSize + AltStreamsSize; }

  bool IsEmpty() const { return
           0 == NumDirs
        && 0 == NumFiles
        && 0 == NumAltStreams
        && 0 == FilesSize
        && 0 == AltStreamsSize
        && 0 == NumErrors; }
  
  CDirItemsStat():
      NumDirs(0),
      NumFiles(0),
      NumAltStreams(0),
      FilesSize(0),
      AltStreamsSize(0),
      NumErrors(0)
    {}
};


struct CDirItemsStat2: public CDirItemsStat
{
  UInt64 Anti_NumDirs;
  UInt64 Anti_NumFiles;
  UInt64 Anti_NumAltStreams;
  
  // UInt64 Get_NumItems() const { return Anti_NumDirs + Anti_NumFiles + Anti_NumAltStreams + CDirItemsStat::Get_NumItems(); }
  UInt64 Get_NumDataItems2() const { return Anti_NumFiles + Anti_NumAltStreams + CDirItemsStat::Get_NumDataItems(); }

  bool IsEmpty() const { return CDirItemsStat::IsEmpty()
        && 0 == Anti_NumDirs
        && 0 == Anti_NumFiles
        && 0 == Anti_NumAltStreams; }
  
  CDirItemsStat2():
      Anti_NumDirs(0),
      Anti_NumFiles(0),
      Anti_NumAltStreams(0)
    {}
};



#define INTERFACE_IDirItemsCallback(x) \
  virtual HRESULT ScanError(const FString &path, DWORD systemError) x; \
  virtual HRESULT ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir) x; \

struct IDirItemsCallback
{
  INTERFACE_IDirItemsCallback(=0)
};

struct CDirItem
{
  UInt64 Size;
  FILETIME CTime;
  FILETIME ATime;
  FILETIME MTime;
  UString Name;
  
  #ifndef UNDER_CE
  CByteBuffer ReparseData;

  #ifdef _WIN32
  // UString ShortName;
  CByteBuffer ReparseData2; // fixed (reduced) absolute links for WIM format
  bool AreReparseData() const { return ReparseData.Size() != 0 || ReparseData2.Size() != 0; }
  #else
  bool AreReparseData() const { return ReparseData.Size() != 0; }
  #endif // _WIN32

  #endif // !UNDER_CE
  
  UInt32 Attrib;
  int PhyParent;
  int LogParent;
  int SecureIndex;

  bool IsAltStream;
  
  CDirItem(): PhyParent(-1), LogParent(-1), SecureIndex(-1), IsAltStream(false) {}
  
  bool IsDir() const { return (Attrib & FILE_ATTRIBUTE_DIRECTORY) != 0; }
  bool IsReadOnly() const { return (Attrib & FILE_ATTRIBUTE_READONLY) != 0; }
  bool Has_Attrib_ReparsePoint() const { return (Attrib & FILE_ATTRIBUTE_REPARSE_POINT) != 0; }

  #ifdef _WIN32
  UInt32 GetPosixAttrib() const
  {
    UInt32 v = IsDir() ? MY_LIN_S_IFDIR : MY_LIN_S_IFREG;
    v |= (IsReadOnly() ? 0555 : 0777);
    return v;
  }
  #endif
};



class CDirItems
{
  UStringVector Prefixes;
  CIntVector PhyParents;
  CIntVector LogParents;

  UString GetPrefixesPath(const CIntVector &parents, int index, const UString &name) const;

  HRESULT EnumerateDir(int phyParent, int logParent, const FString &phyPrefix);

public:
  CObjectVector<CDirItem> Items;

  bool SymLinks;
  bool ScanAltStreams;
  
  CDirItemsStat Stat;

  #if !defined(UNDER_CE)
  HRESULT SetLinkInfo(CDirItem &dirItem, const NWindows::NFile::NFind::CFileInfo &fi,
      const FString &phyPrefix);
  #endif

  #if defined(_WIN32) && !defined(UNDER_CE)

  CUniqBlocks SecureBlocks;
  CByteBuffer TempSecureBuf;
  bool _saclEnabled;
  bool ReadSecure;
  
  HRESULT AddSecurityItem(const FString &path, int &secureIndex);
  HRESULT FillFixedReparse();

  #endif

  IDirItemsCallback *Callback;

  CDirItems();

  void AddDirFileInfo(int phyParent, int logParent, int secureIndex,
      const NWindows::NFile::NFind::CFileInfo &fi);

  HRESULT AddError(const FString &path, DWORD errorCode);
  HRESULT AddError(const FString &path);

  HRESULT ScanProgress(const FString &path);

  // unsigned GetNumFolders() const { return Prefixes.Size(); }
  FString GetPhyPath(unsigned index) const;
  UString GetLogPath(unsigned index) const;

  unsigned AddPrefix(int phyParent, int logParent, const UString &prefix);
  void DeleteLastPrefix();

  // HRESULT EnumerateOneDir(const FString &phyPrefix, CObjectVector<NWindows::NFile::NFind::CDirEntry> &files);
  HRESULT EnumerateOneDir(const FString &phyPrefix, CObjectVector<NWindows::NFile::NFind::CFileInfo> &files);
  
  HRESULT EnumerateItems2(
    const FString &phyPrefix,
    const UString &logPrefix,
    const FStringVector &filePaths,
    FStringVector *requestedPaths);

  void ReserveDown();
};


struct CArcItem
{
  UInt64 Size;
  FILETIME MTime;
  UString Name;
  bool IsDir;
  bool IsAltStream;
  bool SizeDefined;
  bool MTimeDefined;
  bool Censored;
  UInt32 IndexInServer;
  int TimeType;
  
  CArcItem(): IsDir(false), IsAltStream(false), SizeDefined(false), MTimeDefined(false), Censored(false), TimeType(-1) {}
};

#endif
