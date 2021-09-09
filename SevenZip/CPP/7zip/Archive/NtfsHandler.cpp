// NtfsHandler.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO
// #define SHOW_DEBUG_INFO2

#if defined(SHOW_DEBUG_INFO) || defined(SHOW_DEBUG_INFO2)
#include <stdio.h>
#endif

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"
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
#define PRF_UTF16(x) PRF(printf("%S", x))
#else
#define PRF(x)
#define PRF_UTF16(x)
#endif

#ifdef SHOW_DEBUG_INFO2
#define PRF2(x) x
#else
#define PRF2(x)
#endif

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define G16(p, dest) dest = Get16(p);
#define G32(p, dest) dest = Get32(p);
#define G64(p, dest) dest = Get64(p);

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

struct CHeader
{
  unsigned SectorSizeLog;
  unsigned ClusterSizeLog;
  // Byte MediaType;
  UInt32 NumHiddenSectors;
  UInt64 NumSectors;
  UInt64 NumClusters;
  UInt64 MftCluster;
  UInt64 SerialNumber;
  UInt16 SectorsPerTrack;
  UInt16 NumHeads;

  UInt64 GetPhySize_Clusters() const { return NumClusters << ClusterSizeLog; }
  UInt64 GetPhySize_Max() const { return (NumSectors + 1) << SectorSizeLog; }
  UInt32 ClusterSize() const { return (UInt32)1 << ClusterSizeLog; }
  bool Parse(const Byte *p);
};

static int GetLog(UInt32 num)
{
  for (int i = 0; i < 31; i++)
    if (((UInt32)1 << i) == num)
      return i;
  return -1;
}

bool CHeader::Parse(const Byte *p)
{
  if (p[0x1FE] != 0x55 || p[0x1FF] != 0xAA)
    return false;

  // int codeOffset = 0;
  switch (p[0])
  {
    case 0xE9: /* codeOffset = 3 + (Int16)Get16(p + 1); */ break;
    case 0xEB: if (p[2] != 0x90) return false; /* codeOffset = 2 + (int)(signed char)p[1]; */ break;
    default: return false;
  }
  unsigned sectorsPerClusterLog;

  if (memcmp(p + 3, "NTFS    ", 8) != 0)
    return false;
  {
    int t = GetLog(Get16(p + 11));
    if (t < 9 || t > 12)
      return false;
    SectorSizeLog = t;
    t = GetLog(p[13]);
    if (t < 0)
      return false;
    sectorsPerClusterLog = t;
    ClusterSizeLog = SectorSizeLog + sectorsPerClusterLog;
    if (ClusterSizeLog > 30)
      return false;
  }

  for (int i = 14; i < 21; i++)
    if (p[i] != 0)
      return false;

  if (p[21] != 0xF8) // MediaType = Fixed_Disk
    return false;
  if (Get16(p + 22) != 0) // NumFatSectors
    return false;
  G16(p + 24, SectorsPerTrack); // 63 usually
  G16(p + 26, NumHeads); // 255
  G32(p + 28, NumHiddenSectors); // 63 (XP) / 2048 (Vista and win7) / (0 on media that are not partitioned ?)
  if (Get32(p + 32) != 0) // NumSectors32
    return false;

  // DriveNumber = p[0x24];
  if (p[0x25] != 0) // CurrentHead
    return false;
  /*
  NTFS-HDD:   p[0x26] = 0x80
  NTFS-FLASH: p[0x26] = 0
  */
  if (p[0x26] != 0x80 && p[0x26] != 0) // ExtendedBootSig
    return false;
  if (p[0x27] != 0) // reserved
    return false;
  
  NumSectors = Get64(p + 0x28);
  if (NumSectors >= ((UInt64)1 << (62 - SectorSizeLog)))
    return false;

  NumClusters = NumSectors >> sectorsPerClusterLog;

  G64(p + 0x30, MftCluster);
  // G64(p + 0x38, Mft2Cluster);
  G64(p + 0x48, SerialNumber);
  UInt32 numClustersInMftRec;
  UInt32 numClustersInIndexBlock;
  G32(p + 0x40, numClustersInMftRec); // -10 means 2 ^10 = 1024 bytes.
  G32(p + 0x44, numClustersInIndexBlock);
  return (numClustersInMftRec < 256 && numClustersInIndexBlock < 256);
}

struct CMftRef
{
  UInt64 Val;
  
  UInt64 GetIndex() const { return Val & (((UInt64)1 << 48) - 1); }
  UInt16 GetNumber() const { return (UInt16)(Val >> 48); }
  bool IsBaseItself() const { return Val == 0; }
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

// static const Byte kFileNameType_Posix     = 0; // for hard links
static const Byte kFileNameType_Win32     = 1; // after Dos name
static const Byte kFileNameType_Dos       = 2; // short name
static const Byte kFileNameType_Win32Dos  = 3; // short and full name are same

struct CFileNameAttr
{
  CMftRef ParentDirRef;

  // Probably these timestamps don't contain some useful timestamps. So we don't use them
  // UInt64 CTime;
  // UInt64 MTime;
  // UInt64 ThisRecMTime;  // xp-64: the time of previous name change (not last name change. why?)
  // UInt64 ATime;
  // UInt64 AllocatedSize;
  // UInt64 DataSize;
  // UInt16 PackedEaSize;
  UString2 Name;
  UInt32 Attrib;
  Byte NameType;
  
  bool IsDos() const { return NameType == kFileNameType_Dos; }
  bool IsWin32() const { return (NameType == kFileNameType_Win32); }

  bool Parse(const Byte *p, unsigned size);
};

static void GetString(const Byte *p, unsigned len, UString2 &res)
{
  if (len == 0 && res.IsEmpty())
    return;
  wchar_t *s = res.GetBuf(len);
  unsigned i;
  for (i = 0; i < len; i++)
  {
    wchar_t c = Get16(p + i * 2);
    if (c == 0)
      break;
    s[i] = c;
  }
  s[i] = 0;
  res.ReleaseBuf_SetLen(i);
}

bool CFileNameAttr::Parse(const Byte *p, unsigned size)
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
  G32(p + 0x38, Attrib);
  // G16(p + 0x3C, PackedEaSize);
  NameType = p[0x41];
  unsigned len = p[0x40];
  if (0x42 + len > size)
    return false;
  if (len != 0)
    GetString(p + 0x42, len, Name);
  return true;
}

struct CSiAttr
{
  UInt64 CTime;
  UInt64 MTime;
  // UInt64 ThisRecMTime;
  UInt64 ATime;
  UInt32 Attrib;

  /*
  UInt32 MaxVersions;
  UInt32 Version;
  UInt32 ClassId;
  UInt32 OwnerId;
  */
  UInt32 SecurityId; // SecurityId = 0 is possible ?
  // UInt64 QuotaCharged;

  bool Parse(const Byte *p, unsigned size);
};

bool CSiAttr::Parse(const Byte *p, unsigned size)
{
  if (size < 0x24)
    return false;
  G64(p + 0x00, CTime);
  G64(p + 0x08, MTime);
  // G64(p + 0x10, ThisRecMTime);
  G64(p + 0x18, ATime);
  G32(p + 0x20, Attrib);
  SecurityId = 0;
  if (size >= 0x38)
    G32(p + 0x34, SecurityId);
  return true;
}

static const UInt64 kEmptyExtent = (UInt64)(Int64)-1;

struct CExtent
{
  UInt64 Virt;
  UInt64 Phy;

  bool IsEmpty() const { return Phy == kEmptyExtent; }
};

struct CVolInfo
{
  Byte MajorVer;
  Byte MinorVer;
  // UInt16 Flags;

  bool Parse(const Byte *p, unsigned size);
};

bool CVolInfo::Parse(const Byte *p, unsigned size)
{
  if (size < 12)
    return false;
  MajorVer = p[8];
  MinorVer = p[9];
  // Flags = Get16(p + 10);
  return true;
}

