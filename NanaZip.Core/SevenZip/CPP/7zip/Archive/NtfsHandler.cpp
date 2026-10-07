// NtfsHandler.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO
// #define SHOW_DEBUG_INFO2

#if defined(SHOW_DEBUG_INFO) || defined(SHOW_DEBUG_INFO2)
#include <stdio.h>
#endif

#include "../../../C/CpuArch.h"
#include "../../../C/RotateDefs.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer2.h"
#include "../../Common/MyCom.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/MethodProps.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/DummyOutStream.h"

#ifdef SHOW_DEBUG_INFO
#define PRF(x) x
#else
#define PRF(x)
#endif

#ifdef SHOW_DEBUG_INFO2
#define PRF2(x) x
#else
#define PRF2(x)
#endif

#define PRF_UTF16(x) PRF(printf("%ls", x.GetRawPtr());)
#if defined(Z7_MSC_VER_ORIGINAL)
#define PRINT_UI64(s, val)    PRF( printf(s " = %9I64x", (UInt64)(val));)
#define PRINT_UI64_2(s, val)  PRF2(printf(s " = %9I64x", (UInt64)(val));)
#else
#define PRINT_UI64(s, val)    PRF( printf(s " = %9llx", (unsigned long long)(val));)
#define PRINT_UI64_2(s, val)  PRF2(printf(s " = %9llx", (unsigned long long)(val));)
#endif

// data pointers in all Parse() functions are aligned for 8-bytes (UInt64) accesses.
#if 1 // 0 for debug
#define Get16(p) GetUi16a(p)
#define Get32(p) GetUi32a(p)
#define Get64(p) GetUi64a(p)
#else
static UInt16 Get16(const void *p) { if ((ptrdiff_t)p & 1) throw 1; return GetUi16a(p); }
static UInt32 Get32(const void *p) { if ((ptrdiff_t)p & 3) throw 1; return GetUi32a(p); }
static UInt64 Get64(const void *p) { if ((ptrdiff_t)p & 7) throw 1; return GetUi64a(p); }
#endif
#define G16(p, dest) dest = Get16(p)
#define G32(p, dest) dest = Get32(p)
#define G64(p, dest) dest = Get64(p)

using namespace NWindows;

namespace NArchive {
namespace Ntfs {

static const wchar_t * const kVirtualFolder_System = L"[SYSTEM]";
static const wchar_t * const kVirtualFolder_Lost_Normal = L"[LOST]";
static const wchar_t * const kVirtualFolder_Lost_Deleted = L"[UNKNOWN]";
static const unsigned kNumSysRecs = 16;

static const unsigned kRecIndex_Volume    = 3;
static const unsigned kRecIndex_RootDir   = 5;
static const unsigned kRecIndex_BadClus   = 8;
static const unsigned kRecIndex_Security  = 9;

static const Byte k_Signature[] = { 'N', 'T', 'F', 'S', ' ', ' ', ' ', ' ', 0 };

struct CHeader
{
  unsigned SectorSizeLog;
  unsigned ClusterSizeLog;
  unsigned MftRecordSizeLog;
  // Byte MediaType;
  // UInt32 NumHiddenSectors;
  UInt64 NumSectors;
  UInt64 NumClusters;
  UInt64 MftCluster;
  UInt64 SerialNumber;
  // UInt16 SectorsPerTrack;
  // UInt16 NumHeads;

  UInt64 GetPhySize_Clusters() const { return NumClusters << ClusterSizeLog; }
  UInt64 GetPhySize_Max() const { return (NumSectors + 1) << SectorSizeLog; }
  UInt32 ClusterSize() const { return (UInt32)1 << ClusterSizeLog; }
  bool Parse(const Byte *p);
};

static unsigned GetLog(const UInt32 num)
{
  unsigned i;
  for (i = 0; i < 31; i++)
    if ((UInt32)1 << i == num)
      break;
  return i;
}

// (p) is aligned for 8-bytes (UInt64) accesses
bool CHeader::Parse(const Byte * const p)
{
  if (Get16(p + 0x1FE) != 0xAA55)
    return false;
  if (p[0] != 0xE9 && (p[0] != 0xEB || p[2] != 0x90))
    return false;
  if (memcmp(p + 3, k_Signature, sizeof(k_Signature)))
    return false;
  {
    for (size_t i = 14; i < 21; i++)
      if (p[i])
        return false;
  }
  {
    const unsigned t = GetLog(GetUi16(p + 11)); // unaligned
    if (t < 9 || t > 12)
      return false;
    SectorSizeLog = (unsigned)t;
  }
  unsigned sectorsPerClusterLog;
  {
    const unsigned v = p[13];
    if (v <= 0x80)
      sectorsPerClusterLog = GetLog(v);
    else
      sectorsPerClusterLog = 0x100 - v;
    ClusterSizeLog = SectorSizeLog + sectorsPerClusterLog;
    if (ClusterSizeLog > 21)
      return false;
  }
  if (p[21] != 0xF8) // MediaType == 0xF8 : a hard disk (Fixed_Disk), 0xF0 : high-density 3.5-inch floppy disk
    return false;
  if (*(const UInt16 *)(const void *)(p + 22)) // NumFatSectors : Get16()
    return false;
  // G16(p + 24, SectorsPerTrack); // 63 usually
  // G16(p + 26, NumHeads); // 255
  // G32(p + 28, NumHiddenSectors); // 63 (XP) / 2048 (Vista and win7) / (0 on media that are not partitioned ?)
  if (*(const UInt32 *)(const void *)(p + 32)) // NumSectors32 : Get32()
    return false;
  // DriveNumber = p[0x24];
  if (p[0x25]) // CurrentHead
    return false;
  /*
  NTFS-HDD:   p[0x26] = 0x80
  NTFS-FLASH: p[0x26] = 0
  */
  if (p[0x26] != 0x80 && p[0x26] != 0) // ExtendedBootSig
    return false;
  if (p[0x27]) // reserved
    return false;
  
  G64(p + 0x28, NumSectors);
  if (NumSectors >= ((UInt64)1 << (62 - SectorSizeLog)))
    return false;
  NumClusters = NumSectors >> sectorsPerClusterLog;
  G64(p + 0x30, MftCluster);   // $MFT
  if (MftCluster > NumClusters)
    return false;
  // G64(p + 0x38, Mft2Cluster);
  G64(p + 0x48, SerialNumber); // $MFTMirr
  /*
    numClusters_per_MftRecord:
    numClusters_per_IndexBlock:
    only low byte from 4 bytes is used. Another 3 high bytes are zeros.
      If the number is positive (number < 0x80),
          then it represents the number of clusters.
      If the number is negative (number >= 0x80),
          then the size of the file record is 2 raised to the absolute value of this number.
          example: (0xF6 == -10) means 2^10 = 1024 bytes.
  */
  {
    UInt32 numClusters_per_MftRecord;
    G32(p + 0x40, numClusters_per_MftRecord);
    if (numClusters_per_MftRecord <= 0x7f)
      MftRecordSizeLog = GetLog(numClusters_per_MftRecord) + ClusterSizeLog;
    else
      MftRecordSizeLog = 0x100 - numClusters_per_MftRecord;
#define k_MftRecordSizeLog_MAX  12
    if (MftRecordSizeLog < SectorSizeLog || MftRecordSizeLog > k_MftRecordSizeLog_MAX)
      return false;
  }
  {
    UInt32 numClusters_per_IndexBlock;
    G32(p + 0x44, numClusters_per_IndexBlock);
    return numClusters_per_IndexBlock < 0x100;
  }
}


struct CMftRef
{
  UInt64 Val;
  
  UInt64 GetIndex() const { return Val & (((UInt64)1 << 48) - 1); }
  UInt16 GetNumber() const { return (UInt16)(Val >> 48); }
  bool IsBaseItself() const { return Val == 0; }

  CMftRef(): Val(0) {}
};

#define ATNAME(n) ATTR_TYPE_ ## n
#define DEF_ATTR_TYPE(v, n) ATNAME(n) = v

enum
{
  DEF_ATTR_TYPE(0x00, UNUSED),
  DEF_ATTR_TYPE(0x10, STANDARD_INFO),
  DEF_ATTR_TYPE(0x20, ATTRIBUTE_LIST),
  DEF_ATTR_TYPE(0x30, FILE_NAME),
  DEF_ATTR_TYPE(0x40, OBJECT_ID),
  DEF_ATTR_TYPE(0x50, SECURITY_DESCRIPTOR),
  DEF_ATTR_TYPE(0x60, VOLUME_NAME),
  DEF_ATTR_TYPE(0x70, VOLUME_INFO),
  DEF_ATTR_TYPE(0x80, DATA),
  DEF_ATTR_TYPE(0x90, INDEX_ROOT),
  DEF_ATTR_TYPE(0xA0, INDEX_ALLOCATION),
  DEF_ATTR_TYPE(0xB0, BITMAP),
  DEF_ATTR_TYPE(0xC0, REPARSE_POINT),
  DEF_ATTR_TYPE(0xD0, EA_INFO),
  DEF_ATTR_TYPE(0xE0, EA),
  DEF_ATTR_TYPE(0xF0, PROPERTY_SET),
  DEF_ATTR_TYPE(0x100, LOGGED_UTILITY_STREAM),
  DEF_ATTR_TYPE(0x1000, FIRST_USER_DEFINED_ATTRIBUTE)
};


/* WinXP-64:
    Probably only one short name (dos name) per record is allowed.
    There are no short names for hard links.
   The pair (Win32,Dos) can be in any order.
   Posix name can be after or before Win32 name
*/
// static const Byte kFileNameType_Posix     = 0; // for hard links, also used for usual files by Windows.
static const Byte kFileNameType_Win32     = 1; // after Dos name
static const Byte kFileNameType_Dos       = 2; // short name
static const Byte kFileNameType_Win32Dos  = 3; // short and full name are same

struct CFileNameAttr
{
  CMftRef ParentDirRef;
  UString2 Name;
  Byte NameType;
  // These values may be outdated, as they are not updated instantly by system.
  // Probably these timestamps are not too useful.
  // UInt64 CTime;
  // UInt64 MTime;
  // UInt64 ThisRecMTime;  // xp-64: the time of previous name change (not last name change. why?)
  // UInt64 ATime;
  // UInt64 AllocatedSize;
  // UInt64 DataSize;
  // UInt16 PackedEaSize;
  // UInt32 ReparseTag;
  // UInt32 NameAttrib;
  
  bool IsDos() const { return NameType == kFileNameType_Dos; }
  bool IsWin32() const { return (NameType == kFileNameType_Win32); }

  bool ParseFileNameAttr(const Byte *p, unsigned size);

  CFileNameAttr():
      // NameAttrib(0),
      NameType(0)
      {}
};

// (p) is aligned for 2-bytes
static void GetString(const Byte * const p, const size_t len, UString2 &res)
{
  if (len == 0 && res.IsEmpty())
    return;
  wchar_t *s = res.GetBuf((unsigned)len);
  size_t i;
  for (i = 0; i < len; i++)
  {
    const wchar_t c = Get16(p + i * 2);
    if (c == 0)
      break; // we ignore this error
    s[i] = c;
  }
  s[i] = 0;
  res.ReleaseBuf_SetLen((unsigned)i);
}

// Name is empty
bool CFileNameAttr::ParseFileNameAttr(const Byte * const p, const unsigned size)
{
  if (size < 0x42)
    return false;
  G64(p + 0x00, ParentDirRef.Val);
  // G64(p + 0x08, CTime);
  // G64(p + 0x10, MTime);
  // G64(p + 0x18, ThisRecMTime);
  // G64(p + 0x20, ATime);
  // G64(p + 0x28, AllocatedSize);
  // G64(p + 0x30, DataSize);
  // G32(p + 0x38, NameAttrib); // similar to file attributes, but flag for directory is (1 << 28)
  // G16(p + 0x3C, PackedEaSize);
  // G32(p + 0x3C, ReparseTag);
  NameType = p[0x41];
  const unsigned len = p[0x40];
  if (0x42 + len * 2 > size)
    return false;
  if (len != 0)
    GetString(p + 0x42, len, Name);
  return true;
}


struct CSiAttr
{
  UInt64 CTime;
  UInt64 MTime;
  UInt64 ThisRecMTime;
  UInt64 ATime;
  UInt32 Attrib;
  /*
  UInt32 MaxVersions;
  UInt32 Version;
  UInt32 ClassId;
  UInt32 OwnerId;
  */
  UInt32 SecurityId; // SecurityId == 0 in kRecIndex_RootDir MFT record
  // UInt64 QuotaCharged;

