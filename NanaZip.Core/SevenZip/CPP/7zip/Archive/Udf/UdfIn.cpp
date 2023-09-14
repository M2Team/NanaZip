// Archive/UdfIn.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#endif

#include "../../../../C/CpuArch.h"

#include "../../../Windows/PropVariantUtils.h"

#include "../../Common/RegisterArc.h"
#include "../../Common/StreamUtils.h"

#include "UdfIn.h"

#ifdef SHOW_DEBUG_INFO
#define PRF(x) x
#else
#define PRF(x)
#endif

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define G16(_offs_, dest) dest = Get16(p + (_offs_))
#define G32(_offs_, dest) dest = Get32(p + (_offs_))
#define G64(_offs_, dest) dest = Get64(p + (_offs_))

namespace NArchive {
namespace NUdf {

static const unsigned kNumPartitionsMax = 64;
static const unsigned kNumLogVolumesMax = 64;
static const unsigned kNumRecursionLevelsMax = 1 << 10;
static const unsigned kNumItemsMax = 1 << 27;
static const unsigned kNumFilesMax = 1 << 28;
static const unsigned kNumRefsMax = 1 << 28;
static const UInt32 kNumExtentsMax = (UInt32)1 << 30;
static const UInt64 kFileNameLengthTotalMax = (UInt64)1 << 33;
static const UInt64 kInlineExtentsSizeMax = (UInt64)1 << 33;

#define CRC16_INIT_VAL 0
#define CRC16_UPDATE_BYTE(crc, b) ((UInt16)(g_Crc16Table[(((crc) >> 8) ^ (b)) & 0xFF] ^ ((crc) << 8)))

#define kCrc16Poly 0x1021
static UInt16 g_Crc16Table[256];

static void Z7_FASTCALL Crc16GenerateTable(void)
{
  UInt32 i;
  for (i = 0; i < 256; i++)
  {
    UInt32 r = (i << 8);
    for (unsigned j = 0; j < 8; j++)
      r = ((r << 1) ^ (kCrc16Poly & ((UInt32)0 - (r >> 15)))) & 0xFFFF;
    g_Crc16Table[i] = (UInt16)r;
  }
}

static UInt32 Z7_FASTCALL Crc16Calc(const void *data, size_t size)
{
  UInt32 v = CRC16_INIT_VAL;
  const Byte *p = (const Byte *)data;
  const Byte *pEnd = p + size;
  for (; p != pEnd; p++)
    v = CRC16_UPDATE_BYTE(v, *p);
  return v;
}

static struct CCrc16TableInit { CCrc16TableInit() { Crc16GenerateTable(); } } g_Crc16TableInit;


// ---------- ECMA Part 1 ----------

void CDString::Parse(const Byte *p, unsigned size)
{
  Data.CopyFrom(p, size);
}

static UString ParseDString(const Byte *data, unsigned size)
{
  UString res;
  if (size != 0)
  {
    wchar_t *p;
    const Byte type = *data++;
    size--;
    if (type == 8)
    {
      p = res.GetBuf(size);
      for (unsigned i = 0; i < size; i++)
      {
        const wchar_t c = data[i];
        if (c == 0)
          break;
        *p++ = c;
      }
    }
    else if (type == 16)
    {
      size &= ~(unsigned)1;
      p = res.GetBuf(size / 2);
      for (unsigned i = 0; i < size; i += 2)
      {
        const wchar_t c = GetBe16(data + i);
        if (c == 0)
          break;
        *p++ = c;
      }
    }
    else
      return UString("[unknown]");
    *p = 0;
    res.ReleaseBuf_SetLen((unsigned)(p - (const wchar_t *)res));
  }
  return res;
}

UString CDString32::GetString() const
{
  const unsigned size = Data[sizeof(Data) - 1];
  return ParseDString(Data, MyMin(size, (unsigned)(sizeof(Data) - 1)));
}

UString CDString128::GetString() const
{
  const unsigned size = Data[sizeof(Data) - 1];
  return ParseDString(Data, MyMin(size, (unsigned)(sizeof(Data) - 1)));
}

UString CDString::GetString() const { return ParseDString(Data, (unsigned)Data.Size()); }

void CTime::Parse(const Byte *p) { memcpy(Data, p, sizeof(Data)); }


static void AddCommentChars(UString &dest, const char *s, size_t size)
{
  for (size_t i = 0; i < size; i++)
  {
    char c = s[i];
    if (c == 0)
      break;
    if (c < 0x20)
      c = '_';
    dest += (wchar_t)c;
  }
}


void CRegId::Parse(const Byte *p)
{
  Flags = p[0];
  memcpy(Id, p + 1, sizeof(Id));
  memcpy(Suffix, p + 24, sizeof(Suffix));
}

void CRegId::AddCommentTo(UString &s) const
{
  AddCommentChars(s, Id, sizeof(Id));
}

void CRegId::AddUdfVersionTo(UString &s) const
{
  // use it only for "Domain Identifier Suffix" and "UDF Identifier Suffix"
  // UDF 2.1.5.3
  // Revision in hex (3 digits)
  const Byte minor = Suffix[0];
  const Byte major = Suffix[1];
  if (major != 0 || minor != 0)
  {
    char temp[16];
    ConvertUInt32ToHex(major, temp);
    s += temp;
    s.Add_Dot();
    ConvertUInt32ToHex8Digits(minor, temp);
    s += &temp[8 - 2];
  }
}


// ---------- ECMA Part 3: Volume Structure ----------

void CExtent::Parse(const Byte *p)
{
  /* Len shall be less than < 2^30.
     Unless otherwise specified, the length shall be an integral multiple of the logical sector size.
     If (Len == 0), no extent is specified and (Pos) shall contain 0 */
  G32 (0, Len);
  G32 (4, Pos);
}


// ECMA 3/7.2

struct CTag
{
  UInt16 Id;
  // UInt16 Version;
  // Byte Checksum;
  // UInt16 SerialNumber;
  // UInt16 Crc;
  UInt16 CrcLen;
  // UInt32 TagLocation; // the number of the logical sector
  
