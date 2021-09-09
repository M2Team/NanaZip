// FatHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/MyCom.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/DummyOutStream.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)

#define PRF(x) /* x */

namespace NArchive {
namespace NFat {

static const UInt32 kFatItemUsedByDirMask = (UInt32)1 << 31;

struct CHeader
{
  UInt32 NumSectors;
  UInt16 NumReservedSectors;
  Byte NumFats;
  UInt32 NumFatSectors;
  UInt32 RootDirSector;
  UInt32 NumRootDirSectors;
  UInt32 DataSector;

  UInt32 FatSize;
  UInt32 BadCluster;

  Byte NumFatBits;
  Byte SectorSizeLog;
  Byte SectorsPerClusterLog;
  Byte ClusterSizeLog;
  
  UInt16 SectorsPerTrack;
  UInt16 NumHeads;
  UInt32 NumHiddenSectors;

  bool VolFieldsDefined;
  
  UInt32 VolId;
  // Byte VolName[11];
  // Byte FileSys[8];

  // Byte OemName[5];
  Byte MediaType;

  // 32-bit FAT
  UInt16 Flags;
  UInt16 FsInfoSector;
  UInt32 RootCluster;

  bool IsFat32() const { return NumFatBits == 32; }
  UInt64 GetPhySize() const { return (UInt64)NumSectors << SectorSizeLog; }
  UInt32 SectorSize() const { return (UInt32)1 << SectorSizeLog; }
  UInt32 ClusterSize() const { return (UInt32)1 << ClusterSizeLog; }
  UInt32 ClusterToSector(UInt32 c) const { return DataSector + ((c - 2) << SectorsPerClusterLog); }
  UInt32 IsEoc(UInt32 c) const { return c > BadCluster; }
  UInt32 IsEocAndUnused(UInt32 c) const { return c > BadCluster && (c & kFatItemUsedByDirMask) == 0; }
  UInt32 IsValidCluster(UInt32 c) const { return c >= 2 && c < FatSize; }
  UInt32 SizeToSectors(UInt32 size) const { return (size + SectorSize() - 1) >> SectorSizeLog; }
  UInt32 CalcFatSizeInSectors() const { return SizeToSectors((FatSize * (NumFatBits / 4) + 1) / 2); }

  UInt32 GetFatSector() const
  {
    UInt32 index = (IsFat32() && (Flags & 0x80) != 0) ? (Flags & 0xF) : 0;
    if (index > NumFats)
      index = 0;
    return NumReservedSectors + index * NumFatSectors;
  }

  UInt64 GetFilePackSize(UInt32 unpackSize) const
  {
    UInt64 mask = ClusterSize() - 1;
    return (unpackSize + mask) & ~mask;
  }

  UInt32 GetNumClusters(UInt32 size) const
    { return (UInt32)(((UInt64)size + ClusterSize() - 1) >> ClusterSizeLog); }