  bool Parse(const Byte *p, unsigned size);

  CSiAttr():
      CTime(0),
      MTime(0),
      ThisRecMTime(0),
      ATime(0),
      Attrib(0),
      SecurityId(0)
      {}
};

bool CSiAttr::Parse(const Byte * const p, unsigned size)
{
  if (size < 0x24)
    return false;
  G64(p + 0x00, CTime);
  G64(p + 0x08, MTime);
  G64(p + 0x10, ThisRecMTime);
  G64(p + 0x18, ATime);
  G32(p + 0x20, Attrib);
  SecurityId = 0;
  if (size >= 0x38)
    G32(p + 0x34, SecurityId);
  return true;
}


static const UInt64 kEmptyExtent = (UInt64)0-1;

struct CExtent
{
  UInt64 Virt;
  UInt64 Phy;

  bool IsEmpty() const { return Phy == kEmptyExtent; }
};

struct CAttr
{
  unsigned NextAttrIndex; /* DataAttrs[NextAttrIndex] is nearest
        item with a different name, skipping items with identical names. */
  Byte NonResident;

  // Non-Resident
  Byte CompressionUnit;

  // UInt32 Len;
  UString2 Name;
  // UInt16 Flags;
  // UInt16 Instance;
  CByteBuffer Data;

  // Non-Resident
  UInt64 LowVcn;
  UInt64 HighVcn;
  UInt64 AllocatedSize;
  UInt64 Size;
  UInt64 PackSize;
  UInt64 InitializedSize;

  // Resident
  // UInt16 ResidentFlags;

// We support only 2 values for CompressionUnit: 0 (no compression) and 4, which means 16 clusters.
#define COMPRESSION_UNIT_VAL_4  4
  bool IsCompressionUnitSupported() const { return CompressionUnit == 0 || CompressionUnit == COMPRESSION_UNIT_VAL_4; }

  UInt32 ParseAttr(const Byte *p, unsigned size, UInt32 &type);
  bool ParseFileName(CFileNameAttr &a) const { return a.ParseFileNameAttr(Data, (unsigned)Data.Size()); }
  bool ParseSi(CSiAttr &a) const { return a.Parse(Data, (unsigned)Data.Size()); }
  bool ParseExtents(CRecordVector<CExtent> &extents, UInt64 numClustersMax, unsigned compressionUnit) const;
  UInt64 GetSize() const { return NonResident ? Size : Data.Size(); }
  UInt64 GetPackSize() const
  {
    if (!NonResident)
      return Data.Size();
    if (CompressionUnit != 0)
      return PackSize;
    return AllocatedSize;
  }
  CAttr():
    // NextAttrIndex(0), // optional
    LowVcn((UInt64)0-1) // used by CompareAttr()
    {}
};

#define RINOZ(x) { int _tt_ = (x); if (_tt_ != 0) return _tt_; }

static int CompareAttr(void *const *elem1, void *const *elem2, void *)
{
  const CAttr &a1 = *(*((const CAttr *const *)elem1));
  const CAttr &a2 = *(*((const CAttr *const *)elem2));
  if (a1.Name.IsEmpty())
  {
    if (!a2.Name.IsEmpty())
      return -1;
  }
  else if (a2.Name.IsEmpty())
    return 1;
  else
  {
    RINOZ(a1.Name.Compare(a2.Name.GetRawPtr()))
  }
  return MyCompare(a1.LowVcn, a2.LowVcn);
}


/*
in: CAttr is empty after constructor
return: len_of_attr : len_of_attr % 8 == 0
*/
unsigned CAttr::ParseAttr(const Byte * const p, const unsigned size, UInt32 &type)
{
  if (size < 4)
    return 0;
  G32(p, type);
  if (type == 0xFFFFFFFF)
    return 8; // required size is 4, but attributes are 8 bytes aligned. So we return 8
  if (size < 0x18)
    return 0;
  PRF(printf(" T=%2X", type));
  
  const UInt32 len = Get32(p + 4);
  PRF(printf(" L=%3u", len));
  if (len > size || (len & 7))
    return 0;
  NonResident = p[8];
  {
    const unsigned nameLen = p[9];
    const UInt32 nameOffset = Get16(p + 0x0A);
    if (nameOffset & 1) // v26.04
      return 0;
    if (nameLen != 0)
    {
      if (nameOffset + nameLen * 2 > len)
        return 0;
      GetString(p + nameOffset, nameLen, Name);
      PRF(printf(" N="));
      PRF_UTF16(Name)
    }
  }
  // G16(p + 0x0C, Flags);
  // G16(p + 0x0E, Instance);
  // PRF(printf(" F=%4X", Flags));
  // PRF(printf(" Inst=%d", Instance));

  UInt32 dataSize, offs;
  if (NonResident)
  {
    if (len < 0x40)
      return 0;
    PRF(printf(" NR"));
    G64(p + 0x10, LowVcn);
    G64(p + 0x18, HighVcn);
    G64(p + 0x28, AllocatedSize);
    G64(p + 0x30, Size);
    PackSize = Size;
    G64(p + 0x38, InitializedSize);
    G16(p + 0x20, offs);
    CompressionUnit = p[0x22];
    if (CompressionUnit != 0)
    {
      if (len < 0x48)
        return 0;
      G64(p + 0x40, PackSize);
      PRINT_UI64(" PS", PackSize)
    }
    // PRF(printf("\n"));
    PRINT_UI64(" ASize", AllocatedSize)
    PRINT_UI64(" Size", Size)
    PRINT_UI64(" IS", InitializedSize)
    PRINT_UI64(" Low", LowVcn)
    PRINT_UI64(" High", HighVcn)
    PRF(printf(" CU=%u", (unsigned)CompressionUnit);)
    dataSize = len - offs;
  }
  else
  {
    if (len < 0x18)
      return 0;
    G32(p + 0x10, dataSize);
    G16(p + 0x14, offs);
    // G16(p + 0x16, ResidentFlags);
    PRF(printf(" RES"));
    PRF(printf(" dataSize=%3u", dataSize));
    // PRF(printf(" ResFlags=%4X", ResidentFlags));
  }
  
  if (len < offs || len - offs < dataSize)
    return 0;
  /* we don't check alignment for (offs),
     because we copy the data to aligned (Data) buffer.
     So we will access the data of attribute via aligned (Data) */
  Data.CopyFrom(p + offs, dataSize);
  
  #ifdef SHOW_DEBUG_INFO
  PRF(printf("  : "));
  for (unsigned i = 0; i < Data.Size(); i++)
  {
    PRF(printf(" %02X", (unsigned)Data[i]));
  }
  #endif
  
  return len;
}


/*
in  : (Extents.Back().IsEmpty() == true)
out : (Extents.Back().IsEmpty() == true), if function returns true.
      if function returns false, then there is error and Extents[] are not valid.
*/
bool CAttr::ParseExtents(CRecordVector<CExtent> &extents,
    const UInt64 numClustersMax, const unsigned compressionUnit) const
{
  PRINT_UI64_2("\n# ParseExtents LowVcn", LowVcn)
  PRINT_UI64_2(" HighVcn", HighVcn)
  UInt64 vcn = LowVcn;
  const UInt64 highVcn1 = HighVcn + 1;
  if (vcn >= highVcn1)
  {
    if (vcn)
      return false;
    // (vcn == 0 && highVcn1 == 0)
    /* it's allowed empty Non-Resident file:
       "[SYSTEM]\$Extend\$RmMetadata\$Repair" and other */
  }
  if (highVcn1 >= ((UInt64)1 << 63) || vcn != extents.Back().Virt)
    return false;

  extents.DeleteBack();
  const Byte *p = Data;
  unsigned size = (unsigned)Data.Size();
  UInt64 lcn = 0;
  for (;;)
  {
    if (size == 0)
    {
      // return false; // no end marker. Do we need to exit with error?
      break;
    }
    size--;
    const unsigned b = *p++;
    if (b == 0)
      break;
    unsigned num = b & 0xF;
    if (num == 0 || num > 8 || num > size)
      return false;
    CExtent e;
    {
      UInt64 v = 0;
      {
        size_t i = num;
        do v = (v << 8) | p[--i]; while (i);
      }
      if (v == 0)
        return false;
      p += num;
      size -= num;
      if (highVcn1 - vcn < v)
        return false;
      e.Virt = vcn;
      vcn += v;
    }

    num = b >> 4;
    if (num == 0)
    {
      // no LCN : sparse or compressed.
      /* if Unit is compressed, it can have many Elements for each compressed Unit:
         and last Element for unit MUST be without LCN.
           Element 0: numCompressedClusters2, LCN_0
           Element 1: numCompressedClusters2, LCN_1
           ...
           Last Element : (16 - total_clusters_in_previous_elements), no LCN
      */
      if (compressionUnit == 0)
        return false; // we need test examples to test case of Sparse with (compressionUnit == 0).
      e.Phy = kEmptyExtent;
    }
    else
    {
      // num != 0
      if (num > 8 || num > size)
        return false;
      size_t i = num - 1;
      UInt64 v = (UInt64)(Int64)(signed char)p[i];
      while (i)
        v = (v << 8) | p[--i];
      p += num;
      size -= num;
      lcn += v;
      if (lcn > numClustersMax)
        return false;
      e.Phy = lcn;
    }
    extents.Add(e);
  }

  CExtent e;
  e.Phy = kEmptyExtent;
  e.Virt = vcn;
  extents.Add(e);
  return highVcn1 == vcn;
}


static const UInt64 kEmptyTag = (UInt64)0-1;
/* Big cache can be faster for non-sequential access to same chunks.
   We use small cache that is better for sequential access: */
static const unsigned kNumCacheChunksLog = 1; // [0,4]
static const size_t kNumCacheChunks = (size_t)1 << kNumCacheChunksLog;

Z7_CLASS_IMP_IInStream(
  CInStream
)
  UInt64 _virtPos;
  UInt64 _physPos;
  UInt64 _curRem;
  bool _sparseMode;
public:
  bool InUse;
private:
  unsigned _chunkSizeLog;
  CAlignedBuffer _inBuf;
  CAlignedBuffer _outBuf;
public:
  UInt64 Size;
  UInt64 InitializedSize;
  unsigned BlockSizeLog;
  unsigned CompressionUnit; // 0 or 4
  CRecordVector<CExtent> Extents;
  CMyComPtr<IInStream> Stream;
private:
  UInt64 _tags[kNumCacheChunks];

  HRESULT SeekToPhys() { return InStream_SeekSet(Stream, _physPos); }
  size_t GetCuSize() const { return (size_t)1 << (BlockSizeLog + CompressionUnit); }
public:

  void Clear()
  {
    Extents.Clear();
    Stream.Release();
  }
 
