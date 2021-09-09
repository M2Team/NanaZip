// Archive/UdfIn.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#endif

#include "../../../../C/CpuArch.h"

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
// #define CRC16_GET_DIGEST(crc) (crc)
#define CRC16_UPDATE_BYTE(crc, b) ((UInt16)(g_Crc16Table[(((crc) >> 8) ^ (b)) & 0xFF] ^ ((crc) << 8)))

#define kCrc16Poly 0x1021
static UInt16 g_Crc16Table[256];

static void MY_FAST_CALL Crc16GenerateTable(void)
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

static UInt32 MY_FAST_CALL Crc16_Update(UInt32 v, const void *data, size_t size)
{
  const Byte *p = (const Byte *)data;
  for (; size > 0 ; size--, p++)
    v = CRC16_UPDATE_BYTE(v, *p);
  return v;
}

static UInt32 MY_FAST_CALL Crc16Calc(const void *data, size_t size)
{
  return Crc16_Update(CRC16_INIT_VAL, data, size);
}

static struct CCrc16TableInit { CCrc16TableInit() { Crc16GenerateTable(); } } g_Crc16TableInit;



void CDString::Parse(const Byte *p, unsigned size)
{
  Data.CopyFrom(p, size);
}

static UString ParseDString(const Byte *data, unsigned size)
{
  UString res;
  if (size > 0)
  {
    wchar_t *p;
    Byte type = data[0];
    if (type == 8)
    {
      p = res.GetBuf(size);
      for (unsigned i = 1; i < size; i++)
      {
        wchar_t c = data[i];
        if (c == 0)
          break;
        *p++ = c;
      }
    }
    else if (type == 16)
    {
      p = res.GetBuf(size / 2);
      for (unsigned i = 1; i + 2 <= size; i += 2)
      {
        wchar_t c = GetBe16(data + i);
        if (c == 0)
          break;
        *p++ = c;
      }
    }
    else
      return UString("[unknow]");
    *p = 0;
    res.ReleaseBuf_SetLen((unsigned)(p - (const wchar_t *)res));
  }
  return res;
}

UString CDString128::GetString() const
{
  unsigned size = Data[sizeof(Data) - 1];
  return ParseDString(Data, MyMin(size, (unsigned)(sizeof(Data) - 1)));
}

UString CDString::GetString() const { return ParseDString(Data, (unsigned)Data.Size()); }

void CTime::Parse(const Byte *buf) { memcpy(Data, buf, sizeof(Data)); }

/*
void CRegId::Parse(const Byte *buf)
{
  Flags = buf[0];
  memcpy(Id, buf + 1, sizeof(Id));
  memcpy(Suffix, buf + 24, sizeof(Suffix));
}
*/

// ECMA 3/7.1

struct CExtent
{
  UInt32 Len;
  UInt32 Pos;

  void Parse(const Byte *buf);
};

void CExtent::Parse(const Byte *buf)
{
  Len = Get32(buf);
  Pos = Get32(buf + 4);
}

// ECMA 3/7.2

struct CTag
{
  UInt16 Id;
  UInt16 Version;
  // Byte Checksum;
  // UInt16 SerialNumber;
  // UInt16 Crc;
  // UInt16 CrcLen;
  // UInt32 TagLocation;
  
  HRESULT Parse(const Byte *buf, size_t size);
};

HRESULT CTag::Parse(const Byte *buf, size_t size)
{
  if (size < 16)
    return S_FALSE;
  Byte sum = 0;
  int i;
  for (i = 0; i <  4; i++) sum = (Byte)(sum + buf[i]);
  for (i = 5; i < 16; i++) sum = (Byte)(sum + buf[i]);
  if (sum != buf[4] || buf[5] != 0) return S_FALSE;

  Id = Get16(buf);
  Version = Get16(buf + 2);
  // SerialNumber = Get16(buf + 6);
  UInt32 crc = Get16(buf + 8);
  UInt32 crcLen = Get16(buf + 10);
  // TagLocation = Get32(buf + 12);

  if (size >= 16 + crcLen)
    if (crc == Crc16Calc(buf + 16, (size_t)crcLen))
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
  DESC_TYPE_UnallocatedSpace    = 263,
  DESC_TYPE_SpaceBitmap         = 264,
  DESC_TYPE_PartitionIntegrity  = 265,
  DESC_TYPE_ExtendedFile        = 266
};


void CLogBlockAddr::Parse(const Byte *buf)
{
  Pos = Get32(buf);
  PartitionRef = Get16(buf + 4);
}

