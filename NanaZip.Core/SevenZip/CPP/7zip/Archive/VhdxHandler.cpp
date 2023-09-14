// VhdxHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define G32(_offs_, dest) dest = Get32(p + (_offs_))
#define G64(_offs_, dest) dest = Get64(p + (_offs_))

using namespace NWindows;


EXTERN_C_BEGIN

// CRC-32C (Castagnoli) : reversed for poly 0x1EDC6F41
#define k_Crc32c_Poly 0x82f63b78

static UInt32 g_Crc32c_Table[256];

static void Z7_FASTCALL Crc32c_GenerateTable()
{
  UInt32 i;
  for (i = 0; i < 256; i++)
  {
    UInt32 r = i;
    unsigned j;
    for (j = 0; j < 8; j++)
      r = (r >> 1) ^ (k_Crc32c_Poly & ((UInt32)0 - (r & 1)));
    g_Crc32c_Table[i] = r;
  }
}


#define CRC32C_INIT_VAL 0xFFFFFFFF

#define CRC_UPDATE_BYTE_2(crc, b) (table[((crc) ^ (b)) & 0xFF] ^ ((crc) >> 8))

// UInt32 Z7_FASTCALL CrcUpdateT1(UInt32 v, const void *data, size_t size, const UInt32 *table);
static UInt32 Z7_FASTCALL CrcUpdateT1_vhdx(UInt32 v, const void *data, size_t size, const UInt32 *table)
{
  const Byte *p = (const Byte *)data;
  const Byte *pEnd = p + size;
  for (; p != pEnd; p++)
    v = CRC_UPDATE_BYTE_2(v, *p);
  return v;
}

static UInt32 Z7_FASTCALL Crc32c_Calc(const void *data, size_t size)
{
  return CrcUpdateT1_vhdx(CRC32C_INIT_VAL, data, size, g_Crc32c_Table) ^ CRC32C_INIT_VAL;
}

EXTERN_C_END


namespace NArchive {
namespace NVhdx {

static struct C_CRC32c_TableInit { C_CRC32c_TableInit() { Crc32c_GenerateTable(); } } g_CRC32c_TableInit;

static const unsigned kSignatureSize = 8;
static const Byte kSignature[kSignatureSize] =
  { 'v', 'h', 'd', 'x', 'f', 'i', 'l', 'e' };

static const unsigned kBitmapSize_Log = 20;
static const size_t kBitmapSize = (size_t)1 << kBitmapSize_Log;


static bool IsZeroArr(const Byte *p, size_t size)
{
  for (size_t i = 0; i < size; i++)
    if (p[i] != 0)
      return false;
  return true;
}


#define ValToHex(t) ((char)(((t) < 10) ? ('0' + (t)) : ('a' + ((t) - 10))))

static void AddByteToHex2(unsigned val, UString &s)
{
  unsigned t;
  t = val >> 4;
  s += ValToHex(t);
  t = val & 0xF;
  s += ValToHex(t);
}


static int HexToVal(const wchar_t c)
{
  if (c >= '0' && c <= '9')  return c - '0';
  if (c >= 'a' && c <= 'z')  return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z')  return c - 'A' + 10;
  return -1;
}

static int DecodeFrom2HexChars(const wchar_t *s)
{
  const int v0 = HexToVal(s[0]);  if (v0 < 0)  return -1;
  const int v1 = HexToVal(s[1]);  if (v1 < 0)  return -1;
  return (int)(((unsigned)v0 << 4) | (unsigned)v1);
}


struct CGuid
{
  Byte Data[16];

  bool IsZero() const { return IsZeroArr(Data, 16); }
  bool IsEqualTo(const Byte *a) const { return memcmp(Data, a, 16) == 0; }
  bool IsEqualTo(const CGuid &g) const { return IsEqualTo(g.Data); }
  void AddHexToString(UString &s) const;

  void SetFrom(const Byte *p) { memcpy(Data, p, 16); }

  bool ParseFromFormatedHexString(const UString &s)
  {
    const unsigned kLen = 16 * 2 + 4 + 2;
    if (s.Len() != kLen || s[0] != '{' || s[kLen - 1] != '}')
      return false;
    unsigned pos = 0;
    for (unsigned i = 1; i < kLen - 1;)
    {
      if (i == 9 || i == 14 || i == 19 || i == 24)
      {
        if (s[i] != '-')
          return false;
        i++;
        continue;
      }
      const int v = DecodeFrom2HexChars(s.Ptr(i));
      if (v < 0)
        return false;
      unsigned pos2 = pos;
      if (pos < 8)
        pos2 ^= (pos < 4 ? 3 : 1);
      Data[pos2] = (Byte)v;
      pos++;
      i += 2;
    }
    return true; // pos == 16;
  }
};

void CGuid::AddHexToString(UString &s) const
{
  for (unsigned i = 0; i < 16; i++)
    AddByteToHex2(Data[i], s);
}


#define IS_NON_ALIGNED(v) (((v) & 0xFFFFF) != 0)

static const unsigned kHeader_GUID_Index_FileWriteGuid = 0;
static const unsigned kHeader_GUID_Index_DataWriteGuid = 1;
static const unsigned kHeader_GUID_Index_LogGuid = 2;

struct CHeader
{
  UInt64 SequenceNumber;
  // UInt16 LogVersion;
  // UInt16 Version;
  UInt32 LogLength;
  UInt64 LogOffset;
  CGuid Guids[3];

  bool IsEqualTo(const CHeader &h) const
  {
    if (SequenceNumber != h.SequenceNumber)
      return false;
    if (LogLength != h.LogLength)
      return false;
    if (LogOffset != h.LogOffset)
      return false;
    for (unsigned i = 0; i < 3; i++)
      if (!Guids[i].IsEqualTo(h.Guids[i]))
        return false;
    return true;
  }

  bool Parse(Byte *p);
};

static const unsigned kHeader2Size = 1 << 12;

bool CHeader::Parse(Byte *p)
{
  if (Get32(p) != 0x64616568) // "head"
    return false;
  const UInt32 crc = Get32(p + 4);
  SetUi32(p + 4, 0)
  if (Crc32c_Calc(p, kHeader2Size) != crc)
    return false;
  G64(8, SequenceNumber);
  for (unsigned i = 0; i < 3; i++)
    Guids[i].SetFrom(p + 0x10 + 0x10 * i);
  // LogVersion = Get16(p + 0x40);
  /* LogVersion MUST be set to zero, for known log format
     but we don't parse log so we ignore it */
  G32(0x44, LogLength);
  G64(0x48, LogOffset);
  if (Get16(p + 0x42) != 1) // Header format Version
    return false;
  if (IS_NON_ALIGNED(LogLength))
    return false;
  if (IS_NON_ALIGNED(LogOffset))
    return false;
  return true;
  // return IsZeroArr(p + 0x50, kHeader2Size - 0x50);
}



static const Byte kBat[16] =
  { 0x66,0x77,0xC2,0x2D,0x23,0xF6,0x00,0x42,0x9D,0x64,0x11,0x5E,0x9B,0xFD,0x4A,0x08 };
static const Byte kMetadataRegion[16] =
  { 0x06,0xA2,0x7C,0x8B,0x90,0x47,0x9A,0x4B,0xB8,0xFE,0x57,0x5F,0x05,0x0F,0x88,0x6E };

struct CRegionEntry
{
  // CGuid Guid;
  UInt64 Offset;
  UInt32 Len;
  UInt32 Required;