  HRESULT InitAndSeek(const unsigned compressionUnit)
  {
    CompressionUnit = compressionUnit;
    _chunkSizeLog = BlockSizeLog + CompressionUnit;
    if (compressionUnit != 0)
    {
      const size_t cuSize = GetCuSize();
      _inBuf.Alloc(cuSize);
      if (!_inBuf.IsAllocated())
        return E_OUTOFMEMORY;
      const size_t cacheSize = kNumCacheChunks << _chunkSizeLog;
      // if ((cacheSize >> _chunkSizeLog) != kNumCacheChunks) return E_OUTOFMEMORY;
      _outBuf.Alloc(cacheSize);
      if (!_outBuf.IsAllocated())
        return E_OUTOFMEMORY;
    }
    for (size_t i = 0; i < kNumCacheChunks; i++)
      _tags[i] = kEmptyTag;
    _sparseMode = false;
    _curRem = 0;
    _virtPos = 0;
    _physPos = 0;
    const CExtent &e = Extents[0];
    if (!e.IsEmpty())
      _physPos = e.Phy << BlockSizeLog;
    return SeekToPhys();
  }
};


#define LZNT_CHUNK_SIZE (1 << 12)
#if LZNT_CHUNK_SIZE > (512 << COMPRESSION_UNIT_VAL_4)
  #error Stop_Compiling_Bad_COMPRESSION_UNIT
#endif
/*
in:
  dest[] must have space for (dest_up_size) bytes:
    dest_up_size = (destLen) rounded up to the nearest multiple of LZNT_CHUNK_SIZE.
    dest_up_size = (destLen + LZNT_CHUNK_SIZE - 1) & ~(LZNT_CHUNK_SIZE - 1)
return:
  destSize == 0 : no data was decoded, or there is some error in compressed stream.
  destSize <  destLen : some chunks were decoded, but there is error in last chunk or unavailable input data.
  destSize == destLen : end of output stream (destLen) was reached after chunk decoding.
  destSize >  destLen && destSize <= dest_up_size : is also normal case.
*/
static size_t Lznt1Dec(Byte * const dest, const size_t destLen,
    const Byte *src, size_t srcLen)
{
  size_t destSize = 0;
  while (destSize < destLen)
  {
    // we check that last decoded chunk contained (LZNT_CHUNK_SIZE) bytes:
    if (srcLen < 2 || (destSize & (LZNT_CHUNK_SIZE - 1)))
      break;
    unsigned comprSize;
    {
      const unsigned v = GetUi16(src);
      if (v == 0) // end_of_stream marker
        break;
      // bits [12:14] : chunk_size_order : (chunk_size = 512 << chunk_size_order)
      // we support only (chunk_size_order == 3) and (chunk_size = 4096)
      if ((v & 0x7000) != 0x3000) // v26.04 check: (chunk_size_order == 3)
        return 0; // break;
      src += 2;
      srcLen -= 2;
      comprSize = (v & 0xFFF) + 1;
      if (srcLen < comprSize)
        break; // unavailable data
      srcLen -= comprSize;
      if ((v & 0x8000) == 0)
      {
        /* win10 probably produces full (LZNT_CHUNK_SIZE) chunk even for last final partial chunk.
           so we use it to check for errors: */
        if (comprSize != LZNT_CHUNK_SIZE)
          break;
        memcpy(dest + destSize, src, comprSize);
        src += comprSize;
        destSize += comprSize;
        continue;
      }
    }
    {
      if (comprSize > 1 && (src[0] & 1)) // we check that first LZ-symbol is LITERAL
        return 0;
      unsigned numDistBits = 4;
      unsigned sbOffset = 0;
      do
      {
        unsigned mask = *src++ | 0x100;
        if (--comprSize == 0)
        {
          // is it error case or normal case?
          // that case can allow some padding data in compressed stream.
          break; // by LZNT specification : it's allowed case
          // return 0; // more stict check
        }
        do
        {
          if ((mask & 1) == 0)
          {
            if (sbOffset >= LZNT_CHUNK_SIZE)
              return 0;
            sbOffset++;
            dest[destSize++] = *src++;
            comprSize--;
          }
          else
          {
            if (comprSize < 2)
              return 0;
            comprSize -= 2;
            const unsigned v = GetUi16(src);
            src += 2;

            while (((sbOffset - 1) >> numDistBits) != 0)
              numDistBits++;

            const unsigned len = (v & (0xFFFF >> numDistBits)) + 3;
            if (sbOffset + len > LZNT_CHUNK_SIZE)
              return 0;
            const unsigned dist = v >> (16 - numDistBits);
            if (dist >= sbOffset)
              return 0;
            const size_t offs = 1 + dist;
            Byte *p = dest + destSize - offs;
            destSize += len;
            sbOffset += len;
            const Byte *lim = p + len;
            p[offs] = *p; ++p;
            p[offs] = *p; ++p;
            do
              p[offs] = *p;
            while (++p != lim);
          }
        }
        while ((mask >>= 1) > 1 && comprSize);
      }
      while (comprSize);
#if 0 // 1 for debug
      /* win10 probably produces full (LZNT_CHUNK_SIZE) chunk even for last final partial chunk.
         But we are not sure that all LZNT1 encoders do it.
         So we don't use the following check: */
      if (sbOffset != LZNT_CHUNK_SIZE)
        break;
#endif
    }
  }
  return destSize;
}


Z7_COM7F_IMF(CInStream::Read(void * const data, UInt32 size, UInt32 * const processedSize))
{
  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;
  if (_virtPos >= Size)
    return S_OK; // return (Size == _virtPos) ? S_OK: E_FAIL;
  {
    const UInt64 rem = Size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }
  if (_virtPos >= InitializedSize)
  {
    memset(data, 0, size);
    _virtPos += size;
    if (processedSize)
      *processedSize = size;
    return S_OK;
  }
  {
    const UInt64 rem = InitializedSize - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }

  while (_curRem == 0)
  {
    const UInt64 cacheTag = _virtPos >> _chunkSizeLog;
    const size_t cacheIndex = (size_t)cacheTag & (kNumCacheChunks - 1);
    
    if (_tags[cacheIndex] == cacheTag)
    {
      const size_t chunkSize = (size_t)1 << _chunkSizeLog;
      const size_t offset = (size_t)_virtPos & (chunkSize - 1);
      size_t cur = chunkSize - offset;
      if (cur > size)
        cur = size;
      memcpy(data, _outBuf + (cacheIndex << _chunkSizeLog) + offset, cur);
      if (processedSize)
        *processedSize = (UInt32)cur;
      _virtPos += cur;
      return S_OK;
    }

    PRINT_UI64_2("\nVirtPos", _virtPos)
    
    const UInt32 comprUnitSize = (UInt32)1 << CompressionUnit;
    const UInt64 virtBlock = _virtPos >> BlockSizeLog;
    const UInt64 virtBlock2 = virtBlock & ~((UInt64)comprUnitSize - 1);
    
    unsigned left = 0, right = Extents.Size();
    for (;;)
    {
      const unsigned mid = (left + right) / 2;
      if (mid == left)
        break;
      if (virtBlock2 < Extents[mid].Virt)
        right = mid;
      else
        left = mid;
    }
    
    bool isCompressed = false;
    const UInt64 virtBlock2End = virtBlock2 + comprUnitSize;
    if (CompressionUnit != 0)
      for (unsigned i = left; i < Extents.Size(); i++)
      {
        const CExtent &e = Extents[i];
        if (e.Virt >= virtBlock2End)
          break;
        if (e.IsEmpty())
        {
          isCompressed = true;
          break;
        }
      }

    unsigned i;
    for (i = left; Extents[i + 1].Virt <= virtBlock; i++);
    
    _sparseMode = false;
    if (!isCompressed)
    {
      const CExtent &e = Extents[i];
      const UInt64 newPos = (e.Phy << BlockSizeLog) + _virtPos - (e.Virt << BlockSizeLog);
      if (newPos != _physPos)
      {
        _physPos = newPos;
        RINOK(SeekToPhys())
      }
      UInt64 next = Extents[i + 1].Virt;
      if (next > virtBlock2End)
        next &= ~((UInt64)comprUnitSize - 1);
      next <<= BlockSizeLog;
      if (next > Size)
        next = Size;
      _curRem = next - _virtPos;
      break;
    }
    
    bool thereArePhy = false;
    
    for (unsigned i2 = left; i2 < Extents.Size(); i2++)
    {
      const CExtent &e = Extents[i2];
      if (e.Virt >= virtBlock2End)
        break;
      if (!e.IsEmpty())
      {
        thereArePhy = true;
        break;
      }
    }
    
    if (!thereArePhy)
    {
      _curRem = (Extents[i + 1].Virt << BlockSizeLog) - _virtPos;
      _sparseMode = true;
      break;
    }
    
    size_t offs = 0;
    UInt64 curVirt = virtBlock2;
    
    for (i = left; i < Extents.Size(); i++)
    {
      const CExtent &e = Extents[i];
      if (e.IsEmpty())
        break;
      if (e.Virt >= virtBlock2End)
        return S_FALSE;
      const UInt64 newPos = (e.Phy + (curVirt - e.Virt)) << BlockSizeLog;
      if (newPos != _physPos)
      {
        _physPos = newPos;
        RINOK(SeekToPhys())
      }
      UInt64 numChunks = Extents[i + 1].Virt - curVirt;
      if (curVirt + numChunks > virtBlock2End)
        numChunks = virtBlock2End - curVirt;
      const size_t compressed = (size_t)numChunks << BlockSizeLog;
      RINOK(ReadStream_FALSE(Stream, _inBuf + offs, compressed))
      curVirt += numChunks;
      _physPos += compressed;
      offs += compressed;
    }
    
    const size_t destLenMax = GetCuSize(); // [8,16,32,64] KB
    // destLenMax >= LZNT_CHUNK_SIZE
    size_t destLen = destLenMax;
    const UInt64 rem = Size - (virtBlock2 << BlockSizeLog);
    if (destLen > rem)
      destLen = (size_t)rem;

    Byte * const dest = _outBuf + (cacheIndex << _chunkSizeLog);
    _tags[cacheIndex] = cacheTag;
    const size_t destSizeRes = Lznt1Dec(dest, destLen, _inBuf, offs);
    // (destSizeRes > destLen) is normal case for win10 ntfs
    if (destSizeRes < destLen)
    {
      // destSizeRes = 0; // to discard partial data.
      memset(dest + destSizeRes, 0, destLenMax - destSizeRes);
#if 1 // 0 for debug : 0 to ignore any errors in compressed data
      if (InUse)
      {
        // we don't want to return any partial decoded data
        _tags[cacheIndex] = kEmptyTag;
        return S_FALSE;
      }
#endif
    }
  }
  
  if (size > _curRem)
    size = (UInt32)_curRem;
  HRESULT res = S_OK;
  if (_sparseMode)
    memset(data, 0, size);
  else
  {
    res = Stream->Read(data, size, &size);
    _physPos += size;
  }
  if (processedSize)
    *processedSize = size;
  _virtPos += size;
  _curRem -= size;
  return res;
}

 
Z7_COM7F_IMF(CInStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition))
{
  switch (seekOrigin)
  {
    case STREAM_SEEK_SET: break;
    case STREAM_SEEK_CUR: offset += _virtPos; break;
    case STREAM_SEEK_END: offset += Size; break;
    default: return STG_E_INVALIDFUNCTION;
  }
  if (offset < 0)
    return HRESULT_WIN32_ERROR_NEGATIVE_SEEK;
  if (_virtPos != (UInt64)offset)
  {
    _curRem = 0;
    _virtPos = (UInt64)offset;
  }
  if (newPosition)
    *newPosition = (UInt64)offset;
  return S_OK;
}