  bool Parse(const Byte *p);
};

static int GetLog(UInt32 num)
{
  for (int i = 0; i < 31; i++)
    if (((UInt32)1 << i) == num)
      return i;
  return -1;
}

static const UInt32 kHeaderSize = 512;

API_FUNC_static_IsArc IsArc_Fat(const Byte *p, size_t size)
{
  if (size < kHeaderSize)
    return k_IsArc_Res_NEED_MORE;
  CHeader h;
  return h.Parse(p) ? k_IsArc_Res_YES : k_IsArc_Res_NO;
}
}

bool CHeader::Parse(const Byte *p)
{
  if (p[0x1FE] != 0x55 || p[0x1FF] != 0xAA)
    return false;

  int codeOffset = 0;
  switch (p[0])
  {
    case 0xE9: codeOffset = 3 + (Int16)Get16(p + 1); break;
    case 0xEB: if (p[2] != 0x90) return false; codeOffset = 2 + (signed char)p[1]; break;
    default: return false;
  }
  {
    {
      UInt32 val32 = Get16(p + 11);
      int s = GetLog(val32);
      if (s < 9 || s > 12)
        return false;
      SectorSizeLog = (Byte)s;
    }
    {
      UInt32 val32 = p[13];
      int s = GetLog(val32);
      if (s < 0)
        return false;
      SectorsPerClusterLog = (Byte)s;
    }
    ClusterSizeLog = (Byte)(SectorSizeLog + SectorsPerClusterLog);
    if (ClusterSizeLog > 24)
      return false;
  }

  NumReservedSectors = Get16(p + 14);
  if (NumReservedSectors == 0)
    return false;

  NumFats = p[16];
  if (NumFats < 1 || NumFats > 4)
    return false;

  // we also support images that contain 0 in offset field.
  bool isOkOffset = (codeOffset == 0)
      || (codeOffset == (p[0] == 0xEB ? 2 : 3));

  UInt16 numRootDirEntries = Get16(p + 17);
  if (numRootDirEntries == 0)
  {
    if (codeOffset < 90 && !isOkOffset)
      return false;
    NumFatBits = 32;
    NumRootDirSectors = 0;
  }
  else
  {
    // Some FAT12s don't contain VolFields
    if (codeOffset < 62 - 24 && !isOkOffset)
      return false;
    NumFatBits = 0;
    UInt32 mask = (1 << (SectorSizeLog - 5)) - 1;
    if ((numRootDirEntries & mask) != 0)
      return false;
    NumRootDirSectors = (numRootDirEntries + mask) >> (SectorSizeLog - 5);
  }

  NumSectors = Get16(p + 19);
  if (NumSectors == 0)
    NumSectors = Get32(p + 32);
  else if (IsFat32())
    return false;

  MediaType = p[21];
  NumFatSectors = Get16(p + 22);
  SectorsPerTrack = Get16(p + 24);
  NumHeads = Get16(p + 26);
  NumHiddenSectors = Get32(p + 28);

  // memcpy(OemName, p + 3, 5);

  int curOffset = 36;
  p += 36;
  if (IsFat32())
  {
    if (NumFatSectors != 0)
      return false;
    NumFatSectors = Get32(p);
    if (NumFatSectors >= (1 << 24))
      return false;

    Flags = Get16(p + 4);
    if (Get16(p + 6) != 0)
      return false;
    RootCluster = Get32(p + 8);
    FsInfoSector = Get16(p + 12);
    for (int i = 16; i < 28; i++)
      if (p[i] != 0)
        return false;
    p += 28;
    curOffset += 28;
  }

  // DriveNumber = p[0];
  VolFieldsDefined = false;
  if (codeOffset >= curOffset + 3)
  {
    VolFieldsDefined = (p[2] == 0x29); // ExtendedBootSig
    if (VolFieldsDefined)
    {
      if (codeOffset < curOffset + 26)
        return false;
      VolId = Get32(p + 3);
      // memcpy(VolName, p + 7, 11);
      // memcpy(FileSys, p + 18, 8);
    }
  }

  if (NumFatSectors == 0)
    return false;
  RootDirSector = NumReservedSectors + NumFatSectors * NumFats;
  DataSector = RootDirSector + NumRootDirSectors;
  if (NumSectors < DataSector)
    return false;
  UInt32 numDataSectors = NumSectors - DataSector;
  UInt32 numClusters = numDataSectors >> SectorsPerClusterLog;
  
  BadCluster = 0x0FFFFFF7;
  if (numClusters < 0xFFF5)
  {
    if (NumFatBits == 32)
      return false;
    NumFatBits = (Byte)(numClusters < 0xFF5 ? 12 : 16);
    BadCluster &= ((1 << NumFatBits) - 1);
  }
  else if (NumFatBits != 32)
    return false;

  FatSize = numClusters + 2;
  if (FatSize > BadCluster || CalcFatSizeInSectors() > NumFatSectors)
    return false;
  return true;
}

struct CItem
{
  UString UName;
  char DosName[11];
  Byte CTime2;
  UInt32 CTime;
  UInt32 MTime;
  UInt16 ADate;
  Byte Attrib;
  Byte Flags;
  UInt32 Size;
  UInt32 Cluster;
  Int32 Parent;