  UInt64 GetEndPos() const { return Offset + Len; }
  bool Parse(const Byte *p);
};

bool CRegionEntry::Parse(const Byte *p)
{
  // Guid.SetFrom(p);
  G64(0x10, Offset);
  G32(0x18, Len);
  G32(0x1c, Required);
  if (IS_NON_ALIGNED(Offset))
    return false;
  if (IS_NON_ALIGNED(Len))
    return false;
  if (Offset + Len < Offset)
    return false;
  return true;
}
  

struct CRegion
{
  bool Bat_Defined;
  bool Meta_Defined;
  UInt64 EndPos;
  UInt64 DataSize;

  CRegionEntry BatEntry;
  CRegionEntry MetaEntry;

  bool Parse(Byte *p);
};


static const unsigned kRegionSize = 1 << 16;
static const unsigned kNumRegionEntriesMax = (1 << 11) - 1;

bool CRegion::Parse(Byte *p)
{
  Bat_Defined = false;
  Meta_Defined = false;
  EndPos = 0;
  DataSize = 0;

  if (Get32(p) != 0x69676572) // "regi"
    return false;
  const UInt32 crc = Get32(p + 4);
  SetUi32(p + 4, 0)
  const UInt32 crc_calced = Crc32c_Calc(p, kRegionSize);
  if (crc_calced != crc)
    return false;
  
  const UInt32 EntryCount = Get32(p + 8);
  if (Get32(p + 12) != 0) // reserved field must be set to 0.
    return false;
  if (EntryCount > kNumRegionEntriesMax)
    return false;
  for (UInt32 i = 0; i < EntryCount; i++)
  {
    CRegionEntry e;
    const Byte *p2 = p + 0x10 + 0x20 * (size_t)i;
    if (!e.Parse(p2))
      return false;
    DataSize += e.Len;
    const UInt64 endPos = e.GetEndPos();
    if (EndPos < endPos)
      EndPos = endPos;
    CGuid Guid;
    Guid.SetFrom(p2);
    if (Guid.IsEqualTo(kBat))
    {
      if (Bat_Defined)
        return false;
      BatEntry = e;
      Bat_Defined = true;
    }
    else if (Guid.IsEqualTo(kMetadataRegion))
    {
      if (Meta_Defined)
        return false;
      MetaEntry = e;
      Meta_Defined = true;
    }
    else
    {
      if (e.Required != 0)
        return false;
      // it's allowed to ignore unknown non-required region entries
    }
  }
  /*
  const size_t k = 0x10 + 0x20 * EntryCount;
  return IsZeroArr(p + k, kRegionSize - k);
  */
  return true;
}




struct CMetaEntry
{
  CGuid Guid;
  UInt32 Offset;
  UInt32 Len;
  UInt32 Flags0;
  // UInt32 Flags1;

  bool IsUser()        const { return (Flags0 & 1) != 0; }
  bool IsVirtualDisk() const { return (Flags0 & 2) != 0; }
  bool IsRequired()    const { return (Flags0 & 4) != 0; }

  bool CheckLimit(size_t regionSize) const
  {
    return Offset <= regionSize && Len <= regionSize - Offset;
  }

  bool Parse(const Byte *p);
};


bool CMetaEntry::Parse(const Byte *p)
{
  Guid.SetFrom(p);
  
  G32(0x10, Offset);
  G32(0x14, Len);
  G32(0x18, Flags0);
  UInt32 Flags1;
  G32(0x1C, Flags1);

  if (Offset != 0 && Offset < (1 << 16))
    return false;
  if (Len > (1 << 20))
    return false;
  if (Len == 0 && Offset != 0)
    return false;
  if ((Flags0 >> 3) != 0) // Reserved
    return false;
  if ((Flags1 & 3) != 0) // Reserved2
    return false;
  return true;
}
  

struct CParentPair
{
  UString Key;
  UString Value;
};


struct CMetaHeader
{
  // UInt16 EntryCount;
  bool Guid_Defined;
  bool VirtualDiskSize_Defined;
  bool Locator_Defined;

  unsigned BlockSize_Log;
  unsigned LogicalSectorSize_Log;
  unsigned PhysicalSectorSize_Log;

  UInt32 Flags;
  UInt64 VirtualDiskSize;
  CGuid Guid;
  // CGuid LocatorType;

  CObjectVector<CParentPair> ParentPairs;

  int FindParentKey(const char *name) const
  {
    FOR_VECTOR (i, ParentPairs)
    {
      const CParentPair &pair = ParentPairs[i];
      if (pair.Key.IsEqualTo(name))
        return (int)i;
    }
    return -1;
  }

  bool Is_LeaveBlockAllocated() const { return (Flags & 1) != 0; }
  bool Is_HasParent() const { return (Flags & 2) != 0; }

  void Clear()
  {
    Guid_Defined = false;
    VirtualDiskSize_Defined = false;
    Locator_Defined = false;
    BlockSize_Log = 0;
    LogicalSectorSize_Log = 0;
    PhysicalSectorSize_Log = 0;
    Flags = 0;
    VirtualDiskSize = 0;
    ParentPairs.Clear();
  }