static HRESULT DataParseExtents(const unsigned clusterSizeLog,
    const CObjectVector<CAttr> &attrs, const unsigned attrIndex,
    const UInt64 numPhysClusters, CRecordVector<CExtent> &Extents)
{
  Extents.Clear();
  {
    CExtent e;
    e.Virt = 0;
    e.Phy = kEmptyExtent;
    Extents.Add(e);
  }
  const CAttr &attr0 = attrs[attrIndex];
  const unsigned attrIndexLim = attr0.NextAttrIndex;
  if (attr0.AllocatedSize < attr0.Size
      || attrs[attrIndexLim - 1].HighVcn + 1 != (attr0.AllocatedSize >> clusterSizeLog)
      || (attr0.AllocatedSize & ((1u << clusterSizeLog) - 1)))
    return S_FALSE;
  
  for (unsigned i = attrIndex; i < attrIndexLim; i++)
    if (!attrs[i].ParseExtents(Extents, numPhysClusters, attr0.CompressionUnit))
      return S_FALSE;

  // (Extents.Back().IsEmpty() == true)
  UInt64 numClusters = 0;
  for (unsigned k = 1; k < Extents.Size(); k++)
  {
    const CExtent &e = Extents[k - 1];
    const UInt64 next = Extents[k].Virt;
    if (next <= e.Virt)
      return E_FAIL;
    if (!e.IsEmpty())
      numClusters += next - e.Virt;
    PRINT_UI64_2("\nVCN", e.Virt)
    PRINT_UI64_2(" Size", next - e.Virt)
    PRINT_UI64_2(" Pos", e.Phy)
  }
  const UInt64 packSizeCalc = numClusters << clusterSizeLog;
  if ((packSizeCalc >> clusterSizeLog) != numClusters) // optional
    return S_FALSE;
  if (attr0.CompressionUnit != 0)
  {
    if (packSizeCalc != attr0.PackSize)
      return S_FALSE;
  }
  else
  {
    if (packSizeCalc != attr0.AllocatedSize)
      return S_FALSE;
  }
  return S_OK;
}


struct CVolumeInfo
{
  UString2 VolName;
  int FsVerMajor; // -1 if not defined
  Byte FsVerMinor;
  CVolumeInfo(): FsVerMajor(-1) /* , Minor(-1) */ {}
  void Clear()
  {
    FsVerMajor = -1;
    if (!VolName.IsEmpty())
      VolName.SetFromAscii("");
  }
};


static const UInt32 kMagic_FILE = 0x454C4946;
static const UInt32 kMagic_BAAD = 0x44414142;
// 22.02: we support some rare case magic values:
static const UInt32 kMagic_INDX = 0x58444e49;
static const UInt32 kMagic_HOLE = 0x454c4f48;
static const UInt32 kMagic_RSTR = 0x52545352;
static const UInt32 kMagic_RCRD = 0x44524352;
static const UInt32 kMagic_CHKD = 0x444b4843;
static const UInt32 kMagic_FFFFFFFF = 0xFFFFFFFF;


struct CMftRec
{
  UInt32 Magic;
  // UInt64 Lsn;
  UInt16 SeqNumber;  // Number of times this MFT record has been reused
  UInt16 Flags;
  // UInt16 LinkCount;
  // UInt16 NextAttrInstance;
  CMftRef BaseMftRef;
  
  unsigned MyNumNameLinks;
  int MyItemIndex; // index in Items[] of main item for that record, or -1 if there is no item for that record
  int ReparseDataIndex;

  CObjectVector<CAttr> DataAttrs;    // will be sorted by CAttr::Name and CAttr::LowVcn
  CObjectVector<CFileNameAttr> FileNames;
      /* usually there is one FileName per CMftRec record,
         but there are additional names for Hard links or for DosName. */
  // CAttr SecurityAttr;
  CSiAttr SiAttr;

  int FindWin32Name_for_DosName(unsigned dosNameIndex) const
  {
    const CFileNameAttr &cur = FileNames[dosNameIndex];
    if (cur.IsDos())
      FOR_VECTOR (i, FileNames)
      {
        const CFileNameAttr &next = FileNames[i];
        if (next.IsWin32() && cur.ParentDirRef.Val == next.ParentDirRef.Val)
          return (int)i;
      }
    return -1;
  }

  int FindDosName(unsigned nameIndex) const
  {
    const CFileNameAttr &cur = FileNames[nameIndex];
    if (cur.IsWin32())
      FOR_VECTOR (i, FileNames)
      {
        const CFileNameAttr &next = FileNames[i];
        if (next.IsDos() && cur.ParentDirRef.Val == next.ParentDirRef.Val)
          return (int)i;
      }
    return -1;
  }

  /*
  bool IsAltStream(int dataIndex) const
  {
    return dataIndex >= 0 && (
      (IsDir() ||
      !DataAttrs[dataIndex].Name.IsEmpty()));
  }
  */
  void MoveAttrsFrom(CMftRec &src)
  {
    DataAttrs += src.DataAttrs;
    FileNames += src.FileNames;
    src.DataAttrs.ClearAndFree();
    src.FileNames.ClearAndFree();
  }
  
  UInt64 GetPackSize() const
  {
    UInt64 res = 0;
    for (unsigned i = 0; i < DataAttrs.Size();)
    {
      const CAttr &attr = DataAttrs[i];
      res += attr.GetPackSize();
      i = attr.NextAttrIndex;
    }
    return res;
  }

  bool ParseRec(Byte *p, unsigned sectorSizeLog, unsigned numSectors, unsigned recNumber,
      CObjectVector<CByteBuffer> &reparseDataVector, CVolumeInfo *volInfo);

  bool Is_Magic_Empty() const
  {
    // what exact Magic values are possible for empty and unused records?
    const UInt32 k_Magic_Unused_MAX = 5; // 22.02
    return (Magic <= k_Magic_Unused_MAX);
  }
  bool Is_Magic_FILE() const { return (Magic == kMagic_FILE); }
  // bool Is_Magic_BAAD() const { return (Magic == kMagic_BAAD); }
  bool Is_Magic_CanIgnore() const
  {
    return Is_Magic_Empty()
        || Magic == kMagic_BAAD
        || Magic == kMagic_INDX
        || Magic == kMagic_HOLE
        || Magic == kMagic_RSTR
        || Magic == kMagic_RCRD
        || Magic == kMagic_CHKD
        || Magic == kMagic_FFFFFFFF;
  }

  bool InUse() const { return (Flags & 1) != 0; }
  bool IsDir() const { return (Flags & 2) != 0; }

  void ParseDataNames();
  HRESULT GetStream(IInStream *mainStream, int dataIndex,
      unsigned clusterSizeLog, UInt64 numPhysClusters, IInStream **stream,
      CMyComPtr2<IInStream, CInStream> *inStream_Object = NULL) const;
  unsigned GetNumExtents(int dataIndex, unsigned clusterSizeLog, UInt64 numPhysClusters) const;

  UInt64 GetSize(unsigned dataIndex) const { return DataAttrs[dataIndex].GetSize(); }

  CMftRec():
      SeqNumber(0),
      Flags(0),
      MyNumNameLinks(0),
      MyItemIndex(-1),
      ReparseDataIndex(-1) {}
};


void CMftRec::ParseDataNames()
{
  DataAttrs.Sort(CompareAttr, NULL);
  for (unsigned i = 0; i < DataAttrs.Size();)
  {
    CAttr &attr = DataAttrs[i];
    for (i++; i < DataAttrs.Size(); i++)
      if (attr.Name != DataAttrs[i].Name)
        break;
    attr.NextAttrIndex = i;
  }
}

HRESULT CMftRec::GetStream(IInStream *mainStream, const int dataIndex,
    const unsigned clusterSizeLog, const UInt64 numPhysClusters,
    IInStream **destStream,
    CMyComPtr2<IInStream, CInStream> *inStream_Object) const
{
  *destStream = NULL;
  CBufferInStream *streamSpec = new CBufferInStream;
  CMyComPtr<IInStream> streamTemp = streamSpec;

  if (dataIndex >= 0 /* (unsigned)dataIndex < GetNumDataRefs() */)
  {
    const CAttr &attr0 = DataAttrs[dataIndex];
    unsigned numNonResident = 0;
    for (unsigned i = (unsigned)dataIndex; i < attr0.NextAttrIndex; i++)
      if (DataAttrs[i].NonResident)
        numNonResident++;
    const unsigned refNum = attr0.NextAttrIndex - (unsigned)dataIndex;
    if (numNonResident != 0 || refNum != 1)
    {
      if (numNonResident != refNum || !attr0.IsCompressionUnitSupported())
        return S_FALSE;
      if (clusterSizeLog > 12 && attr0.CompressionUnit)
        return S_FALSE; // NOT SUPPORTED
      CInStream *ss;
      if (inStream_Object)
      {
        inStream_Object->Create_if_Empty();
        ss = inStream_Object->ClsPtr();
        ss->Clear(); // we clear Extents[] filled for previous file.
      }
      else
        ss = new CInStream;
      CMyComPtr<IInStream> streamTemp2 = ss;
      RINOK(DataParseExtents(clusterSizeLog, DataAttrs,
          (unsigned)dataIndex, numPhysClusters, ss->Extents))
      ss->Size = attr0.Size;
      ss->InitializedSize = attr0.InitializedSize;
      ss->Stream = mainStream;
      ss->BlockSizeLog = clusterSizeLog;
      ss->InUse = InUse();
      RINOK(ss->InitAndSeek(attr0.CompressionUnit))
      *destStream = streamTemp2.Detach();
      return S_OK;
    }
  
    streamSpec->Buf = attr0.Data;
  }

  streamSpec->Init();
  *destStream = streamTemp.Detach();
  return S_OK;
}

unsigned CMftRec::GetNumExtents(const int dataIndex,
    const unsigned clusterSizeLog, const UInt64 numPhysClusters) const
{
  if (dataIndex < 0)
    return 0;
  {
    const CAttr &attr0 = DataAttrs[dataIndex];
    unsigned numNonResident = 0;
    for (unsigned i = (unsigned)dataIndex; i < attr0.NextAttrIndex; i++)
      if (DataAttrs[i].NonResident)
        numNonResident++;
    const unsigned refNum = attr0.NextAttrIndex - (unsigned)dataIndex;
    if (numNonResident != 0 || refNum != 1)
    {
      if (numNonResident != refNum || !attr0.IsCompressionUnitSupported())
        return 0; // error;
      if (clusterSizeLog > 12 && attr0.CompressionUnit)
        return 0; // error;
      CRecordVector<CExtent> extents;
      if (DataParseExtents(clusterSizeLog, DataAttrs, (unsigned)dataIndex,
            numPhysClusters, extents) != S_OK)
        return 0; // error;
      return extents.Size() - 1;
    }
    // if (attr0.Data.Size() != 0) return 1;
    return 0;
  }
}