  HRESULT Parse(const Byte *p, size_t size);
};

HRESULT CTag::Parse(const Byte *p, size_t size)
{
  if (size < 16)
    return S_FALSE;
  {
    unsigned sum = 0;
    for (unsigned i = 0; i < 16; i++)
      sum = sum + p[i];
    if ((Byte)(sum - p[4]) != p[4] || p[5] != 0)
      return S_FALSE;
  }
  Id = Get16(p);
  const UInt16 Version = Get16(p + 2);
  if (Version != 2 && Version != 3)
    return S_FALSE;
  // SerialNumber = Get16(p + 6);
  const UInt32 crc = Get16(p + 8);
  CrcLen = Get16(p + 10);
  // TagLocation = Get32(p + 12);

  if (size >= 16 + (size_t)CrcLen)
    if (crc == Crc16Calc(p + 16, (size_t)CrcLen))
      return S_OK;
  return S_FALSE;
}

// ECMA 3/7.2.1

enum EDescriptorType
{
  DESC_TYPE_SpoaringTable       = 0, // UDF
  DESC_TYPE_PrimVol             = 1,
  DESC_TYPE_AnchorVolPtr        = 2,
  DESC_TYPE_VolPtr              = 3,
  DESC_TYPE_ImplUseVol          = 4,
  DESC_TYPE_Partition           = 5,
  DESC_TYPE_LogicalVol          = 6,
  DESC_TYPE_UnallocSpace        = 7,
  DESC_TYPE_Terminating         = 8,
  DESC_TYPE_LogicalVolIntegrity = 9,
  DESC_TYPE_FileSet             = 256,
  DESC_TYPE_FileId              = 257,
  DESC_TYPE_AllocationExtent    = 258,
  DESC_TYPE_Indirect            = 259,
  DESC_TYPE_Terminal            = 260,
  DESC_TYPE_File                = 261,
  DESC_TYPE_ExtendedAttrHeader  = 262,
  DESC_TYPE_UnallocatedSpaceEntry = 263,
  DESC_TYPE_SpaceBitmap         = 264,
  DESC_TYPE_PartitionIntegrity  = 265,
  DESC_TYPE_ExtendedFile        = 266
};


void CLogBlockAddr::Parse(const Byte *p)
{
  G32 (0, Pos);
  G16 (4, PartitionRef);
}

void CShortAllocDesc::Parse(const Byte *p)
{
  G32 (0, Len);
  G32 (4, Pos);
}

/*
void CADImpUse::Parse(const Byte *p)
{
  G16 (0, Flags);
  G32 (2, UdfUniqueId);
}
*/

void CLongAllocDesc::Parse(const Byte *p)
{
  G32 (0, Len);
  Location.Parse(p + 4);
  // memcpy(ImplUse, p + 10, sizeof(ImplUse));
  // adImpUse.Parse(ImplUse);
}


void CPrimeVol::Parse(const Byte *p)
{
  // G32 (16, VolumeDescriptorSequenceNumber);
  G32 (20, PrimaryVolumeDescriptorNumber);
  VolumeId.Parse(p + 24);
  G16 (56, VolumeSequenceNumber);
  G16 (58, MaximumVolumeSequenceNumber);
  // G16 (60, InterchangeLevel);
  // G16 (62, MaximumInterchangeLevel);
  // G32 (64, CharacterSetList)
  // G32 (68, MaximumCharacterSetList)
  VolumeSetId.Parse(p + 72);
  // 200 64 Descriptor Character Set charspec (1/7.2.1)
  // 264 64 Explanatory Character Set charspec (1/7.2.1)
  // VolumeAbstract.Parse(p + 328);
  // VolumeCopyrightNotice.Parse(p + 336);
  ApplicationId.Parse(p + 344);
  RecordingTime.Parse(p + 376);
  ImplId.Parse(p + 388);
  // 420 64 Implementation Use bytes
  // G32 (484, PredecessorVolumeDescriptorSequenceLocation);
  // G16 (488, Flags);
}



bool CInArchive::CheckExtent(unsigned volIndex, unsigned partitionRef, UInt32 blockPos, UInt32 len) const
{
  const CLogVol &vol = LogVols[volIndex];
  if (partitionRef >= vol.PartitionMaps.Size())
    return false;
  const CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];
  return ((UInt64)blockPos * vol.BlockSize + len) <= ((UInt64)partition.Len << SecLogSize);
}

bool CInArchive::CheckItemExtents(unsigned volIndex, const CItem &item) const
{
  FOR_VECTOR (i, item.Extents)
  {
    const CMyExtent &e = item.Extents[i];
    if (!CheckExtent(volIndex, e.PartitionRef, e.Pos, e.GetLen()))
      return false;
  }
  return true;
}

HRESULT CInArchive::Read(unsigned volIndex, unsigned partitionRef, UInt32 blockPos, UInt32 len, Byte *buf)
{
  if (!CheckExtent(volIndex, partitionRef, blockPos, len))
    return S_FALSE;
  const CLogVol &vol = LogVols[volIndex];
  const CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];
  UInt64 offset = ((UInt64)partition.Pos << SecLogSize) + (UInt64)blockPos * vol.BlockSize;
  RINOK(InStream_SeekSet(_stream, offset))
  offset += len;
  UpdatePhySize(offset);
  const HRESULT res = ReadStream_FALSE(_stream, buf, len);
  if (res == S_FALSE && offset > FileSize)
    UnexpectedEnd = true;
  return res;
}

HRESULT CInArchive::ReadLad(unsigned volIndex, const CLongAllocDesc &lad, Byte *buf)
{
  return Read(volIndex, lad.Location.PartitionRef, lad.Location.Pos, lad.GetLen(), (Byte *)buf);
}

HRESULT CInArchive::ReadFromFile(unsigned volIndex, const CItem &item, CByteBuffer &buf)
{
  if (item.Size >= (UInt32)1 << 30)
    return S_FALSE;
  if (item.IsInline)
  {
    buf = item.InlineData;
    return S_OK;
  }
  buf.Alloc((size_t)item.Size);
  size_t pos = 0;
  FOR_VECTOR (i, item.Extents)
  {
    const CMyExtent &e = item.Extents[i];
    const UInt32 len = e.GetLen();
    RINOK(Read(volIndex, e.PartitionRef, e.Pos, len, (Byte *)buf + pos))
    pos += len;
  }
  return S_OK;
}


void CIcbTag::Parse(const Byte *p)
{
  // G32 (0, PriorDirectNum);
  // G16 (4, StrategyType);
  // G16 (6, StrategyParam);
  // G16 (8, MaxNumOfEntries);
  FileType = p[11];
  // ParentIcb.Parse(p + 12);
  G16 (18, Flags);
}


// ECMA 4/14.9 File Entry
// UDF FileEntry 2.3.6

// ECMA 4/14.17 Extended File Entry

void CItem::Parse(const Byte *p)
{
  // (-1) can be stored in Uid/Gid.
  // G32 (36, Uid);
  // G32 (40, Gid);
  // G32 (44, Permissions);
  G16 (48, FileLinkCount);
  // RecordFormat = p[50];
  // RecordDisplayAttr = p[51];
  // G32 (52, RecordLen);
  G64 (56, Size);
  if (IsExtended)
  {
    // The sum of all Information Length fields for all streams of a file (including the default stream). If this file has no
    // streams, the Object Size shall be equal to the Information Length.
    // G64 (64, ObjectSize);
    p += 8;
  }
  G64 (64, NumLogBlockRecorded);
  ATime.Parse(p + 72);
  MTime.Parse(p + 84);
  if (IsExtended)
  {
    CreateTime.Parse(p + 96);
    p += 12;
  }
  AttribTime.Parse(p + 96);
  // G32 (108, CheckPoint);
  /*
  if (IsExtended)
  {
    // Get32(p + 112); // reserved
    p += 4;
  }
  // ExtendedAttrIcb.Parse(p + 112);
  if (IsExtended)
  {
    StreamDirectoryIcb.Parse(p + 128);
    p += 16;
  }
  */
  
  // ImplId.Parse(p + 128);
  // G64 (160, UniqueId);
}


// ECMA 4/14.4
// UDF 2.3.4