  bool Parse(const Byte *p, size_t size);
};


static unsigned GetLogSize(UInt32 size)
{
  unsigned k;
  for (k = 0; k < 32; k++)
    if (((UInt32)1 << k) == size)
      return k;
  return k;
}

 
static const unsigned kMetadataSize = 8;
static const Byte kMetadata[kMetadataSize] =
  { 'm','e','t','a','d','a','t','a' };

static const unsigned k_Num_MetaEntries_Max = (1 << 11) - 1;

static const Byte kFileParameters[16] =
  { 0x37,0x67,0xa1,0xca,0x36,0xfa,0x43,0x4d,0xb3,0xb6,0x33,0xf0,0xaa,0x44,0xe7,0x6b };
static const Byte kVirtualDiskSize[16] =
  { 0x24,0x42,0xa5,0x2f,0x1b,0xcd,0x76,0x48,0xb2,0x11,0x5d,0xbe,0xd8,0x3b,0xf4,0xb8 };
static const Byte kVirtualDiskID[16] =
  { 0xab,0x12,0xca,0xbe,0xe6,0xb2,0x23,0x45,0x93,0xef,0xc3,0x09,0xe0,0x00,0xc7,0x46 };
static const Byte kLogicalSectorSize[16] =
  { 0x1d,0xbf,0x41,0x81,0x6f,0xa9,0x09,0x47,0xba,0x47,0xf2,0x33,0xa8,0xfa,0xab,0x5f };
static const Byte kPhysicalSectorSize[16] =
  { 0xc7,0x48,0xa3,0xcd,0x5d,0x44,0x71,0x44,0x9c,0xc9,0xe9,0x88,0x52,0x51,0xc5,0x56 };
static const Byte kParentLocator[16] =
  { 0x2d,0x5f,0xd3,0xa8,0x0b,0xb3,0x4d,0x45,0xab,0xf7,0xd3,0xd8,0x48,0x34,0xab,0x0c };

static bool GetString16(UString &s, const Byte *p, size_t size)
{
  s.Empty();
  if (size & 1)
    return false;
  for (size_t i = 0; i < size; i += 2)
  {
    const wchar_t c = Get16(p + i);
    if (c == 0)
      return false;
    s += c;
  }
  return true;
}


bool CMetaHeader::Parse(const Byte *p, size_t size)
{
  if (memcmp(p, kMetadata, kMetadataSize) != 0)
    return false;
  if (Get16(p + 8) != 0) // Reserved
    return false;
  const UInt32 EntryCount = Get16(p + 10);
  if (EntryCount > k_Num_MetaEntries_Max)
    return false;
  if (!IsZeroArr(p + 12, 20)) // Reserved
    return false;

  for (unsigned i = 0; i < EntryCount; i++)
  {
    CMetaEntry e;
    if (!e.Parse(p + 32 + 32 * (size_t)i))
      return false;
    if (!e.CheckLimit(size))
      return false;
    const Byte *p2 = p + e.Offset;
    
    if (e.Guid.IsEqualTo(kFileParameters))
    {
      if (BlockSize_Log != 0)
        return false;
      if (e.Len != 8)
        return false;
      const UInt32 v = Get32(p2);
      Flags = Get32(p2 + 4);
      BlockSize_Log = GetLogSize(v);
      if (BlockSize_Log < 20 || BlockSize_Log > 28) // specification from 1 MB to 256 MB
        return false;
      if ((Flags >> 2) != 0) // reserved
        return false;
    }
    else if (e.Guid.IsEqualTo(kVirtualDiskSize))
    {
      if (VirtualDiskSize_Defined)
        return false;
      if (e.Len != 8)
        return false;
      VirtualDiskSize = Get64(p2);
      VirtualDiskSize_Defined = true;
    }
    else if (e.Guid.IsEqualTo(kVirtualDiskID))
    {
      if (e.Len != 16)
        return false;
      Guid.SetFrom(p2);
      Guid_Defined = true;
    }
    else if (e.Guid.IsEqualTo(kLogicalSectorSize))
    {
      if (LogicalSectorSize_Log != 0)
        return false;
      if (e.Len != 4)
        return false;
      const UInt32 v = Get32(p2);
      LogicalSectorSize_Log = GetLogSize(v);
      if (LogicalSectorSize_Log != 9 && LogicalSectorSize_Log != 12)
        return false;
    }
    else if (e.Guid.IsEqualTo(kPhysicalSectorSize))
    {
      if (PhysicalSectorSize_Log != 0)
        return false;
      if (e.Len != 4)
        return false;
      const UInt32 v = Get32(p2);
      PhysicalSectorSize_Log = GetLogSize(v);
      if (PhysicalSectorSize_Log != 9 && PhysicalSectorSize_Log != 12)
        return false;
    }
    else if (e.Guid.IsEqualTo(kParentLocator))
    {
      if (Locator_Defined)
        return false;
      if (e.Len < 20)
        return false;
      // LocatorType.SetFrom(p2);
      /* Specifies the type of the parent virtual disk.
         is different for each type: VHDX, VHD or iSCSI.
         only "B04AEFB7-D19E-4A81-B789-25B8E9445913" (for VHDX) is supported now
      */
      Locator_Defined = true;
      if (Get16(p2 + 16) != 0) // reserved
        return false;
      const UInt32 KeyValueCount = Get16(p2 + 18);
      if (20 + (UInt32)KeyValueCount * 12 > e.Len)
        return false;
      for (unsigned k = 0; k < KeyValueCount; k++)
      {
        const Byte *p3 = p2 + 20 + (size_t)k * 12;
        const UInt32 KeyOffset   = Get32(p3);
        const UInt32 ValueOffset = Get32(p3 + 4);
        const UInt32 KeyLength   = Get16(p3 + 8);
        const UInt32 ValueLength = Get16(p3 + 10);
        if (KeyOffset > e.Len || KeyLength > e.Len - KeyOffset)
          return false;
        if (ValueOffset > e.Len || ValueLength > e.Len - ValueOffset)
          return false;
        CParentPair pair;
        if (!GetString16(pair.Key, p2 + KeyOffset, KeyLength))
          return false;
        if (!GetString16(pair.Value, p2 + ValueOffset, ValueLength))
          return false;
        ParentPairs.Add(pair);
      }
    }
    else
    {
      if (e.IsRequired())
        return false;
      // return false; // unknown metadata;
    }
  }

  // some properties are required for correct processing

  if (BlockSize_Log == 0)
    return false;
  if (LogicalSectorSize_Log == 0)
    return false;
  if (!VirtualDiskSize_Defined)
    return false;
  if (((UInt32)VirtualDiskSize & ((UInt32)1 << LogicalSectorSize_Log)) != 0)
    return false;

  // vhdx specification sets limit for 64 TB.
  // do we need to check over same limit ?
  const UInt64 kVirtualDiskSize_Max = (UInt64)1 << 46;
  if (VirtualDiskSize > kVirtualDiskSize_Max)
    return false;

  return true;
}



struct CBat
{
  CByteBuffer Data;

  void Clear() { Data.Free(); }
  UInt64 GetItem(size_t n) const
  {
    return Get64(Data + n * 8);
  }
};



Z7_class_CHandler_final: public CHandlerImg
{
  UInt64 _phySize;

  CBat Bat;
  CObjectVector<CByteBuffer> BitMaps;

  unsigned ChunkRatio_Log;
  size_t ChunkRatio;
  size_t TotalBatEntries;

  CMetaHeader Meta;
  CHeader Header;

  UInt32 NumUsedBlocks;
  UInt32 NumUsedBitMaps;
  UInt64 HeadersSize;

  UInt32 NumLevels;
  UInt64 PackSize_Total;

  /*
  UInt64 NumUsed_1MB_Blocks; // data and bitmaps
  bool NumUsed_1MB_Blocks_Defined;
  */

  CMyComPtr<IInStream> ParentStream;
  CHandler *Parent;
  UString _errorMessage;
  UString _creator;

  bool _nonEmptyLog;
  bool _isDataContiguous;
  // bool _batOverlap;

  CGuid _parentGuid;
  bool _parentGuid_IsDefined;
  UStringVector ParentNames;
  UString ParentName_Used;

  const CHandler *_child;
  unsigned _level;
  bool _isCyclic;
  bool _isCyclic_or_CyclicParent;

  void AddErrorMessage(const char *message);
  void AddErrorMessage(const char *message, const wchar_t *name);

  void UpdatePhySize(UInt64 value)
  {
    if (_phySize < value)
      _phySize = value;
  }

  HRESULT Seek2(UInt64 offset);
  HRESULT Read_FALSE(Byte *data, size_t size)
  {
    return ReadStream_FALSE(Stream, data, size);
  }
  HRESULT ReadToBuf_FALSE(CByteBuffer &buf, size_t size)
  {
    buf.Alloc(size);
    return ReadStream_FALSE(Stream, buf, size);
  }
  
  void InitSeekPositions();
  HRESULT ReadPhy(UInt64 offset, void *data, UInt32 size, UInt32 &processed);

  bool IsDiff() const
  {
    // here we suppose that only HasParent() flag is mandatory for Diff archive type
    return Meta.Is_HasParent();
    // return _parentGuid_IsDefined;
  }

  void AddTypeString(AString &s) const
  {
    if (IsDiff())
      s += "Differencing";
    else
    {
      if (Meta.Is_LeaveBlockAllocated())
        s +=  _isDataContiguous ? "fixed" : "fixed-non-cont";
      else
        s += "dynamic";
    }
  }

  void AddComment(UString &s) const;

  UInt64 GetPackSize() const
  {
    return (UInt64)NumUsedBlocks << Meta.BlockSize_Log;
  }

  UString GetParentSequence() const
  {
    const CHandler *p = this;
    UString res;
    while (p && p->IsDiff())
    {
      if (!res.IsEmpty())
        res += " -> ";
      res += ParentName_Used;
      p = p->Parent;
    }
    return res;
  }

  bool AreParentsOK() const
  {
    if (_isCyclic_or_CyclicParent)
      return false;
    const CHandler *p = this;
    while (p->IsDiff())
    {
      p = p->Parent;
      if (!p)
        return false;
    }
    return true;
  }

  // bool ParseLog(CByteBuffer &log);
  bool ParseBat();
  bool CheckBat();

  HRESULT Open3();
  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openArchiveCallback) Z7_override;
  HRESULT OpenParent(IArchiveOpenCallback *openArchiveCallback, bool &_parentFileWasOpen);
  virtual void CloseAtError() Z7_override;

public:
  Z7_IFACE_COM7_IMP(IInArchive_Img)