void CShortAllocDesc::Parse(const Byte *buf)
{
  Len = Get32(buf);
  Pos = Get32(buf + 4);
}

/*
void CADImpUse::Parse(const Byte *buf)
{
  Flags = Get16(buf);
  UdfUniqueId = Get32(buf + 2);
}
*/

void CLongAllocDesc::Parse(const Byte *buf)
{
  Len = Get32(buf);
  Location.Parse(buf + 4);
  // memcpy(ImplUse, buf + 10, sizeof(ImplUse));
  // adImpUse.Parse(ImplUse);
}

bool CInArchive::CheckExtent(int volIndex, int partitionRef, UInt32 blockPos, UInt32 len) const
{
  const CLogVol &vol = LogVols[volIndex];
  if (partitionRef >= (int)vol.PartitionMaps.Size())
    return false;
  const CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];
  UInt64 offset = ((UInt64)partition.Pos << SecLogSize) + (UInt64)blockPos * vol.BlockSize;
  return (offset + len) <= (((UInt64)partition.Pos + partition.Len) << SecLogSize);
}

bool CInArchive::CheckItemExtents(int volIndex, const CItem &item) const
{
  FOR_VECTOR (i, item.Extents)
  {
    const CMyExtent &e = item.Extents[i];
    if (!CheckExtent(volIndex, e.PartitionRef, e.Pos, e.GetLen()))
      return false;
  }
  return true;
}

HRESULT CInArchive::Read(int volIndex, int partitionRef, UInt32 blockPos, UInt32 len, Byte *buf)
{
  if (!CheckExtent(volIndex, partitionRef, blockPos, len))
    return S_FALSE;
  const CLogVol &vol = LogVols[volIndex];
  const CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];
  UInt64 offset = ((UInt64)partition.Pos << SecLogSize) + (UInt64)blockPos * vol.BlockSize;
  RINOK(_stream->Seek(offset, STREAM_SEEK_SET, NULL));
  HRESULT res = ReadStream_FALSE(_stream, buf, len);
  if (res == S_FALSE && offset + len > FileSize)
    UnexpectedEnd = true;
  RINOK(res);
  UpdatePhySize(offset + len);
  return S_OK;
}

HRESULT CInArchive::Read(int volIndex, const CLongAllocDesc &lad, Byte *buf)
{
  return Read(volIndex, lad.Location.PartitionRef, lad.Location.Pos, lad.GetLen(), (Byte *)buf);
}

HRESULT CInArchive::ReadFromFile(int volIndex, const CItem &item, CByteBuffer &buf)
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
    UInt32 len = e.GetLen();
    RINOK(Read(volIndex, e.PartitionRef, e.Pos, len, (Byte *)buf + pos));
    pos += len;
  }
  return S_OK;
}


void CIcbTag::Parse(const Byte *p)
{
  // PriorDirectNum = Get32(p);
  // StrategyType = Get16(p + 4);
  // StrategyParam = Get16(p + 6);
  // MaxNumOfEntries = Get16(p + 8);
  FileType = p[11];
  // ParentIcb.Parse(p + 12);
  Flags = Get16(p + 18);
}

void CItem::Parse(const Byte *p)
{
  // Uid = Get32(p + 36);
  // Gid = Get32(p + 40);
  // Permissions = Get32(p + 44);
  // FileLinkCount = Get16(p + 48);
  // RecordFormat = p[50];
  // RecordDisplayAttr = p[51];
  // RecordLen = Get32(p + 52);
  Size = Get64(p + 56);
  NumLogBlockRecorded = Get64(p + 64);
  ATime.Parse(p + 72);
  MTime.Parse(p + 84);
  // AttrtTime.Parse(p + 96);
  // CheckPoint = Get32(p + 108);
  // ExtendedAttrIcb.Parse(p + 112);
  // ImplId.Parse(p + 128);
  // UniqueId = Get64(p + 160);
}

// 4/14.4
struct CFileId
{
  // UInt16 FileVersion;
  Byte FileCharacteristics;
  // CByteBuffer ImplUse;
  CDString Id;
  CLongAllocDesc Icb;

  bool IsItLinkParent() const { return (FileCharacteristics & FILEID_CHARACS_Parent) != 0; }
  HRESULT Parse(const Byte *p, size_t size, size_t &processed);
};

