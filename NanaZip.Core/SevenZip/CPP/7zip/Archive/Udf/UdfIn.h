// Archive/UdfIn.h -- UDF / ECMA-167

#ifndef ZIP7_INC_ARCHIVE_UDF_IN_H
#define ZIP7_INC_ARCHIVE_UDF_IN_H

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
// UDF 2.1.3

struct CDString32
{
  Byte Data[32];
  
  void Parse(const Byte *buf) { memcpy(Data, buf, sizeof(Data)); }
  UString GetString() const;
};

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
// UDF 2.1.4 timestamp

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


// ECMA 1/7.4 regid
// UDF 2.1.5 EntityID

struct CRegId
{
  Byte Flags;
  char Id[23];
  Byte Suffix[8];

  void Parse(const Byte *buf);
  void AddCommentTo(UString &s) const;
  void AddUdfVersionTo(UString &s) const;
};



// ---------- ECMA Part 3: Volume Structure ----------

// ECMA 3/7.1

struct CExtent
{
  UInt32 Len;
  UInt32 Pos; // logical sector number

  void Parse(const Byte *p);
};


// ECMA 3/10.1
// UDF 2.2.2 PrimaryVolumeDescriptor

struct CPrimeVol
{
  // UInt32 VolumeDescriptorSequenceNumber;
  UInt32 PrimaryVolumeDescriptorNumber;
  CDString32 VolumeId;
  UInt16 VolumeSequenceNumber;
  UInt16 MaximumVolumeSequenceNumber;
  // UInt16 InterchangeLevel;
  // UInt16 MaximumInterchangeLevel;
  // UInt32 CharacterSetList;
  // UInt32 MaximumCharacterSetList;
  CDString128 VolumeSetId;
  // charspec DescriptorCharacterSet; // (1/7.2.1)
  // charspec ExplanatoryCharacterSet; // (1/7.2.1)
  // CExtent VolumeAbstract;
  // CExtent VolumeCopyrightNotice;
  CRegId ApplicationId;
  CTime RecordingTime;
  CRegId ImplId;
  // bytes ImplementationUse
  // UInt32 PredecessorVolumeDescriptorSequenceLocation;
  // UInt16 Flags;

  void Parse(const Byte *p);
};


// ECMA 3/10.5
// UDF 2.2.14 PartitionDescriptor

struct CPartition
{
  UInt32 Pos;
  UInt32 Len;

  UInt16 Flags;
  UInt16 Number;
  CRegId ContentsId;
  // Byte ContentsUse[128];
  UInt32 AccessType;

  CRegId ImplId;
  // Byte ImplUse[128];

  // int VolIndex;
  CMap32 Map;

  bool IsMetadata;

  CPartition():
    //  VolIndex(-1),
    IsMetadata(false) {}

  // bool IsNsr() const { return (strncmp(ContentsId.Id, "+NSR0", 5) == 0); }
  // bool IsAllocated() const { return ((Flags & 1) != 0); }
};


// ECMA 4/7.1 lb_addr

struct CLogBlockAddr
{
  UInt32 Pos;
  UInt16 PartitionRef;
  
  void Parse(const Byte *p);
};


enum EShortAllocDescType
{
  SHORT_ALLOC_DESC_TYPE_RecordedAndAllocated = 0,
  SHORT_ALLOC_DESC_TYPE_NotRecordedButAllocated = 1,
  SHORT_ALLOC_DESC_TYPE_NotRecordedAndNotAllocated = 2,
  SHORT_ALLOC_DESC_TYPE_NextExtent = 3
};


// ECMA 4/14.14.1 short_ad

struct CShortAllocDesc
{
  UInt32 Len;
  UInt32 Pos;

  // UInt32 GetLen() const { return Len & 0x3FFFFFFF; }
  // UInt32 GetType() const { return Len >> 30; }
  // bool IsRecAndAlloc() const { return GetType() == SHORT_ALLOC_DESC_TYPE_RecordedAndAllocated; }
  void Parse(const Byte *p);
};