  Z7_IFACE_COM7_IMP(IInArchiveGetStream)
  Z7_IFACE_COM7_IMP(ISequentialInStream)

  CHandler():
    _child(NULL),
    _level(0),
    _isCyclic(false),
    _isCyclic_or_CyclicParent(false)
    {}
};


HRESULT CHandler::Seek2(UInt64 offset)
{
  return InStream_SeekSet(Stream, offset);
}


void CHandler::InitSeekPositions()
{
  /* (_virtPos) and (_posInArc) is used only in Read() (that calls ReadPhy()).
     So we must reset these variables before first call of Read() */
  Reset_VirtPos();
  Reset_PosInArc();
  if (ParentStream)
    Parent->InitSeekPositions();
}


HRESULT CHandler::ReadPhy(UInt64 offset, void *data, UInt32 size, UInt32 &processed)
{
  processed = 0;
  if (offset > _phySize
      || offset + size > _phySize)
  {
    // we don't expect these cases, if (_phySize) was set correctly.
    return S_FALSE;
  }
  if (offset != _posInArc)
  {
    const HRESULT res = Seek2(offset);
    if (res != S_OK)
    {
      Reset_PosInArc(); // we don't trust seek_pos in case of error
      return res;
    }
    _posInArc = offset;
  }
  {
    size_t size2 = size;
    const HRESULT res = ReadStream(Stream, data, &size2);
    processed = (UInt32)size2;
    _posInArc += size2;
    if (res != S_OK)
      Reset_PosInArc(); // we don't trust seek_pos in case of reading error
    return res;
  }
}


#define PAYLOAD_BLOCK_NOT_PRESENT   0
#define PAYLOAD_BLOCK_UNDEFINED     1
#define PAYLOAD_BLOCK_ZERO          2
#define PAYLOAD_BLOCK_UNMAPPED      3
#define PAYLOAD_BLOCK_FULLY_PRESENT 6
#define PAYLOAD_BLOCK_PARTIALLY_PRESENT 7

#define SB_BLOCK_NOT_PRESENT 0
#define SB_BLOCK_PRESENT     6

#define BAT_GET_OFFSET(v) ((v) & ~(UInt64)0xFFFFF)
#define BAT_GET_STATE(v)  ((UInt32)(v) & 7)

/* The log contains only   updates to metadata, bat and region tables
   The log doesn't contain updates to start header, and 2 headers (first 192 KB of file).
   The log is array of 4 KB blocks and each block has 4-byte signature.
   So it's possible to scan whole log to find the latest entry sequence (and header for replay).
*/

/*
struct CLogEntry
{
  UInt32 EntryLength;
  UInt32 Tail;
  UInt64 SequenceNumber;
  CGuid LogGuid;
  UInt32 DescriptorCount;
  UInt64 FlushedFileOffset;
  UInt64 LastFileOffset;
  
  bool Parse(const Byte *p);
};

bool CLogEntry::Parse(const Byte *p)
{
  G32 (8, EntryLength);
  G32 (12,Tail);
  G64 (16, SequenceNumber);
  G32 (24, DescriptorCount); // it's 32-bit, but specification says 64-bit
  if (Get32(p + 28) != 0) // reserved
    return false;
  LogGuid.SetFrom(p + 32);
  G64 (48, FlushedFileOffset);
  G64 (56, LastFileOffset);

  if (SequenceNumber == 0)
    return false;
  if ((Tail & 0xfff) != 0)
    return false;
  if (IS_NON_ALIGNED(FlushedFileOffset))
    return false;
  if (IS_NON_ALIGNED(LastFileOffset))
    return false;
  return true;
}


bool CHandler::ParseLog(CByteBuffer &log)
{
  CLogEntry lastEntry;
  lastEntry.SequenceNumber = 0;
  bool lastEntry_found = false;
  size_t lastEntry_Offset = 0;
  for (size_t i = 0; i < log.Size(); i += 1 << 12)
  {
    Byte *p = (Byte *)(log + i);

    if (Get32(p) != 0x65676F6C) // "loge"
      continue;
    const UInt32 crc = Get32(p + 4);

    CLogEntry e;
    if (!e.Parse(p))
    {
      return false;
      continue;
    }
    const UInt32 entryLength = Get32(p + 8);
    if (e.EntryLength > log.Size() || (e.EntryLength & 0xFFF) != 0 || e.EntryLength == 0)
    {
      return false;
      continue;
    }
    SetUi32(p + 4, 0);
    const UInt32 crc_calced = Crc32c_Calc(p, entryLength);
    SetUi32(p + 4, crc); // we must restore crc if we want same data in log
    if (crc_calced != crc)
      continue;
    if (!lastEntry_found || lastEntry.SequenceNumber < e.SequenceNumber)
    {
      lastEntry = e;
      lastEntry_found = true;
      lastEntry_Offset = i;
    }
  }

  return true;
}
*/


bool CHandler::ParseBat()
{
  ChunkRatio_Log = kBitmapSize_Log + 3 + Meta.LogicalSectorSize_Log - Meta.BlockSize_Log;
  ChunkRatio = (size_t)1 << (ChunkRatio_Log);
  
  UInt64 totalBatEntries64;
  const bool isDiff = IsDiff();
  const UInt32 blockSize = (UInt32)1 << Meta.BlockSize_Log;
  {
    const UInt64 up = Meta.VirtualDiskSize + blockSize - 1;
    if (up < Meta.VirtualDiskSize)
      return false;
    const UInt64 numDataBlocks = up >> Meta.BlockSize_Log;
    
    if (isDiff)
    {
      // differencing table must be finished with bitmap entry
      const UInt64 numBitmaps = (numDataBlocks + ChunkRatio - 1) >> ChunkRatio_Log;
      totalBatEntries64 = numBitmaps * (ChunkRatio + 1);
    }
    else
    {
      // we don't need last Bitmap entry
      totalBatEntries64 = numDataBlocks + ((numDataBlocks - 1) >> ChunkRatio_Log);
    }
  }
  
  if (totalBatEntries64 > Bat.Data.Size() / 8)
    return false;

  const size_t totalBatEntries = (size_t)totalBatEntries64;
  TotalBatEntries = totalBatEntries;
  
  bool isCont = (!isDiff && Meta.Is_LeaveBlockAllocated());
  UInt64 prevBlockOffset = 0;
  UInt64 maxBlockOffset = 0;

  size_t remEntries = ChunkRatio + 1;
  
  size_t i;
  for (i = 0; i < totalBatEntries; i++)
  {
    const UInt64 v = Bat.GetItem(i);
    if ((v & 0xFFFF8) != 0)
      return false;
    const UInt64 offset = BAT_GET_OFFSET(v);
    const unsigned state = BAT_GET_STATE(v);
    
    /*
    UInt64 index64 = v >> 20;
    printf("\n%7d", i);
    printf("%10d, ", (unsigned)index64);
    printf("%4x, ", (unsigned)state);
    */
    
    remEntries--;
    if (remEntries == 0)
    {
      // printf(" ========");
      // printf("\n");
      remEntries = ChunkRatio + 1;
      if (state == SB_BLOCK_PRESENT)
      {
        isCont = false;
        if (!isDiff)
          return false;
        if (offset == 0)
          return false;
        const UInt64 lim = offset + kBitmapSize;
        if (lim < offset)
          return false;
        if (_phySize < lim)
          _phySize = lim;
        NumUsedBitMaps++;
      }
      else if (state != SB_BLOCK_NOT_PRESENT)
        return false;
    }
    else
    {
      if (state == PAYLOAD_BLOCK_FULLY_PRESENT
          || state == PAYLOAD_BLOCK_PARTIALLY_PRESENT)
      {
        if (offset == 0)
          return false;
        if (maxBlockOffset < offset)
          maxBlockOffset = offset;

        if (state == PAYLOAD_BLOCK_PARTIALLY_PRESENT)
        {
          isCont = false;
          if (!isDiff)
            return false;
        }
        else if (isCont)
        {
          if (prevBlockOffset != 0 && prevBlockOffset + blockSize != offset)
            isCont = false;
          else
            prevBlockOffset = offset;
        }

        NumUsedBlocks++;
      }
      else if (state == PAYLOAD_BLOCK_UNMAPPED)
      {
        isCont = false;
        // non-empty (offset) is allowed
      }
      else if (state == PAYLOAD_BLOCK_NOT_PRESENT
          || state == PAYLOAD_BLOCK_UNDEFINED
          || state == PAYLOAD_BLOCK_ZERO)
      {
        isCont = false;
        /* (offset) is reserved and (offset == 0) is expected here,
           but we ignore (offset) here */
        // if (offset != 0) return false;
      }
      else
        return false;
    }
  }

  _isDataContiguous = isCont;

  if (maxBlockOffset != 0)
  {
    const UInt64 lim = maxBlockOffset + blockSize;
    if (lim < maxBlockOffset)
      return false;
    if (_phySize < lim)
      _phySize = lim;
    const UInt64 kPhyLimit = (UInt64)1 << 62;
    if (maxBlockOffset >= kPhyLimit)
      return false;
  }
  return true;
}
    

bool CHandler::CheckBat()
{
  const UInt64 upSize = _phySize + kBitmapSize * 8 - 1;
  if (upSize < _phySize)
    return false;
  const UInt64 useMapSize64 = upSize >> (kBitmapSize_Log + 3);
  const size_t useMapSize = (size_t)useMapSize64;
  
  const UInt32 blockSizeMB = (UInt32)1 << (Meta.BlockSize_Log - kBitmapSize_Log);

  // we don't check useMap, if it's too big.
  if (useMapSize != useMapSize64)
    return true;
  if (useMapSize == 0 || useMapSize > ((size_t)1 << 28))
    return true;

  CByteArr useMap;
  useMap.Alloc(useMapSize);
  memset(useMap, 0, useMapSize);
  // useMap[0] = (Byte)(1 << 0); // first 1 MB is used by headers
  // we can also update useMap for log, and region data.
  
  const size_t totalBatEntries = TotalBatEntries;
  size_t remEntries = ChunkRatio + 1;
  
  size_t i;
  for (i = 0; i < totalBatEntries; i++)
  {
    const UInt64 v = Bat.GetItem(i);
    const UInt64 offset = BAT_GET_OFFSET(v);
    const unsigned state = BAT_GET_STATE(v);
    const UInt64 index = offset >> kBitmapSize_Log;
    UInt32 numBlocks = 1;
    remEntries--;
    if (remEntries == 0)
    {
      remEntries = ChunkRatio + 1;
      if (state != SB_BLOCK_PRESENT)
        continue;
    }
    else
    {
      if (state != PAYLOAD_BLOCK_FULLY_PRESENT &&
          state != PAYLOAD_BLOCK_PARTIALLY_PRESENT)
        continue;
      numBlocks = blockSizeMB;
    }
    
    for (unsigned k = 0; k < numBlocks; k++)
    {
      const UInt64 index2 = index + k;
      const unsigned flag = (unsigned)1 << ((unsigned)index2 & 7);
      const size_t byteIndex = (size_t)(index2 >> 3);
      if (byteIndex >= useMapSize)
        return false;
      const unsigned m = useMap[byteIndex];
      if (m & flag)
        return false;
      useMap[byteIndex] = (Byte)(m | flag);
    }
  }

  /*
  UInt64 num = 0;
  for (i = 0; i < useMapSize; i++)
  {
    Byte b = useMap[i];
    unsigned t = 0;
    t += (b & 1);  b >>= 1;
    t += (b & 1);  b >>= 1;
    t += (b & 1);  b >>= 1;
    t += (b & 1);  b >>= 1;
    t += (b & 1);  b >>= 1;
    t += (b & 1);  b >>= 1;
    t += (b & 1);  b >>= 1;
    t += (b & 1);
    num += t;
  }
  NumUsed_1MB_Blocks = num;
  NumUsed_1MB_Blocks_Defined = true;
  */
  
  return true;
}



HRESULT CHandler::Open3()
{
  {
    const unsigned kHeaderSize = 512; // + 8
    Byte header[kHeaderSize];
    
    RINOK(Read_FALSE(header, kHeaderSize))

    if (memcmp(header, kSignature, kSignatureSize) != 0)
      return S_FALSE;

    const Byte *p = &header[0];
    for (unsigned i = kSignatureSize; i < kHeaderSize; i += 2)
    {
      const wchar_t c = Get16(p + i);
      if (c < 0x20 || c > 0x7F)
        break;
      _creator += c;
    }
  }

  HeadersSize = (UInt32)1 << 20;
  CHeader headers[2];
  {
    Byte header[kHeader2Size];
    for (unsigned i = 0; i < 2; i++)
    {
      RINOK(Seek2((1 << 16) * (1 + i)))
      RINOK(Read_FALSE(header, kHeader2Size))
      bool headerIsOK = headers[i].Parse(header);
      if (!headerIsOK)
        return S_FALSE;
    }
  }
  unsigned mainIndex;
       if (headers[0].SequenceNumber > headers[1].SequenceNumber) mainIndex = 0;
  else if (headers[0].SequenceNumber < headers[1].SequenceNumber) mainIndex = 1;
  else
  {
    /* Disk2vhd v2.02 can create image with 2 full copies of headers.
       It's violation of VHDX specification:
          "A header is current if it is the only valid header
           or if it is valid and its SequenceNumber field is
           greater than the other header's SequenceNumber".
       but we support such Disk2vhd archives. */
    if (!headers[0].IsEqualTo(headers[1]))
      return S_FALSE;
    mainIndex = 0;
  }

  const CHeader &h = headers[mainIndex];
  Header = h;
  if (h.LogLength != 0)
  {
    HeadersSize += h.LogLength;
    UpdatePhySize(h.LogOffset + h.LogLength);
    if (!h.Guids[kHeader_GUID_Index_LogGuid].IsZero())
    {
      _nonEmptyLog = true;
      AddErrorMessage("non-empty LOG was not replayed");
      /*
      if (h.LogVersion != 0)
        AddErrorMessage("unknown LogVresion");
      else
      {
        CByteBuffer log;
        RINOK(Seek2(h.LogOffset));
        RINOK(ReadToBuf_FALSE(log, h.LogLength));
        if (!ParseLog(log))
        {
          return S_FALSE;
        }
      }
      */
    }
  }
  CRegion regions[2];
  int correctRegionIndex = -1;

  {
    CByteBuffer temp;
    temp.Alloc(kRegionSize * 2);
    RINOK(Seek2((1 << 16) * 3))
    RINOK(Read_FALSE(temp, kRegionSize * 2))
    unsigned numTables = 1;
    if (memcmp(temp, temp + kRegionSize, kRegionSize) != 0)
    {
      AddErrorMessage("Region tables mismatch");
      numTables = 2;
    }

    for (unsigned i = 0; i < numTables; i++)
    {
      // RINOK(Seek2((1 << 16) * (3 + i)));
      // RINOK(Read_FALSE(temp, kRegionSize));
      if (regions[i].Parse(temp))
      {
        if (correctRegionIndex < 0)
          correctRegionIndex = (int)i;
      }
      else
      {
        AddErrorMessage("Incorrect region table");
      }
    }
    if (correctRegionIndex < 0)
      return S_FALSE;
    /*
    if (!regions[0].IsEqualTo(regions[1]))
      return S_FALSE;
    */
  }
  
  // UpdatePhySize((1 << 16) * 5);
  UpdatePhySize(1 << 20);
  
  {
    const CRegion &region = regions[correctRegionIndex];
    HeadersSize += region.DataSize;
    UpdatePhySize(region.EndPos);
    {
      if (!region.Meta_Defined)
        return S_FALSE;
      const CRegionEntry &e = region.MetaEntry;
      if (e.Len == 0)
        return S_FALSE;
      {
        // static const kMetaTableSize = 1 << 16;
        CByteBuffer temp;
        {
          RINOK(Seek2(e.Offset))
          RINOK(ReadToBuf_FALSE(temp, e.Len))
        }
        if (!Meta.Parse(temp, temp.Size()))
          return S_FALSE;
      }
      // UpdatePhySize(e.GetEndPos());
    }
    {
      if (!region.Bat_Defined)
        return S_FALSE;
      const CRegionEntry &e = region.BatEntry;
      if (e.Len == 0)
        return S_FALSE;
      // UpdatePhySize(e.GetEndPos());
      {
        RINOK(Seek2(e.Offset))
        RINOK(ReadToBuf_FALSE(Bat.Data, e.Len))
      }
      if (!ParseBat())
        return S_FALSE;
      if (!CheckBat())
      {
        AddErrorMessage("BAT overlap");
        // _batOverlap = true;
        // return S_FALSE;
      }
    }
  }

  {
    // do we need to check "parent_linkage2" also?
    FOR_VECTOR (i, Meta.ParentPairs)
    {
      const CParentPair &pair = Meta.ParentPairs[i];
      if (pair.Key.IsEqualTo("parent_linkage"))
      {
        _parentGuid_IsDefined = _parentGuid.ParseFromFormatedHexString(pair.Value);
        break;
      }
    }
  }

  {
    // absolute paths for parent stream can be rejected later in client callback
    // the order of check by specification:
    const char * const g_ParentKeys[] =
    {
        "relative_path"       // "..\..\path2\sub3\parent.vhdx"
      , "volume_path"         // "\\?\Volume{26A21BDA-A627-11D7-9931-806E6F6E6963}\path2\sub3\parent.vhdx")
      , "absolute_win32_path" // "d:\path2\sub3\parent.vhdx"
    };
    for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_ParentKeys); i++)
    {
      const int index = Meta.FindParentKey(g_ParentKeys[i]);
      if (index < 0)
        continue;
      ParentNames.Add(Meta.ParentPairs[index].Value);
    }
  }

  if (Meta.Is_HasParent())
  {
    if (!Meta.Locator_Defined)
      AddErrorMessage("Parent locator is not defined");
    else
    {
      if (!_parentGuid_IsDefined)
        AddErrorMessage("Parent GUID is not defined");
      if (ParentNames.IsEmpty())
        AddErrorMessage("Parent VHDX file name is not defined");
    }
  }
  else
  {
    if (Meta.Locator_Defined)
      AddErrorMessage("Unexpected parent locator");
  }

  // here we suppose that and locator can be used only with HasParent flag
    
  // return S_FALSE;

  _size = Meta.VirtualDiskSize; // CHandlerImg

  // _posInArc = 0;
  // Reset_PosInArc();
  // RINOK(InStream_SeekToBegin(Stream))

  return S_OK;
}