/*
File Characteristics:
Deleted bit:
  ECMA: If set to ONE, shall mean this File Identifier Descriptor
        identifies a file that has been deleted;
  UDF:  If the space for the file or directory is deallocated,
        the implementation shall set the ICB field to zero.
    ECMA 167 4/8.6 requires that the File Identifiers of all FIDs in a directory shall be unique.
    The implementations shall follow these rules when a Deleted bit is set:
    rewrire the compression ID of the File Identifier: 8 -> 254, 16 -> 255.
*/

struct CFileId
{
  // UInt16 FileVersion;
  Byte FileCharacteristics;
  // CByteBuffer ImplUse;
  CDString Id;
  CLongAllocDesc Icb;

  bool IsItLink_Dir    () const { return (FileCharacteristics & FILEID_CHARACS_Dir)     != 0; }
  bool IsItLink_Deleted() const { return (FileCharacteristics & FILEID_CHARACS_Deleted) != 0; }
  bool IsItLink_Parent () const { return (FileCharacteristics & FILEID_CHARACS_Parent)  != 0; }

  size_t Parse(const Byte *p, size_t size);
};

size_t CFileId::Parse(const Byte *p, size_t size)
{
  size_t processed = 0;
  if (size < 38)
    return 0;
  CTag tag;
  if (tag.Parse(p, size) != S_OK)
    return 0;
  if (tag.Id != DESC_TYPE_FileId)
    return 0;
  // FileVersion = Get16(p + 16);
  // UDF: There shall be only one version of a file as specified below with the value being set to 1.

  FileCharacteristics = p[18];
  const unsigned idLen = p[19];
  Icb.Parse(p + 20);
  const unsigned impLen = Get16(p + 36);
  if (size < 38 + idLen + impLen)
    return 0;
  processed = 38;
  // ImplUse.CopyFrom(p + processed, impLen);
  processed += impLen;
  Id.Parse(p + processed, idLen);
  processed += idLen;
  for (;(processed & 3) != 0; processed++)
    if (p[processed] != 0)
      return 0;
  if ((size_t)tag.CrcLen + 16 != processed) return 0;
  return (processed <= size) ? processed : 0;
}



HRESULT CInArchive::ReadFileItem(unsigned volIndex, unsigned fsIndex, const CLongAllocDesc &lad, bool isDir, int numRecurseAllowed)
{
  if (Files.Size() % 100 == 0)
    RINOK(_progress->SetCompleted(Files.Size(), _processedProgressBytes))
  if (numRecurseAllowed-- == 0)
    return S_FALSE;
  CFile &file = Files.Back();
  const CLogVol &vol = LogVols[volIndex];
  const unsigned partitionRef = lad.Location.PartitionRef;
  if (partitionRef >= vol.PartitionMaps.Size())
    return S_FALSE;
  CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];

  const UInt32 key = lad.Location.Pos;
  UInt32 value;
  const UInt32 kRecursedErrorValue = (UInt32)(Int32)-1;
  if (partition.Map.Find(key, value))
  {
    if (value == kRecursedErrorValue)
      return S_FALSE;
    file.ItemIndex = (int)(Int32)value;
  }
  else
  {
    value = Items.Size();
    file.ItemIndex = (int)(Int32)value;
    if (partition.Map.Set(key, kRecursedErrorValue))
      return S_FALSE;
    RINOK(ReadItem(volIndex, (int)fsIndex, lad, isDir, numRecurseAllowed))
    if (!partition.Map.Set(key, value))
      return S_FALSE;
  }
  return S_OK;
}


// (fsIndex = -1) means that it's metadata file

HRESULT CInArchive::ReadItem(unsigned volIndex, int fsIndex, const CLongAllocDesc &lad, bool isDir, int numRecurseAllowed)
{
  if (Items.Size() >= kNumItemsMax)
    return S_FALSE;
  CItem &item = Items.AddNew();
  
  const CLogVol &vol = LogVols[volIndex];
 
  const size_t size = lad.GetLen();
  if (size != vol.BlockSize)
    return S_FALSE;

  CByteBuffer buf(size);
  RINOK(ReadLad(volIndex, lad, buf))

  CTag tag;
  const Byte *p = buf;
  RINOK(tag.Parse(p, size))

  item.IsExtended = (tag.Id == DESC_TYPE_ExtendedFile);
  const size_t kExtendOffset = item.IsExtended ? 40 : 0;

  if (size < kExtendOffset + 176)
    return S_FALSE;
  if (tag.Id != DESC_TYPE_File &&
      tag.Id != DESC_TYPE_ExtendedFile)
    return S_FALSE;

  item.IcbTag.Parse(p + 16);

  if (fsIndex < 0)
  {
    if (item.IcbTag.FileType != ICB_FILE_TYPE_METADATA &&
        item.IcbTag.FileType != ICB_FILE_TYPE_METADATA_MIRROR)
      return S_FALSE;
  }
  else if (
      item.IcbTag.FileType != ICB_FILE_TYPE_DIR &&
      item.IcbTag.FileType != ICB_FILE_TYPE_FILE)
    return S_FALSE;

  item.Parse(p);

  _processedProgressBytes += (UInt64)item.NumLogBlockRecorded * vol.BlockSize + size;

  const UInt32 extendedAttrLen = Get32(p + 168 + kExtendOffset);
  const UInt32 allocDescriptorsLen = Get32(p + 172 + kExtendOffset);

  if ((extendedAttrLen & 3) != 0)
    return S_FALSE;
  size_t pos = 176 + kExtendOffset;
  if (extendedAttrLen > size - pos)
    return S_FALSE;
  /*
  if (extendedAttrLen != 16)
  {
    if (extendedAttrLen < 24)
      return S_FALSE;
    CTag attrTag;
    RINOK(attrTag.Parse(p + pos, size));
    if (attrTag.Id != DESC_TYPE_ExtendedAttrHeader)
      return S_FALSE;
    // UInt32 implAttrLocation = Get32(p + pos + 16);
    // UInt32 applicationlAttrLocation = Get32(p + pos + 20);
  }
  */
  pos += extendedAttrLen;

  const int descType = item.IcbTag.GetDescriptorType();
  if (allocDescriptorsLen > size - pos)
    return S_FALSE;
  if (descType == ICB_DESC_TYPE_INLINE)
  {
    item.IsInline = true;
    item.InlineData.CopyFrom(p + pos, allocDescriptorsLen);
  }
  else
  {
    item.IsInline = false;
    if (descType != ICB_DESC_TYPE_SHORT && descType != ICB_DESC_TYPE_LONG)
      return S_FALSE;
    for (UInt32 i = 0; i < allocDescriptorsLen;)
    {
      CMyExtent e;
      if (descType == ICB_DESC_TYPE_SHORT)
      {
        if (i + 8 > allocDescriptorsLen)
          return S_FALSE;
        CShortAllocDesc sad;
        sad.Parse(p + pos + i);
        e.Pos = sad.Pos;
        e.Len = sad.Len;
        e.PartitionRef = lad.Location.PartitionRef;
        i += 8;
      }
      else
      {
        if (i + 16 > allocDescriptorsLen)
          return S_FALSE;
        CLongAllocDesc ladNew;
        ladNew.Parse(p + pos + i);
        e.Pos = ladNew.Location.Pos;
        e.PartitionRef = ladNew.Location.PartitionRef;
        e.Len = ladNew.Len;
        i += 16;
      }
      item.Extents.Add(e);
    }
  }

  if (isDir != item.IcbTag.IsDir())
    return S_FALSE;

  if (item.IcbTag.IsDir())
  {
    if (fsIndex < 0)
      return S_FALSE;

    if (!item.CheckChunkSizes() || !CheckItemExtents(volIndex, item))
      return S_FALSE;
    CByteBuffer buf2;
    RINOK(ReadFromFile(volIndex, item, buf2))
    item.Size = 0;
    item.Extents.ClearAndFree();
    item.InlineData.Free();

    const Byte *p2 = buf2;
    size_t size2 = buf2.Size();
    while (size2 != 0)
    {
      CFileId fileId;
      {
        const size_t cur = fileId.Parse(p2, size2);
        if (cur == 0)
          return S_FALSE;
        p2 += cur;
        size2 -= cur;
      }
      if (fileId.IsItLink_Parent())
        continue;
      if (fileId.IsItLink_Deleted())
        continue;
      {
        CFile file;
        // file.FileVersion = fileId.FileVersion;
        // file.FileCharacteristics = fileId.FileCharacteristics;
        // file.ImplUse = fileId.ImplUse;
        file.Id = fileId.Id;
        
        _fileNameLengthTotal += file.Id.Data.Size();
        if (_fileNameLengthTotal > kFileNameLengthTotalMax)
          return S_FALSE;
        
        item.SubFiles.Add(Files.Size());
        if (Files.Size() >= kNumFilesMax)
          return S_FALSE;
        Files.Add(file);
        RINOK(ReadFileItem(volIndex, (unsigned)fsIndex, fileId.Icb,
            fileId.IsItLink_Dir(), numRecurseAllowed))
      }
    }
  }
  else
  {
    if ((UInt32)item.Extents.Size() > kNumExtentsMax - _numExtents)
      return S_FALSE;
    _numExtents += item.Extents.Size();

    if (item.InlineData.Size() > kInlineExtentsSizeMax - _inlineExtentsSize)
      return S_FALSE;
    _inlineExtentsSize += item.InlineData.Size();
  }

  return S_OK;
}