/*
struct CADImpUse
{
  UInt16 Flags;
  UInt32 UdfUniqueId;
  void Parse(const Byte *p);
};
*/

// ECMA 4/14.14.2 long_ad
// UDF 2.3.10.1

struct CLongAllocDesc
{
  UInt32 Len;
  CLogBlockAddr Location;
  
  // Byte ImplUse[6];
  // CADImpUse adImpUse; // UDF
  
  UInt32 GetLen() const { return Len & 0x3FFFFFFF; }
  UInt32 GetType() const { return Len >> 30; }
  bool IsRecAndAlloc() const { return GetType() == SHORT_ALLOC_DESC_TYPE_RecordedAndAllocated; }
  void Parse(const Byte *p);
};


// ECMA 3/10.7 Partition maps
// UDF 2.2.8-2.2.10 Partition Maps

struct CPartitionMap
{
  unsigned PartitionIndex;

  Byte Type;
  // Byte Len;

  // ECMA 10.7.2
  UInt16 VolumeSequenceNumber;
  UInt16 PartitionNumber;
  
  CRegId PartitionTypeId;

  // UDF 2.2.10 Metadata Partition Map
  UInt32 MetadataFileLocation;
  // UInt32 MetadataMirrorFileLocation;
  // UInt32 MetadataBitmapFileLocation;
  // UInt32 AllocationUnitSize; // (Blocks)
  // UInt16 AlignmentUnitSize; // (Blocks)
  // Byte Flags;

  // Byte Data[256];
  // CPartitionMap(): PartitionIndex(-1) {}
};


// ECMA 4/14.6.6

enum EIcbFileType
{
  ICB_FILE_TYPE_DIR = 4,
  ICB_FILE_TYPE_FILE = 5,

  ICB_FILE_TYPE_METADATA = 250,        // 2.2.13.1 Metadata File
  ICB_FILE_TYPE_METADATA_MIRROR = 251
};

enum EIcbDescriptorType
{
  ICB_DESC_TYPE_SHORT = 0,
  ICB_DESC_TYPE_LONG = 1,
  ICB_DESC_TYPE_EXTENDED = 2,
  ICB_DESC_TYPE_INLINE = 3
};

// ECMA 4/14.6
// UDF 3.3.2

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


// ECMA 4/14.4.3
// UDF 2.3.4.2 FileCharacteristics

// const Byte FILEID_CHARACS_Existance = (1 << 0);
const Byte FILEID_CHARACS_Dir     = (1 << 1);
const Byte FILEID_CHARACS_Deleted = (1 << 2);
const Byte FILEID_CHARACS_Parent  = (1 << 3);
// const Byte FILEID_CHARACS_Metadata = (1 << 4);

struct CFile
{
  int ItemIndex;
  // UInt16 FileVersion;
  // Byte FileCharacteristics;
  // CByteBuffer ImplUse;
  CDString Id;

  CFile(): /* FileVersion(0), FileCharacteristics(0), */ ItemIndex(-1) {}
  UString GetName() const { return Id.GetString(); }
};


struct CMyExtent
{
  UInt32 Pos;
  UInt32 Len;
  unsigned PartitionRef; // index in CLogVol::PartitionMaps
  
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
  UInt16 FileLinkCount;
  // Byte RecordFormat;
  // Byte RecordDisplayAttr;
  // UInt32 RecordLen;
  UInt64 Size;
  UInt64 NumLogBlockRecorded;
  // UInt64 ObjectSize;

  CTime ATime;
  CTime MTime;
  CTime AttribTime; // Attribute time : most recent date and time of the day of file creation or modification of the attributes of.
  CTime CreateTime;
  // UInt32 CheckPoint;
  // CLongAllocDesc ExtendedAttrIcb;
  // CRegId ImplId;
  // UInt64 UniqueId;

  bool IsExtended;
  bool IsInline;
  CByteBuffer InlineData;
  CRecordVector<CMyExtent> Extents;
  CUIntVector SubFiles;

  void Parse(const Byte *p);

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
  unsigned FileIndex;
  int Parent;
};


// ECMA 4 / 14.1
struct CFileSet
{
  CRecordVector<CRef> Refs;