HRESULT CFileId::Parse(const Byte *p, size_t size, size_t &processed)
{
  processed = 0;
  if (size < 38)
    return S_FALSE;
  CTag tag;
  RINOK(tag.Parse(p, size));
  if (tag.Id != DESC_TYPE_FileId)
    return S_FALSE;
  // FileVersion = Get16(p + 16);
  FileCharacteristics = p[18];
  unsigned idLen = p[19];
  Icb.Parse(p + 20);
  unsigned impLen = Get16(p + 36);
  if (size < 38 + idLen + impLen)
    return S_FALSE;
  // ImplUse.SetCapacity(impLen);
  processed = 38;
  // memcpy(ImplUse, p + processed, impLen);
  processed += impLen;
  Id.Parse(p + processed, idLen);
  processed += idLen;
  for (;(processed & 3) != 0; processed++)
    if (p[processed] != 0)
      return S_FALSE;
  return (processed <= size) ? S_OK : S_FALSE;
}

HRESULT CInArchive::ReadFileItem(int volIndex, int fsIndex, const CLongAllocDesc &lad, int numRecurseAllowed)
{
  if (Files.Size() % 100 == 0)
    RINOK(_progress->SetCompleted(Files.Size(), _processedProgressBytes));
  if (numRecurseAllowed-- == 0)
    return S_FALSE;
  CFile &file = Files.Back();
  const CLogVol &vol = LogVols[volIndex];
  unsigned partitionRef = lad.Location.PartitionRef;
  if (partitionRef >= vol.PartitionMaps.Size())
    return S_FALSE;
  CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];

  UInt32 key = lad.Location.Pos;
  UInt32 value;
  const UInt32 kRecursedErrorValue = (UInt32)(Int32)-1;
  if (partition.Map.Find(key, value))
  {
    if (value == kRecursedErrorValue)
      return S_FALSE;
    file.ItemIndex = value;
  }
  else
  {
    value = Items.Size();
    file.ItemIndex = (int)value;
    if (partition.Map.Set(key, kRecursedErrorValue))
      return S_FALSE;
    RINOK(ReadItem(volIndex, fsIndex, lad, numRecurseAllowed));
    if (!partition.Map.Set(key, value))
      return S_FALSE;
  }
  return S_OK;
}