HRESULT CInArchive::FillRefs(CFileSet &fs, unsigned fileIndex, int parent, int numRecurseAllowed)
{
  if ((_numRefs & 0xFFF) == 0)
  {
    RINOK(_progress->SetCompleted())
  }
  if (numRecurseAllowed-- == 0)
    return S_FALSE;
  if (_numRefs >= kNumRefsMax)
    return S_FALSE;
  _numRefs++;
  CRef ref;
  ref.FileIndex = fileIndex;
  ref.Parent = parent;
  parent = (int)fs.Refs.Size();
  fs.Refs.Add(ref);
  const CItem &item = Items[Files[fileIndex].ItemIndex];
  FOR_VECTOR (i, item.SubFiles)
  {
    RINOK(FillRefs(fs, item.SubFiles[i], parent, numRecurseAllowed))
  }
  return S_OK;
}


API_FUNC_IsArc IsArc_Udf(const Byte *p, size_t size)
{
  UInt32 res = k_IsArc_Res_NO;
  unsigned SecLogSize;
  for (SecLogSize = 11;; SecLogSize -= 2)
  {
    if (SecLogSize < 9)
      return res;
    const UInt32 offset = (UInt32)256 << SecLogSize;
    const UInt32 bufSize = (UInt32)1 << SecLogSize;
    if (offset + bufSize > size)
      res = k_IsArc_Res_NEED_MORE;
    else
    {
      CTag tag;
      if (tag.Parse(p + offset, bufSize) == S_OK)
        if (tag.Id == DESC_TYPE_AnchorVolPtr)
        {
          if (Get32(p + offset + 12) == 256 &&  // TagLocation
              tag.CrcLen >= 16)
            return k_IsArc_Res_YES;
        }
    }
  }
}


