// AgentProxy.h

#ifndef __AGENT_PROXY_H
#define __AGENT_PROXY_H

#include "../Common/OpenArchive.h"

struct CProxyFile
{
  const wchar_t *Name;
  unsigned NameLen;
  bool NeedDeleteName;
  
  CProxyFile(): Name(NULL), NameLen(0), NeedDeleteName(false)  {}
  ~CProxyFile() { if (NeedDeleteName) delete [](wchar_t *)(void *)Name; } // delete [](wchar_t *)Name;
};

const unsigned k_Proxy_RootDirIndex = 0;

struct CProxyDir
{
  const wchar_t *Name;
  unsigned NameLen;

  int ArcIndex;  // index in proxy->Files[] ;  -1 if there is no item for that folder
  int ParentDir; // index in proxy->Dirs[]  ;  -1 for root folder; ;
  CRecordVector<unsigned> SubDirs;
  CRecordVector<unsigned> SubFiles;

  UInt64 Size;
  UInt64 PackSize;
  UInt32 Crc;
  UInt32 NumSubDirs;
  UInt32 NumSubFiles;
  bool CrcIsDefined;

  CProxyDir(): Name(NULL), NameLen(0), ParentDir(-1) {};
  ~CProxyDir() { delete [](wchar_t *)(void *)Name; }

  void Clear();
  bool IsLeaf() const { return ArcIndex >= 0; }
};

class CProxyArc
{
  int FindSubDir(unsigned dirIndex, const wchar_t *name, unsigned &insertPos) const;

  void CalculateSizes(unsigned dirIndex, IInArchive *archive);
  unsigned AddDir(unsigned dirIndex, int arcIndex, const UString &name);
public:
  CObjectVector<CProxyDir> Dirs; // Dirs[0] - root
  CObjArray<CProxyFile> Files;   // all items from archive in same order

  // returns index in Dirs[], or -1,
  int FindSubDir(unsigned dirIndex, const wchar_t *name) const;

  void GetDirPathParts(int dirIndex, UStringVector &pathParts) const;
  // returns full path of Dirs[dirIndex], including back slash
  UString GetDirPath_as_Prefix(int dirIndex) const;
  
  // AddRealIndices DOES ADD also item represented by dirIndex (if it's Leaf)
  void AddRealIndices(unsigned dirIndex, CUIntVector &realIndices) const;
  int GetRealIndex(unsigned dirIndex, unsigned index) const;
  void GetRealIndices(unsigned dirIndex, const UInt32 *indices, UInt32 numItems, CUIntVector &realIndices) const;

  HRESULT Load(const CArc &arc, IProgress *progress);
};


// ---------- for Tree-mode archive ----------

struct CProxyFile2
{
  int DirIndex;     // >= 0 for dir. (index in ProxyArchive2->Dirs)
  int AltDirIndex;  // >= 0 if there are alt streams. (index in ProxyArchive2->Dirs)
  int Parent;          // >= 0 if there is parent. (index in archive and in ProxyArchive2->Files)
  const wchar_t *Name;
  unsigned NameLen;
  bool NeedDeleteName;
  bool Ignore;
  bool IsAltStream;
  
  int GetDirIndex(bool forAltStreams) const { return forAltStreams ? AltDirIndex : DirIndex; }

  bool IsDir() const { return DirIndex >= 0; }
  CProxyFile2():
      DirIndex(-1), AltDirIndex(-1), Parent(-1),
      Name(NULL), NameLen(0),
      NeedDeleteName(false),
      Ignore(false),
      IsAltStream(false)
      {}
  ~CProxyFile2()
  {
    if (NeedDeleteName)
      delete [](wchar_t *)(void *)Name;
  }
};

struct CProxyDir2
{
  int ArcIndex;   // = -1 for root folders, index in proxy->Files[]
  CRecordVector<unsigned> Items;
  UString PathPrefix;
  UInt64 Size;
  UInt64 PackSize;
  bool CrcIsDefined;
  UInt32 Crc;
  UInt32 NumSubDirs;
  UInt32 NumSubFiles;

  CProxyDir2(): ArcIndex(-1) {};
  void AddFileSubItem(UInt32 index, const UString &name);
  void Clear();
};

const unsigned k_Proxy2_RootDirIndex = k_Proxy_RootDirIndex;
const unsigned k_Proxy2_AltRootDirIndex = 1;
const unsigned k_Proxy2_NumRootDirs = 2;

class CProxyArc2
{
  void CalculateSizes(unsigned dirIndex, IInArchive *archive);
  // AddRealIndices_of_Dir DOES NOT ADD item itself represented by dirIndex
  void AddRealIndices_of_Dir(unsigned dirIndex, bool includeAltStreams, CUIntVector &realIndices) const;
public:
  CObjectVector<CProxyDir2> Dirs;  // Dirs[0] - root folder
                                   // Dirs[1] - for alt streams of root dir
  CObjArray<CProxyFile2> Files;    // all items from archive in same order

  bool IsThere_SubDir(unsigned dirIndex, const UString &name) const;

  void GetDirPathParts(int dirIndex, UStringVector &pathParts, bool &isAltStreamDir) const;
  UString GetDirPath_as_Prefix(unsigned dirIndex, bool &isAltStreamDir) const;
  bool IsAltDir(unsigned dirIndex) const;
  
  // AddRealIndices_of_ArcItem DOES ADD item and subItems
  void AddRealIndices_of_ArcItem(unsigned arcIndex, bool includeAltStreams, CUIntVector &realIndices) const;
  unsigned GetRealIndex(unsigned dirIndex, unsigned index) const;
  void GetRealIndices(unsigned dirIndex, const UInt32 *indices, UInt32 numItems, bool includeAltStreams, CUIntVector &realIndices) const;

  HRESULT Load(const CArc &arc, IProgress *progress);

  int GetParentDirOfFile(UInt32 arcIndex) const
  {
    const CProxyFile2 &file = Files[arcIndex];
    
    if (file.Parent < 0)
      return file.IsAltStream ?
          k_Proxy2_AltRootDirIndex :
          k_Proxy2_RootDirIndex;
    
    const CProxyFile2 &parentFile = Files[file.Parent];
    return file.IsAltStream ?
        parentFile.AltDirIndex :
        parentFile.DirIndex;
  }
  
  int FindItem(unsigned dirIndex, const wchar_t *name, bool foldersOnly) const;
};

#endif