struct CAttr
{
  UInt32 Type;

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

  bool IsCompressionUnitSupported() const { return CompressionUnit == 0 || CompressionUnit == 4; }

  UInt32 Parse(const Byte *p, unsigned size);
  bool ParseFileName(CFileNameAttr &a) const { return a.Parse(Data, (unsigned)Data.Size()); }
  bool ParseSi(CSiAttr &a) const { return a.Parse(Data, (unsigned)Data.Size()); }
  bool ParseVolInfo(CVolInfo &a) const { return a.Parse(Data, (unsigned)Data.Size()); }
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
};

#define RINOZ(x) { int __tt = (x); if (__tt != 0) return __tt; }

static int CompareAttr(void *const *elem1, void *const *elem2, void *)
{
  const CAttr &a1 = *(*((const CAttr *const *)elem1));
  const CAttr &a2 = *(*((const CAttr *const *)elem2));
  RINOZ(MyCompare(a1.Type, a2.Type));
  if (a1.Name.IsEmpty())
  {
    if (!a2.Name.IsEmpty())
      return -1;
  }
  else if (a2.Name.IsEmpty())
    return 1;
  else
  {
    RINOZ(a1.Name.Compare(a2.Name.GetRawPtr()));
  }
  return MyCompare(a1.LowVcn, a2.LowVcn);
}