HRESULT CInArchive::Open2()
{
  Clear();
  UInt64 fileSize;
  RINOK(InStream_GetSize_SeekToEnd(_stream, fileSize))
  FileSize = fileSize;

  // Some UDFs contain additional pad zeros (2 KB).
  // Seek to STREAM_SEEK_END for direct DVD reading can return 8 KB more, so we check last 16 KB.
  // And when we read last block, result read size can be smaller than required size.

  /*
  const size_t kBufSize = 1 << 14;
  Byte buf[kBufSize];
  size_t readSize = (fileSize < kBufSize) ? (size_t)fileSize : kBufSize;
  RINOK(InStream_SeekSet(_stream, fileSize - readSize))
  RINOK(ReadStream(_stream, buf, &readSize));
  size_t i = readSize;
  for (;;)
  {
    const size_t kSecSizeMin = 1 << 8;
    if (i < kSecSizeMin)
      return S_FALSE;
    i -= kSecSizeMin;
    SecLogSize = (readSize - i < ((size_t)1 << 11)) ? 8 : 11;
    CTag tag;
    if (tag.Parse(buf + i, (1 << SecLogSize)) == S_OK)
      if (tag.Id == DESC_TYPE_AnchorVolPtr)
        break;
  }
  PhySize = fileSize;
  CExtent extentVDS;
  extentVDS.Parse(buf + i + 16);
  */

  /*
  An Anchor Volume Descriptor Pointer structure shall be recorded in at
  least 2 of the following 3 locations on the media:
      Logical Sector 256.
      Logical Sector (N - 256).
      N
  */

  const size_t kBufSize = 1 << 11;
  Byte buf[kBufSize];
  
  for (SecLogSize = 11;; SecLogSize -= 2)
  {
    // Windows 10 uses unusual (SecLogSize = 9)
    if (SecLogSize < 9)
      return S_FALSE;
    const UInt32 offset = (UInt32)256 << SecLogSize;
    if (offset >= fileSize)
      continue;
    RINOK(InStream_SeekSet(_stream, offset))
    const size_t bufSize = (size_t)1 << SecLogSize;
    size_t readSize = bufSize;
    RINOK(ReadStream(_stream, buf, &readSize))
    if (readSize == bufSize)
    {
      CTag tag;
      if (tag.Parse(buf, readSize) == S_OK)
        if (tag.Id == DESC_TYPE_AnchorVolPtr)
        {
          if (Get32(buf + 12) == 256 &&
            tag.CrcLen >= 16) // TagLocation
            break;
        }
    }
  }
  
  PhySize = (UInt32)(256 + 1) << SecLogSize;
  IsArc = true;

  // UDF 2.2.3  AnchorVolumeDescriptorPointer

  CExtent extentVDS;
  extentVDS.Parse(buf + 16);
  {
    CExtent extentVDS2;
    extentVDS2.Parse(buf + 24);
    UpdatePhySize(extentVDS);
    UpdatePhySize(extentVDS2);
  }

  for (UInt32 location = 0; ; location++)
  {
    if (location >= (extentVDS.Len >> SecLogSize))
      return S_FALSE;

    const size_t bufSize = (size_t)1 << SecLogSize;
    {
      const UInt64 offs = ((UInt64)extentVDS.Pos + location) << SecLogSize;
      RINOK(InStream_SeekSet(_stream, offs))
      const HRESULT res = ReadStream_FALSE(_stream, buf, bufSize);
      if (res == S_FALSE && offs + bufSize > FileSize)
        UnexpectedEnd = true;
      RINOK(res)
    }

    CTag tag;
    RINOK(tag.Parse(buf, bufSize))

    if (tag.Id == DESC_TYPE_Terminating)
      break;

    if (tag.Id == DESC_TYPE_PrimVol)
    {
      CPrimeVol &pm = PrimeVols.AddNew();
      pm.Parse(buf);
      continue;
    }

    if (tag.Id == DESC_TYPE_Partition)
    {
      // Partition Descriptor
      // ECMA 3/10.5
      // UDF 2.2.14
      if (Partitions.Size() >= kNumPartitionsMax)
        return S_FALSE;
      CPartition partition;
      // const UInt32 volDescSeqNumer = Get32(buf + 16);
      partition.Flags = Get16(buf + 20);
      partition.Number = Get16(buf + 22);
      partition.ContentsId.Parse(buf + 24);
      
      // memcpy(partition.ContentsUse, buf + 56, sizeof(partition.ContentsUse));
      // ContentsUse contains Partition Header Description.
      // ECMA 4/14.3
      // UDF PartitionHeaderDescriptor 2.3.3

      partition.AccessType = Get32(buf + 184);
      partition.Pos = Get32(buf + 188);
      partition.Len = Get32(buf + 192);
      partition.ImplId.Parse(buf + 196);
      // memcpy(partition.ImplUse, buf + 228, sizeof(partition.ImplUse));

      PRF(printf("\nPartition number = %2d   pos = %d   len = %d", partition.Number, partition.Pos, partition.Len));
      Partitions.Add(partition);
      continue;
    }
    
    if (tag.Id == DESC_TYPE_LogicalVol)
    {
      /* Logical Volume Descriptor
          ECMA 3/10.6
          UDF 2.60 2.2.4 */

      if (LogVols.Size() >= kNumLogVolumesMax)
        return S_FALSE;
      CLogVol &vol = LogVols.AddNew();

      vol.Id.Parse(buf + 84);
      vol.BlockSize = Get32(buf + 212);
      if (vol.BlockSize != ((UInt32)1 << SecLogSize))
      {
        // UDF 2.2.4.2 LogicalBlockSize
        // UDF probably doesn't allow different sizes
        return S_FALSE;
      }
      /*
      if (vol.BlockSize < 512 || vol.BlockSize > ((UInt32)1 << 30))
        return S_FALSE;
      */

      vol.DomainId.Parse(buf + 216);

      // ECMA 4/3.1
      // UDF 2.2.4.4 LogicalVolumeContentsUse
      /* the extent in which the first File Set Descriptor Sequence
         of the logical volume is recorded */
      vol.FileSetLocation.Parse(buf + 248);
      // memcpy(vol.ContentsUse, buf + 248, sizeof(vol.ContentsUse));

      vol.ImplId.Parse(buf + 272);
      // memcpy(vol.ImplUse, buf + 304, sizeof(vol.ImplUse));
      // vol.IntegritySequenceExtent.Parse(buf + 432);

      const UInt32 mapTableLen = Get32(buf + 264);
      const UInt32 numPartitionMaps = Get32(buf + 268);
      if (numPartitionMaps > kNumPartitionsMax)
        return S_FALSE;

      PRF(printf("\nLogicalVol numPartitionMaps = %2d", numPartitionMaps));

      size_t pos = 440;
      if (mapTableLen > bufSize - pos)
        return S_FALSE;
      const size_t posLimit = pos + mapTableLen;
      
      for (UInt32 i = 0; i < numPartitionMaps; i++)
      {
        // ECMA 3/10.7 Partition maps
        if (pos + 2 > posLimit)
          return S_FALSE;
        CPartitionMap pm;
        pm.Type = buf[pos + 0];
        // pm.Length = buf[pos + 1];
        const Byte len = buf[pos + 1];
        if (pos + len > posLimit)
          return S_FALSE;
        
        // memcpy(pm.Data, buf + pos + 2, pm.Length - 2);
        if (pm.Type == 1)
        {
          // ECMA 3/10.7.2
          if (len != 6)
            return S_FALSE;
          pm.VolumeSequenceNumber = Get16(buf + pos + 2);
          pm.PartitionNumber = Get16(buf + pos + 4);
          PRF(printf("\nPartitionMap type 1 PartitionNumber = %2d", pm.PartitionNumber));
        }
        else if (pm.Type == 2)
        {
          if (len != 64)
            return S_FALSE;
          /* ECMA 10.7.3 / Type 2 Partition Map
             62 bytes: Partition Identifier. */

          /* UDF
              2.2.8   "*UDF Virtual Partition"
              2.2.9   "*UDF Sparable Partition"
              2.2.10  "*UDF Metadata Partition"
          */

          if (Get16(buf + pos + 2) != 0) // reserved
            return S_FALSE;

          pm.PartitionTypeId.Parse(buf + pos + 4);
          pm.VolumeSequenceNumber = Get16(buf + pos + 36);
          pm.PartitionNumber = Get16(buf + pos + 38);

          if (memcmp(pm.PartitionTypeId.Id, "*UDF Metadata Partition", 23) != 0)
            return S_FALSE;

          // UDF 2.2.10 Metadata Partition Map
          pm.MetadataFileLocation = Get32(buf + pos + 40);
          // pm.MetadataMirrorFileLocation = Get32(buf + pos + 44);
          // pm.MetadataBitmapFileLocation = Get32(buf + pos + 48);
          // pm.AllocationUnitSize = Get32(buf + pos + 52);
          // pm.AlignmentUnitSize = Get16(buf + pos + 56);
          // pm.Flags = buf[pos + 58];

          PRF(printf("\nPartitionMap type 2 PartitionNumber = %2d", pm.PartitionNumber));
          // Unsupported = true;
          // return S_FALSE;
        }
        else
          return S_FALSE;
        pos += len;
        vol.PartitionMaps.Add(pm);
      }
      continue;
    }
    
    /*
    if (tag.Id == DESC_TYPE_UnallocSpace)
    {
      // UInt32 volDescSeqNumer = Get32(buf + 16);
      const UInt32 numAlocDescs = Get32(buf + 20);
      // we need examples for (numAlocDescs != 0) case
      if (numAlocDescs > (bufSize - 24) / 8)
        return S_FALSE;
      for (UInt32 i = 0; i < numAlocDescs; i++)
      {
        CExtent e;
        e.Parse(buf + 24 + i * 8);
      }
      continue;
    }
    else
      continue;
    */
  }

  UInt64 totalSize = 0;

  unsigned volIndex;
  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
    CLogVol &vol = LogVols[volIndex];
    FOR_VECTOR (pmIndex, vol.PartitionMaps)
    {
      CPartitionMap &pm = vol.PartitionMaps[pmIndex];
      for (unsigned i = 0;; i++)
      {
        if (i == Partitions.Size())
          return S_FALSE;
        CPartition &part = Partitions[i];
        if (part.Number == pm.PartitionNumber)
        {
          pm.PartitionIndex = i;
          if (pm.Type == 2)
            break;
          
          /*
          if (part.VolIndex >= 0)
          {
            // it's for 2.60. Fix it
            if (part.VolIndex != (int)volIndex)
              return S_FALSE;
            // return S_FALSE;
          }
          part.VolIndex = volIndex;
          */

          totalSize += (UInt64)part.Len << SecLogSize;
          break;
        }
      }
    }
  }

  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
    CLogVol &vol = LogVols[volIndex];
    FOR_VECTOR (pmIndex, vol.PartitionMaps)
    {
      CPartitionMap &pm = vol.PartitionMaps[pmIndex];
      if (pm.Type != 2)
        continue;

      {
        CLongAllocDesc lad;
        lad.Len = vol.BlockSize;
        lad.Location.Pos = pm.MetadataFileLocation;
        // lad.Location.Pos = pm.MetadataMirrorFileLocation;

        lad.Location.PartitionRef = (UInt16)pmIndex;

        /* we need correct PartitionMaps[lad.Location.PartitionRef].PartitionIndex.
           so we can use pmIndex or find (Type==1) PartitionMap */
        FOR_VECTOR (pmIndex2, vol.PartitionMaps)
        {
          const CPartitionMap &pm2 = vol.PartitionMaps[pmIndex2];
          if (pm2.PartitionNumber == pm.PartitionNumber && pm2.Type == 1)
          {
            lad.Location.PartitionRef = (UInt16)pmIndex2;
            break;
          }
        }
        
        RINOK(ReadItem(volIndex,
            -1, // (fsIndex = -1) means that it's metadata
            lad,
            false, // isDir
            1)) // numRecurseAllowed
      }
      {
        const CItem &item = Items.Back();
        if (!CheckItemExtents(volIndex, item))
          return S_FALSE;
        if (item.Extents.Size() != 1)
        {
          if (item.Extents.Size() < 1)
            return S_FALSE;
          /* Windows 10 writes empty record item.Extents[1].
             we ignore such extent here */
          for (unsigned k = 1; k < item.Extents.Size(); k++)
          {
            const CMyExtent &e = item.Extents[k];
            if (e.GetLen() != 0)
              return S_FALSE;
          }
        }

        const CMyExtent &e = item.Extents[0];
        const CPartition &part = Partitions[pm.PartitionIndex];
        CPartition mp = part;
        mp.IsMetadata = true;
        // mp.Number = part.Number;
        mp.Pos = part.Pos + e.Pos;
        mp.Len = e.Len >> SecLogSize;
        pm.PartitionIndex = Partitions.Add(mp);
      }
      // Items.DeleteBack(); // we can delete that metadata item

      /*
      // short version of code to read metadata file.
      RINOK(CInArchive::Read(volIndex, pmIndex, pm.MetadataFileLocation, 224, buf));
      CTag tag;
      RINOK(tag.Parse(buf, 224));
      if (tag.Id != DESC_TYPE_ExtendedFile)
        return S_FALSE;
      CShortAllocDesc sad;
      sad.Parse(buf + 216);
      const CPartition &part = Partitions[pm.PartitionIndex];
      CPartition mp = part;
      mp.IsMetadata = true;
      // mp.Number = part.Number;
      mp.Pos = part.Pos + sad.Pos;
      mp.Len = sad.Len >> SecLogSize;
      pm.PartitionIndex = Partitions.Add(mp);
      */
    }
  }

  RINOK(_progress->SetTotal(totalSize))

  PRF(printf("\n Read files"));

  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
    CLogVol &vol = LogVols[volIndex];

    PRF(printf("\nLogVol %2d", volIndex));

    CLongAllocDesc nextExtent = vol.FileSetLocation;
    // while (nextExtent.ExtentLen != 0)
    // for (int i = 0; i < 1; i++)
    {
      if (nextExtent.GetLen() < 512)
        return S_FALSE;
      CByteBuffer buf2(nextExtent.GetLen());
      RINOK(ReadLad(volIndex, nextExtent, buf2))
      const Byte *p = buf2;
      const size_t size = nextExtent.GetLen();

      CTag tag;
      RINOK(tag.Parse(p, size))

      /*
      // commented in 22.01
      if (tag.Id == DESC_TYPE_ExtendedFile)
      {
        // ECMA 4 / 14.17
        // 2.60 ??
        return S_FALSE;
      }
      */

      if (tag.Id != DESC_TYPE_FileSet)
        return S_FALSE;
      
      PRF(printf("\n FileSet", volIndex));
      CFileSet fs;
      fs.RecordingTime.Parse(p + 16);
      // fs.InterchangeLevel = Get16(p + 18);
      // fs.MaxInterchangeLevel = Get16(p + 20);
      fs.FileSetNumber = Get32(p + 40);
      fs.FileSetDescNumber = Get32(p + 44);
      
      fs.LogicalVolumeId.Parse(p + 112);
      fs.Id.Parse(p + 304);
      fs.CopyrightId.Parse(p + 336);
      fs.AbstractId.Parse(p + 368);
      
      fs.RootDirICB.Parse(p + 400);
      fs.DomainId.Parse(p + 416);
      
      // fs.SystemStreamDirICB.Parse(p + 464);
      
      vol.FileSets.Add(fs);

      // nextExtent.Parse(p + 448);
    }

    FOR_VECTOR (fsIndex, vol.FileSets)
    {
      CFileSet &fs = vol.FileSets[fsIndex];
      const unsigned fileIndex = Files.Size();
      Files.AddNew();
      RINOK(ReadFileItem(volIndex, fsIndex, fs.RootDirICB,
          true, // isDir
          kNumRecursionLevelsMax))
      RINOK(FillRefs(fs, fileIndex, -1, kNumRecursionLevelsMax))
    }
  }


  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
    const CLogVol &vol = LogVols[volIndex];
    // bool showFileSetName = (vol.FileSets.Size() > 1);
    FOR_VECTOR (fsIndex, vol.FileSets)
    {
      const CFileSet &fs = vol.FileSets[fsIndex];
      for (unsigned i =
          // ((showVolName || showFileSetName) ? 0 : 1)
            0; i < fs.Refs.Size(); i++)
      {
        const CRef &ref = vol.FileSets[fsIndex].Refs[i];
        const CFile &file = Files[ref.FileIndex];
        const CItem &item = Items[file.ItemIndex];
        UInt64 size = item.Size;

        if (!item.IsRecAndAlloc() || !item.CheckChunkSizes() || !CheckItemExtents(volIndex, item))
          continue;

        FOR_VECTOR (extentIndex, item.Extents)
        {
          const CMyExtent &extent = item.Extents[extentIndex];
          const UInt32 len = extent.GetLen();
          if (len == 0)
            continue;
          if (size < len)
            break;
          
          const unsigned partitionIndex = vol.PartitionMaps[extent.PartitionRef].PartitionIndex;
          const UInt32 logBlockNumber = extent.Pos;
          const CPartition &partition = Partitions[partitionIndex];
          const UInt64 offset = ((UInt64)partition.Pos << SecLogSize) +
              (UInt64)logBlockNumber * vol.BlockSize;
          UpdatePhySize(offset + len);
        }
      }
    }
  }

  {
    const UInt32 secMask = ((UInt32)1 << SecLogSize) - 1;
    PhySize = (PhySize + secMask) & ~(UInt64)secMask;
  }
  
  NoEndAnchor = true;

  if (PhySize < fileSize)
  {
    UInt64 rem = fileSize - PhySize;
    const size_t secSize = (size_t)1 << SecLogSize;

    RINOK(InStream_SeekSet(_stream, PhySize))

    // some UDF images contain ZEROs before "Anchor Volume Descriptor Pointer" at the end

    for (unsigned sec = 0; sec < 1024; sec++)
    {
      if (rem == 0)
        break;
      
      size_t readSize = secSize;
      if (readSize > rem)
        readSize = (size_t)rem;
      
      RINOK(ReadStream(_stream, buf, &readSize))
      
      if (readSize == 0)
        break;
      
      // some udf contain many EndAnchors
      if (readSize == secSize /* && NoEndAnchor */)
      {
        CTag tag;
        if (tag.Parse(buf, readSize) == S_OK
            && tag.Id == DESC_TYPE_AnchorVolPtr
            && Get32(buf + 12) == (UInt32)((fileSize - rem) >> SecLogSize))
        {
          NoEndAnchor = false;
          rem -= readSize;
          PhySize = fileSize - rem;
          continue;
        }
      }
      
      size_t i;
      for (i = 0; i < readSize && buf[i] == 0; i++);
      if (i != readSize)
        break;
      rem -= readSize;
    }

    if (rem == 0)
      PhySize = fileSize;
  }

  return S_OK;
}