bool CMftRec::ParseRec(Byte * const p, const unsigned sectorSizeLog,
    const unsigned numSectors, const unsigned recNumber,
    CObjectVector<CByteBuffer> &reparseDataVector, CVolumeInfo *volInfo)
{
  G32(p, Magic);
  if (!Is_Magic_FILE())
    return Is_Magic_CanIgnore();
  {
    UInt32 usaOffset, numUsaItems;
    G16(p + 0x04, usaOffset);
    G16(p + 0x06, numUsaItems);
    /* NTFS stores (usn) to 2 last bytes in each sector (before writing record to disk).
       Original values of these two bytes are stored in table.
       So we restore original data from table */
    if ((usaOffset & 1)
        || usaOffset + numUsaItems * 2 > (1u << sectorSizeLog) - 2
        || numUsaItems - 1 != numSectors)
      return false;
    if (usaOffset >= 0x30) // NTFS 3.1+
    {
      const UInt32 iii = Get32(p + 0x2C);
      if (iii != recNumber)
      {
        // ntfs-3g probably writes 0 (that probably is incorrect value) to this field for unused records.
        // so we support that "bad" case.
        if (iii != 0)
          return false;
      }
    }
    const UInt16 usn = Get16(p + usaOffset);
    // PRF(printf("\nusn = %d", usn));
    for (UInt32 i = 1; i < numUsaItems; i++)
    {
      void *pp = p + ((size_t)i << sectorSizeLog) - 2;
      if (Get16(pp) != usn)
        return false;
      SetUi16a(pp, Get16(p + usaOffset + i * 2))
    }
  }

  // G64(p + 0x08, Lsn);
  G16(p + 0x10, SeqNumber);
  // G16(p + 0x12, LinkCount);
  // PRF(printf(" L=%d", LinkCount));
  const unsigned attrOffs = Get16(p + 0x14);
  G16(p + 0x16, Flags);
  PRF(printf(" F=%4X", Flags));
  const UInt32 bytesInUse = Get32(p + 0x18);
  const UInt32 bytesAlloc = Get32(p + 0x1C);
  G64(p + 0x20, BaseMftRef.Val);
  if (BaseMftRef.Val != 0)
  {
    PRINT_UI64("  BaseRef", BaseMftRef.Val)
  }
  // G16(p + 0x28, NextAttrInstance);
  unsigned limit = numSectors << sectorSizeLog;
  if (attrOffs >= limit
      || (attrOffs & 7)
      || (bytesInUse & 7)
      || bytesInUse > limit
      || bytesAlloc != limit)
    return false;
  limit = bytesInUse;

  for (unsigned t = attrOffs;;)
  {
    if (t >= limit)
      return false;
    // PRF(printf("\n  %2d:", Attrs.Size()));
    PRF(printf("\n"));
    CAttr attr;
    UInt32 type;
    const UInt32 len = attr.ParseAttr(p + t, limit - t, type);
    if (len == 0 || limit - t < len)
      return false;
    t += len;
    if (type == 0xFFFFFFFF)
    {
      if (t != limit)
        return false;
      break;
    }
    switch (type)
    {
      case ATTR_TYPE_FILE_NAME:
      {
        CFileNameAttr fna;
        if (!attr.ParseFileName(fna))
          return false;
        FileNames.Add(fna);
        PRF(printf(" \n NameType = %1u: ", (unsigned)fna.NameType));
        PRF_UTF16(fna.Name)
        break;
      }
      case ATTR_TYPE_STANDARD_INFO:
        if (!attr.ParseSi(SiAttr))
          return false;
        break;
      case ATTR_TYPE_DATA:
        DataAttrs.Add(attr);
        break;
      case ATTR_TYPE_REPARSE_POINT:
        if (ReparseDataIndex < 0)
          ReparseDataIndex = (int)reparseDataVector.Add(attr.Data);
        break;
      /*
      case ATTR_TYPE_SECURITY_DESCRIPTOR:
        SecurityAttr = attr;
        break;
      */
      default:
        // if (attrs) attrs->Add(attr);
        if (volInfo)
        {
          if (type == ATTR_TYPE_VOLUME_NAME && volInfo->VolName.IsEmpty())
            GetString(attr.Data, (unsigned)attr.Data.Size() / 2, volInfo->VolName);
          else if (type == ATTR_TYPE_VOLUME_INFO /* && volInfo->FsVerMajor < 0 */)
          {
            volInfo->FsVerMajor = attr.Data[8];
            volInfo->FsVerMinor = attr.Data[9];
            // volInfo->Flags = Get16(attr.Data + 10);
          }
        }
        break;
    }
  }

  return true;
}

/*
  NTFS probably creates empty DATA_ATTRIBUTE for empty file,
  But it doesn't do it for
    $Secure (:$SDS),
    $Extend\$Quota
    $Extend\$ObjId
    $Extend\$Reparse
*/

static const int k_Item_DataIndex_IsEmptyFile = -1; // file without unnamed data stream
static const int k_Item_DataIndex_IsDir = -2;

// static const int k_ParentFolderIndex_Root = -1;
static const int k_ParentFolderIndex_Lost = -2;
static const int k_ParentFolderIndex_Deleted = -3;

struct CItem
{
  unsigned RecIndex;  // index in Recs[]
  unsigned NameIndex; // index in CMftRec::FileNames[] : name of file (is not name of alt stream)

  int DataIndex;      /* index in CMftRec::Attrs[] for main files and alt streams.
                         -1: file without unnamed data stream
                         -2: for directories */
                         
  int ParentFolder;   /* index in Items[]
                         -1: for root items
                         -2: [LOST] folder
                         -3: [UNKNOWN] folder (deleted lost) */
  int ParentHost;     /* index in Items[] array of item that main file for alt stream, if it's AltStream
                         -1: if it's not AltStream */
  
/* RecIndex, NameIndex, ParentFolder : are identical for main file and alt substreams of that main files
   DataIndex, ParentHost : are different for main file and alt substreams of that main files. */

  void Construct()
  {
    DataIndex = k_Item_DataIndex_IsDir;
    ParentFolder = -1;
    ParentHost = -1;
  }
  
  bool IsAltStream() const { return ParentHost != -1; }
  bool IsDir() const { return DataIndex == k_Item_DataIndex_IsDir; }
        // check it !!!
        // probably NTFS for empty file still creates empty DATA_ATTRIBUTE
        // But it doesn't do it for $Secure:$SDS
};


struct CDatabase
{
  CRecordVector<CItem> Items;
  CObjectVector<CMftRec> Recs;
  CHeader Header;
  UInt64 PhySize;

  // bool _headerWarning;
  bool ThereAreAltStreams;
  bool _showSystemFiles;
  bool _showDeletedFiles;

  int _systemFolderIndex;
  int _lostFolderIndex_Normal;
  int _lostFolderIndex_Deleted;

  CMyComPtr<IInStream> InStream;
  IArchiveOpenCallback *OpenCallback;
  CAlignedBuffer ByteBuf;

  CObjectVector<CByteBuffer> ReparseDataVector;

  CByteBuffer SecurData;
  CRecordVector<UInt32> SecurOffsets;
  CRecordVector<UInt32> SecurIds; // sorted

  CVolumeInfo VolumeInfo;
  CObjectVector<UString2> VirtFolderNames;
  UString2 EmptyString;

  void InitProps()
  {
    _showSystemFiles = true;
    // we show SystemFiles by default since it's difficult to track $Extend\* system files
    // it must be fixed later
    _showDeletedFiles = false;
  }

  CDatabase(): EmptyString(L"") { InitProps(); }
  // ~CDatabase() { ClearAndClose(); }

  void ClearSecurInfo()
  {
    SecurIds.Clear();
    SecurOffsets.Clear();
    SecurData.Free();
  }
  void Clear();
  void ClearAndClose();

  void GetItemPath(unsigned index, NCOM::CPropVariant &path) const;
  HRESULT Open();

  HRESULT SeekToCluster(UInt64 cluster);

  int Find_DirItem_For_MftRec(UInt64 recIndex) const
  {
    if (recIndex >= Recs.Size())
      return -1;
    const CMftRec &rec = Recs[(unsigned)recIndex];
    if (!rec.IsDir())
      return -1;
    return rec.MyItemIndex;
    /*
    unsigned left = 0, right = Items.Size();
    while (left != right)
    {
      unsigned mid = (left + right) / 2;
      const CItem &item = Items[mid];
      UInt64 midValue = item.RecIndex;
      if (recIndex == midValue)
      {
        // if item is not dir (file or alt stream we don't return it)
        // if (item.DataIndex < 0)
        if (item.IsDir())
          return mid;
        right = mid;
      }
      else if (recIndex < midValue)
        right = mid;
      else
        left = mid + 1;
    }
    return -1;
    */
  }

  bool ParseSecuritySDS(ISequentialInStream *stream, const size_t size);
};

HRESULT CDatabase::SeekToCluster(UInt64 cluster)
{
  return InStream_SeekSet(InStream, cluster << Header.ClusterSizeLog);
}

void CDatabase::Clear()
{
  Items.Clear();
  Recs.Clear();
  ClearSecurInfo();
  VirtFolderNames.Clear();
  _systemFolderIndex = -1;
  _lostFolderIndex_Normal = -1;
  _lostFolderIndex_Deleted = -1;
  ThereAreAltStreams = false;
  // _headerWarning = false;
  VolumeInfo.Clear();
  ReparseDataVector.Clear();
  PhySize = 0;
}

void CDatabase::ClearAndClose()
{
  Clear();
  InStream.Release();
}


// src[0 ... num-1] != 0
// it doesn't write NUL to dest[num]
static void CopyName(wchar_t *dest, const wchar_t *src, unsigned num)
{
  if (num) do
  {
    wchar_t c = *src++;
    // 18.06
    if (c == '\\' || c == '/')
      c = '_';
    *dest++ = c;
  }
  while (--num);
}

#define kLongPath "[LONG_PATH]" STRING_PATH_SEPARATOR "[LONG_PATH_ITEM]"

void CDatabase::GetItemPath(const unsigned index, NCOM::CPropVariant &path) const
{
  const CItem *item = &Items[index];
  const UString2 *altName = NULL;
  unsigned size = 0;
  if (item->IsAltStream())
  {
    const CMftRec &rec = Recs[item->RecIndex];
    altName = &rec.DataAttrs[item->DataIndex].Name;
    size = altName->Len() + 1;
    if (item->RecIndex == kRecIndex_RootDir)
    {
      // we don't show main name from kRecIndex_RootDir record for root alt streams:
      wchar_t *s = path.AllocBstr(size);
      s[0] = L':';
      CopyName(s + 1, altName->GetRawPtr(), altName->Len());
      return;
    }
  }

  for (unsigned i = 0;; i++)
  {
    size += Recs[item->RecIndex].FileNames[item->NameIndex].Name.Len();
    if (i > 256 || size >= 1u << 15)
    {
      path = kLongPath;
      return;
    }
    if (item->RecIndex == kRecIndex_RootDir)
      break;
    const wchar_t *serv;
    if (item->RecIndex < kNumSysRecs)
      serv = kVirtualFolder_System;
    else
    {
      const int index2 = item->ParentFolder;
      if (index2 >= 0)
      {
        item = &Items[index2];
        size++;
        continue;
      }
      if (index2 == -1)
        break;
      serv = (index2 == k_ParentFolderIndex_Lost) ?
          kVirtualFolder_Lost_Normal :
          kVirtualFolder_Lost_Deleted;
    }
    size += MyStringLen(serv) + 1;
    break;
  }

  wchar_t *s = path.AllocBstr(size);
  
  if (altName)
  {
    const unsigned len = altName->Len();
    size -= len;
    CopyName(s + size, altName->GetRawPtr(), len);
    s[--size] = ':';
  }

  item = &Items[index];
  for (;;)
  {
    {
      const UString2 &name = Recs[item->RecIndex].FileNames[item->NameIndex].Name;
      const unsigned len = name.Len();
      size -= len;
      CopyName(s + size, name.GetRawPtr(), len);
    }
    if (item->RecIndex == kRecIndex_RootDir)
      break;
    const wchar_t *serv;
    if (item->RecIndex < kNumSysRecs)
      serv = kVirtualFolder_System;
    else
    {
      const int index2 = item->ParentFolder;
      if (index2 >= 0)
      {
        s[--size] = WCHAR_PATH_SEPARATOR;
        item = &Items[index2];
        continue;
      }
      if (index2 == -1)
        break;
      serv = (index2 == k_ParentFolderIndex_Lost) ?
          kVirtualFolder_Lost_Normal :
          kVirtualFolder_Lost_Deleted;
    }
    s[--size] = WCHAR_PATH_SEPARATOR;
    // if (size != MyStringLen(serv)) throw 1;
    CopyName(s, serv, size);
    break;
  }
}