/*
static UInt32 g_NumCalls = 0;
static UInt32 g_NumCalls2 = 0;
static struct CCounter { ~CCounter()
{
  printf("\nNumCalls = %10u\n", g_NumCalls);
  printf("NumCalls2 = %10u\n", g_NumCalls2);
} } g_Counter;
*/

Z7_COM7F_IMF(CHandler::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  // g_NumCalls++;
  if (processedSize)
    *processedSize = 0;
  if (_virtPos >= Meta.VirtualDiskSize)
    return S_OK;
  {
    const UInt64 rem = Meta.VirtualDiskSize - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }
  if (size == 0)
    return S_OK;
  const size_t blockIndex = (size_t)(_virtPos >> Meta.BlockSize_Log);
  const size_t chunkIndex = blockIndex >> ChunkRatio_Log;
  const size_t chunkRatio = (size_t)1 << ChunkRatio_Log;
  const size_t blockIndex2 = chunkIndex * (chunkRatio + 1) + (blockIndex & (chunkRatio - 1));
  const UInt64 blockSectVal = Bat.GetItem(blockIndex2);
  const UInt64 blockOffset = BAT_GET_OFFSET(blockSectVal);
  const UInt32 blockState = BAT_GET_STATE(blockSectVal);

  const UInt32 blockSize = (UInt32)1 << Meta.BlockSize_Log;
  const UInt32 offsetInBlock = (UInt32)_virtPos & (blockSize - 1);
  size = MyMin(blockSize - offsetInBlock, size);

  bool needParent = false;
  bool needRead = false;
  
  if (blockState == PAYLOAD_BLOCK_FULLY_PRESENT)
    needRead = true;
  else if (blockState == PAYLOAD_BLOCK_NOT_PRESENT)
  {
    /* for a differencing VHDX: parent virtual disk SHOULD be
         inspected to determine the associated contents (SPECIFICATION).
         we suppose that we should not check BitMap.
       for fixed or dynamic VHDX files: the block contents are undefined and
         can contain arbitrary data (SPECIFICATION). NTFS::pagefile.sys can use such state. */
    if (IsDiff())
      needParent = true;
  }
  else if (blockState == PAYLOAD_BLOCK_PARTIALLY_PRESENT)
  {
    // only allowed for differencing VHDX files.
    // associated sector bitmap block MUST be valid
    if (chunkIndex >= BitMaps.Size())
      return S_FALSE;
    // else
    {
      const CByteBuffer &bitmap = BitMaps[(unsigned)chunkIndex];
      const Byte *p = (const Byte *)bitmap;
      if (!p)
        return S_FALSE;
      // else
      {
        // g_NumCalls2++;
        const UInt64 sectorIndex = _virtPos >> Meta.LogicalSectorSize_Log;

        #define BIT_MAP_UNIT_LOG  3 // it's for small block (4 KB)
        // #define BIT_MAP_UNIT_LOG  5 // speed optimization for large blocks (16 KB)

        const size_t offs = (size_t)(sectorIndex >> 3) &
            (
              (kBitmapSize - 1)
              & ~(((UInt32)1 << (BIT_MAP_UNIT_LOG - 3)) - 1)
            );

        unsigned sector2 = (unsigned)sectorIndex & ((1 << BIT_MAP_UNIT_LOG) - 1);
      #if BIT_MAP_UNIT_LOG == 5
        UInt32 v = GetUi32(p + offs) >> sector2;
      #else
        unsigned v = (unsigned)p[offs] >> sector2;
      #endif
        // UInt32 v = GetUi32(p + offs) >> sector2;
        const UInt32 sectorSize = (UInt32)1 << Meta.LogicalSectorSize_Log;
        const UInt32 offsetInSector = (UInt32)_virtPos & (sectorSize - 1);
        const unsigned bit = (unsigned)(v & 1);
        if (bit)
          needRead = true;
        else
          needParent = true; // zero - from the parent VHDX file
        UInt32 rem = sectorSize - offsetInSector;
        for (sector2++; sector2 < (1 << BIT_MAP_UNIT_LOG); sector2++)
        {
          v >>= 1;
          if (bit != (v & 1))
            break;
          rem += sectorSize;
        }
        if (size > rem)
          size = rem;
      }
    }
  }

  bool needZero = true;

  HRESULT res = S_OK;

  if (needParent)
  {
    if (!ParentStream)
      return S_FALSE;
    // if (ParentStream)
    {
      RINOK(InStream_SeekSet(ParentStream, _virtPos))
      size_t processed = size;
      res = ReadStream(ParentStream, (Byte *)data, &processed);
      size = (UInt32)processed;
      needZero = false;
    }
  }
  else if (needRead)
  {
    UInt32 processed = 0;
    res = ReadPhy(blockOffset + offsetInBlock, data, size, processed);
    size = processed;
    needZero = false;
  }

  if (needZero)
    memset(data, 0, size);

  if (processedSize)
    *processedSize = size;

  _virtPos += size;
  return res;
}