HRESULT CInArchive::Open(IInStream *inStream, CProgressVirt *progress)
{
  _progress = progress;
  _stream = inStream;
  HRESULT res = Open2();
  if (res == S_FALSE && IsArc && !UnexpectedEnd)
    Unsupported = true;
  return res;

  /*
  HRESULT res;
  try
  {
    res = Open2();
  }
  catch(...)
  {
    // Clear();
    // res = S_FALSE;
    _stream.Release();
    throw;
  }
  _stream.Release();
  return res;
  */
}

void CInArchive::Clear()
{
  IsArc = false;
  Unsupported = false;
  UnexpectedEnd = false;
  NoEndAnchor = false;

  PhySize = 0;
  FileSize = 0;

  Partitions.Clear();
  LogVols.Clear();
  PrimeVols.Clear();
  Items.Clear();
  Files.Clear();
  _fileNameLengthTotal = 0;
  _numRefs = 0;
  _numExtents = 0;
  _inlineExtentsSize = 0;
  _processedProgressBytes = 0;
}


static const char * const g_PartitionTypes[] =
{
    "Pseudo-Overwritable" // UDF
  , "Read-Only"
  , "Write-Once"
  , "Rewritable"
  , "Overwritable"
};


static void AddComment_Align(UString &s)
{
  s += "  ";
}