  // NT uses Flags to store Low Case status
  bool NameIsLow() const { return (Flags & 0x8) != 0; }
  bool ExtIsLow() const { return (Flags & 0x10) != 0; }
  bool IsDir() const { return (Attrib & 0x10) != 0; }
  UString GetShortName() const;
  UString GetName() const;
  UString GetVolName() const;
};

static unsigned CopyAndTrim(char *dest, const char *src, unsigned size, bool toLower)
{
  memcpy(dest, src, size);
  if (toLower)
  {
    for (unsigned i = 0; i < size; i++)
    {
      char c = dest[i];
      if (c >= 'A' && c <= 'Z')
        dest[i] = (char)(c + 0x20);
    }
  }
  
  for (unsigned i = size;;)
  {
    if (i == 0)
      return 0;
    if (dest[i - 1] != ' ')
      return i;
    i--;
  }
}

static UString FatStringToUnicode(const char *s)
{
  return MultiByteToUnicodeString(s, CP_OEMCP);
}

UString CItem::GetShortName() const
{
  char s[16];
  unsigned i = CopyAndTrim(s, DosName, 8, NameIsLow());
  s[i++] = '.';
  unsigned j = CopyAndTrim(s + i, DosName + 8, 3, ExtIsLow());
  if (j == 0)
    i--;
  s[i + j] = 0;
  return FatStringToUnicode(s);
}

UString CItem::GetName() const
{
  if (!UName.IsEmpty())
    return UName;
  return GetShortName();
}

UString CItem::GetVolName() const
{
  if (!UName.IsEmpty())
    return UName;
  char s[12];
  unsigned i = CopyAndTrim(s, DosName, 11, false);
  s[i] = 0;
  return FatStringToUnicode(s);
}

struct CDatabase
{
  CHeader Header;
  CObjectVector<CItem> Items;
  UInt32 *Fat;
  CMyComPtr<IInStream> InStream;
  IArchiveOpenCallback *OpenCallback;

  UInt32 NumFreeClusters;
  bool VolItemDefined;
  CItem VolItem;
  UInt32 NumDirClusters;
  CByteBuffer ByteBuf;
  UInt64 NumCurUsedBytes;

  UInt64 PhySize;

  CDatabase(): Fat(0) {}
  ~CDatabase() { ClearAndClose(); }

  void Clear();
  void ClearAndClose();
  HRESULT OpenProgressFat(bool changeTotal = true);
  HRESULT OpenProgress();

  UString GetItemPath(Int32 index) const;
  HRESULT Open();
  HRESULT ReadDir(Int32 parent, UInt32 cluster, unsigned level);

