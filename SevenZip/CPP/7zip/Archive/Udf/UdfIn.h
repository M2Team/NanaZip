// Archive/UdfIn.h -- UDF / ECMA-167

#ifndef __ARCHIVE_UDF_IN_H
#define __ARCHIVE_UDF_IN_H

#include "../../../Common/IntToString.h"
#include "../../../Common/MyBuffer.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/MyMap.h"
#include "../../../Common/MyString.h"

#include "../../IStream.h"

namespace NArchive {
namespace NUdf {

// ---------- ECMA Part 1 ----------

// ECMA 1/7.2.12

/*
struct CDString32
{
  Byte Data[32];
  
  void Parse(const Byte *buf);
  // UString GetString() const;
};
*/

struct CDString128
{
  Byte Data[128];
  
  void Parse(const Byte *buf) { memcpy(Data, buf, sizeof(Data)); }
  UString GetString() const;
};

struct CDString
{
  CByteBuffer Data;
  
  void Parse(const Byte *p, unsigned size);
  UString GetString() const;
};


// ECMA 1/7.3

struct CTime
{
  Byte Data[12];

  unsigned GetType() const { return Data[1] >> 4; }
  bool IsLocal() const { return GetType() == 1; }
  int GetMinutesOffset() const
  {
    int t = (Data[0] | ((unsigned)Data[1] << 8)) & 0xFFF;
    if ((t >> 11) != 0)
      t -= (1 << 12);
    return (t > (60 * 24) || t < -(60 * 24)) ? 0 : t;
  }
  unsigned GetYear() const { return (Data[2] | ((unsigned)Data[3] << 8)); }
  void Parse(const Byte *buf);
};


// ECMA 1/7.4

/*
struct CRegId
{
  Byte Flags;
  char Id[23];
  char Suffix[8];

  void Parse(const Byte *buf);
};
*/

// ---------- ECMA Part 3: Volume Structure ----------

// ECMA 3/10.5

struct CPartition
{
  // UInt16 Flags;
  UInt16 Number;
  // CRegId ContentsId;
  // Byte ContentsUse[128];
  // UInt32 AccessType;

  UInt32 Pos;
  UInt32 Len;

  // CRegId ImplId;
  // Byte ImplUse[128];

  int VolIndex;
  CMap32 Map;

  CPartition(): VolIndex(-1) {}

  // bool IsNsr() const { return (strncmp(ContentsId.Id, "+NSR0", 5) == 0); }
  // bool IsAllocated() const { return ((Flags & 1) != 0); }
};

struct CLogBlockAddr
{
  UInt32 Pos;
  UInt16 PartitionRef;
  
  void Parse(const Byte *buf);
};

enum EShortAllocDescType
{
  SHORT_ALLOC_DESC_TYPE_RecordedAndAllocated = 0,
  SHORT_ALLOC_DESC_TYPE_NotRecordedButAllocated = 1,
  SHORT_ALLOC_DESC_TYPE_NotRecordedAndNotAllocated = 2,
  SHORT_ALLOC_DESC_TYPE_NextExtent = 3
};

struct CShortAllocDesc
{
  UInt32 Len;
  UInt32 Pos;

  // 4/14.14.1
  // UInt32 GetLen() const { return Len & 0x3FFFFFFF; }
  // UInt32 GetType() const { return Len >> 30; }
  // bool IsRecAndAlloc() const { return GetType() == SHORT_ALLOC_DESC_TYPE_RecordedAndAllocated; }
  void Parse(const Byte *buf);
};

/*
struct CADImpUse
{
  UInt16 Flags;
  UInt32 UdfUniqueId;
  void Parse(const Byte *buf);
};
*/

struct CLongAllocDesc
{
  UInt32 Len;
  CLogBlockAddr Location;
  
  // Byte ImplUse[6];
  // CADImpUse adImpUse; // UDF
  
  UInt32 GetLen() const { return Len & 0x3FFFFFFF; }
  UInt32 GetType() const { return Len >> 30; }
  bool IsRecAndAlloc() const { return GetType() == SHORT_ALLOC_DESC_TYPE_RecordedAndAllocated; }
  void Parse(const Byte *buf);
};

struct CPartitionMap
{
  Byte Type;
  // Byte Len;

  // Type - 1
  // UInt16 VolSeqNumber;
  UInt16 PartitionNumber;

  // Byte Data[256];

  int PartitionIndex;
};

// ECMA 4/14.6

enum EIcbFileType
{
  ICB_FILE_TYPE_DIR = 4,
  ICB_FILE_TYPE_FILE = 5
};

enum EIcbDescriptorType
{
  ICB_DESC_TYPE_SHORT = 0,
  ICB_DESC_TYPE_LONG = 1,
  ICB_DESC_TYPE_EXTENDED = 2,
  ICB_DESC_TYPE_INLINE = 3
};

struct CIcbTag
{
  // UInt32 PriorDirectNum;
  // UInt16 StrategyType;
  // UInt16 StrategyParam;
  // UInt16 MaxNumOfEntries;
  Byte FileType;
  // CLogBlockAddr ParentIcb;
  UInt16 Flags;

  bool IsDir() const { return FileType == ICB_FILE_TYPE_DIR; }
  int GetDescriptorType() const { return Flags & 3; }
  void Parse(const Byte *p);
};

// const Byte FILEID_CHARACS_Existance = (1 << 0);
const Byte FILEID_CHARACS_Parent = (1 << 3);

struct CFile
{
  // UInt16 FileVersion;
  // Byte FileCharacteristics;
  // CByteBuffer ImplUse;
  CDString Id;