static void AddComment_PropName(UString &s, const char *name)
{
  AddComment_Align(s);
  s += name;
  s += ": ";
}

static void AddComment_UInt32(UString &s, const char *name, UInt32 val)
{
  AddComment_PropName(s, name);
  s.Add_UInt32(val);
  s.Add_LF();
}

static void AddComment_UInt32_2(UString &s, const char *name, UInt32 val)
{
  AddComment_Align(s);
  AddComment_UInt32(s, name, val);
}


static void AddComment_UInt64(UString &s, const char *name, UInt64 val)
{
  AddComment_PropName(s, name);
  s.Add_UInt64(val);
  s.Add_LF();
}

static void AddComment_RegId(UString &s, const char *name, const CRegId &ri)
{
  AddComment_PropName(s, name);
  ri.AddCommentTo(s);
  s.Add_LF();
}

static void AddComment_RegId_Domain(UString &s, const char *name, const CRegId &ri)
{
  AddComment_PropName(s, name);
  ri.AddCommentTo(s);
  {
    UString s2;
    ri.AddUdfVersionTo(s2);
    if (!s2.IsEmpty())
    {
      s += "::";
      s += s2;
    }
  }
  s.Add_LF();
}


// UDF 6.3.1 OS Class

static const char * const g_OsClasses[] =
{
    NULL
  , "DOS"
  , "OS/2"
  , "Macintosh OS"
  , "UNIX"
  , "Windows 9x"
  , "Windows NT"
  , "OS/400"
  , "BeOS"
  , "Windows CE"
};

// UDF 6.3.2 OS Identifier

static const char * const g_OsIds_Unix[] =
{
    NULL // "Generic"
  , "AIX"
  , "SUN OS / Solaris"
  , "HP/UX"
  , "Silicon Graphics Irix"
  , "Linux"
  , "MKLinux"
  , "FreeBSD"
  , "NetBSD"
};

static void AddOs_Class_Id(UString &s, const Byte *p)
{
  // UDF 2.1.5.3 Implementation Identifier Suffix
  // Appendix 6.3 Operating System Identifiers.
  const Byte osClass = p[0];
  if (osClass != 0)
  {
    s += "::";
    s += TypeToString(g_OsClasses, Z7_ARRAY_SIZE(g_OsClasses), osClass);
  }
  const Byte osId = p[1];
  if (osId != 0)
  {
    s += "::";
    if (osClass == 4) // unix
    {
      s += TypeToString(g_OsIds_Unix, Z7_ARRAY_SIZE(g_OsIds_Unix), osId);
    }
    else
      s.Add_UInt32(osId);
  }
}


static void AddComment_RegId_Impl(UString &s, const char *name, const CRegId &ri)
{
  AddComment_PropName(s, name);
  ri.AddCommentTo(s);
  {
    AddOs_Class_Id(s, ri.Suffix);
  }
  s.Add_LF();
}


static void AddComment_RegId_UdfId(UString &s, const char *name, const CRegId &ri)
{
  AddComment_PropName(s, name);
  ri.AddCommentTo(s);
  {
    // UDF 2.1.5.3
    // UDF Identifier Suffix format
    UString s2;
    ri.AddUdfVersionTo(s2);
    if (!s2.IsEmpty())
    {
      s += "::";
      s += s2;
    }
    AddOs_Class_Id(s, &ri.Suffix[2]);
  }
  s.Add_LF();
}

static void AddComment_DString32(UString &s, const char *name, const CDString32 &d)
{
  AddComment_Align(s);
  AddComment_PropName(s, name);
  s += d.GetString();
  s.Add_LF();
}