  CTime RecordingTime;
  // UInt16 InterchangeLevel;
  // UInt16 MaxInterchangeLevel;
  UInt32 FileSetNumber;
  UInt32 FileSetDescNumber;
  CDString128 LogicalVolumeId;
  CDString32 Id;
  CDString32 CopyrightId;
  CDString32 AbstractId;

  CLongAllocDesc RootDirICB;
  CRegId DomainId;
  // CLongAllocDesc SystemStreamDirICB;
};


/* 8.3 Volume descriptors
8.4
A Volume Descriptor Sequence:
 shall contain one or more Primary Volume Descriptors.
*/

// ECMA 3/10.6
// UDF 2.2.4  LogicalVolumeDescriptor

struct CLogVol
{
  CObjectVector<CPartitionMap> PartitionMaps;
  CObjectVector<CFileSet> FileSets;

  UInt32 BlockSize;
  CDString128 Id;
  CRegId DomainId;
  
  // Byte ContentsUse[16];
  CLongAllocDesc FileSetLocation; // UDF

  CRegId ImplId;
  // Byte ImplUse[128];
  // CExtent IntegritySequenceExtent;

  UString GetName() const { return Id.GetString(); }
};


Z7_PURE_INTERFACES_BEGIN
struct Z7_DECLSPEC_NOVTABLE CProgressVirt
{
  virtual HRESULT SetTotal(UInt64 numBytes) =0; \
  virtual HRESULT SetCompleted(UInt64 numFiles, UInt64 numBytes) =0; \
  virtual HRESULT SetCompleted() =0; \
};
Z7_PURE_INTERFACES_END

class CInArchive
{
public:
  CObjectVector<CLogVol> LogVols;
  CObjectVector<CItem> Items;
  CObjectVector<CFile> Files;
  CObjectVector<CPartition> Partitions;

  unsigned SecLogSize;
  UInt64 PhySize;
  UInt64 FileSize;

  bool IsArc;
  bool Unsupported;
  bool UnexpectedEnd;
  bool NoEndAnchor;

  CObjectVector<CPrimeVol> PrimeVols;

  HRESULT Open(IInStream *inStream, CProgressVirt *progress);
  void Clear();

  UString GetComment() const;
  UString GetItemPath(unsigned volIndex, unsigned fsIndex, unsigned refIndex,
      bool showVolName, bool showFsName) const;

  bool CheckItemExtents(unsigned volIndex, const CItem &item) const;

private:
  IInStream *_stream;
  CProgressVirt *_progress;

  HRESULT Read(unsigned volIndex, unsigned partitionRef, UInt32 blockPos, UInt32 len, Byte *buf);
  HRESULT ReadLad(unsigned volIndex, const CLongAllocDesc &lad, Byte *buf);
  HRESULT ReadFromFile(unsigned volIndex, const CItem &item, CByteBuffer &buf);

  HRESULT ReadFileItem(unsigned volIndex, unsigned fsIndex, const CLongAllocDesc &lad, bool isDir, int numRecurseAllowed);
  HRESULT ReadItem(unsigned volIndex, int fsIndex, const CLongAllocDesc &lad, bool isDir, int numRecurseAllowed);

  HRESULT Open2();
  HRESULT FillRefs(CFileSet &fs, unsigned fileIndex, int parent, int numRecurseAllowed);

  UInt64 _processedProgressBytes;

  UInt64 _fileNameLengthTotal;
  unsigned _numRefs;
  UInt32 _numExtents;
  UInt64 _inlineExtentsSize;
  bool CheckExtent(unsigned volIndex, unsigned partitionRef, UInt32 blockPos, UInt32 len) const;

  void UpdatePhySize(UInt64 val)
  {
    if (PhySize < val)
      PhySize = val;
  }

  void UpdatePhySize(const CExtent &e)
  {
    UpdatePhySize(((UInt64)e.Pos << SecLogSize) + e.Len);
  }
};

API_FUNC_IsArc IsArc_Udf(const Byte *p, size_t size);

}}
  
#endif
