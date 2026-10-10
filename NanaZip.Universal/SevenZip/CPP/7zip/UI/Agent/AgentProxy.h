// AgentProxy.h

#ifndef ZIP7_INC_AGENT_PROXY_H
#define ZIP7_INC_AGENT_PROXY_H

#include "../Common/OpenArchive.h"

struct CProxyFile
{
  const wchar_t *Name;
  unsigned NameLen;
  bool NeedDeleteName;

  void Construct()
  {
    Name = NULL;
    NameLen = 0;
    NeedDeleteName = false;
  }
};

const unsigned k_Proxy_RootDirIndex = 0;

struct CProxyDir
{
  const wchar_t *Name;
  unsigned NameLen;

  int ArcIndex;  // index in proxy->Files[] ;  -1 if there is no item for that folder
  int ParentDir; // index in proxy->Dirs[]  ;  -1 for root folder
  CUIntVector SubDirs;
  CUIntVector SubDirs2;
  CUIntVector SubFiles;

  UInt64 Size;
  UInt64 PackSize;
  UInt32 Crc;
  UInt32 NumSubDirs;
  UInt32 NumSubFiles;
  bool CrcIsDefined;
  bool Is_Changed_LongPath;

  CProxyDir(): Name(NULL), NameLen(0), ParentDir(-1), Is_Changed_LongPath(false) {}
  ~CProxyDir() { delete [](wchar_t *)(void *)Name; }

  bool IsLeaf() const { return ArcIndex != -1; }
};

class CProxyArc
{
  int FindSubDir(const unsigned *items, unsigned right,
      const wchar_t *name, unsigned &insertPos) const;

  void CalculateSizes(IInArchive *archive);
  void MergeSubDirs(CProxyDir &baseDir);
  unsigned AddDir(unsigned dirIndex, int arcIndex, const UString &name);
  void FreeFiles();
public:
  CObjectVector<CProxyDir> Dirs; // Dirs[0] - root
  CProxyFile *Files;  // all items from archive in same order
  unsigned NumFiles;
  bool Are_Changed_LongPaths;
  UInt64 MemUsage;
  UInt64 MemUsage_Limit;

  CProxyArc(): Files(NULL), NumFiles(0), Are_Changed_LongPaths(false), MemUsage_Limit((UInt64)(Int64)-1) {}
  ~CProxyArc() { FreeFiles(); }

  // returns index in Dirs[], or -1,
  int FindSubDir(unsigned dirIndex, const wchar_t *name) const;

  void GetDirPathParts_isChanged(unsigned dirIndex,
      UStringVector &pathPartsm, bool &isChangedPath) const;
  // returns full path of Dirs[dirIndex], including back slash
  UString GetDirPath_as_Prefix(unsigned dirIndex) const;
  void GetDirPath_as_Prefix_from_Base(unsigned dirIndex, UString &prefix, unsigned baseDirIndex) const;

  // AddRealIndices DOES ADD also item represented by dirIndex (if it's Leaf)
  void AddRealIndices(unsigned dirIndex, CUIntVector &realIndices) const;
  int GetRealIndex(unsigned dirIndex, unsigned index) const;
  void GetRealIndices_Unsorted(unsigned dirIndex, const UInt32 *indices, UInt32 numItems, CUIntVector &realIndices) const;

  HRESULT Load(const CArc &arc, IArchiveOpenCallback *progress);
};


// ---------- for Tree-mode archive ----------

// #define Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX

struct CProxyFile2
{
  int DirIndex;     // >= 0 for dir. (index in ProxyArchive2->Dirs)
  int AltDirIndex;  // >= 0 if there are alt streams. (index in ProxyArchive2->Dirs)
  int Parent;       // >= 0 if there is parent. (index in archive and in ProxyArchive2->Files)
  const wchar_t *Name;
  unsigned NameLen;
  bool NeedDeleteName;
  bool Ignore;  // = false always
  bool IsAltStream;
  bool IsAux;