enum
{
  kpidParent = kpidUserDefined
};

static const CStatProp kArcProps[] =
{
  { NULL, kpidClusterSize, VT_UI4},
  { NULL, kpidSectorSize, VT_UI4},
  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidNumVolumes, VT_UI4},
  { NULL, kpidTotalPhySize, VT_UI8},
  { "Parent", kpidParent, VT_BSTR},
  { NULL, kpidCreatorApp, VT_BSTR},
  { NULL, kpidComment, VT_BSTR},
  { NULL, kpidId, VT_BSTR}
 };

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME


void CHandler::AddErrorMessage(const char *message)
{
  if (!_errorMessage.IsEmpty())
    _errorMessage.Add_LF();
  _errorMessage += message;
}

void CHandler::AddErrorMessage(const char *message, const wchar_t *name)
{
  AddErrorMessage(message);
  _errorMessage += name;
}


static void AddComment_Name(UString &s, const char *name)
{
  s += name;
  s += ": ";
}

static void AddComment_Bool(UString &s, const char *name, bool val)
{
  AddComment_Name(s, name);
  s += val ? "+" : "-";
  s.Add_LF();
}

static void AddComment_UInt64(UString &s, const char *name, UInt64 v, bool showMB = false)
{
  AddComment_Name(s, name);
  s.Add_UInt64(v);
  if (showMB)
  {
    s += " (";
    s.Add_UInt64(v >> 20);
    s += " MiB)";
  }
  s.Add_LF();
}