UInt32 CAttr::Parse(const Byte *p, unsigned size)
{
  if (size < 4)
    return 0;
  G32(p, Type);
  if (Type == 0xFFFFFFFF)
    return 8; // required size is 4, but attributes are 8 bytes aligned. So we return 8
  if (size < 0x18)
    return 0;

  PRF(printf(" T=%2X", Type));
  
  UInt32 len = Get32(p + 4);
  PRF(printf(" L=%3d", len));
  if (len > size)
    return 0;
  if ((len & 7) != 0)
    return 0;
  NonResident = p[8];
  {
    unsigned nameLen = p[9];
    UInt32 nameOffset = Get16(p + 0x0A);
    if (nameLen != 0)
    {
      if (nameOffset + nameLen * 2 > len)
        return 0;
      GetString(p + nameOffset, nameLen, Name);
      PRF(printf(" N="));
      PRF_UTF16(Name);
    }
  }

  // G16(p + 0x0C, Flags);
  // G16(p + 0x0E, Instance);
  // PRF(printf(" F=%4X", Flags));
  // PRF(printf(" Inst=%d", Instance));

  UInt32 dataSize;
  UInt32 offs;
  
  if (NonResident)
  {
    if (len < 0x40)
      return 0;
    PRF(printf(" NR"));
    G64(p + 0x10, LowVcn);
    G64(p + 0x18, HighVcn);
    G64(p + 0x28, AllocatedSize);
    G64(p + 0x30, Size);
    G64(p + 0x38, InitializedSize);
    G16(p + 0x20, offs);
    CompressionUnit = p[0x22];

    PackSize = Size;
    if (CompressionUnit != 0)
    {
      if (len < 0x48)
        return 0;
      G64(p + 0x40, PackSize);
      PRF(printf(" PS=%I64x", PackSize));
    }

    // PRF(printf("\n"));
    PRF(printf(" ASize=%4I64d", AllocatedSize));
    PRF(printf(" Size=%I64d", Size));
    PRF(printf(" IS=%I64d", InitializedSize));
    PRF(printf(" Low=%I64d", LowVcn));
    PRF(printf(" High=%I64d", HighVcn));
    PRF(printf(" CU=%d", (unsigned)CompressionUnit));
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
    PRF(printf(" dataSize=%3d", dataSize));
    // PRF(printf(" ResFlags=%4X", ResidentFlags));
  }
  
  if (offs > len || dataSize > len || len - dataSize < offs)
    return 0;
  
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


bool CAttr::ParseExtents(CRecordVector<CExtent> &extents, UInt64 numClustersMax, unsigned compressionUnit) const
{
  const Byte *p = Data;
  unsigned size = (unsigned)Data.Size();
  UInt64 vcn = LowVcn;
  UInt64 lcn = 0;
  const UInt64 highVcn1 = HighVcn + 1;
  
  if (LowVcn != extents.Back().Virt || highVcn1 > (UInt64)1 << 63)
    return false;

  extents.DeleteBack();

  PRF2(printf("\n# ParseExtents # LowVcn = %4I64X # HighVcn = %4I64X", LowVcn, HighVcn));

  while (size > 0)
  {
    Byte b = *p++;
    size--;
    if (b == 0)
      break;
    UInt32 num = b & 0xF;
    if (num == 0 || num > 8 || num > size)
      return false;

    UInt64 vSize = 0;
    {
      unsigned i = num;
      do vSize = (vSize << 8) | p[--i]; while (i);
    }
    if (vSize == 0)
      return false;
    p += num;
    size -= num;
    if ((highVcn1 - vcn) < vSize)
      return false;

    CExtent e;
    e.Virt = vcn;
    vcn += vSize;

    num = (b >> 4) & 0xF;
    if (num > 8 || num > size)
      return false;
    
    if (num == 0)
    {
      // Sparse
      
      /* if Unit is compressed, it can have many Elements for each compressed Unit:
         and last Element for unit MUST be without LCN.
           Element 0: numCompressedClusters2, LCN_0
           Element 1: numCompressedClusters2, LCN_1
           ...
           Last Element : (16 - total_clusters_in_previous_elements), no LCN
      */
      
      // sparse is not allowed for (compressionUnit == 0) ? Why ?
      if (compressionUnit == 0)
        return false;

      e.Phy = kEmptyExtent;
    }
    else
    {
      Int64 v = (signed char)p[num - 1];
      {
        for (unsigned i = num - 1; i != 0;)
          v = (v << 8) | p[--i];
      }
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
  return (highVcn1 == vcn);
}


static const UInt64 kEmptyTag = (UInt64)(Int64)-1;

static const unsigned kNumCacheChunksLog = 1;
static const size_t kNumCacheChunks = (size_t)1 << kNumCacheChunksLog;

class CInStream:
  public IInStream,
  public CMyUnknownImp
{
  UInt64 _virtPos;
  UInt64 _physPos;
  UInt64 _curRem;
  bool _sparseMode;
  

  unsigned _chunkSizeLog;
  UInt64 _tags[kNumCacheChunks];
  CByteBuffer _inBuf;
  CByteBuffer _outBuf;
public:
  UInt64 Size;
  UInt64 InitializedSize;
  unsigned BlockSizeLog;
  unsigned CompressionUnit;
  CRecordVector<CExtent> Extents;
  bool InUse;
  CMyComPtr<IInStream> Stream;

  HRESULT SeekToPhys() { return Stream->Seek(_physPos, STREAM_SEEK_SET, NULL); }

  UInt32 GetCuSize() const { return (UInt32)1 << (BlockSizeLog + CompressionUnit); }
  HRESULT InitAndSeek(unsigned compressionUnit)
  {
    CompressionUnit = compressionUnit;
    _chunkSizeLog = BlockSizeLog + CompressionUnit;
    if (compressionUnit != 0)
    {
      UInt32 cuSize = GetCuSize();
      _inBuf.Alloc(cuSize);
      _outBuf.Alloc(kNumCacheChunks << _chunkSizeLog);
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

  MY_UNKNOWN_IMP1(IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};

static size_t Lznt1Dec(Byte *dest, size_t outBufLim, size_t destLen, const Byte *src, size_t srcLen)
{
  size_t destSize = 0;
  while (destSize < destLen)
  {
    if (srcLen < 2 || (destSize & 0xFFF) != 0)
      break;
    UInt32 comprSize;
    {
      const UInt32 v = Get16(src);
      if (v == 0)
        break;
      src += 2;
      srcLen -= 2;
      comprSize = (v & 0xFFF) + 1;
      if (comprSize > srcLen)
        break;
      srcLen -= comprSize;
      if ((v & 0x8000) == 0)
      {
        if (comprSize != (1 << 12))
          break;
        memcpy(dest + destSize, src, comprSize);
        src += comprSize;
        destSize += comprSize;
        continue;
      }
    }
    {
      if (destSize + (1 << 12) > outBufLim || (src[0] & 1) != 0)
        return 0;
      unsigned numDistBits = 4;
      UInt32 sbOffset = 0;
      UInt32 pos = 0;

      do
      {
        comprSize--;
        for (UInt32 mask = src[pos++] | 0x100; mask > 1 && comprSize > 0; mask >>= 1)
        {
          if ((mask & 1) == 0)
          {
            if (sbOffset >= (1 << 12))
              return 0;
            dest[destSize++] = src[pos++];
            sbOffset++;
            comprSize--;
          }
          else
          {
            if (comprSize < 2)
              return 0;
            const UInt32 v = Get16(src + pos);
            pos += 2;
            comprSize -= 2;

            while (((sbOffset - 1) >> numDistBits) != 0)
              numDistBits++;

            UInt32 len = (v & (0xFFFF >> numDistBits)) + 3;
            if (sbOffset + len > (1 << 12))
              return 0;
            UInt32 dist = (v >> (16 - numDistBits));
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
      }
      while (comprSize > 0);
      src += pos;
    }
  }
  return destSize;
}

STDMETHODIMP CInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  if (_virtPos >= Size)
    return (Size == _virtPos) ? S_OK: E_FAIL;
  if (size == 0)
    return S_OK;
  {
    const UInt64 rem = Size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }

  if (_virtPos >= InitializedSize)
  {
    memset((Byte *)data, 0, size);
    _virtPos += size;
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
      *processedSize = (UInt32)cur;
      _virtPos += cur;
      return S_OK;
    }

    PRF2(printf("\nVirtPos = %6d", _virtPos));
    
    const UInt32 comprUnitSize = (UInt32)1 << CompressionUnit;
    const UInt64 virtBlock = _virtPos >> BlockSizeLog;
    const UInt64 virtBlock2 = virtBlock & ~((UInt64)comprUnitSize - 1);
    
    unsigned left = 0, right = Extents.Size();
    for (;;)
    {
      unsigned mid = (left + right) / 2;
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
      UInt64 newPos = (e.Phy << BlockSizeLog) + _virtPos - (e.Virt << BlockSizeLog);
      if (newPos != _physPos)
      {
        _physPos = newPos;
        RINOK(SeekToPhys());
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
      UInt64 newPos = (e.Phy + (curVirt - e.Virt)) << BlockSizeLog;
      if (newPos != _physPos)
      {
        _physPos = newPos;
        RINOK(SeekToPhys());
      }
      UInt64 numChunks = Extents[i + 1].Virt - curVirt;
      if (curVirt + numChunks > virtBlock2End)
        numChunks = virtBlock2End - curVirt;
      size_t compressed = (size_t)numChunks << BlockSizeLog;
      RINOK(ReadStream_FALSE(Stream, _inBuf + offs, compressed));
      curVirt += numChunks;
      _physPos += compressed;
      offs += compressed;
    }
    
    size_t destLenMax = GetCuSize();
    size_t destLen = destLenMax;
    const UInt64 rem = Size - (virtBlock2 << BlockSizeLog);
    if (destLen > rem)
      destLen = (size_t)rem;

    Byte *dest = _outBuf + (cacheIndex << _chunkSizeLog);
    size_t destSizeRes = Lznt1Dec(dest, destLenMax, destLen, _inBuf, offs);
    _tags[cacheIndex] = cacheTag;

    // some files in Vista have destSize > destLen
    if (destSizeRes < destLen)
    {
      memset(dest, 0, destLenMax);
      if (InUse)
        return S_FALSE;
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
 
STDMETHODIMP CInStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
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
    _virtPos = offset;
  }
  if (newPosition)
    *newPosition = offset;
  return S_OK;
}

static HRESULT DataParseExtents(unsigned clusterSizeLog, const CObjectVector<CAttr> &attrs,
    unsigned attrIndex, unsigned attrIndexLim, UInt64 numPhysClusters, CRecordVector<CExtent> &Extents)
{
  {
    CExtent e;
    e.Virt = 0;
    e.Phy = kEmptyExtent;
    Extents.Add(e);
  }
  
  const CAttr &attr0 = attrs[attrIndex];

  /*
  if (attrs[attrIndexLim - 1].HighVcn + 1 != (attr0.AllocatedSize >> clusterSizeLog))
  {
  }
  */

  if (attr0.AllocatedSize < attr0.Size ||
      (attrs[attrIndexLim - 1].HighVcn + 1) != (attr0.AllocatedSize >> clusterSizeLog) ||
      (attr0.AllocatedSize & ((1 << clusterSizeLog) - 1)) != 0)
    return S_FALSE;
  
  for (unsigned i = attrIndex; i < attrIndexLim; i++)
    if (!attrs[i].ParseExtents(Extents, numPhysClusters, attr0.CompressionUnit))
      return S_FALSE;

  UInt64 packSizeCalc = 0;
  FOR_VECTOR (k, Extents)
  {
    CExtent &e = Extents[k];
    if (!e.IsEmpty())
      packSizeCalc += (Extents[k + 1].Virt - e.Virt) << clusterSizeLog;
    PRF2(printf("\nSize = %4I64X", Extents[k + 1].Virt - e.Virt));
    PRF2(printf("  Pos = %4I64X", e.Phy));
  }
  
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

struct CDataRef
{
  unsigned Start;
  unsigned Num;
};

static const UInt32 kMagic_FILE = 0x454C4946;
static const UInt32 kMagic_BAAD = 0x44414142;

struct CMftRec
{
  UInt32 Magic;
  // UInt64 Lsn;
  UInt16 SeqNumber;  // Number of times this mft record has been reused
  UInt16 Flags;
  // UInt16 LinkCount;
  // UInt16 NextAttrInstance;
  CMftRef BaseMftRef;
  // UInt32 ThisRecNumber;
  
  UInt32 MyNumNameLinks;
  int MyItemIndex; // index in Items[] of main item  for that record, or -1 if there is no item for that record

  CObjectVector<CAttr> DataAttrs;
  CObjectVector<CFileNameAttr> FileNames;
  CRecordVector<CDataRef> DataRefs;
  // CAttr SecurityAttr;

  CSiAttr SiAttr;
  
  CByteBuffer ReparseData;

  int FindWin32Name_for_DosName(unsigned dosNameIndex) const
  {
    const CFileNameAttr &cur = FileNames[dosNameIndex];
    if (cur.IsDos())
      for (unsigned i = 0; i < FileNames.Size(); i++)
      {
        const CFileNameAttr &next = FileNames[i];
        if (next.IsWin32() && cur.ParentDirRef.Val == next.ParentDirRef.Val)
          return i;
      }
    return -1;
  }

  int FindDosName(unsigned nameIndex) const
  {
    const CFileNameAttr &cur = FileNames[nameIndex];
    if (cur.IsWin32())
      for (unsigned i = 0; i < FileNames.Size(); i++)
      {
        const CFileNameAttr &next = FileNames[i];
        if (next.IsDos() && cur.ParentDirRef.Val == next.ParentDirRef.Val)
          return i;
      }
    return -1;
  }

  /*
  bool IsAltStream(int dataIndex) const
  {
    return dataIndex >= 0 && (
      (IsDir() ||
      !DataAttrs[DataRefs[dataIndex].Start].Name.IsEmpty()));
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
    FOR_VECTOR (i, DataRefs)
      res += DataAttrs[DataRefs[i].Start].GetPackSize();
    return res;
  }

  bool Parse(Byte *p, unsigned sectorSizeLog, UInt32 numSectors, UInt32 recNumber, CObjectVector<CAttr> *attrs);

  bool IsEmpty() const { return (Magic <= 2); }
  bool IsFILE() const { return (Magic == kMagic_FILE); }
  bool IsBAAD() const { return (Magic == kMagic_BAAD); }

  bool InUse() const { return (Flags & 1) != 0; }
  bool IsDir() const { return (Flags & 2) != 0; }

  void ParseDataNames();
  HRESULT GetStream(IInStream *mainStream, int dataIndex,
      unsigned clusterSizeLog, UInt64 numPhysClusters, IInStream **stream) const;
  unsigned GetNumExtents(int dataIndex, unsigned clusterSizeLog, UInt64 numPhysClusters) const;

  UInt64 GetSize(unsigned dataIndex) const { return DataAttrs[DataRefs[dataIndex].Start].GetSize(); }

  CMftRec(): MyNumNameLinks(0), MyItemIndex(-1) {}
};

void CMftRec::ParseDataNames()
{
  DataRefs.Clear();
  DataAttrs.Sort(CompareAttr, NULL);

  for (unsigned i = 0; i < DataAttrs.Size();)
  {
    CDataRef ref;
    ref.Start = i;
    for (i++; i < DataAttrs.Size(); i++)
      if (DataAttrs[ref.Start].Name != DataAttrs[i].Name)
        break;
    ref.Num = i - ref.Start;
    DataRefs.Add(ref);
  }
}

HRESULT CMftRec::GetStream(IInStream *mainStream, int dataIndex,
    unsigned clusterSizeLog, UInt64 numPhysClusters, IInStream **destStream) const
{
  *destStream = 0;
  CBufferInStream *streamSpec = new CBufferInStream;
  CMyComPtr<IInStream> streamTemp = streamSpec;

  if (dataIndex >= 0)
  if ((unsigned)dataIndex < DataRefs.Size())
  {
    const CDataRef &ref = DataRefs[dataIndex];
    unsigned numNonResident = 0;
    unsigned i;
    for (i = ref.Start; i < ref.Start + ref.Num; i++)
      if (DataAttrs[i].NonResident)
        numNonResident++;

    const CAttr &attr0 = DataAttrs[ref.Start];
      
    if (numNonResident != 0 || ref.Num != 1)
    {
      if (numNonResident != ref.Num || !attr0.IsCompressionUnitSupported())
        return S_FALSE;
      CInStream *ss = new CInStream;
      CMyComPtr<IInStream> streamTemp2 = ss;
      RINOK(DataParseExtents(clusterSizeLog, DataAttrs, ref.Start, ref.Start + ref.Num, numPhysClusters, ss->Extents));
      ss->Size = attr0.Size;
      ss->InitializedSize = attr0.InitializedSize;
      ss->Stream = mainStream;
      ss->BlockSizeLog = clusterSizeLog;
      ss->InUse = InUse();
      RINOK(ss->InitAndSeek(attr0.CompressionUnit));
      *destStream = streamTemp2.Detach();
      return S_OK;
    }
  
    streamSpec->Buf = attr0.Data;
  }

  streamSpec->Init();
  *destStream = streamTemp.Detach();
  return S_OK;
}

unsigned CMftRec::GetNumExtents(int dataIndex, unsigned clusterSizeLog, UInt64 numPhysClusters) const
{
  if (dataIndex < 0)
    return 0;
  {
    const CDataRef &ref = DataRefs[dataIndex];
    unsigned numNonResident = 0;
    unsigned i;
    for (i = ref.Start; i < ref.Start + ref.Num; i++)
      if (DataAttrs[i].NonResident)
        numNonResident++;

    const CAttr &attr0 = DataAttrs[ref.Start];
      
    if (numNonResident != 0 || ref.Num != 1)
    {
      if (numNonResident != ref.Num || !attr0.IsCompressionUnitSupported())
        return 0; // error;
      CRecordVector<CExtent> extents;
      if (DataParseExtents(clusterSizeLog, DataAttrs, ref.Start, ref.Start + ref.Num, numPhysClusters, extents) != S_OK)
        return 0; // error;
      return extents.Size() - 1;
    }
    // if (attr0.Data.Size() != 0)
    //   return 1;
    return 0;
  }
}

bool CMftRec::Parse(Byte *p, unsigned sectorSizeLog, UInt32 numSectors, UInt32 recNumber,
    CObjectVector<CAttr> *attrs)
{
  G32(p, Magic);
  if (!IsFILE())
    return IsEmpty() || IsBAAD();

  
  {
    UInt32 usaOffset;
    UInt32 numUsaItems;
    G16(p + 0x04, usaOffset);
    G16(p + 0x06, numUsaItems);
      
    /* NTFS stores (usn) to 2 last bytes in each sector (before writing record to disk).
       Original values of these two bytes are stored in table.
       So we restore original data from table */

    if ((usaOffset & 1) != 0
        || usaOffset + numUsaItems * 2 > ((UInt32)1 << sectorSizeLog) - 2
        || numUsaItems == 0
        || numUsaItems - 1 != numSectors)
      return false;

    if (usaOffset >= 0x30) // NTFS 3.1+
    {
      UInt32 iii = Get32(p + 0x2C);
      if (iii != recNumber)
      {
        // ntfs-3g probably writes 0 (that probably is incorrect value) to this field for unused records.
        // so we support that "bad" case.
        if (iii != 0)
          return false;
      }
    }
    
    UInt16 usn = Get16(p + usaOffset);
    // PRF(printf("\nusn = %d", usn));
    for (UInt32 i = 1; i < numUsaItems; i++)
    {
      void *pp = p + (i << sectorSizeLog) - 2;
      if (Get16(pp) != usn)
        return false;
      SetUi16(pp, Get16(p + usaOffset + i * 2));
    }
  }

  // G64(p + 0x08, Lsn);
  G16(p + 0x10, SeqNumber);
  // G16(p + 0x12, LinkCount);
  // PRF(printf(" L=%d", LinkCount));
  UInt32 attrOffs = Get16(p + 0x14);
  G16(p + 0x16, Flags);
  PRF(printf(" F=%4X", Flags));

  UInt32 bytesInUse = Get32(p + 0x18);
  UInt32 bytesAlloc = Get32(p + 0x1C);
  G64(p + 0x20, BaseMftRef.Val);
  if (BaseMftRef.Val != 0)
  {
    PRF(printf("  BaseRef=%d", (int)BaseMftRef.Val));
    // return false; // Check it;
  }
  // G16(p + 0x28, NextAttrInstance);

  UInt32 limit = numSectors << sectorSizeLog;
  if (attrOffs >= limit
      || (attrOffs & 7) != 0
      || (bytesInUse & 7) != 0
      || bytesInUse > limit
      || bytesAlloc != limit)
    return false;

  limit = bytesInUse;

  for (UInt32 t = attrOffs;;)
  {
    if (t >= limit)
      return false;

    CAttr attr;
    // PRF(printf("\n  %2d:", Attrs.Size()));
    PRF(printf("\n"));
    UInt32 len = attr.Parse(p + t, limit - t);
    if (len == 0 || limit - t < len)
      return false;
    t += len;
    if (attr.Type == 0xFFFFFFFF)
    {
      if (t != limit)
        return false;
      break;
    }
    switch (attr.Type)
    {
      case ATTR_TYPE_FILE_NAME:
      {
        CFileNameAttr fna;
        if (!attr.ParseFileName(fna))
          return false;
        FileNames.Add(fna);
        PRF(printf("  flags = %4x\n  ", (int)fna.NameType));
        PRF_UTF16(fna.Name);
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
        ReparseData = attr.Data;
        break;
      /*
      case ATTR_TYPE_SECURITY_DESCRIPTOR:
        SecurityAttr = attr;
        break;
      */
      default:
        if (attrs)
          attrs->Add(attr);
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
  unsigned RecIndex;  // index in Recs array
  unsigned NameIndex; // index in CMftRec::FileNames

  int DataIndex;      /* index in CMftRec::DataRefs
                         -1: file without unnamed data stream
                         -2: for directories */
                         
  int ParentFolder;   /* index in Items array
                         -1: for root items
                         -2: [LOST] folder
                         -3: [UNKNOWN] folder (deleted lost) */
  int ParentHost;     /* index in Items array, if it's AltStream
                         -1: if it's not AltStream */
  
  CItem(): DataIndex(k_Item_DataIndex_IsDir), ParentFolder(-1), ParentHost(-1) {}
  
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
  CMyComPtr<IInStream> InStream;
  CHeader Header;
  unsigned RecSizeLog;
  UInt64 PhySize;

  IArchiveOpenCallback *OpenCallback;

  CByteBuffer ByteBuf;

  CObjectVector<CAttr> VolAttrs;

  CByteBuffer SecurData;
  CRecordVector<size_t> SecurOffsets;

  bool _showSystemFiles;
  bool _showDeletedFiles;
  CObjectVector<UString2> VirtFolderNames;
  UString EmptyString;

  int _systemFolderIndex;
  int _lostFolderIndex_Normal;
  int _lostFolderIndex_Deleted;

  // bool _headerWarning;

  bool ThereAreAltStreams;

  void InitProps()
  {
    _showSystemFiles = true;
    // we show SystemFiles by default since it's difficult to track $Extend\* system files
    // it must be fixed later
    _showDeletedFiles = false;
  }

  CDatabase() { InitProps(); }
  ~CDatabase() { ClearAndClose(); }

  void Clear();
  void ClearAndClose();

  void GetItemPath(unsigned index, NCOM::CPropVariant &path) const;
  HRESULT Open();

  HRESULT SeekToCluster(UInt64 cluster);

  int FindDirItemForMtfRec(UInt64 recIndex) const
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

  bool FindSecurityDescritor(UInt32 id, UInt64 &offset, UInt32 &size) const;
  
  HRESULT ParseSecuritySDS_2();
  void ParseSecuritySDS()
  {
    HRESULT res = ParseSecuritySDS_2();
    if (res != S_OK)
    {
      SecurOffsets.Clear();
      SecurData.Free();
    }
  }

};

HRESULT CDatabase::SeekToCluster(UInt64 cluster)
{
  return InStream->Seek(cluster << Header.ClusterSizeLog, STREAM_SEEK_SET, NULL);
}

void CDatabase::Clear()
{
  Items.Clear();
  Recs.Clear();
  SecurOffsets.Clear();
  SecurData.Free();
  VirtFolderNames.Clear();
  _systemFolderIndex = -1;
  _lostFolderIndex_Normal = -1;
  _lostFolderIndex_Deleted = -1;
  ThereAreAltStreams = false;
  // _headerWarning = false;
  PhySize = 0;
}

void CDatabase::ClearAndClose()
{
  Clear();
  InStream.Release();
}


static void CopyName(wchar_t *dest, const wchar_t *src)
{
  for (;;)
  {
    wchar_t c = *src++;
    // 18.06
    if (c == '\\' || c == '/')
      c = '_';
    *dest++ = c;
    if (c == 0)
      return;
  }
}

void CDatabase::GetItemPath(unsigned index, NCOM::CPropVariant &path) const
{
  const CItem *item = &Items[index];
  unsigned size = 0;
  const CMftRec &rec = Recs[item->RecIndex];
  size += rec.FileNames[item->NameIndex].Name.Len();

  bool isAltStream = item->IsAltStream();

  if (isAltStream)
  {
    const CAttr &data = rec.DataAttrs[rec.DataRefs[item->DataIndex].Start];
    if (item->RecIndex == kRecIndex_RootDir)
    {
      wchar_t *s = path.AllocBstr(data.Name.Len() + 1);
      s[0] = L':';
      if (!data.Name.IsEmpty())
        CopyName(s + 1, data.Name.GetRawPtr());
      return;
    }

    size += data.Name.Len();
    size++;
  }

  for (unsigned i = 0;; i++)
  {
    if (i > 256)
    {
      path = "[TOO-LONG]";
      return;
    }
    const wchar_t *servName;
    if (item->RecIndex < kNumSysRecs
        /* && item->RecIndex != kRecIndex_RootDir */)
      servName = kVirtualFolder_System;
    else
    {
      int index2 = item->ParentFolder;
      if (index2 >= 0)
      {
        item = &Items[index2];
        size += Recs[item->RecIndex].FileNames[item->NameIndex].Name.Len() + 1;
        continue;
      }
      if (index2 == -1)
        break;
      servName = (index2 == k_ParentFolderIndex_Lost) ?
          kVirtualFolder_Lost_Normal :
          kVirtualFolder_Lost_Deleted;
    }
    size += MyStringLen(servName) + 1;
    break;
  }

  wchar_t *s = path.AllocBstr(size);
  
  item = &Items[index];

  bool needColon = false;
  if (isAltStream)
  {
    const UString2 &name = rec.DataAttrs[rec.DataRefs[item->DataIndex].Start].Name;
    if (!name.IsEmpty())
    {
      size -= name.Len();
      CopyName(s + size, name.GetRawPtr());
    }
    s[--size] = ':';
    needColon = true;
  }

  {
    const UString2 &name = rec.FileNames[item->NameIndex].Name;
    unsigned len = name.Len();
    if (len != 0)
      CopyName(s + size - len, name.GetRawPtr());
    if (needColon)
      s[size] =  ':';
    size -= len;
  }

  for (;;)
  {
    const wchar_t *servName;
    if (item->RecIndex < kNumSysRecs
        /* && && item->RecIndex != kRecIndex_RootDir */)
      servName = kVirtualFolder_System;
    else
    {
      int index2 = item->ParentFolder;
      if (index2 >= 0)
      {
        item = &Items[index2];
        const UString2 &name = Recs[item->RecIndex].FileNames[item->NameIndex].Name;
        unsigned len = name.Len();
        size--;
        if (len != 0)
        {
          size -= len;
          CopyName(s + size, name.GetRawPtr());
        }
        s[size + len] = WCHAR_PATH_SEPARATOR;
        continue;
      }
      if (index2 == -1)
        break;
      servName = (index2 == k_ParentFolderIndex_Lost) ?
          kVirtualFolder_Lost_Normal :
          kVirtualFolder_Lost_Deleted;
    }
    MyStringCopy(s, servName);
    s[MyStringLen(servName)] = WCHAR_PATH_SEPARATOR;
    break;
  }
}

bool CDatabase::FindSecurityDescritor(UInt32 item, UInt64 &offset, UInt32 &size) const
{
  offset = 0;
  size = 0;
  unsigned left = 0, right = SecurOffsets.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    size_t offs = SecurOffsets[mid];
    UInt32 midValue = Get32(((const Byte *)SecurData) + offs + 4);
    if (item == midValue)
    {
      offset = Get64((const Byte *)SecurData + offs + 8) + 20;
      size = Get32((const Byte *)SecurData + offs + 16) - 20;
      return true;
    }
    if (item < midValue)
      right = mid;
    else
      left = mid + 1;
  }
  return false;
}

/*
static int CompareIDs(const size_t *p1, const size_t *p2, void *data)
{
  UInt32 id1 = Get32(((const Byte *)data) + *p1 + 4);
  UInt32 id2 = Get32(((const Byte *)data) + *p2 + 4);
  return MyCompare(id1, id2);
}
*/

// security data contains duplication copy after each 256 KB.
static const unsigned kSecureDuplicateStepBits = 18;

HRESULT CDatabase::ParseSecuritySDS_2()
{
  const Byte *p = SecurData;
  size_t size = SecurData.Size();
  const size_t kDuplicateStep = (size_t)1 << kSecureDuplicateStepBits;
  const size_t kDuplicateMask = kDuplicateStep - 1;
  size_t lim = MyMin(size, kDuplicateStep);
  UInt32 idPrev = 0;
  for (size_t pos = 0; pos < size && size - pos >= 20;)
  {
    UInt32 id = Get32(p + pos + 4);
    UInt64 offs = Get64(p + pos + 8);
    UInt32 entrySize = Get32(p + pos + 16);
    if (offs == pos && entrySize >= 20 && lim - pos >= entrySize)
    {
      if (id <= idPrev)
        return S_FALSE;
      idPrev = id;
      SecurOffsets.Add(pos);
      pos += entrySize;
      pos = (pos + 0xF) & ~(size_t)0xF;
      if ((pos & kDuplicateMask) != 0)
        continue;
    }
    else
      pos = (pos + kDuplicateStep) & ~kDuplicateMask;
    pos += kDuplicateStep;
    lim = pos + kDuplicateStep;
    if (lim >= size)
      lim = size;
  }
  // we checked that IDs are sorted, so we don't need Sort
  // SecurOffsets.Sort(CompareIDs, (void *)p);
  return S_OK;
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
  
  {
    static const UInt32 kHeaderSize = 512;
    Byte buf[kHeaderSize];
    RINOK(ReadStream_FALSE(InStream, buf, kHeaderSize));
    if (!Header.Parse(buf))
      return S_FALSE;
    
    UInt64 fileSize;
    RINOK(InStream->Seek(0, STREAM_SEEK_END, &fileSize));
    PhySize = Header.GetPhySize_Clusters();
    if (fileSize < PhySize)
      return S_FALSE;
    
    UInt64 phySizeMax = Header.GetPhySize_Max();
    if (fileSize >= phySizeMax)
    {
      RINOK(InStream->Seek(Header.NumSectors << Header.SectorSizeLog, STREAM_SEEK_SET, NULL));
      Byte buf2[kHeaderSize];
      if (ReadStream_FALSE(InStream, buf2, kHeaderSize) == S_OK)
      {
        if (memcmp(buf, buf2, kHeaderSize) == 0)
          PhySize = phySizeMax;
        // else _headerWarning = true;
      }
    }
  }
 
  SeekToCluster(Header.MftCluster);

  CMftRec mftRec;
  UInt32 numSectorsInRec;

  CMyComPtr<IInStream> mftStream;
  {
    UInt32 blockSize = 1 << 12;
    ByteBuf.Alloc(blockSize);
    RINOK(ReadStream_FALSE(InStream, ByteBuf, blockSize));
    
    {
      UInt32 allocSize = Get32(ByteBuf + 0x1C);
      int t = GetLog(allocSize);
      if (t < (int)Header.SectorSizeLog)
        return S_FALSE;
      RecSizeLog = t;
      if (RecSizeLog > 15)
        return S_FALSE;
    }

    numSectorsInRec = 1 << (RecSizeLog - Header.SectorSizeLog);
    if (!mftRec.Parse(ByteBuf, Header.SectorSizeLog, numSectorsInRec, 0, NULL))
      return S_FALSE;
    if (!mftRec.IsFILE())
      return S_FALSE;
    mftRec.ParseDataNames();
    if (mftRec.DataRefs.IsEmpty())
      return S_FALSE;
    RINOK(mftRec.GetStream(InStream, 0, Header.ClusterSizeLog, Header.NumClusters, &mftStream));
    if (!mftStream)
      return S_FALSE;
  }

  // CObjectVector<CAttr> SecurityAttrs;

  UInt64 mftSize = mftRec.DataAttrs[0].Size;
  if ((mftSize >> 4) > Header.GetPhySize_Clusters())
    return S_FALSE;

  const size_t kBufSize = (1 << 15);
  const size_t recSize = ((size_t)1 << RecSizeLog);
  if (kBufSize < recSize)
    return S_FALSE;

  {
    const UInt64 numFiles = mftSize >> RecSizeLog;
    if (numFiles > (1 << 30))
      return S_FALSE;
    if (OpenCallback)
    {
      RINOK(OpenCallback->SetTotal(&numFiles, &mftSize));
    }
    
    ByteBuf.Alloc(kBufSize);
    Recs.ClearAndReserve((unsigned)numFiles);
  }
  
  for (UInt64 pos64 = 0;;)
  {
    if (OpenCallback)
    {
      const UInt64 numFiles = Recs.Size();
      if ((numFiles & 0x3FF) == 0)
      {
        RINOK(OpenCallback->SetCompleted(&numFiles, &pos64));
      }
    }
    size_t readSize = kBufSize;
    {
      const UInt64 rem = mftSize - pos64;
      if (readSize > rem)
        readSize = (size_t)rem;
    }
    if (readSize < recSize)
      break;
    RINOK(ReadStream_FALSE(mftStream, ByteBuf, readSize));
    pos64 += readSize;

    for (size_t i = 0; readSize >= recSize; i += recSize, readSize -= recSize)
    {
      PRF(printf("\n---------------------"));
      PRF(printf("\n%5d:", Recs.Size()));
      
      Byte *p = ByteBuf + i;
      CMftRec rec;

      CObjectVector<CAttr> *attrs = NULL;
      unsigned recIndex = Recs.Size();
      switch (recIndex)
      {
        case kRecIndex_Volume: attrs = &VolAttrs; break;
        // case kRecIndex_Security: attrs = &SecurityAttrs; break;
      }

      if (!rec.Parse(p, Header.SectorSizeLog, numSectorsInRec, (UInt32)Recs.Size(), attrs))
        return S_FALSE;
      Recs.Add(rec);
    }
  }

  /*
  // that code looks too complex. And we can get security info without index parsing
  for (i = 0; i < SecurityAttrs.Size(); i++)
  {
    const CAttr &attr = SecurityAttrs[i];
    if (attr.Name == L"$SII")
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
    if (!rec.BaseMftRef.IsBaseItself())
    {
      UInt64 refIndex = rec.BaseMftRef.GetIndex();
      if (refIndex > (UInt32)Recs.Size())
        return S_FALSE;
      CMftRec &refRec = Recs[(unsigned)refIndex];
      bool moveAttrs = (refRec.SeqNumber == rec.BaseMftRef.GetNumber() && refRec.BaseMftRef.IsBaseItself());
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
  }

  for (i = 0; i < Recs.Size(); i++)
    Recs[i].ParseDataNames();
  
  for (i = 0; i < Recs.Size(); i++)
  {
    CMftRec &rec = Recs[i];
    if (!rec.IsFILE() || !rec.BaseMftRef.IsBaseItself())
      continue;
    if (i < kNumSysRecs && !_showSystemFiles)
      continue;
    if (!rec.InUse() && !_showDeletedFiles)
      continue;

    rec.MyNumNameLinks = rec.FileNames.Size();
    
    // printf("\n%4d: ", i);
    
    /* Actually DataAttrs / DataRefs are sorted by name.
       It can not be more than one unnamed stream in DataRefs
       And indexOfUnnamedStream <= 0.
    */

    int indexOfUnnamedStream = -1;
    if (!rec.IsDir())
    {
      FOR_VECTOR (di, rec.DataRefs)
        if (rec.DataAttrs[rec.DataRefs[di].Start].Name.IsEmpty())
        {
          indexOfUnnamedStream = di;
          break;
        }
    }

    if (rec.FileNames.IsEmpty())
    {
      bool needShow = true;
      if (i < kNumSysRecs)
      {
        needShow = false;
        FOR_VECTOR (di, rec.DataRefs)
          if (rec.GetSize(di) != 0)
          {
            needShow = true;
            break;
          }
      }
      if (needShow)
      {
        CFileNameAttr &fna = rec.FileNames.AddNew();
        // we set incorrect ParentDirRef, that will place item to [LOST] folder
        fna.ParentDirRef.Val = (UInt64)(Int64)-1;
        char s[16 + 16];
        ConvertUInt32ToString(i, MyStpCpy(s, "[NONAME]-"));
        fna.Name.SetFromAscii(s);
        fna.NameType = kFileNameType_Win32Dos;
        fna.Attrib = 0;
      }
    }

    // bool isMainName = true;

    FOR_VECTOR (t, rec.FileNames)
    {
      #ifdef SHOW_DEBUG_INFO
      const CFileNameAttr &fna = rec.FileNames[t];
      #endif
      PRF(printf("\n %4d ", (int)fna.NameType));
      PRF_UTF16(fna.Name);
      // PRF(printf("  | "));

      if (rec.FindWin32Name_for_DosName(t) >= 0)
      {
        rec.MyNumNameLinks--;
        continue;
      }
      
      CItem item;
      item.NameIndex = t;
      item.RecIndex = i;
      item.DataIndex = rec.IsDir() ?
          k_Item_DataIndex_IsDir :
            (indexOfUnnamedStream < 0 ?
          k_Item_DataIndex_IsEmptyFile :
          indexOfUnnamedStream);
      
      if (rec.MyItemIndex < 0)
        rec.MyItemIndex = Items.Size();
      item.ParentHost = Items.Add(item);
      
      /* we can use that code to reduce the number of alt streams:
         it will not show how alt streams for hard links. */
      // if (!isMainName) continue; isMainName = false;

      unsigned numAltStreams = 0;

      FOR_VECTOR (di, rec.DataRefs)
      {
        if (!rec.IsDir() && (int)di == indexOfUnnamedStream)
          continue;

        const UString2 &subName = rec.DataAttrs[rec.DataRefs[di].Start].Name;
        
        PRF(printf("\n alt stream: "));
        PRF_UTF16(subName);

        {
          // $BadClus:$Bad is sparse file for all clusters. So we skip it.
          if (i == kRecIndex_BadClus && subName == L"$Bad")
            continue;
        }

        numAltStreams++;
        ThereAreAltStreams = true;
        item.DataIndex = di;
        Items.Add(item);
      }
    }
  }
  
  if (Recs.Size() > kRecIndex_Security)
  {
    const CMftRec &rec = Recs[kRecIndex_Security];
    FOR_VECTOR (di, rec.DataRefs)
    {
      const CAttr &attr = rec.DataAttrs[rec.DataRefs[di].Start];
      if (attr.Name == L"$SDS")
      {
        CMyComPtr<IInStream> sdsStream;
        RINOK(rec.GetStream(InStream, di, Header.ClusterSizeLog, Header.NumClusters, &sdsStream));
        if (sdsStream)
        {
          UInt64 size64 = attr.GetSize();
          if (size64 < (UInt32)1 << 29)
          {
            size_t size = (size_t)size64;
            if ((((size + 1) >> kSecureDuplicateStepBits) & 1) != 0)
            {
              size -= (1 << kSecureDuplicateStepBits);
              SecurData.Alloc(size);
              if (ReadStream_FALSE(sdsStream, SecurData, size) == S_OK)
              {
                ParseSecuritySDS();
                break;
              }
            }
          }
        }
        break;
      }
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
    UInt64 refIndex = parentDirRef.GetIndex();
    if (refIndex == kRecIndex_RootDir)
      item.ParentFolder = -1;
    else
    {
      int index = FindDirItemForMtfRec(refIndex);
      if (index < 0 ||
          Recs[Items[index].RecIndex].SeqNumber != parentDirRef.GetNumber())
      {
        if (Recs[item.RecIndex].InUse())
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
  
  unsigned virtIndex = Items.Size();
  if (_showSystemFiles)
  {
    _systemFolderIndex = virtIndex++;
    VirtFolderNames.Add(kVirtualFolder_System);
  }
  if (thereAreUnknownFolders_Normal)
  {
    _lostFolderIndex_Normal = virtIndex++;
    VirtFolderNames.Add(kVirtualFolder_Lost_Normal);
  }
  if (thereAreUnknownFolders_Deleted)
  {
    _lostFolderIndex_Deleted = virtIndex++;
    VirtFolderNames.Add(kVirtualFolder_Lost_Deleted);
  }

  return S_OK;
}

class CHandler:
  public IInArchive,
  public IArchiveGetRawProps,
  public IInArchiveGetStream,
  public ISetProperties,
  public CMyUnknownImp,
  CDatabase
{
public:
  MY_UNKNOWN_IMP4(
      IInArchive,
      IArchiveGetRawProps,
      IInArchiveGetStream,
      ISetProperties)
  INTERFACE_IInArchive(;)
  INTERFACE_IArchiveGetRawProps(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);
};

STDMETHODIMP CHandler::GetNumRawProps(UInt32 *numProps)
{
  *numProps = 2;
  return S_OK;
}

STDMETHODIMP CHandler::GetRawPropInfo(UInt32 index, BSTR *name, PROPID *propID)
{
  *name = NULL;
  *propID = index == 0 ? kpidNtReparse : kpidNtSecure;
  return S_OK;
}

STDMETHODIMP CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType)
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
      if (_showSystemFiles)
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

STDMETHODIMP CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType)
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
      if (item.IsAltStream())
        s = &rec.DataAttrs[rec.DataRefs[item.DataIndex].Start].Name;
      else
        s = &rec.FileNames[item.NameIndex].Name;
    }
    if (s->IsEmpty())
      *data = (const wchar_t *)EmptyString;
    else
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
    const CByteBuffer &reparse = rec.ReparseData;

    if (reparse.Size() != 0)
    {
      *dataSize = (UInt32)reparse.Size();
      *propType = NPropDataType::kRaw;
      *data = (const Byte *)reparse;
    }
  }

  if (propID == kpidNtSecure)
  {
    if (index >= Items.Size())
      return S_OK;
    const CItem &item = Items[index];
    const CMftRec &rec = Recs[item.RecIndex];
    if (rec.SiAttr.SecurityId > 0)
    {
      UInt64 offset;
      UInt32 size;
      if (FindSecurityDescritor(rec.SiAttr.SecurityId, offset, size))
      {
        *dataSize = size;
        *propType = NPropDataType::kRaw;
        *data = (const Byte *)SecurData + offset;
      }
    }
  }
  
  return S_OK;
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  *stream = 0;
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
  { "Record Size", kpidRecordSize, VT_UI4},
  { NULL, kpidHeadersSize, VT_UI8},
  { NULL, kpidCTime, VT_FILETIME},
  { NULL, kpidId, VT_UI8},
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

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
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
    {
      FOR_VECTOR (i, VolAttrs)
      {
        const CAttr &attr = VolAttrs[i];
        if (attr.Type == ATTR_TYPE_VOLUME_NAME)
        {
          UString2 name;
          GetString(attr.Data, (unsigned)attr.Data.Size() / 2, name);
          if (!name.IsEmpty())
            prop = name.GetRawPtr();
          break;
        }
      }
      break;
    }
    case kpidFileSystem:
    {
      AString s ("NTFS");
      FOR_VECTOR (i, VolAttrs)
      {
        const CAttr &attr = VolAttrs[i];
        if (attr.Type == ATTR_TYPE_VOLUME_INFO)
        {
          CVolInfo vi;
          if (attr.ParseVolInfo(vi))
          {
            s.Add_Space();
            s.Add_UInt32(vi.MajorVer);
            s += '.';
            s.Add_UInt32(vi.MinorVer);
          }
          break;
        }
      }
      prop = s;
      break;
    }
    case kpidSectorSize: prop = (UInt32)1 << Header.SectorSizeLog; break;
    case kpidRecordSize: prop = (UInt32)1 << RecSizeLog; break;
    case kpidId: prop = Header.SerialNumber; break;

    case kpidIsTree: prop = true; break;
    case kpidIsDeleted: prop = _showDeletedFiles; break;
    case kpidIsAltStream: prop = ThereAreAltStreams; break;
    case kpidIsAux: prop = true; break;
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

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
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
      case kpidIsAux: prop = true; break;
      case kpidIsDeleted:
        if ((int)index == _lostFolderIndex_Deleted)
          prop = true;
        break;
    }
    prop.Detach(value);
    return S_OK;
  }

  const CItem &item = Items[index];
  const CMftRec &rec = Recs[item.RecIndex];

  const CAttr *data= NULL;
  if (item.DataIndex >= 0)
    data = &rec.DataAttrs[rec.DataRefs[item.DataIndex].Start];

  // const CFileNameAttr *fn = &rec.FileNames[item.NameIndex];
  /*
  if (rec.FileNames.Size() > 0)
    fn = &rec.FileNames[0];
  */

  switch (propID)
  {
    case kpidPath:
      GetItemPath(index, prop);
      break;

    /*
    case kpidLink:
      if (!rec.ReparseAttr.SubsName.IsEmpty())
      {
        prop = rec.ReparseAttr.SubsName;
      }
      break;
    case kpidLink2:
      if (!rec.ReparseAttr.PrintName.IsEmpty())
      {
        prop = rec.ReparseAttr.PrintName;
      }
      break;

    case kpidLinkType:
      if (rec.ReparseAttr.Tag != 0)
      {
        prop = (rec.ReparseAttr.Tag & 0xFFFF);
      }
      break;
    */
    
    case kpidINode:
    {
      // const CMftRec &rec = Recs[item.RecIndex];
      // prop = ((UInt64)rec.SeqNumber << 48) | item.RecIndex;
      prop = item.RecIndex;
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
      const UString2 *s;
      if (item.IsAltStream())
        s = &rec.DataAttrs[rec.DataRefs[item.DataIndex].Start].Name;
      else
        s = &rec.FileNames[item.NameIndex].Name;
      if (s->IsEmpty())
        prop = (const wchar_t *)EmptyString;
      else
        prop = s->GetRawPtr();
      break;
    }

    case kpidShortName:
    {
      if (!item.IsAltStream())
      {
        int dosNameIndex = rec.FindDosName(item.NameIndex);
        if (dosNameIndex >= 0)
        {
          const UString2 &s = rec.FileNames[dosNameIndex].Name;
          if (s.IsEmpty())
            prop = (const wchar_t *)EmptyString;
          else
            prop = s.GetRawPtr();
        }
      }
      break;
    }

    case kpidIsDir: prop = item.IsDir(); break;
    case kpidIsAltStream: prop = item.IsAltStream(); break;
    case kpidIsDeleted: prop = !rec.InUse(); break;
    case kpidIsAux: prop = false; break;

    case kpidMTime: NtfsTimeToProp(rec.SiAttr.MTime, prop); break;
    case kpidCTime: NtfsTimeToProp(rec.SiAttr.CTime, prop); break;
    case kpidATime: NtfsTimeToProp(rec.SiAttr.ATime, prop); break;
    // case kpidRecMTime: if (fn) NtfsTimeToProp(rec.SiAttr.ThisRecMTime, prop); break;

    /*
    case kpidMTime2: if (fn) NtfsTimeToProp(fn->MTime, prop); break;
    case kpidCTime2: if (fn) NtfsTimeToProp(fn->CTime, prop); break;
    case kpidATime2: if (fn) NtfsTimeToProp(fn->ATime, prop); break;
    case kpidRecMTime2: if (fn) NtfsTimeToProp(fn->ThisRecMTime, prop); break;
    */
      
    case kpidAttrib:
    {
      UInt32 attrib;
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

      prop = attrib;
      break;
    }
    case kpidLinks: if (rec.MyNumNameLinks != 1) prop = rec.MyNumNameLinks; break;
    
    case kpidNumAltStreams:
    {
      if (!item.IsAltStream())
      {
        unsigned num = rec.DataRefs.Size();
        if (num > 0)
        {
          if (!rec.IsDir() && rec.DataAttrs[rec.DataRefs[0].Start].Name.IsEmpty())
            num--;
          if (num > 0)
            prop = num;
        }
      }
      break;
    }
    
    case kpidSize: if (data) prop = data->GetSize(); else if (!item.IsDir()) prop = (UInt64)0; break;
    case kpidPackSize: if (data) prop = data->GetPackSize(); else if (!item.IsDir()) prop = (UInt64)0; break;
    case kpidNumBlocks: if (data) prop = (UInt32)rec.GetNumExtents(item.DataIndex, Header.ClusterSizeLog, Header.NumClusters); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback)
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

STDMETHODIMP CHandler::Close()
{
  ClearAndClose();
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = Items.Size();
  if (numItems == 0)
    return S_OK;
  UInt32 i;
  UInt64 totalSize = 0;
  for (i = 0; i < numItems; i++)
  {
    UInt32 index = allFilesMode ? i : indices[i];
    if (index >= (UInt32)Items.Size())
      continue;
    const CItem &item = Items[allFilesMode ? i : indices[i]];
    const CMftRec &rec = Recs[item.RecIndex];
    if (item.DataIndex >= 0)
      totalSize += rec.GetSize(item.DataIndex);
  }
  RINOK(extractCallback->SetTotal(totalSize));

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
  UInt32 clusterSize = Header.ClusterSize();
  CByteBuffer buf(clusterSize);

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CDummyOutStream *outStreamSpec = new CDummyOutStream;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur());
    CMyComPtr<ISequentialOutStream> realOutStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    UInt32 index = allFilesMode ? i : indices[i];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    if (index >= (UInt32)Items.Size() || Items[index].IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode));
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }

    const CItem &item = Items[index];

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));

    outStreamSpec->SetStream(realOutStream);
    realOutStream.Release();
    outStreamSpec->Init();

    const CMftRec &rec = Recs[item.RecIndex];

    int res = NExtract::NOperationResult::kDataError;
    {
      CMyComPtr<IInStream> inStream;
      HRESULT hres = rec.GetStream(InStream, item.DataIndex, Header.ClusterSizeLog, Header.NumClusters, &inStream);
      if (hres == S_FALSE)
        res = NExtract::NOperationResult::kUnsupportedMethod;
      else
      {
        RINOK(hres);
        if (inStream)
        {
          hres = copyCoder->Code(inStream, outStream, NULL, NULL, progress);
          if (hres != S_OK &&  hres != S_FALSE)
          {
            RINOK(hres);
          }
          if (/* copyCoderSpec->TotalSize == item.GetSize() && */ hres == S_OK)
            res = NExtract::NOperationResult::kOK;
        }
      }
    }
    if (item.DataIndex >= 0)
    {
      const CAttr &data = rec.DataAttrs[rec.DataRefs[item.DataIndex].Start];
      totalPackSize += data.GetPackSize();
      totalSize += data.GetSize();
    }
    outStreamSpec->ReleaseStream();
    RINOK(extractCallback->SetOperationResult(res));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = Items.Size() + VirtFolderNames.Size();
  return S_OK;
}

STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  InitProps();

  for (UInt32 i = 0; i < numProps; i++)
  {
    const wchar_t *name = names[i];
    const PROPVARIANT &prop = values[i];

    if (StringsAreEqualNoCase_Ascii(name, "ld"))
    {
      RINOK(PROPVARIANT_to_bool(prop, _showDeletedFiles));
    }
    else if (StringsAreEqualNoCase_Ascii(name, "ls"))
    {
      RINOK(PROPVARIANT_to_bool(prop, _showSystemFiles));
    }
    else
      return E_INVALIDARG;
  }
  return S_OK;
}

static const Byte k_Signature[] = { 'N', 'T', 'F', 'S', ' ', ' ', ' ', ' ', 0 };

REGISTER_ARC_I(
  "NTFS", "ntfs img", 0, 0xD9,
  k_Signature,
  3,
  0,
  NULL)

}}