  UInt64 GetHeadersSize() const
  {
    return (UInt64)(Header.DataSector + (NumDirClusters << Header.SectorsPerClusterLog)) << Header.SectorSizeLog;
  }
  HRESULT SeekToSector(UInt32 sector);
  HRESULT SeekToCluster(UInt32 cluster) { return SeekToSector(Header.ClusterToSector(cluster)); }
};

HRESULT CDatabase::SeekToSector(UInt32 sector)
{
  return InStream->Seek((UInt64)sector << Header.SectorSizeLog, STREAM_SEEK_SET, NULL);
}

void CDatabase::Clear()
{
  PhySize = 0;
  VolItemDefined = false;
  NumDirClusters = 0;
  NumCurUsedBytes = 0;

  Items.Clear();
  delete []Fat;
  Fat = 0;
}

void CDatabase::ClearAndClose()
{
  Clear();
  InStream.Release();
}

HRESULT CDatabase::OpenProgressFat(bool changeTotal)
{
  if (!OpenCallback)
    return S_OK;
  if (changeTotal)
  {
    UInt64 numTotalBytes = (Header.CalcFatSizeInSectors() << Header.SectorSizeLog) +
        ((UInt64)(Header.FatSize - NumFreeClusters) << Header.ClusterSizeLog);
    RINOK(OpenCallback->SetTotal(NULL, &numTotalBytes));
  }
  return OpenCallback->SetCompleted(NULL, &NumCurUsedBytes);
}

HRESULT CDatabase::OpenProgress()
{
  if (!OpenCallback)
    return S_OK;
  UInt64 numItems = Items.Size();
  return OpenCallback->SetCompleted(&numItems, &NumCurUsedBytes);
}

UString CDatabase::GetItemPath(Int32 index) const
{
  const CItem *item = &Items[index];
  UString name = item->GetName();
  for (;;)
  {
    index = item->Parent;
    if (index < 0)
      return name;
    item = &Items[index];
    name.InsertAtFront(WCHAR_PATH_SEPARATOR);
    if (item->UName.IsEmpty())
      name.Insert(0, item->GetShortName());
    else
      name.Insert(0, item->UName);
  }
}

static wchar_t *AddSubStringToName(wchar_t *dest, const Byte *p, unsigned numChars)
{
  for (unsigned i = 0; i < numChars; i++)
  {
    wchar_t c = Get16(p + i * 2);
    if (c != 0 && c != 0xFFFF)
      *dest++ = c;
  }
  *dest = 0;
  return dest;
}

HRESULT CDatabase::ReadDir(Int32 parent, UInt32 cluster, unsigned level)
{
  unsigned startIndex = Items.Size();
  if (startIndex >= (1 << 30) || level > 256)
    return S_FALSE;

  UInt32 sectorIndex = 0;
  UInt32 blockSize = Header.ClusterSize();
  bool clusterMode = (Header.IsFat32() || parent >= 0);
  if (!clusterMode)
  {
    blockSize = Header.SectorSize();
    RINOK(SeekToSector(Header.RootDirSector));
  }

  ByteBuf.Alloc(blockSize);
  UString curName;
  int checkSum = -1;
  int numLongRecords = -1;
  
  for (UInt32 pos = blockSize;; pos += 32)
  {
    if (pos == blockSize)
    {
      pos = 0;

      if ((NumDirClusters & 0xFF) == 0)
      {
        RINOK(OpenProgress());
      }

      if (clusterMode)
      {
        if (Header.IsEoc(cluster))
          break;
        if (!Header.IsValidCluster(cluster))
          return S_FALSE;
        PRF(printf("\nCluster = %4X", cluster));
        RINOK(SeekToCluster(cluster));
        UInt32 newCluster = Fat[cluster];
        if ((newCluster & kFatItemUsedByDirMask) != 0)
          return S_FALSE;
        Fat[cluster] |= kFatItemUsedByDirMask;
        cluster = newCluster;
        NumDirClusters++;
        NumCurUsedBytes += Header.ClusterSize();
      }
      else if (sectorIndex++ >= Header.NumRootDirSectors)
        break;
      
      RINOK(ReadStream_FALSE(InStream, ByteBuf, blockSize));
    }
  
    const Byte *p = ByteBuf + pos;
    
    if (p[0] == 0)
    {
      /*
      // FreeDOS formats FAT partition with cluster chain longer than required.
      if (clusterMode && !Header.IsEoc(cluster))
        return S_FALSE;
      */
      break;
    }
    
    if (p[0] == 0xE5)
    {
      if (numLongRecords > 0)
        return S_FALSE;
      continue;
    }
    
    Byte attrib = p[11];
    if ((attrib & 0x3F) == 0xF)
    {
      if (p[0] > 0x7F || Get16(p + 26) != 0)
        return S_FALSE;
      int longIndex = p[0] & 0x3F;
      if (longIndex == 0)
        return S_FALSE;
      bool isLast = (p[0] & 0x40) != 0;
      if (numLongRecords < 0)
      {
        if (!isLast)
          return S_FALSE;
        numLongRecords = longIndex;
      }
      else if (isLast || numLongRecords != longIndex)
        return S_FALSE;

      numLongRecords--;
      
      if (p[12] == 0)
      {
        wchar_t nameBuf[14];
        wchar_t *dest;
        
        dest = AddSubStringToName(nameBuf, p + 1, 5);
        dest = AddSubStringToName(dest, p + 14, 6);
        AddSubStringToName(dest, p + 28, 2);
        curName = nameBuf + curName;
        if (isLast)
          checkSum = p[13];
        if (checkSum != p[13])
          return S_FALSE;
      }
    }
    else
    {
      if (numLongRecords > 0)
        return S_FALSE;
      CItem item;
      memcpy(item.DosName, p, 11);

      if (checkSum >= 0)
      {
        Byte sum = 0;
        for (unsigned i = 0; i < 11; i++)
          sum = (Byte)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + (Byte)item.DosName[i]);
        if (sum == checkSum)
          item.UName = curName;
      }

      if (item.DosName[0] == 5)
        item.DosName[0] = (char)(Byte)0xE5;
      item.Attrib = attrib;
      item.Flags = p[12];
      item.Size = Get32(p + 28);
      item.Cluster = Get16(p + 26);
      if (Header.NumFatBits > 16)
        item.Cluster |= ((UInt32)Get16(p + 20) << 16);
      else
      {
        // OS/2 and WinNT probably can store EA (extended atributes) in that field.
      }

      item.CTime = Get32(p + 14);
      item.CTime2 = p[13];
      item.ADate = Get16(p + 18);
      item.MTime = Get32(p + 22);
      item.Parent = parent;

      if (attrib == 8)
      {
        VolItem = item;
        VolItemDefined = true;
      }
      else
        if (memcmp(item.DosName, ".          ", 11) != 0 &&
            memcmp(item.DosName, "..         ", 11) != 0)
      {
        if (!item.IsDir())
          NumCurUsedBytes += Header.GetFilePackSize(item.Size);
        Items.Add(item);
        PRF(printf("\n%7d: %S", Items.Size(), GetItemPath(Items.Size() - 1)));
      }
      numLongRecords = -1;
      curName.Empty();
      checkSum = -1;
    }
  }