  int GetDirIndex(bool forAltStreams) const { return forAltStreams ? AltDirIndex : DirIndex; }

  bool IsDir() const { return DirIndex != -1; }

  void Construct()
  {
    DirIndex = -1;
    AltDirIndex = -1;
    Parent = -1;
    Name = NULL;
    NameLen = 0;
    NeedDeleteName = false;
    Ignore = false;
    IsAltStream = false;
    IsAux = false;
  }
};

struct CProxyDir2
{
  int ArcIndex;   // = -1 for root folders, index in CProxyArc2::Files[]
  unsigned PrefixLen;
  CUIntVector Items; // indexes in archive and in CProxyArc2::Files[]
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
  UString PathPrefix;
#endif
  UInt64 Size;
  UInt64 PackSize;
#ifdef Z7_AGENT_PROXY2_USE_DIR_PATH_PREFIX
  bool IsLongPath;
#endif
  bool CrcIsDefined;
  UInt32 Crc;
  UInt32 NumSubDirs;
  UInt32 NumSubFiles;

  CProxyDir2();
};

const unsigned k_Proxy2_RootDirIndex = k_Proxy_RootDirIndex;
const unsigned k_Proxy2_AltRootDirIndex = 1;
const unsigned k_Proxy2_NumRootDirs = 2;

class CProxyArc2
{
  HRESULT CalculateSizes(unsigned dirIndex, IInArchive *archive
      , IProgress *progress, const UInt64 *completed, unsigned &progressCounter);
  // AddRealIndices_of_Dir DOES NOT ADD item itself represented by dirIndex
  void AddRealIndices_of_Dir(unsigned dirIndex, bool includeAltStreams, CUIntVector &realIndices) const;
  void FreeFiles();
  void UpdateMemUsage_with_StringLen(unsigned len)
  {
    MemUsage += (size_t)len * sizeof(Files->Name[0]) + 16;
  }
public:
  CObjectVector<CProxyDir2> Dirs;  // Dirs[0] - root folder
                                   // Dirs[1] - for alt streams of root dir
  CProxyFile2 *Files;   // all items from archive in same order
  unsigned NumFiles;
  UInt64 MemUsage;
  UInt64 MemUsage_Limit;

  CProxyArc2(): Files(NULL), NumFiles(0), MemUsage_Limit((UInt64)(Int64)-1) {}
  ~CProxyArc2() { FreeFiles(); }

  void GetDirPathParts(unsigned dirIndex, UStringVector &pathParts, bool &isAltStreamDir) const;
  void GetDirPath_as_Prefix_from_Base(unsigned dirIndex, UString &prefix, unsigned baseDirIndex, unsigned lenLimit) const;
  UString GetDirPath_as_Prefix(unsigned dirIndex, bool canReducePath = false) const;
  bool IsAltDir(unsigned dirIndex) const;

  // AddRealIndices_of_ArcItem DOES ADD item and subItems
  void AddRealIndices_of_ArcItem(unsigned arcIndex,
      bool includeAltStreams, bool includeDirSubItems,
      CUIntVector &realIndices) const;
  unsigned GetRealIndex(const unsigned dirIndex, const unsigned index) const
  {
    return Dirs[dirIndex].Items[index];
  }
  void GetRealIndices_Unsorted(unsigned dirIndex, const UInt32 *indices, UInt32 numItems,
      bool includeAltStreams, CUIntVector &realIndices) const;

  HRESULT Load(const CArc &arc, IProgress *progress);

  int FindItem(unsigned dirIndex, const wchar_t *name, bool foldersOnly) const;
  bool IsThere_SubDir(unsigned dirIndex, const wchar_t *name) const
  {
    return FindItem(dirIndex, name, true) != -1; // foldersOnly
  }

  void AddDir(int arcIndex);
};

void SetDirPrefix_as_LONG_PATH(UString &s, unsigned fileIndex,
    bool asAltDirPrefix, bool isDir);

#endif