bool CDatabase::ParseSecuritySDS(ISequentialInStream *stream, const size_t size)
{
  /* In most cases, identifiers (IDs) are listed sequentially with a step of 1, starting from 0x100.
     However, gaps in the IDs numbering are possible. So we use SecurIds[] array.
     The security data contains a duplicate copy every 256 KB. */
  const size_t kDupStep = (size_t)1 << 18; // 256 KB
  if (size == 0 || (size & 3))
    return false;
  if (((size - 1) & kDupStep) == 0) // we check that there is duplicate block
    return false;
  const size_t skipDupSize = ((size - 1) & ~(kDupStep * 2 - 1)) / 2;
  const size_t allocSize = size - skipDupSize;
  SecurData.Alloc(allocSize);
  size_t posInFile = 0;
  size_t destPos = 0;
  UInt32 idPrev = 0;
  SecurOffsets.Add(0);
  for (;;)
  {
    size_t rem = size - posInFile;
    if (rem == 0)
      break;
    rem = MyMin(rem, kDupStep * 2);
    if (destPos + rem > allocSize)
      return false; // internal code failure
    Byte * const p = SecurData + destPos;
    if (ReadStream_FALSE(stream, p, rem) != S_OK)
      return false;
    const size_t readSize = rem;
    if (rem < kDupStep)
      return false;
    rem -= kDupStep;
    if (memcmp(p, p + kDupStep, rem))
      return false;
    /* Garbage (non-zero) data is possible after the last entry
       in the main block but before the last block of duplicates.
       So we don't check zeros padding in last block.
    */
    for (size_t pos = 0;;)
    {
      const unsigned kEntrySize = 20;
      UInt32 id;
      if ((pos & 0xF)
          || rem < kEntrySize
          || (id = Get32(p + pos + 4)) == 0)
      {
        if (rem < 4)
          break;
        // we skip zeros of padding or hole in data:
        if (*(const UInt32 *)(const void *)(p + pos))
          return false;
        pos += 4;
        rem -= 4;
        continue;
      }
      if (id <= idPrev)
        return false;
      idPrev = id;
      if (Get64(p + pos + 8) != posInFile + pos) // entry offset
        return false;
      const UInt32 entrySize = Get32(p + pos + 16);
      if (entrySize < kEntrySize
          || (entrySize & 3)
          || rem < entrySize)
        return false;
      rem -= entrySize;
      {
        const Byte *p2 = p + pos + kEntrySize;
        UInt32 hash = 0;
        unsigned num = entrySize - kEntrySize;
        if (num) do
        {
          hash = rotlFixed(hash, 3) + Get32(p2);
          p2 += 4;
        }
        while (num -= 4);
        if (hash != Get32(p + pos))
          return false;
      }
      memmove(SecurData + destPos, p + pos + kEntrySize, entrySize - kEntrySize);
      pos += entrySize;
      destPos += entrySize - kEntrySize;
      SecurOffsets.Add((UInt32)destPos);
      SecurIds.Add(id);
    }
    posInFile += readSize;
  }
  SecurData.ChangeSize_KeepData(destPos, destPos);
  return true;
}


HRESULT CDatabase::Open()
{
  Clear();
  /* NTFS layout:
     1) main part (as specified by NumClusters). Only that part is available, if we open "\\.\c:"
     2) additional empty sectors (as specified by NumSectors)
     3) the copy of first sector (boot sector)
     We support both cases:
      - the file with only main part
      - full file (as raw data on partition), including the copy
        of first sector (boot sector) at the end of data
     We don't support the case, when only the copy of boot sector
     at the end was detected as NTFS signature.
  */
  const size_t kBufSize = (size_t)1 << MyMax(15, k_MftRecordSizeLog_MAX);
  ByteBuf.Alloc(kBufSize);
  if (!ByteBuf.IsAllocated())
    return E_OUTOFMEMORY;
  {
    const unsigned kHeaderSize = 512;
    RINOK(ReadStream_FALSE(InStream, ByteBuf, kHeaderSize))
    if (!Header.Parse(ByteBuf))
      return S_FALSE;
    
    UInt64 fileSize;
    RINOK(InStream_GetSize_SeekToEnd(InStream, fileSize))
    PhySize = Header.GetPhySize_Clusters();
    if (fileSize < PhySize)
      return S_FALSE;
    
    const UInt64 phySizeMax = Header.GetPhySize_Max();
    if (fileSize >= phySizeMax)
    {
      RINOK(InStream_SeekSet(InStream, Header.NumSectors << Header.SectorSizeLog))
      if (ReadStream_FALSE(InStream, ByteBuf + kHeaderSize, kHeaderSize) == S_OK)
      {
        if (memcmp(ByteBuf, ByteBuf + kHeaderSize, kHeaderSize) == 0)
          PhySize = phySizeMax;
        // else _headerWarning = true;
      }
    }
  }
 
  SeekToCluster(Header.MftCluster);
  const size_t recSize = (size_t)1 << Header.MftRecordSizeLog;
  // ByteBuf.Size() >= recSize
  RINOK(ReadStream_FALSE(InStream, ByteBuf, recSize))
  // if (Get32(ByteBuf + 0x1C) != recSize) return S_FALSE; // NTFRec::bytesAlloc
    
  {
  CMyComPtr<IInStream> mftStream;
  CMftRec mftRec;
  // MftRecordSizeLog >= SectorSizeLog
  const unsigned numSectorsInRec = 1u << (Header.MftRecordSizeLog - Header.SectorSizeLog);
  {
    if (!mftRec.ParseRec(ByteBuf, Header.SectorSizeLog, numSectorsInRec, 0, ReparseDataVector, NULL))
      return S_FALSE;
    if (!mftRec.Is_Magic_FILE())
      return S_FALSE;
    mftRec.ParseDataNames(); // it sorts DataAttrs[] by Name and fills DataAttrs[].NextAttrIndex values.
    if (mftRec.DataAttrs.Size() == 0)
      return S_FALSE;
    if (mftRec.DataAttrs[0].NonResident == 0)
      return S_FALSE;
    const int dataIndex = 0; // first stream in DataAttrs[]
    RINOK(mftRec.GetStream(InStream, dataIndex,
          Header.ClusterSizeLog, Header.NumClusters, &mftStream))
    if (!mftStream)
      return S_FALSE;
  }
  const UInt64 mftSize = mftRec.DataAttrs[0].Size; // NonResident Size
  if ((mftSize >> 4) > Header.GetPhySize_Clusters())
    return S_FALSE;
  {
    const UInt64 numFiles = mftSize >> Header.MftRecordSizeLog;
    if (numFiles > 1u << 30)
      return S_FALSE;
    if (OpenCallback)
      RINOK(OpenCallback->SetTotal(&numFiles, &mftSize))
    Recs.ClearAndReserve((unsigned)numFiles);
  }
  // ReparseDataVector.Clear(); // optional

  for (UInt64 pos64 = 0;;)
  {
    if (OpenCallback)
    {
      const UInt64 numFiles = Recs.Size();
      if ((numFiles & 0x3FFF) == 0)
        RINOK(OpenCallback->SetCompleted(&numFiles, &pos64))
    }
    size_t readSize = kBufSize;
    {
      const UInt64 rem = mftSize - pos64;
      if (readSize > rem)
        readSize = (size_t)rem;
    }
    if (readSize < recSize)
      break;
    pos64 += readSize;
    RINOK(ReadStream_FALSE(mftStream, ByteBuf, readSize))

    for (Byte *p = ByteBuf; readSize >= recSize; p += recSize, readSize -= recSize)
    {
      PRF(printf("\n---------------------\n%5u:", Recs.Size()));
      CMftRec rec;
      CVolumeInfo *volInfo = NULL;
      const unsigned recIndex = Recs.Size();
      switch (recIndex)
      {
        case kRecIndex_Volume: volInfo = &VolumeInfo; break;
        // case kRecIndex_Security: attrs = &SecurityAttrs; break;
      }
      if (!rec.ParseRec(p, Header.SectorSizeLog, numSectorsInRec,
          recIndex, ReparseDataVector, volInfo))
        return S_FALSE;
      Recs.Add(rec);
    }
  }
  }

  /*
  // that code looks too complex. And we can get security info without index parsing
  for (i = 0; i < SecurityAttrs.Size(); i++)
  {
    const CAttr &attr = SecurityAttrs[i];
    if (attr.Name.IsEqualTo("$SII"))
    {
      if (attr.Type == ATTR_TYPE_INDEX_ROOT)
      {
        const Byte *data = attr.Data;
        size_t size = attr.Data.Size();
        // Index Root
        UInt32 attrType = Get32(data);
        UInt32 collationRule = Get32(data + 4);
        UInt32 indexAllocationEtrySizeSize = Get32(data + 8);
        UInt32 clustersPerIndexRecord = Get32(data + 0xC);
        data += 0x10;
        // Index Header
        UInt32 firstEntryOffset = Get32(data);
        UInt32 totalSize = Get32(data + 4);
        UInt32 allocSize = Get32(data + 8);
        UInt32 flags = Get32(data + 0xC);
        int num = 0;
        for (int j = 0 ; j < num; j++)
        {
          if (Get32(data) != 0x1414 || // offset and size
              Get32(data + 4) != 0 ||
              Get32(data + 8) != 0x428) // KeySize / EntrySize
            break;
          UInt32 flags = Get32(data + 12);
          UInt32 id = Get32(data + 0x10);
          if (id = Get32(data + 0x18))
            break;
          UInt32 descriptorOffset = Get64(data + 0x1C);
          UInt32 descriptorSize = Get64(data + 0x24);
          data += 0x28;
        }
        // break;
      }
    }
  }
  */

  unsigned i;
  for (i = 0; i < Recs.Size(); i++)
  {
    CMftRec &rec = Recs[i];
    if (!rec.Is_Magic_FILE())
      continue;
    if (rec.BaseMftRef.IsBaseItself())
      continue;

    const UInt64 refIndex = rec.BaseMftRef.GetIndex();
    if (refIndex >= Recs.Size())
      return S_FALSE;
    CMftRec &refRec = Recs[(unsigned)refIndex];
    if (!refRec.Is_Magic_FILE())
      continue;
    
    bool moveAttrs = (refRec.SeqNumber == rec.BaseMftRef.GetNumber()
        && refRec.BaseMftRef.IsBaseItself());
    if (rec.InUse() && refRec.InUse())
    {
      if (!moveAttrs)
        return S_FALSE;
    }
    else if (rec.InUse() || refRec.InUse())
      moveAttrs = false;
    if (moveAttrs)
      refRec.MoveAttrsFrom(rec);
  }

  for (i = 0; i < Recs.Size(); i++)
  {
    CMftRec &rec = Recs[i];
    if (!rec.Is_Magic_FILE())
      continue;
    rec.ParseDataNames();
  }
  
  for (i = 0; i < Recs.Size(); i++)
  {
    CMftRec &rec = Recs[i];
    if (!rec.Is_Magic_FILE() || !rec.BaseMftRef.IsBaseItself())
      continue;
    if (i < kNumSysRecs && !_showSystemFiles)
      continue;
    if (!rec.InUse() && !_showDeletedFiles)
      continue;

    // rec.FileNames.Clear(); // for debug
    rec.MyNumNameLinks = rec.FileNames.Size();
    PRF(printf("\n%4u: ", i);)
    /* DataAttrs[] are sorted already by CAttr::Name.
       There cannot be more than one unnamed stream in DataAttrs[].NextAttrIndex list
    */
    int indexOfUnnamedStream = -1;
    if (!rec.IsDir())
    {
      for (unsigned di = 0; di < rec.DataAttrs.Size();)
      {
        const CAttr &attr = rec.DataAttrs[di];
        if (attr.Name.IsEmpty())
        {
          indexOfUnnamedStream = (int)di;
          break;
        }
        // break; // optional : we need to check only rec.DataAttrs[0] for unnamed attribute.
        di = attr.NextAttrIndex;
      }
    }

    if (rec.FileNames.IsEmpty())
    {
      bool needShow = true;
      if (i < kNumSysRecs)
      {
        needShow = false;
        for (unsigned di = 0; di < rec.DataAttrs.Size();)
        {
          const CAttr &attr = rec.DataAttrs[di];
          if (rec.GetSize(di) != 0)
          {
            needShow = true;
            break;
          }
          di = attr.NextAttrIndex;
        }
      }
      if (needShow)
      {
        CFileNameAttr &fna = rec.FileNames.AddNew();
        fna.NameType = kFileNameType_Win32Dos;
        // we set incorrect ParentDirRef, that will place item to [LOST] folder
        fna.ParentDirRef.Val = (UInt64)0-1;
        char s[16 + 16];
        ConvertUInt32ToString(i, MyStpCpy(s, "[NONAME]-"));
        fna.Name.SetFromAscii(s);
      }
    }

    // bool isMainName = true;

    FOR_VECTOR (t, rec.FileNames)
    {
      PRF(printf("\n %1u : ", (unsigned)rec.FileNames[t].NameType));
      PRF_UTF16(rec.FileNames[t].Name)
      // PRF(printf("  | "));
      if (rec.FindWin32Name_for_DosName(t) >= 0)
      {
        rec.MyNumNameLinks--;
        continue;
      }
      
      CItem item;
      item.Construct();
      item.RecIndex = i;
      item.NameIndex = t;
      item.DataIndex = rec.IsDir() ? k_Item_DataIndex_IsDir :
          indexOfUnnamedStream < 0 ? k_Item_DataIndex_IsEmptyFile :
          indexOfUnnamedStream;
      if (rec.MyItemIndex < 0)
        rec.MyItemIndex = (int)Items.Size();
      item.ParentHost = (int)Items.Add(item);
      if (OpenCallback) // if (Items.Size() > Recs.Size())
      {
        const UInt64 numFiles = Items.Size();
        if ((numFiles & 0xFFFFF) == 0)
          RINOK(OpenCallback->SetCompleted(&numFiles, NULL))
      }
      
      /* we can use that code to reduce the number of alt streams:
         it will not show alt streams for hard links. */
      // if (!isMainName) continue; isMainName = false;

      // unsigned numAltStreams = 0;
      for (unsigned di = 0; di < rec.DataAttrs.Size();)
      {
        const CAttr &attr = rec.DataAttrs[di];
        if (rec.IsDir() || (int)di != indexOfUnnamedStream)
        {
          const UString2 &subName = attr.Name;
          PRF(printf("\n alt stream: "));
          PRF_UTF16(subName)
          // $BadClus:$Bad is sparse file for all clusters. So we skip it.
          if (i != kRecIndex_BadClus || subName != L"$Bad")
          {
            // numAltStreams++;
            ThereAreAltStreams = true;
            item.DataIndex = (int)di;
            Items.Add(item);
            if (OpenCallback)
            {
              const UInt64 numFiles = Items.Size();
              if ((numFiles & 0xFFFFF) == 0)
                RINOK(OpenCallback->SetCompleted(&numFiles, NULL))
            }
          }
        }
        di = attr.NextAttrIndex;
      }
    }
  }
  
  if (Recs.Size() > kRecIndex_Security)
  {
    const CMftRec &rec = Recs[kRecIndex_Security];
    for (unsigned di = 0; di < rec.DataAttrs.Size();)
    {
      const CAttr &attr = rec.DataAttrs[di];
      if (attr.Name == L"$SDS")
      {
        CMyComPtr<IInStream> sdsStream;
        RINOK(rec.GetStream(InStream, (int)di,
            Header.ClusterSizeLog, Header.NumClusters, &sdsStream))
        if (sdsStream)
        {
          const UInt64 size64 = attr.GetSize();
          if (size64 <= (UInt32)1 << 29)
            if (!ParseSecuritySDS(sdsStream, (size_t)size64))
              ClearSecurInfo();
        }
        break;
      }
      di = attr.NextAttrIndex;
    }
  }

  bool thereAreUnknownFolders_Normal = false;
  bool thereAreUnknownFolders_Deleted = false;

  for (i = 0; i < Items.Size(); i++)
  {
    CItem &item = Items[i];
    const CMftRec &rec = Recs[item.RecIndex];
    const CFileNameAttr &fn = rec.FileNames[item.NameIndex];
    const CMftRef &parentDirRef = fn.ParentDirRef;
    const UInt64 refIndex = parentDirRef.GetIndex();
#if 1 // 0 for debug
    // we don't set ParentFolder link to (RootDir) item:
    if (refIndex == kRecIndex_RootDir)
      continue;
#endif
    {
      int index = Find_DirItem_For_MftRec(refIndex);
      if (index < 0 ||
          Recs[(unsigned)refIndex].SeqNumber != parentDirRef.GetNumber())
          // Recs[Items[index].RecIndex].SeqNumber != parentDirRef.GetNumber())
      {
        if (rec.InUse())
        {
          thereAreUnknownFolders_Normal = true;
          index = k_ParentFolderIndex_Lost;
        }
        else
        {
          thereAreUnknownFolders_Deleted = true;
          index = k_ParentFolderIndex_Deleted;
        }
      }
      item.ParentFolder = index;
    }
  }
  
  if (kRecIndex_RootDir < Recs.Size())
  {
    CMftRec &rec = Recs[kRecIndex_RootDir];
    if (rec.MyItemIndex >= 0)
    {
      // v26.04 : we replace Name of RootDir: from "." to "[SYSTEM]".
      _systemFolderIndex = rec.MyItemIndex;
      rec.FileNames[Items[rec.MyItemIndex].NameIndex].Name = kVirtualFolder_System;
    }
  }

  unsigned virtIndex = Items.Size();
  if (_showSystemFiles && _systemFolderIndex < 0)
  {
    _systemFolderIndex = (int)(virtIndex++);
    VirtFolderNames.Add(kVirtualFolder_System);
  }
  if (thereAreUnknownFolders_Normal)
  {
    _lostFolderIndex_Normal = (int)(virtIndex++);
    VirtFolderNames.Add(kVirtualFolder_Lost_Normal);
  }
  if (thereAreUnknownFolders_Deleted)
  {
    _lostFolderIndex_Deleted = (int)(virtIndex++);
    VirtFolderNames.Add(kVirtualFolder_Lost_Deleted);
  }

  return S_OK;
}