  unsigned finishIndex = Items.Size();
  for (unsigned i = startIndex; i < finishIndex; i++)
  {
    const CItem &item = Items[i];
    if (item.IsDir())
    {
      PRF(printf("\n%S", GetItemPath(i)));
      RINOK(CDatabase::ReadDir(i, item.Cluster, level + 1));
    }
  }
  return S_OK;
}

HRESULT CDatabase::Open()
{
  Clear();
  bool numFreeClustersDefined = false;
  {
    Byte buf[kHeaderSize];
    RINOK(ReadStream_FALSE(InStream, buf, kHeaderSize));
    if (!Header.Parse(buf))
      return S_FALSE;
    UInt64 fileSize;
    RINOK(InStream->Seek(0, STREAM_SEEK_END, &fileSize));

    /* we comment that check to support truncated images */
    /*
    if (fileSize < Header.GetPhySize())
      return S_FALSE;
    */

    if (Header.IsFat32())
    {
      SeekToSector(Header.FsInfoSector);
      RINOK(ReadStream_FALSE(InStream, buf, kHeaderSize));
      if (buf[0x1FE] != 0x55 || buf[0x1FF] != 0xAA)
        return S_FALSE;
      if (Get32(buf) == 0x41615252 && Get32(buf + 484) == 0x61417272)
      {
        NumFreeClusters = Get32(buf + 488);
        numFreeClustersDefined = (NumFreeClusters <= Header.FatSize);
      }
    }
  }

  // numFreeClustersDefined = false; // to recalculate NumFreeClusters
  if (!numFreeClustersDefined)
    NumFreeClusters = 0;

  CByteBuffer byteBuf;
  Fat = new UInt32[Header.FatSize];

  RINOK(OpenProgressFat());
  RINOK(SeekToSector(Header.GetFatSector()));
  if (Header.NumFatBits == 32)
  {
    const UInt32 kBufSize = (1 << 15);
    byteBuf.Alloc(kBufSize);
    for (UInt32 i = 0; i < Header.FatSize;)
    {
      UInt32 size = Header.FatSize - i;
      const UInt32 kBufSize32 = kBufSize / 4;
      if (size > kBufSize32)
        size = kBufSize32;
      UInt32 readSize = Header.SizeToSectors(size * 4) << Header.SectorSizeLog;
      RINOK(ReadStream_FALSE(InStream, byteBuf, readSize));
      NumCurUsedBytes += readSize;

      const UInt32 *src = (const UInt32 *)(const void *)(const Byte *)byteBuf;
      UInt32 *dest = Fat + i;
      if (numFreeClustersDefined)
        for (UInt32 j = 0; j < size; j++)
          dest[j] = Get32(src + j) & 0x0FFFFFFF;
      else
      {
        UInt32 numFreeClusters = 0;
        for (UInt32 j = 0; j < size; j++)
        {
          UInt32 v = Get32(src + j) & 0x0FFFFFFF;
          numFreeClusters += (UInt32)(v - 1) >> 31;
          dest[j] = v;
        }
        NumFreeClusters += numFreeClusters;
      }
      i += size;
      if ((i & 0xFFFFF) == 0)
      {
        RINOK(OpenProgressFat(!numFreeClustersDefined));
      }
    }
  }
  else
  {
    const UInt32 kBufSize = (UInt32)Header.CalcFatSizeInSectors() << Header.SectorSizeLog;
    NumCurUsedBytes += kBufSize;
    byteBuf.Alloc(kBufSize);
    Byte *p = byteBuf;
    RINOK(ReadStream_FALSE(InStream, p, kBufSize));
    UInt32 fatSize = Header.FatSize;
    UInt32 *fat = &Fat[0];
    if (Header.NumFatBits == 16)
      for (UInt32 j = 0; j < fatSize; j++)
        fat[j] = Get16(p + j * 2);
    else
      for (UInt32 j = 0; j < fatSize; j++)
        fat[j] = (Get16(p + j * 3 / 2) >> ((j & 1) << 2)) & 0xFFF;

    if (!numFreeClustersDefined)
    {
      UInt32 numFreeClusters = 0;
      for (UInt32 i = 0; i < fatSize; i++)
        numFreeClusters += (UInt32)(fat[i] - 1) >> 31;
      NumFreeClusters = numFreeClusters;
    }
  }

  RINOK(OpenProgressFat());

  if ((Fat[0] & 0xFF) != Header.MediaType)
  {
    // that case can mean error in FAT,
    // but xdf file: (MediaType == 0xF0 && Fat[0] == 0xFF9)
    // 19.00: so we use non-strict check
    if ((Fat[0] & 0xFF) < 0xF0)
      return S_FALSE;
  }

  RINOK(ReadDir(-1, Header.RootCluster, 0));

  PhySize = Header.GetPhySize();
  return S_OK;
}

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp,
  CDatabase
{
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  *stream = 0;
  const CItem &item = Items[index];
  CClusterInStream *streamSpec = new CClusterInStream;
  CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
  streamSpec->Stream = InStream;
  streamSpec->StartOffset = Header.DataSector << Header.SectorSizeLog;
  streamSpec->BlockSizeLog = Header.ClusterSizeLog;
  streamSpec->Size = item.Size;

  UInt32 numClusters = Header.GetNumClusters(item.Size);
  streamSpec->Vector.ClearAndReserve(numClusters);
  UInt32 cluster = item.Cluster;
  UInt32 size = item.Size;

  if (size == 0)
  {
    if (cluster != 0)
      return S_FALSE;
  }
  else
  {
    UInt32 clusterSize = Header.ClusterSize();
    for (;; size -= clusterSize)
    {
      if (!Header.IsValidCluster(cluster))
        return S_FALSE;
      streamSpec->Vector.AddInReserved(cluster - 2);
      cluster = Fat[cluster];
      if (size <= clusterSize)
        break;
    }
    if (!Header.IsEocAndUnused(cluster))
      return S_FALSE;
  }
  RINOK(streamSpec->InitAndSeek());
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

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
  kpidShortName
};