static void AddComment_BlockSize(UString &s, const char *name, unsigned logSize)
{
  if (logSize != 0)
    AddComment_UInt64(s, name, ((UInt64)1 << logSize));
}


void CHandler::AddComment(UString &s) const
{
  AddComment_UInt64(s, "VirtualDiskSize", Meta.VirtualDiskSize);
  AddComment_UInt64(s, "PhysicalSize", _phySize);

  if (!_errorMessage.IsEmpty())
  {
    AddComment_Name(s, "Error");
    s += _errorMessage;
    s.Add_LF();
  }

  if (Meta.Guid_Defined)
  {
    AddComment_Name(s, "Id");
    Meta.Guid.AddHexToString(s);
    s.Add_LF();
  }
  
  AddComment_UInt64(s, "SequenceNumber", Header.SequenceNumber);
  AddComment_UInt64(s, "LogLength", Header.LogLength, true);
  
  for (unsigned i = 0; i < 3; i++)
  {
    const CGuid &g = Header.Guids[i];
    if (g.IsZero())
      continue;
    if (i == 0)
      s += "FileWrite";
    else if (i == 1)
      s += "DataWrite";
    else
      s += "Log";
    AddComment_Name(s, "Guid");
    g.AddHexToString(s);
    s.Add_LF();
  }

  AddComment_Bool(s, "HasParent", Meta.Is_HasParent());
  AddComment_Bool(s, "Fixed", Meta.Is_LeaveBlockAllocated());
  if (Meta.Is_LeaveBlockAllocated())
    AddComment_Bool(s, "DataContiguous", _isDataContiguous);
  
  AddComment_BlockSize(s, "BlockSize", Meta.BlockSize_Log);
  AddComment_BlockSize(s, "LogicalSectorSize", Meta.LogicalSectorSize_Log);
  AddComment_BlockSize(s, "PhysicalSectorSize", Meta.PhysicalSectorSize_Log);

  {
    const UInt64 packSize = GetPackSize();
    AddComment_UInt64(s, "PackSize", packSize, true);
    const UInt64 headersSize = HeadersSize + ((UInt64)NumUsedBitMaps << kBitmapSize_Log);
    AddComment_UInt64(s, "HeadersSize", headersSize, true);
    AddComment_UInt64(s, "FreeSpace", _phySize - packSize - headersSize, true);
    /*
    if (NumUsed_1MB_Blocks_Defined)
      AddComment_UInt64(s, "used2", (NumUsed_1MB_Blocks << 20));
    */
  }

  if (Meta.ParentPairs.Size() != 0)
  {
    s += "Parent:";
    s.Add_LF();
    FOR_VECTOR(i, Meta.ParentPairs)
    {
      const CParentPair &pair = Meta.ParentPairs[i];
      s += "  ";
      s += pair.Key;
      s += ": ";
      s += pair.Value;
      s.Add_LF();
    }
    s.Add_LF();
  }
}



Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidClusterSize: prop = (UInt32)1 << Meta.BlockSize_Log; break;
    case kpidSectorSize: prop = (UInt32)1 << Meta.LogicalSectorSize_Log; break;
    case kpidShortComment:
    case kpidMethod:
    {
      AString s;
      AddTypeString(s);
      if (IsDiff())
      {
        s += " -> ";
        const CHandler *p = this;
        while (p && p->IsDiff())
          p = p->Parent;
        if (!p)
          s += '?';
        else
          p->AddTypeString(s);
      }
      prop = s;
      break;
    }
    case kpidComment:
    {
      UString s;
      {
        if (NumLevels > 1)
        {
          AddComment_UInt64(s, "NumVolumeLevels", NumLevels);
          AddComment_UInt64(s, "PackSizeTotal", PackSize_Total, true);
          s += "----";
          s.Add_LF();
        }

        const CHandler *p = this;
        for (;;)
        {
          if (p->_level != 0 || p->Parent)
            AddComment_UInt64(s, "VolumeLevel", p->_level + 1);
          p->AddComment(s);
          if (!p->Parent)
            break;
          s += "----";
          s.Add_LF();
          {
            s.Add_LF();
            if (!p->ParentName_Used.IsEmpty())
            {
              AddComment_Name(s, "Name");
              s += p->ParentName_Used;
              s.Add_LF();
            }
          }
          p = p->Parent;
        }
      }
      prop = s;
      break;
    }
    case kpidCreatorApp:
    {
      if (!_creator.IsEmpty())
        prop = _creator;
      break;
    }
    case kpidId:
    {
      if (Meta.Guid_Defined)
      {
        UString s;
        Meta.Guid.AddHexToString(s);
        prop = s;
      }
      break;
    }
    case kpidName:
    {
      if (Meta.Guid_Defined)
      {
        UString s;
        Meta.Guid.AddHexToString(s);
        s += ".vhdx";
        prop = s;
      }
      break;
    }
    case kpidParent: if (IsDiff()) prop = GetParentSequence(); break;
    case kpidPhySize: prop = _phySize; break;
    case kpidTotalPhySize:
    {
      const CHandler *p = this;
      UInt64 sum = 0;
      do
      {
        sum += p->_phySize;
        p = p->Parent;
      }
      while (p);
      prop = sum;
      break;
    }
    case kpidNumVolumes: if (NumLevels != 1) prop = (UInt32)NumLevels; break;
    case kpidError:
    {
      UString s;
      const CHandler *p = this;
      do
      {
        if (!p->_errorMessage.IsEmpty())
        {
          if (!s.IsEmpty())
            s.Add_LF();
          s += p->_errorMessage;
        }
        p = p->Parent;
      }
      while (p);
      if (!s.IsEmpty())
        prop = s;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *openArchiveCallback)
{
  Stream = stream;
  if (_level >= (1 << 20))
    return S_FALSE;

  RINOK(Open3())

  NumLevels = 1;
  PackSize_Total = GetPackSize();

  if (_child)
  {
    if (!_child->_parentGuid.IsEqualTo(Header.Guids[kHeader_GUID_Index_DataWriteGuid]))
      return S_FALSE;
    const CHandler *child = _child;
    do
    {
      /* We suppose that only FileWriteGuid is unique.
         Another IDs must be identical in in difference and parent archives. */
      if (Header.Guids[kHeader_GUID_Index_FileWriteGuid].IsEqualTo(
              child->Header.Guids[kHeader_GUID_Index_FileWriteGuid])
          && _phySize == child->_phySize)
      {
        _isCyclic = true;
        _isCyclic_or_CyclicParent = true;
        AddErrorMessage("Cyclic parent archive was blocked");
        return S_OK;
      }
      child = child->_child;
    }
    while (child);
  }

  if (!Meta.Is_HasParent())
    return S_OK;

  if (!Meta.Locator_Defined
      || !_parentGuid_IsDefined
      || ParentNames.IsEmpty())
  {
    return S_OK;
  }

  ParentName_Used = ParentNames.Front();

  HRESULT res;
  const unsigned kNumLevelsMax = (1 << 8);  // Maybe we need to increase that limit
  if (_level >= kNumLevelsMax - 1)
  {
    AddErrorMessage("Too many parent levels");
    return S_OK;
  }
  
  bool _parentFileWasOpen = false;
  
  if (!openArchiveCallback)
    res = S_FALSE;
  else
    res = OpenParent(openArchiveCallback, _parentFileWasOpen);
  
  if (res != S_OK)
  {
    if (res != S_FALSE)
      return res;
    
    if (_parentFileWasOpen)
      AddErrorMessage("Can't parse parent VHDX file : ", ParentName_Used);
    else
      AddErrorMessage("Missing parent VHDX file : ", ParentName_Used);
  }


  return S_OK;
}