HRESULT CInArchive::ReadItem(int volIndex, int fsIndex, const CLongAllocDesc &lad, int numRecurseAllowed)
{
  if (Items.Size() > kNumItemsMax)
    return S_FALSE;
  Items.Add(CItem());
  CItem &item = Items.Back();
  
  const CLogVol &vol = LogVols[volIndex];
 
  if (lad.GetLen() != vol.BlockSize)
    return S_FALSE;

  const size_t size = lad.GetLen();
  CByteBuffer buf(size);
  RINOK(Read(volIndex, lad, buf));

  CTag tag;
  const Byte *p = buf;
  RINOK(tag.Parse(p, size));
  if (size < 176)
    return S_FALSE;
  if (tag.Id != DESC_TYPE_File)
    return S_FALSE;

  item.IcbTag.Parse(p + 16);
  if (item.IcbTag.FileType != ICB_FILE_TYPE_DIR &&
      item.IcbTag.FileType != ICB_FILE_TYPE_FILE)
    return S_FALSE;

  item.Parse(p);

  _processedProgressBytes += (UInt64)item.NumLogBlockRecorded * vol.BlockSize + size;

  UInt32 extendedAttrLen = Get32(p + 168);
  UInt32 allocDescriptorsLen = Get32(p + 172);

  if ((extendedAttrLen & 3) != 0)
    return S_FALSE;
  size_t pos = 176;
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

  int desctType = item.IcbTag.GetDescriptorType();
  if (allocDescriptorsLen > size - pos)
    return S_FALSE;
  if (desctType == ICB_DESC_TYPE_INLINE)
  {
    item.IsInline = true;
    item.InlineData.CopyFrom(p + pos, allocDescriptorsLen);
  }
  else
  {
    item.IsInline = false;
    if (desctType != ICB_DESC_TYPE_SHORT && desctType != ICB_DESC_TYPE_LONG)
      return S_FALSE;
    for (UInt32 i = 0; i < allocDescriptorsLen;)
    {
      CMyExtent e;
      if (desctType == ICB_DESC_TYPE_SHORT)
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

  if (item.IcbTag.IsDir())
  {
    if (!item.CheckChunkSizes() || !CheckItemExtents(volIndex, item))
      return S_FALSE;
    CByteBuffer buf2;
    RINOK(ReadFromFile(volIndex, item, buf2));
    item.Size = 0;
    item.Extents.ClearAndFree();
    item.InlineData.Free();

    const Byte *p2 = buf2;
    const size_t size2 = buf2.Size();
    size_t processedTotal = 0;
    for (; processedTotal < size2;)
    {
      size_t processedCur;
      CFileId fileId;
      RINOK(fileId.Parse(p2 + processedTotal, size2 - processedTotal, processedCur));
      if (!fileId.IsItLinkParent())
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
        if (Files.Size() > kNumFilesMax)
          return S_FALSE;
        Files.Add(file);
        RINOK(ReadFileItem(volIndex, fsIndex, fileId.Icb, numRecurseAllowed));
      }
      processedTotal += processedCur;
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
    RINOK(_progress->SetCompleted());
  }
  if (numRecurseAllowed-- == 0)
    return S_FALSE;
  if (_numRefs >= kNumRefsMax)
    return S_FALSE;
  _numRefs++;
  CRef ref;
  ref.FileIndex = fileIndex;
  ref.Parent = parent;
  parent = fs.Refs.Size();
  fs.Refs.Add(ref);
  const CItem &item = Items[Files[fileIndex].ItemIndex];
  FOR_VECTOR (i, item.SubFiles)
  {
    RINOK(FillRefs(fs, item.SubFiles[i], parent, numRecurseAllowed));
  }
  return S_OK;
}

API_FUNC_IsArc IsArc_Udf(const Byte *p, size_t size)
{
  UInt32 res = k_IsArc_Res_NO;
  unsigned SecLogSize;
  for (SecLogSize = 11;; SecLogSize -= 3)
  {
    if (SecLogSize < 8)
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
          return k_IsArc_Res_YES;
    }
  }
}


HRESULT CInArchive::Open2()
{
  Clear();
  UInt64 fileSize;
  RINOK(_stream->Seek(0, STREAM_SEEK_END, &fileSize));
  FileSize = fileSize;

  // Some UDFs contain additional pad zeros (2 KB).
  // Seek to STREAM_SEEK_END for direct DVD reading can return 8 KB more, so we check last 16 KB.
  // And when we read last block, result read size can be smaller than required size.

  /*
  const size_t kBufSize = 1 << 14;
  Byte buf[kBufSize];
  size_t readSize = (fileSize < kBufSize) ? (size_t)fileSize : kBufSize;
  RINOK(_stream->Seek(fileSize - readSize, STREAM_SEEK_SET, NULL));
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

  const size_t kBufSize = 1 << 11;
  Byte buf[kBufSize];
  
  for (SecLogSize = 11;; SecLogSize -= 3)
  {
    if (SecLogSize < 8)
      return S_FALSE;
    UInt32 offset = (UInt32)256 << SecLogSize;
    if (offset >= fileSize)
      continue;
    RINOK(_stream->Seek(offset, STREAM_SEEK_SET, NULL));
    const size_t bufSize = (size_t)1 << SecLogSize;
    size_t readSize = bufSize;
    RINOK(ReadStream(_stream, buf, &readSize));
    if (readSize == bufSize)
    {
      CTag tag;
      if (tag.Parse(buf, readSize) == S_OK)
        if (tag.Id == DESC_TYPE_AnchorVolPtr)
          break;
    }
  }
  
  PhySize = (UInt32)(256 + 1) << SecLogSize;
  IsArc = true;

  CExtent extentVDS;
  extentVDS.Parse(buf + 16);
  {
    CExtent extentVDS2;
    extentVDS2.Parse(buf + 24);
    UpdatePhySize(((UInt64)extentVDS.Pos << SecLogSize) + extentVDS.Len);
    UpdatePhySize(((UInt64)extentVDS2.Pos << SecLogSize) + extentVDS2.Len);
  }

  for (UInt32 location = 0; ; location++)
  {
    const size_t bufSize = (size_t)1 << SecLogSize;
    if (((UInt64)(location + 1) << SecLogSize) > extentVDS.Len)
      return S_FALSE;

    UInt64 offs = (UInt64)(extentVDS.Pos + location) << SecLogSize;
    RINOK(_stream->Seek(offs, STREAM_SEEK_SET, NULL));
    HRESULT res = ReadStream_FALSE(_stream, buf, bufSize);
    if (res == S_FALSE && offs + bufSize > FileSize)
      UnexpectedEnd = true;
    RINOK(res);


    CTag tag;
    {
      const size_t pos = 0;
      RINOK(tag.Parse(buf + pos, bufSize - pos));
    }
    if (tag.Id == DESC_TYPE_Terminating)
      break;
    
    if (tag.Id == DESC_TYPE_Partition)
    {
      // Partition Descriptor
      // ECMA 167 3/10.5
      // UDF / 2.2.14

      if (Partitions.Size() >= kNumPartitionsMax)
        return S_FALSE;
      CPartition partition;
      // UInt32 volDescSeqNumer = Get32(buf + 16);
      // partition.Flags = Get16(buf + 20);
      partition.Number = Get16(buf + 22);
      // partition.ContentsId.Parse(buf + 24);
      
      // memcpy(partition.ContentsUse, buf + 56, sizeof(partition.ContentsUse));
      // ContentsUse is Partition Header Description.

      // partition.AccessType = Get32(buf + 184);
      partition.Pos = Get32(buf + 188);
      partition.Len = Get32(buf + 192);
      // partition.ImplId.Parse(buf + 196);
      // memcpy(partition.ImplUse, buf + 228, sizeof(partition.ImplUse));

      PRF(printf("\nPartition number = %2d   pos = %d   len = %d", partition.Number, partition.Pos, partition.Len));
      Partitions.Add(partition);
    }
    else if (tag.Id == DESC_TYPE_LogicalVol)
    {
      /* Logical Volume Descriptor
          ECMA 3/10.6
          UDF 2.60 2.2.4 */

      if (LogVols.Size() >= kNumLogVolumesMax)
        return S_FALSE;
      CLogVol vol;
      vol.Id.Parse(buf + 84);
      vol.BlockSize = Get32(buf + 212);
      // vol.DomainId.Parse(buf + 216);

      if (vol.BlockSize < 512 || vol.BlockSize > ((UInt32)1 << 30))
        return S_FALSE;
      
      // memcpy(vol.ContentsUse, buf + 248, sizeof(vol.ContentsUse));
      vol.FileSetLocation.Parse(buf + 248);
      /* the extent in which the first File Set Descriptor Sequence
         of the logical volume is recorded */

      // UInt32 mapTableLength = Get32(buf + 264);
      UInt32 numPartitionMaps = Get32(buf + 268);
      if (numPartitionMaps > kNumPartitionsMax)
        return S_FALSE;
      // vol.ImplId.Parse(buf + 272);
      // memcpy(vol.ImplUse, buf + 128, sizeof(vol.ImplUse));

      PRF(printf("\nLogicalVol numPartitionMaps = %2d", numPartitionMaps));
      size_t pos = 440;
      for (UInt32 i = 0; i < numPartitionMaps; i++)
      {
        if (pos + 2 > bufSize)
          return S_FALSE;
        CPartitionMap pm;
        pm.Type = buf[pos];
        // pm.Length = buf[pos + 1];
        Byte len = buf[pos + 1];

        if (pos + len > bufSize)
          return S_FALSE;
        
        // memcpy(pm.Data, buf + pos + 2, pm.Length - 2);
        if (pm.Type == 1)
        {
          if (len != 6) // < 6
            return S_FALSE;
          // pm.VolSeqNumber = Get16(buf + pos + 2);
          pm.PartitionNumber = Get16(buf + pos + 4);
          PRF(printf("\nPartitionMap type 1 PartitionNumber = %2d", pm.PartitionNumber));
        }
        else if (pm.Type == 2)
        {
          if (len != 64)
            return S_FALSE;
          /* ECMA 10.7.3 / Type 2 Partition Map
             62 bytes: Partition Identifier. */

          /* UDF 2.6
             2.2.8 Virtual Partition Map
             This is an extension of ECMA 167 to expand its scope to include
             sequentially written media (eg. CD-R).  This extension is for a
             Partition Map entry to describe a virtual space.   */

          // It's not implemented still.
          if (Get16(buf + pos + 2) != 0)
            return S_FALSE;
          // pm.VolSeqNumber = Get16(buf + pos + 36);
          pm.PartitionNumber = Get16(buf + pos + 38);
          PRF(printf("\nPartitionMap type 2 PartitionNumber = %2d", pm.PartitionNumber));
          // Unsupported = true;
          return S_FALSE;
        }
        else
          return S_FALSE;
        pos += len;
        vol.PartitionMaps.Add(pm);
      }
      LogVols.Add(vol);
    }
  }

  UInt64 totalSize = 0;

  unsigned volIndex;
  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
    CLogVol &vol = LogVols[volIndex];
    FOR_VECTOR (pmIndex, vol.PartitionMaps)
    {
      CPartitionMap &pm = vol.PartitionMaps[pmIndex];
      unsigned i;
      for (i = 0; i < Partitions.Size(); i++)
      {
        CPartition &part = Partitions[i];
        if (part.Number == pm.PartitionNumber)
        {
          if (part.VolIndex >= 0)
          {
            // it's for 2.60. Fix it
            if (part.VolIndex != (int)volIndex)
              return S_FALSE;
            // return S_FALSE;
          }
          pm.PartitionIndex = i;
          part.VolIndex = volIndex;

          totalSize += (UInt64)part.Len << SecLogSize;
          break;
        }
      }
      if (i == Partitions.Size())
        return S_FALSE;
    }
  }

  RINOK(_progress->SetTotal(totalSize));

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
      RINOK(Read(volIndex, nextExtent, buf2));
      const Byte *p = buf2;
      size_t size = nextExtent.GetLen();

      CTag tag;
      RINOK(tag.Parse(p, size));

      if (tag.Id == DESC_TYPE_ExtendedFile)
      {
        // ECMA 4 / 14.17
        // 2.60 ??
        return S_FALSE;
      }

      if (tag.Id != DESC_TYPE_FileSet)
        return S_FALSE;
      
      PRF(printf("\n FileSet", volIndex));
      CFileSet fs;
      fs.RecodringTime.Parse(p + 16);
      // fs.InterchangeLevel = Get16(p + 18);
      // fs.MaxInterchangeLevel = Get16(p + 20);
      // fs.FileSetNumber = Get32(p + 40);
      // fs.FileSetDescNumber = Get32(p + 44);
      
      // fs.Id.Parse(p + 304);
      // fs.CopyrightId.Parse(p + 336);
      // fs.AbstractId.Parse(p + 368);
      
      fs.RootDirICB.Parse(p + 400);
      // fs.DomainId.Parse(p + 416);
      
      // fs.SystemStreamDirICB.Parse(p + 464);
      
      vol.FileSets.Add(fs);

      // nextExtent.Parse(p + 448);
    }

    FOR_VECTOR (fsIndex, vol.FileSets)
    {
      CFileSet &fs = vol.FileSets[fsIndex];
      unsigned fileIndex = Files.Size();
      Files.AddNew();
      RINOK(ReadFileItem(volIndex, fsIndex, fs.RootDirICB, kNumRecursionLevelsMax));
      RINOK(FillRefs(fs, fileIndex, -1, kNumRecursionLevelsMax));
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
          UInt32 len = extent.GetLen();
          if (len == 0)
            continue;
          if (size < len)
            break;
          
          int partitionIndex = vol.PartitionMaps[extent.PartitionRef].PartitionIndex;
          UInt32 logBlockNumber = extent.Pos;
          const CPartition &partition = Partitions[partitionIndex];
          UInt64 offset = ((UInt64)partition.Pos << SecLogSize) +
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

    RINOK(_stream->Seek(PhySize, STREAM_SEEK_SET, NULL));

    // some UDF images contain ZEROs before "Anchor Volume Descriptor Pointer" at the end

    for (unsigned sec = 0; sec < 1024; sec++)
    {
      if (rem == 0)
        break;
      
      size_t readSize = secSize;
      if (readSize > rem)
        readSize = (size_t)rem;
      
      RINOK(ReadStream(_stream, buf, &readSize));
      
      if (readSize == 0)
        break;
      
      if (readSize == secSize && NoEndAnchor)
      {
        CTag tag;
        if (tag.Parse(buf, readSize) == S_OK &&
            tag.Id == DESC_TYPE_AnchorVolPtr)
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
  Items.Clear();
  Files.Clear();
  _fileNameLengthTotal = 0;
  _numRefs = 0;
  _numExtents = 0;
  _inlineExtentsSize = 0;
  _processedProgressBytes = 0;
}

UString CInArchive::GetComment() const
{
  UString res;
  FOR_VECTOR (i, LogVols)
  {
    if (i != 0)
      res.Add_Space();
    res += LogVols[i].GetName();
  }
  return res;
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

UString CInArchive::GetItemPath(int volIndex, int fsIndex, int refIndex,
    bool showVolName, bool showFsName) const
{
  // showVolName = true;
  const CLogVol &vol = LogVols[volIndex];
  const CFileSet &fs = vol.FileSets[fsIndex];

  UString name;

  for (;;)
  {
    const CRef &ref = fs.Refs[refIndex];
    refIndex = ref.Parent;
    if (refIndex < 0)
      break;
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