  int ItemIndex;

  CFile(): /* FileVersion(0), FileCharacteristics(0), */ ItemIndex(-1) {}
  UString GetName() const { return Id.GetString(); }
};

struct CMyExtent
{
  UInt32 Pos;
  UInt32 Len;
  unsigned PartitionRef;
  
  UInt32 GetLen() const { return Len & 0x3FFFFFFF; }
  UInt32 GetType() const { return Len >> 30; }
  bool IsRecAndAlloc() const { return GetType() == SHORT_ALLOC_DESC_TYPE_RecordedAndAllocated; }
};

struct CItem
{
  CIcbTag IcbTag;

  // UInt32 Uid;
  // UInt32 Gid;
  // UInt32 Permissions;
  // UInt16 FileLinkCount;
  // Byte RecordFormat;
  // Byte RecordDisplayAttr;
  // UInt32 RecordLen;
  UInt64 Size;
  UInt64 NumLogBlockRecorded;
  CTime ATime;
  CTime MTime;
  // CTime AttrtTime;
  // UInt32 CheckPoint;
  // CLongAllocDesc ExtendedAttrIcb;
  // CRegId ImplId;
  // UInt64 UniqueId;

  bool IsInline;
  CByteBuffer InlineData;
  CRecordVector<CMyExtent> Extents;
  CUIntVector SubFiles;

  void Parse(const Byte *buf);

  bool IsRecAndAlloc() const
  {
    FOR_VECTOR (i, Extents)
      if (!Extents[i].IsRecAndAlloc())
        return false;
    return true;
  }

  UInt64 GetChunksSumSize() const
  {
    if (IsInline)
      return InlineData.Size();
    UInt64 size = 0;
    FOR_VECTOR (i, Extents)
      size += Extents[i].GetLen();
    return size;
  }

  bool CheckChunkSizes() const  {  return GetChunksSumSize() == Size; }

  bool IsDir() const { return IcbTag.IsDir(); }
};
 
struct CRef
{
  int Parent;
  unsigned FileIndex;
};


// ECMA 4 / 14.1
struct CFileSet
{
  CTime RecodringTime;
  // UInt16 InterchangeLevel;
  // UInt16 MaxInterchangeLevel;
  // UInt32 FileSetNumber;
  // UInt32 FileSetDescNumber;
  // CDString32 Id;
  // CDString32 CopyrightId;
  // CDString32 AbstractId;

  CLongAllocDesc RootDirICB;
  // CRegId DomainId;
  // CLongAllocDesc SystemStreamDirICB;

  CRecordVector<CRef> Refs;
};


// ECMA 3/10.6

struct CLogVol
{
  CDString128 Id;
  UInt32 BlockSize;
  // CRegId DomainId;
  
  // Byte ContentsUse[16];
  CLongAllocDesc FileSetLocation; // UDF

  // CRegId ImplId;
  // Byte ImplUse[128];

  CObjectVector<CPartitionMap> PartitionMaps;
  CObjectVector<CFileSet> FileSets;

  UString GetName() const { return Id.GetString(); }
};

struct CProgressVirt
{
  virtual HRESULT SetTotal(UInt64 numBytes) PURE;
  virtual HRESULT SetCompleted(UInt64 numFiles, UInt64 numBytes) PURE;
  virtual HRESULT SetCompleted() PURE;
};

class CInArchive
{
  IInStream *_stream;
  CProgressVirt *_progress;

  HRESULT Read(int volIndex, int partitionRef, UInt32 blockPos, UInt32 len, Byte *buf);
  HRESULT Read(int volIndex, const CLongAllocDesc &lad, Byte *buf);
  HRESULT ReadFromFile(int volIndex, const CItem &item, CByteBuffer &buf);

  HRESULT ReadFileItem(int volIndex, int fsIndex, const CLongAllocDesc &lad, int numRecurseAllowed);
  HRESULT ReadItem(int volIndex, int fsIndex, const CLongAllocDesc &lad, int numRecurseAllowed);

  HRESULT Open2();
  HRESULT FillRefs(CFileSet &fs, unsigned fileIndex, int parent, int numRecurseAllowed);

  UInt64 _processedProgressBytes;

  UInt64 _fileNameLengthTotal;
  unsigned _numRefs;
  UInt32 _numExtents;
  UInt64 _inlineExtentsSize;
  bool CheckExtent(int volIndex, int partitionRef, UInt32 blockPos, UInt32 len) const;

public:
  CObjectVector<CPartition> Partitions;
  CObjectVector<CLogVol> LogVols;

  CObjectVector<CItem> Items;
  CObjectVector<CFile> Files;

  unsigned SecLogSize;
  UInt64 PhySize;
  UInt64 FileSize;

  bool IsArc;
  bool Unsupported;
  bool UnexpectedEnd;
  bool NoEndAnchor;

  void UpdatePhySize(UInt64 val)
  {
    if (PhySize < val)
      PhySize = val;
  }
  HRESULT Open(IInStream *inStream, CProgressVirt *progress);
  void Clear();

  UString GetComment() const;
  UString GetItemPath(int volIndex, int fsIndex, int refIndex,
      bool showVolName, bool showFsName) const;

  bool CheckItemExtents(int volIndex, const CItem &item) const;
};

API_FUNC_IsArc IsArc_Udf(const Byte *p, size_t size);

}}
  
#endif