UString CInArchive::GetComment() const
{
  UString s;
  {
    s += "Primary Volumes:";
    s.Add_LF();
    FOR_VECTOR (i, PrimeVols)
    {
      if (i != 0)
        s.Add_LF();
      const CPrimeVol &pv = PrimeVols[i];
      // AddComment_UInt32(s, "VolumeDescriptorSequenceNumber", pv.VolumeDescriptorSequenceNumber);
      // if (PrimeVols.Size() != 1 || pv.PrimaryVolumeDescriptorNumber != 0)
        AddComment_UInt32(s, "PrimaryVolumeDescriptorNumber", pv.PrimaryVolumeDescriptorNumber);
      // if (pv.MaximumVolumeSequenceNumber != 1 || pv.VolumeSequenceNumber != 1)
        AddComment_UInt32(s, "VolumeSequenceNumber", pv.VolumeSequenceNumber);
      if (pv.MaximumVolumeSequenceNumber != 1)
        AddComment_UInt32(s, "MaximumVolumeSequenceNumber", pv.MaximumVolumeSequenceNumber);
      AddComment_PropName(s, "VolumeId");
      s += pv.VolumeId.GetString();
      s.Add_LF();
      AddComment_PropName(s, "VolumeSetId");
      s += pv.VolumeSetId.GetString();
      s.Add_LF();
      // AddComment_UInt32(s, "InterchangeLevel", pv.InterchangeLevel);
      // AddComment_UInt32(s, "MaximumInterchangeLevel", pv.MaximumInterchangeLevel);
      AddComment_RegId(s, "ApplicationId", pv.ApplicationId);
      AddComment_RegId_Impl(s, "ImplementationId", pv.ImplId);
    }
  }
  {
    s += "Partitions:";
    s.Add_LF();
    FOR_VECTOR (i, Partitions)
    {
      if (i != 0)
        s.Add_LF();
      const CPartition &part = Partitions[i];
      AddComment_UInt32(s, "PartitionIndex", i);
      AddComment_UInt32(s, "PartitionNumber", part.Number);
      if (part.IsMetadata)
        AddComment_UInt32(s, "IsMetadata", 1);
      else
      {
        AddComment_RegId(s, "ContentsId", part.ContentsId);
        AddComment_RegId_Impl(s, "ImplementationId", part.ImplId);
        AddComment_PropName(s, "AccessType");
        s += TypeToString(g_PartitionTypes, Z7_ARRAY_SIZE(g_PartitionTypes), part.AccessType);
        s.Add_LF();
      }
      AddComment_UInt64(s, "Size", (UInt64)part.Len << SecLogSize);
      AddComment_UInt64(s, "Pos", (UInt64)part.Pos << SecLogSize);
    }
  }
  s += "Logical Volumes:";
  s.Add_LF();
  {
    FOR_VECTOR (i, LogVols)
    {
      if (i != 0)
        s.Add_LF();
      const CLogVol &vol = LogVols[i];
      if (LogVols.Size() != 1)
        AddComment_UInt32(s, "Number", i);
      AddComment_PropName(s, "Id");
      s += vol.Id.GetString();
      s.Add_LF();
      AddComment_UInt32(s, "BlockSize", vol.BlockSize);
      AddComment_RegId_Domain(s, "DomainId", vol.DomainId);
      AddComment_RegId_Impl(s, "ImplementationId", vol.ImplId);
      // AddComment_UInt64(s, "IntegritySequenceExtent_Len", vol.IntegritySequenceExtent.Len);
      // AddComment_UInt64(s, "IntegritySequenceExtent_Pos", (UInt64)vol.IntegritySequenceExtent.Pos << SecLogSize);

      s += "  Partition Maps:";
      s.Add_LF();
      {
        FOR_VECTOR (j, vol.PartitionMaps)
        {
          if (j != 0)
            s.Add_LF();
          const CPartitionMap &pm = vol.PartitionMaps[j];
          AddComment_UInt32_2(s, "PartitionMap", j);
          AddComment_UInt32_2(s, "Type", pm.Type);
          AddComment_UInt32_2(s, "VolumeSequenceNumber", pm.VolumeSequenceNumber);
          AddComment_UInt32_2(s, "PartitionNumber", pm.PartitionNumber);
          if (pm.Type == 2)
          {
            AddComment_UInt32_2(s, "MetadataFileLocation", pm.MetadataFileLocation);
            // AddComment_UInt32_2(s, "MetadataMirrorFileLocation", pm.MetadataMirrorFileLocation);
            // AddComment_UInt32_2(s, "MetadataBitmapFileLocation", pm.MetadataBitmapFileLocation);
            // AddComment_UInt32_2(s, "AllocationUnitSize", pm.AllocationUnitSize);
            // AddComment_UInt32_2(s, "AlignmentUnitSize", pm.AlignmentUnitSize);
            // AddComment_UInt32_2(s, "Flags", pm.Flags);
            AddComment_Align(s); AddComment_RegId_UdfId(s, "PartitionTypeId", pm.PartitionTypeId);
          }
        }
      }
      s += "  File Sets:";
      s.Add_LF();
      {
        FOR_VECTOR (j, vol.FileSets)
        {
          if (j != 0)
            s.Add_LF();
          const CFileSet &fs = vol.FileSets[j];
          AddComment_Align(s); AddComment_UInt32(s, "FileSetNumber", fs.FileSetNumber);
          AddComment_Align(s); AddComment_UInt32(s, "FileSetDescNumber", fs.FileSetDescNumber);

          AddComment_Align(s);
          AddComment_PropName(s, "LogicalVolumeId");
          s += fs.LogicalVolumeId.GetString();
          s.Add_LF();

          AddComment_DString32(s, "Id", fs.Id);
          AddComment_DString32(s, "CopyrightId", fs.CopyrightId);
          AddComment_DString32(s, "AbstractId", fs.AbstractId);
          
          AddComment_Align(s);
          AddComment_RegId_Domain(s, "DomainId", fs.DomainId);
        }
      }
    }
  }
  return s;
}

static UString GetSpecName(const UString &name)
{
  UString name2 = name;
  name2.Trim();
  if (name2.IsEmpty())
    return UString("[]");
  return name;
}

static void UpdateWithName(UString &res, const UString &addString)
{
  if (res.IsEmpty())
    res = addString;
  else
    res.Insert(0, addString + WCHAR_PATH_SEPARATOR);
}

UString CInArchive::GetItemPath(unsigned volIndex, unsigned fsIndex, unsigned refIndex,
    bool showVolName, bool showFsName) const
{
  // showVolName = true;
  const CLogVol &vol = LogVols[volIndex];
  const CFileSet &fs = vol.FileSets[fsIndex];

  UString name;

  for (;;)
  {
    const CRef &ref = fs.Refs[refIndex];
    // we break on root file (that probably has empty name)
    if (ref.Parent < 0)
      break;
    refIndex = (unsigned)ref.Parent;
    UpdateWithName(name, GetSpecName(Files[ref.FileIndex].GetName()));
  }

  if (showFsName)
  {
    UString newName ("File Set ");
    newName.Add_UInt32(fsIndex);
    UpdateWithName(name, newName);
  }

  if (showVolName)
  {
    UString newName;
    newName.Add_UInt32(volIndex);
    UString newName2 = vol.GetName();
    if (newName2.IsEmpty())
      newName2 = "Volume";
    newName += '-';
    newName += newName2;
    UpdateWithName(name, newName);
  }
  return name;
}

}}