enum
{
  kpidNumFats = kpidUserDefined
  // kpidOemName,
  // kpidVolName,
  // kpidFileSysType
};

static const CStatProp kArcProps[] =
{
  { NULL, kpidFileSystem, VT_BSTR},
  { NULL, kpidClusterSize, VT_UI4},
  { NULL, kpidFreeSpace, VT_UI8},
  { NULL, kpidHeadersSize, VT_UI8},
  { NULL, kpidMTime, VT_FILETIME},
  { NULL, kpidVolumeName, VT_BSTR},

  { "FATs", kpidNumFats, VT_UI4},
  { NULL, kpidSectorSize, VT_UI4},
  { NULL, kpidId, VT_UI4},
  // { "OEM Name", kpidOemName, VT_BSTR},
  // { "Volume Name", kpidVolName, VT_BSTR},
  // { "File System Type", kpidFileSysType, VT_BSTR}
  // { NULL, kpidSectorsPerTrack, VT_UI4},
  // { NULL, kpidNumHeads, VT_UI4},
  // { NULL, kpidHiddenSectors, VT_UI4}
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME

static void FatTimeToProp(UInt32 dosTime, UInt32 ms10, NWindows::NCOM::CPropVariant &prop)
{
  FILETIME localFileTime, utc;
  if (NWindows::NTime::DosTimeToFileTime(dosTime, localFileTime))
    if (LocalFileTimeToFileTime(&localFileTime, &utc))
    {
      UInt64 t64 = (((UInt64)utc.dwHighDateTime) << 32) + utc.dwLowDateTime;
      t64 += ms10 * 100000;
      utc.dwLowDateTime = (DWORD)t64;
      utc.dwHighDateTime = (DWORD)(t64 >> 32);
      prop = utc;
    }
}

/*
static void StringToProp(const Byte *src, unsigned size, NWindows::NCOM::CPropVariant &prop)
{
  char dest[32];
  memcpy(dest, src, size);
  dest[size] = 0;
  prop = FatStringToUnicode(dest);
}

#define STRING_TO_PROP(s, p) StringToProp(s, sizeof(s), prop)
*/

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidFileSystem:
    {
      char s[16];
      s[0] = 'F';
      s[1] = 'A';
      s[2] = 'T';
      ConvertUInt32ToString(Header.NumFatBits, s + 3);
      prop = s;
      break;
    }
    case kpidClusterSize: prop = Header.ClusterSize(); break;
    case kpidPhySize: prop = PhySize; break;
    case kpidFreeSpace: prop = (UInt64)NumFreeClusters << Header.ClusterSizeLog; break;
    case kpidHeadersSize: prop = GetHeadersSize(); break;
    case kpidMTime: if (VolItemDefined) FatTimeToProp(VolItem.MTime, 0, prop); break;
    case kpidShortComment:
    case kpidVolumeName: if (VolItemDefined) prop = VolItem.GetVolName(); break;
    case kpidNumFats: if (Header.NumFats != 2) prop = Header.NumFats; break;
    case kpidSectorSize: prop = (UInt32)1 << Header.SectorSizeLog; break;
    // case kpidSectorsPerTrack: prop = Header.SectorsPerTrack; break;
    // case kpidNumHeads: prop = Header.NumHeads; break;
    // case kpidOemName: STRING_TO_PROP(Header.OemName, prop); break;
    case kpidId: if (Header.VolFieldsDefined) prop = Header.VolId; break;
    // case kpidVolName: if (Header.VolFieldsDefined) STRING_TO_PROP(Header.VolName, prop); break;
    // case kpidFileSysType: if (Header.VolFieldsDefined) STRING_TO_PROP(Header.FileSys, prop); break;
    // case kpidHiddenSectors: prop = Header.NumHiddenSectors; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CItem &item = Items[index];
  switch (propID)
  {
    case kpidPath: prop = GetItemPath(index); break;
    case kpidShortName: prop = item.GetShortName(); break;
    case kpidIsDir: prop = item.IsDir(); break;
    case kpidMTime: FatTimeToProp(item.MTime, 0, prop); break;
    case kpidCTime: FatTimeToProp(item.CTime, item.CTime2, prop); break;
    case kpidATime: FatTimeToProp(((UInt32)item.ADate << 16), 0, prop); break;
    case kpidAttrib: prop = (UInt32)item.Attrib; break;
    case kpidSize: if (!item.IsDir()) prop = item.Size; break;
    case kpidPackSize: if (!item.IsDir()) prop = Header.GetFilePackSize(item.Size); break;
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
    const CItem &item = Items[allFilesMode ? i : indices[i]];
    if (!item.IsDir())
      totalSize += item.Size;
  }
  RINOK(extractCallback->SetTotal(totalSize));

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
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
    Int32 index = allFilesMode ? i : indices[i];
    const CItem &item = Items[index];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    if (item.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode));
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }

    totalPackSize += Header.GetFilePackSize(item.Size);
    totalSize += item.Size;

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));

    outStreamSpec->SetStream(realOutStream);
    realOutStream.Release();
    outStreamSpec->Init();

    int res = NExtract::NOperationResult::kDataError;
    CMyComPtr<ISequentialInStream> inStream;
    HRESULT hres = GetStream(index, &inStream);
    if (hres != S_FALSE)
    {
      RINOK(hres);
      if (inStream)
      {
        RINOK(copyCoder->Code(inStream, outStream, NULL, NULL, progress));
        if (copyCoderSpec->TotalSize == item.Size)
          res = NExtract::NOperationResult::kOK;
      }
    }
    outStreamSpec->ReleaseStream();
    RINOK(extractCallback->SetOperationResult(res));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = Items.Size();
  return S_OK;
}

static const Byte k_Signature[] = { 0x55, 0xAA };

REGISTER_ARC_I(
  "FAT", "fat img", 0, 0xDA,
  k_Signature,
  0x1FE,
  0,
  IsArc_Fat)

}}