HRESULT CHandler::OpenParent(IArchiveOpenCallback *openArchiveCallback, bool &_parentFileWasOpen)
{
  _parentFileWasOpen = false;
  Z7_DECL_CMyComPtr_QI_FROM(
      IArchiveOpenVolumeCallback,
      openVolumeCallback, openArchiveCallback)
  if (!openVolumeCallback)
    return S_FALSE;
  {
    CMyComPtr<IInStream> nextStream;
    HRESULT res = S_FALSE;
    UString name;
    
    FOR_VECTOR (i, ParentNames)
    {
      name = ParentNames[i];
      
      // we remove prefix ".\\', but client already can support any variant
      if (name[0] == L'.' && name[1] == L'\\')
        name.DeleteFrontal(2);

      res = openVolumeCallback->GetStream(name, &nextStream);

      if (res == S_OK && nextStream)
        break;
         
      if (res != S_OK && res != S_FALSE)
        return res;
    }
    
    if (res == S_FALSE || !nextStream)
      return S_FALSE;
    
    ParentName_Used = name;
    _parentFileWasOpen = true;

    Parent = new CHandler;
    ParentStream = Parent;

    try
    {
      Parent->_level = _level + 1;
      Parent->_child = this;
      /* we could call CHandlerImg::Open() here.
         but we don't need (_imgExt) in (Parent). So we call Open2() here */
      Parent->Close();
      res = Parent->Open2(nextStream, openArchiveCallback);
    }
    catch(...)
    {
      Parent = NULL;
      ParentStream.Release();
      res = S_FALSE;
      throw;
    }

    if (res != S_OK)
    {
      Parent = NULL;
      ParentStream.Release();
      if (res == E_ABORT)
        return res;
      if (res != S_FALSE)
      {
        // we must show that error code
      }
    }

    if (res == S_OK)
    {
      if (Parent->_isCyclic_or_CyclicParent)
        _isCyclic_or_CyclicParent = true;

      NumLevels = Parent->NumLevels + 1;
      PackSize_Total += Parent->GetPackSize();

      // we read BitMaps only if Parent was open

      UInt64 numBytes = (UInt64)NumUsedBitMaps << kBitmapSize_Log;
      if (openArchiveCallback && numBytes != 0)
      {
        RINOK(openArchiveCallback->SetTotal(NULL, &numBytes))
      }
      numBytes = 0;
      for (size_t i = ChunkRatio; i < TotalBatEntries; i += ChunkRatio + 1)
      {
        const UInt64 v = Bat.GetItem(i);
        const UInt64 offset = BAT_GET_OFFSET(v);
        const unsigned state = BAT_GET_STATE(v);

        CByteBuffer &buf = BitMaps.AddNew();
        if (state == SB_BLOCK_PRESENT)
        {
          if (openArchiveCallback)
          {
            RINOK(openArchiveCallback->SetCompleted(NULL, &numBytes))
          }
          numBytes += kBitmapSize;
          buf.Alloc(kBitmapSize);
          RINOK(Seek2(offset))
          RINOK(Read_FALSE(buf, kBitmapSize))
          /*
          for (unsigned i = 0; i < (1 << 20); i+=4)
          {
            UInt32 v = GetUi32(buf + i);
            if (v != 0 && v != (UInt32)(Int32)-1)
              printf("\n%7d %8x", i, v);
          }
          */
        }
      }
    }
  }

  return S_OK;
}


void CHandler::CloseAtError()
{
  // CHandlerImg
  Clear_HandlerImg_Vars();
  Stream.Release();

  _phySize = 0;
  Bat.Clear();
  BitMaps.Clear();
  NumUsedBlocks = 0;
  NumUsedBitMaps = 0;
  HeadersSize = 0;
  /*
  NumUsed_1MB_Blocks = 0;
  NumUsed_1MB_Blocks_Defined = false;
  */

  Parent = NULL;
  ParentStream.Release();
  _errorMessage.Empty();
  _creator.Empty();
  _nonEmptyLog = false;
  _parentGuid_IsDefined = false;
  _isDataContiguous = false;
  // _batOverlap = false;

  ParentNames.Clear();
  ParentName_Used.Empty();

  Meta.Clear();

  ChunkRatio_Log = 0;
  ChunkRatio = 0;
  TotalBatEntries = 0;
  NumLevels = 0;
  PackSize_Total = 0;

  _isCyclic = false;
  _isCyclic_or_CyclicParent = false;
}

Z7_COM7F_IMF(CHandler::Close())
{
  CloseAtError();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  switch (propID)
  {
    case kpidSize: prop = Meta.VirtualDiskSize; break;
    case kpidPackSize: prop = PackSize_Total; break;
    case kpidExtension: prop = (_imgExt ? _imgExt : "img"); break;
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 /* index */, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  // if some prarent is not OK, we don't create stream
  if (!AreParentsOK())
    return S_FALSE;
  InitSeekPositions();
  CMyComPtr<ISequentialInStream> streamTemp = this;
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I(
  "VHDX", "vhdx avhdx", NULL, 0xc4,
  kSignature,
  0,
  0,
  NULL)

}}