Z7_class_CHandler_final:
  public IInArchive,
  public IArchiveGetRawProps,
  public IInArchiveGetStream,
  public ISetProperties,
  public CMyUnknownImp,
  public CDatabase
{
  Z7_IFACES_IMP_UNK_4(
      IInArchive,
      IArchiveGetRawProps,
      IInArchiveGetStream,
      ISetProperties)
};

Z7_COM7F_IMF(CHandler::GetNumRawProps(UInt32 *numProps))
{
  *numProps = 2;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetRawPropInfo(const UInt32 index, BSTR * const name, PROPID * const propID))
{
  *name = NULL;
  *propID = index == 0 ? kpidNtReparse : kpidNtSecure;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetParent(const UInt32 index, UInt32 * const parent, UInt32 * const parentType))
{
  *parentType = NParentType::kDir;
  int par = -1;

  if (index < Items.Size())
  {
    const CItem &item = Items[index];
    
    if (item.ParentHost >= 0)
    {
      *parentType = NParentType::kAltStream;
      par = (item.RecIndex == kRecIndex_RootDir ? -1 : item.ParentHost);
    }
    else if (item.RecIndex < kNumSysRecs)
    {
      if (_showSystemFiles && item.RecIndex != kRecIndex_RootDir)
        par = _systemFolderIndex;
    }
    else if (item.ParentFolder >= 0)
      par = item.ParentFolder;
    else if (item.ParentFolder == k_ParentFolderIndex_Lost)
      par = _lostFolderIndex_Normal;
    else if (item.ParentFolder == k_ParentFolderIndex_Deleted)
      par = _lostFolderIndex_Deleted;
  }
  *parent = (UInt32)(Int32)par;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetRawProp(const UInt32 index, const PROPID propID,
    const void ** const data, UInt32 * const dataSize, UInt32 * const propType))
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;

  if (propID == kpidName)
  {
    #ifdef MY_CPU_LE
    const UString2 *s;
    if (index >= Items.Size())
      s = &VirtFolderNames[index - Items.Size()];
    else
    {
      const CItem &item = Items[index];
      const CMftRec &rec = Recs[item.RecIndex];
      s = item.IsAltStream() ?
        &rec.DataAttrs[item.DataIndex].Name :
        &rec.FileNames[item.NameIndex].Name;
    }
    if (s->IsEmpty())
      s = &EmptyString;
    *data = s->GetRawPtr();
    *dataSize = (s->Len() + 1) * (UInt32)sizeof(wchar_t);
    *propType = PROP_DATA_TYPE_wchar_t_PTR_Z_LE;
    #endif
    return S_OK;
  }

  if (propID == kpidNtReparse)
  {
    if (index >= Items.Size())
      return S_OK;
    const CItem &item = Items[index];
    const CMftRec &rec = Recs[item.RecIndex];
    if (rec.ReparseDataIndex >= 0)
    {
      const CByteBuffer &reparse = ReparseDataVector[rec.ReparseDataIndex];
      if (reparse.Size() != 0)
      {
        *dataSize = (UInt32)reparse.Size();
        *propType = NPropDataType::kRaw;
        *data = (const Byte *)reparse;
      }
    }
  }

  if (propID == kpidNtSecure)
  {
    if (index >= Items.Size())
      return S_OK;
    const CItem &item = Items[index];
    const CMftRec &rec = Recs[item.RecIndex];
    const UInt32 securId = rec.SiAttr.SecurityId;
    if (securId)
    {
      const int idIndex = SecurIds.FindInSorted(securId);
      if (idIndex >= 0)
      {
        const UInt32 offset = SecurOffsets[idIndex];
        const UInt32 size = SecurOffsets[(unsigned)idIndex + 1] - offset;
        if (SecurData.Size() >= offset
            && SecurData.Size() - (size_t)offset >= size)
        {
          *dataSize = size;
          *propType = NPropDataType::kRaw;
          *data = (const Byte *)SecurData + offset;
        }
      }
    }
  }
  
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetStream(const UInt32 index, ISequentialInStream ** const stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  if (index >= Items.Size())
    return S_OK;
  IInStream *stream2;
  const CItem &item = Items[index];
  const CMftRec &rec = Recs[item.RecIndex];
  HRESULT res = rec.GetStream(InStream, item.DataIndex, Header.ClusterSizeLog, Header.NumClusters, &stream2);
  *stream = (ISequentialInStream *)stream2;
  return res;
  COM_TRY_END
}

/*
enum
{
  kpidLink2 = kpidUserDefined,
  kpidLinkType,
  kpidRecMTime,
  kpidRecMTime2,
  kpidMTime2,
  kpidCTime2,
  kpidATime2
};

static const CStatProp kProps[] =
{
  { NULL, kpidPath, VT_BSTR},
  { NULL, kpidSize, VT_UI8},
  { NULL, kpidPackSize, VT_UI8},

  // { NULL, kpidLink, VT_BSTR},
  
  // { "Link 2", kpidLink2, VT_BSTR},
  // { "Link Type", kpidLinkType, VT_UI2},
  { NULL, kpidINode, VT_UI8},
 
  { NULL, kpidMTime, VT_FILETIME},
  { NULL, kpidCTime, VT_FILETIME},
  { NULL, kpidATime, VT_FILETIME},
  
  // { "Record Modified", kpidRecMTime, VT_FILETIME},

  // { "Modified 2", kpidMTime2, VT_FILETIME},
  // { "Created 2", kpidCTime2, VT_FILETIME},
  // { "Accessed 2", kpidATime2, VT_FILETIME},
  // { "Record Modified 2", kpidRecMTime2, VT_FILETIME},

  { NULL, kpidAttrib, VT_UI4},
  { NULL, kpidNumBlocks, VT_UI4},
  { NULL, kpidIsDeleted, VT_BOOL},
};
*/

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidChangeTime,
  kpidAttrib,
  kpidLinks,
  kpidINode,
  kpidNumBlocks,
  kpidNumAltStreams,
  kpidIsAltStream,
  kpidShortName,
  kpidIsDeleted
};

enum
{
  kpidRecordSize = kpidUserDefined
};

static const CStatProp kArcProps[] =
{
  { NULL, kpidVolumeName, VT_BSTR},
  { NULL, kpidFileSystem, VT_BSTR},
  { NULL, kpidClusterSize, VT_UI4},
  { NULL, kpidSectorSize, VT_UI4},
  { "MFT Record Size", kpidRecordSize, VT_UI4},
  { NULL, kpidHeadersSize, VT_UI8},
  { NULL, kpidCTime, VT_FILETIME},
  { NULL, kpidId, VT_UI8}
};

/*
static const Byte kArcProps[] =
{
  kpidVolumeName,
  kpidFileSystem,
  kpidClusterSize,
  kpidHeadersSize,
  kpidCTime,
  kpidSectorSize,
  kpidId
  // kpidSectorsPerTrack,
  // kpidNumHeads,
  // kpidHiddenSectors
};
*/

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME

static void NtfsTimeToProp(UInt64 t, NCOM::CPropVariant &prop)
{
  FILETIME ft;
  ft.dwLowDateTime = (DWORD)t;
  ft.dwHighDateTime = (DWORD)(t >> 32);
  prop = ft;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(const PROPID propID, PROPVARIANT * const value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  const CMftRec *volRec = (Recs.Size() > kRecIndex_Volume ? &Recs[kRecIndex_Volume] : NULL);

  switch (propID)
  {
    case kpidClusterSize: prop = Header.ClusterSize(); break;
    case kpidPhySize: prop = PhySize; break;
    /*
    case kpidHeadersSize:
    {
      UInt64 val = 0;
      for (unsigned i = 0; i < kNumSysRecs; i++)
      {
        printf("\n%2d: %8I64d ", i, Recs[i].GetPackSize());
        if (i == 8)
          i = i
        val += Recs[i].GetPackSize();
      }
      prop = val;
      break;
    }
    */
    case kpidCTime: if (volRec) NtfsTimeToProp(volRec->SiAttr.CTime, prop); break;
    case kpidMTime: if (volRec) NtfsTimeToProp(volRec->SiAttr.MTime, prop); break;
    case kpidShortComment:
    case kpidVolumeName:
      if (!VolumeInfo.VolName.IsEmpty())
        prop = VolumeInfo.VolName.GetRawPtr();
      break;
    case kpidFileSystem:
    {
      char buf[32];
      char *s = MyStpCpy(buf, "NTFS");
      if (VolumeInfo.FsVerMajor >= 0)
      {
        *s++ = ' ';  s = ConvertUInt32ToString((UInt32)(unsigned)VolumeInfo.FsVerMajor, s);
        *s++ = '.';      ConvertUInt32ToString(VolumeInfo.FsVerMinor, s);
      }
      prop = buf;
      break;
    }
    case kpidSectorSize: prop = (UInt32)1 << Header.SectorSizeLog; break;
    case kpidRecordSize: prop = (UInt32)1 << Header.MftRecordSizeLog; break;
    case kpidId: prop = Header.SerialNumber; break;

    case kpidIsTree: prop = true; break;
    case kpidIsDeleted: prop = _showDeletedFiles; break;
    case kpidIsAltStream: prop = ThereAreAltStreams; break;
    // case kpidIsAux: prop = true; break;
    case kpidINode: prop = true; break;

    case kpidWarning:
      if (_lostFolderIndex_Normal >= 0)
        prop = "There are lost files";
      break;

    /*
    case kpidWarningFlags:
    {
      UInt32 flags = 0;
      if (_headerWarning)
        flags |= k_ErrorFlags_HeadersError;
      if (flags != 0)
        prop = flags;
      break;
    }
    */
    // case kpidMediaType: prop = Header.MediaType; break;
    // case kpidSectorsPerTrack: prop = Header.SectorsPerTrack; break;
    // case kpidNumHeads: prop = Header.NumHeads; break;
    // case kpidHiddenSectors: prop = Header.NumHiddenSectors; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetProperty(const UInt32 index, const PROPID propID, PROPVARIANT * const value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (index >= Items.Size())
  {
    switch (propID)
    {
      case kpidName:
      case kpidPath:
        prop = VirtFolderNames[index - Items.Size()].GetRawPtr();
        break;
      case kpidIsDir: prop = true; break;
      // case kpidIsAux: prop = true; break;
      case kpidIsDeleted:
        if ((int)index == _lostFolderIndex_Deleted)
          prop = true;
        break;
    }
  }
  else
  {

  const CItem &item = Items[index];
  const CMftRec &rec = Recs[item.RecIndex];
  const CAttr *data = NULL;
  if (item.DataIndex >= 0)
    data = &rec.DataAttrs[item.DataIndex];
  // const CFileNameAttr *fn = &rec.FileNames[item.NameIndex];
  /*
  if (rec.FileNames.Size())
    fn = &rec.FileNames[0];
  */

  switch (propID)
  {
    case kpidPath:
      GetItemPath(index, prop);
      break;
    /*
    case kpidLink: if (!rec.ReparseAttr.SubsName.IsEmpty())
      { prop = rec.ReparseAttr.SubsName; }
      break;
    case kpidLink2: if (!rec.ReparseAttr.PrintName.IsEmpty())
      { prop = rec.ReparseAttr.PrintName; }
      break;
    case kpidLinkType: if (rec.ReparseAttr.Tag != 0)
      { prop = (rec.ReparseAttr.Tag & 0xFFFF); }
      break;
    */
    case kpidINode:
    {
      // prop = ((UInt64)rec.SeqNumber << 48) | item.RecIndex;
      prop = (UInt32)item.RecIndex;
      break;
    }
    case kpidStreamId:
    {
      if (item.DataIndex >= 0)
        prop = ((UInt64)item.RecIndex << 32) | (unsigned)item.DataIndex;
      break;
    }

    case kpidName:
    {
      const UString2 *s = item.IsAltStream() ?
           &data->Name :
           &rec.FileNames[item.NameIndex].Name;
      if (s->IsEmpty())
        s = &EmptyString;
      prop = s->GetRawPtr();
      break;
    }

    case kpidShortName:
    {
      if (!item.IsAltStream())
      {
        const int dosNameIndex = rec.FindDosName(item.NameIndex);
        if (dosNameIndex >= 0)
        {
          const UString2 *s = &rec.FileNames[dosNameIndex].Name;
          if (s->IsEmpty())
            s = &EmptyString;
          prop = s->GetRawPtr();
        }
      }
      break;
    }

    case kpidIsDir: prop = item.IsDir(); break;
    case kpidIsAltStream: prop = item.IsAltStream(); break;
    case kpidIsDeleted: prop = !rec.InUse(); break;
    // case kpidIsAux: prop = false; break;

    case kpidMTime: NtfsTimeToProp(rec.SiAttr.MTime, prop); break;
    case kpidCTime: NtfsTimeToProp(rec.SiAttr.CTime, prop); break;
    case kpidATime: NtfsTimeToProp(rec.SiAttr.ATime, prop); break;
    case kpidChangeTime: NtfsTimeToProp(rec.SiAttr.ThisRecMTime, prop); break;
    /*
    case kpidMTime2: if (fn) NtfsTimeToProp(fn->MTime, prop); break;
    case kpidCTime2: if (fn) NtfsTimeToProp(fn->CTime, prop); break;
    case kpidATime2: if (fn) NtfsTimeToProp(fn->ATime, prop); break;
    case kpidRecMTime2: if (fn) NtfsTimeToProp(fn->ThisRecMTime, prop); break;
    */
    case kpidAttrib:
    {
      UInt32 attrib = 0;
      if (item.IsAltStream() && item.RecIndex == kRecIndex_RootDir)
      {
        break;
      }
      else
      {
      /* WinXP-64: The CFileNameAttr::Attrib is not updated  after some changes. Why?
         CSiAttr:attrib is updated better. So we use CSiAttr:Sttrib */
      /*
      if (fn)
        attrib = fn->Attrib;
      else
      */
        attrib = rec.SiAttr.Attrib;
      if (item.IsDir())
        attrib |= FILE_ATTRIBUTE_DIRECTORY;

      /* some system entries can contain extra flags (Index View).
      // 0x10000000   (Directory)
      // 0x20000000   FILE_ATTR_VIEW_INDEX_PRESENT MFT_RECORD_IS_VIEW_INDEX (Index View)
      But we don't need them */
      attrib &= 0xFFFF;
      }

      prop = attrib;
      break;
    }
    case kpidLinks: if (rec.MyNumNameLinks != 1) prop = (UInt32)rec.MyNumNameLinks; break;
    
    case kpidNumAltStreams:
    {
      if (!item.IsAltStream())
      {
        unsigned num = 0;
        for (unsigned i = 0; i < rec.DataAttrs.Size(); num++)
          i = rec.DataAttrs[i].NextAttrIndex;
        if (num)
        {
          if (!rec.IsDir() && rec.DataAttrs[0].Name.IsEmpty())
            num--;
          if (num)
            prop = (UInt32)num;
        }
      }
      break;
    }
    
    case kpidSize: if (data) prop = data->GetSize(); else if (!item.IsDir()) prop = (UInt64)0; break;
    case kpidPackSize: if (data) prop = data->GetPackSize(); else if (!item.IsDir()) prop = (UInt64)0; break;
    case kpidNumBlocks: if (data) prop = (UInt32)rec.GetNumExtents(item.DataIndex, Header.ClusterSizeLog, Header.NumClusters); break;
  }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  {
    OpenCallback = callback;
    InStream = stream;
    HRESULT res;
    try
    {
      res = CDatabase::Open();
      if (res == S_OK)
        return S_OK;
    }
    catch(...)
    {
      Close();
      throw;
    }
    Close();
    return res;
  }
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  ClearAndClose();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = Items.Size() + VirtFolderNames.Size();
  if (numItems == 0)
    return S_OK;
  UInt32 i;
  UInt64 totalSize = 0;
  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = allFilesMode ? i : indices[i];
    if (index >= Items.Size())
      continue;
    const CItem &item = Items[allFilesMode ? i : indices[i]];
    const CMftRec &rec = Recs[item.RecIndex];
    if (item.DataIndex >= 0)
      totalSize += rec.GetSize((unsigned)item.DataIndex);
  }
  RINOK(extractCallback->SetTotal(totalSize))

  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ISequentialOutStream, CDummyOutStream> outStream;
  CMyComPtr2<IInStream, CInStream> inStream_Object;
  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;

  for (i = 0;; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;

    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))

    if (index >= Items.Size() || Items[index].IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))

    outStream->SetStream(realOutStream);
    realOutStream.Release();
    outStream->Init();

    const CItem &item = Items[index];
    const CMftRec &rec = Recs[item.RecIndex];
    UInt64 unpackSize = 0;
    if (item.DataIndex >= 0)
    {
      const CAttr &data = rec.DataAttrs[item.DataIndex];
      totalPackSize += data.GetPackSize();
      unpackSize = data.GetSize();
      totalSize += unpackSize;
    }
    int res = NExtract::NOperationResult::kDataError;
    {
      CMyComPtr<IInStream> inStream;
      HRESULT hres = rec.GetStream(InStream, item.DataIndex,
          Header.ClusterSizeLog, Header.NumClusters, &inStream, &inStream_Object);
      if (hres == S_FALSE)
        res = NExtract::NOperationResult::kUnsupportedMethod;
      else
      {
        RINOK(hres)
        if (inStream)
        {
          hres = copyCoder.Interface()->Code(inStream, outStream, NULL, NULL, lps);
          if (hres == S_OK)
          {
            // if (copyCoder->TotalSize == unpackSize)
            res = NExtract::NOperationResult::kOK;
          }
          else if (hres != S_FALSE)
            return hres;
        }
      }
    }
    outStream->ReleaseStream();
    RINOK(extractCallback->SetOperationResult(res))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Items.Size() + VirtFolderNames.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps))
{
  InitProps();

  for (UInt32 i = 0; i < numProps; i++)
  {
    const wchar_t *name = names[i];
    const PROPVARIANT &prop = values[i];

    if (StringsAreEqualNoCase_Ascii(name, "ld"))
    {
      RINOK(PROPVARIANT_to_bool(prop, _showDeletedFiles))
    }
    else if (StringsAreEqualNoCase_Ascii(name, "ls"))
    {
      RINOK(PROPVARIANT_to_bool(prop, _showSystemFiles))
    }
    else if (IsString1PrefixedByString2_NoCase_Ascii(name, "mt"))
    {
    }
    else if (IsString1PrefixedByString2_NoCase_Ascii(name, "memuse"))
    {
    }
    else
      return E_INVALIDARG;
  }
  return S_OK;
}

REGISTER_ARC_I(
  "NTFS", "ntfs img", NULL, 0xD9,
  k_Signature,
  3,
  0,
  NULL)

}}
