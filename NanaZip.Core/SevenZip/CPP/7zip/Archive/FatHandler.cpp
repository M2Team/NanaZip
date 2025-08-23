// FatHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/MyBuffer2.h"
#include "../../Common/MyCom.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/ItemNameUtils.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get16a(p) GetUi16a(p)
#define Get32a(p) GetUi32a(p)

#if 0
#include <stdio.h>
#define PRF(x) x
#else
#define PRF(x)
#endif

namespace NArchive {
namespace NFat {

static const UInt32 kFatItemUsedByDirMask = (UInt32)1 << 31;

struct CHeader
{
  Byte NumFatBits;
  Byte SectorSizeLog;
  Byte SectorsPerClusterLog;
  Byte ClusterSizeLog;
  Byte NumFats;
  Byte MediaType;

  bool VolFieldsDefined;
  bool HeadersWarning;

  UInt32 FatSize;
  UInt32 BadCluster;

  UInt16 NumReservedSectors;
  UInt32 NumSectors;
  UInt32 NumFatSectors;
  UInt32 RootDirSector;
  UInt32 NumRootDirSectors;
  UInt32 DataSector;

  UInt16 SectorsPerTrack;
  UInt16 NumHeads;
  UInt32 NumHiddenSectors;
  
  UInt32 VolId;
  // Byte VolName[11];
  // Byte FileSys[8];
  // Byte OemName[5];

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


static const unsigned kHeaderSize = 512;

API_FUNC_IsArc IsArc_Fat(const Byte *p, size_t size);
API_FUNC_IsArc IsArc_Fat(const Byte *p, size_t size)
{
  if (size < kHeaderSize)
    return k_IsArc_Res_NEED_MORE;
  CHeader h;
  return h.Parse(p) ? k_IsArc_Res_YES : k_IsArc_Res_NO;
}

bool CHeader::Parse(const Byte *p)
{
  if (Get16(p + 0x1FE) != 0xAA55)
    return false;

  HeadersWarning = false;

  int codeOffset = 0;
  switch (p[0])
  {
    case 0xE9: codeOffset = 3 + (Int16)Get16(p + 1); break;
    case 0xEB: if (p[2] != 0x90) return false; codeOffset = 2 + (signed char)p[1]; break;
    default: return false;
  }
  {
    {
      const unsigned num = Get16(p + 11);
      unsigned i = 9;
      unsigned m = 1 << i;
      for (;;)
      {
        if (m == num)
          break;
        m <<= 1;
        if (++i > 12)
          return false;
      }
      SectorSizeLog = (Byte)i;
    }
    {
      const unsigned num = p[13];
      unsigned i = 0;
      unsigned m = 1 << i;
      for (;;)
      {
        if (m == num)
          break;
        m <<= 1;
        if (++i > 7)
          return false;
      }
      SectorsPerClusterLog = (Byte)i;
      i += SectorSizeLog;
      ClusterSizeLog = (Byte)i;
      // (2^15 = 32 KB is safe cluster size that is suported by all system.
      // (2^16 = 64 KB is supported by some systems
      // (128 KB / 256 KB) can be created by some tools, but it is not supported by many tools.
      if (i > 18) // 256 KB
        return false;
    }
  }

  NumReservedSectors = Get16(p + 14);
  if (NumReservedSectors == 0)
    return false;

  NumFats = p[16];
  if (NumFats < 1 || NumFats > 4)
    return false;

  // we also support images that contain 0 in offset field.
  const bool isOkOffset = (codeOffset == 0)
      || (codeOffset == (p[0] == 0xEB ? 2 : 3));

  const unsigned numRootDirEntries = Get16(p + 17);
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
    const unsigned mask = (1u << (SectorSizeLog - 5)) - 1;
    if (numRootDirEntries & mask)
      return false;
    NumRootDirSectors = (numRootDirEntries /* + mask */) >> (SectorSizeLog - 5);
  }

  NumSectors = Get16(p + 19);
  if (NumSectors == 0)
    NumSectors = Get32(p + 32);
  /*
  // mkfs.fat could create fat32 image with 16-bit number of sectors.
  // v23: we allow 16-bit value for number of sectors in fat32.
  else if (IsFat32())
    return false;
  */
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
    for (unsigned i = 16; i < 28; i++)
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
  const UInt32 numDataSectors = NumSectors - DataSector;
  const UInt32 numClusters = numDataSectors >> SectorsPerClusterLog;
  
  BadCluster = 0x0FFFFFF7;
  // v23: we support unusual (< 0xFFF5) numClusters values in fat32 systems
  if (NumFatBits != 32)
  {
    if (numClusters >= 0xFFF5)
      return false;
    NumFatBits = (Byte)(numClusters < 0xFF5 ? 12 : 16);
    BadCluster &= (((UInt32)1 << NumFatBits) - 1);
  }

  FatSize = numClusters + 2;
  if (FatSize > BadCluster)
    return false;
  if (CalcFatSizeInSectors() > NumFatSectors)
  {
    /* some third-party program can create such FAT image, where
       size of FAT table (NumFatSectors from headers) is smaller than
       required value that is calculated from calculated (FatSize) value.
       Another FAT unpackers probably ignore that error.
       v23.02: we also ignore that error, and
       we recalculate (FatSize) value from (NumFatSectors).
       New (FatSize) will be smaller than original "full" (FatSize) value.
       So we will have some unused clusters at the end of archive.
    */
    FatSize = (UInt32)(((UInt64)NumFatSectors << (3 + SectorSizeLog)) / NumFatBits);
    HeadersWarning = true;
  }
  return true;
}



class CItem
{
  Z7_CLASS_NO_COPY(CItem)
public:
  UInt32 Size;
  Byte Attrib;
  Byte CTime2;
  UInt16 ADate;
  CByteBuffer LongName; // if LongName.Size() == 0 : no long name
                        // if LongName.Size() != 0 : it's NULL terminated UTF16-LE string.
  char DosName[11];
  Byte Flags;
  UInt32 MTime;
  UInt32 CTime;
  UInt32 Cluster;
  Int32 Parent;

  CItem() {}

  // NT uses Flags to store Low Case status
  bool NameIsLow() const { return (Flags & 0x8) != 0; }
  bool ExtIsLow() const { return (Flags & 0x10) != 0; }
  bool IsDir() const { return (Attrib & 0x10) != 0; }
  void GetShortName(UString &dest) const;
  void GetName(UString &name) const;
};


static char *CopyAndTrim(char *dest, const char *src,
    unsigned size, unsigned toLower)
{
  do
  {
    if (src[(size_t)size - 1] != ' ')
    {
      const unsigned range = toLower ? 'Z' - 'A' + 1 : 0;
      do
      {
        unsigned c = (Byte)*src++;
        if ((unsigned)(c - 'A') < range)
          c += 0x20;
        *dest++ = (char)c;
      }
      while (--size);
      break;
    }
  }
  while (--size);
  *dest = 0;
  return dest;
}


static void FatStringToUnicode(UString &dest, const char *s)
{
  MultiByteToUnicodeString2(dest, AString(s), CP_OEMCP);
}

void CItem::GetShortName(UString &shortName) const
{
  char s[16];
  char *dest = CopyAndTrim(s, DosName, 8, NameIsLow());
  *dest++ = '.';
  char *dest2 = CopyAndTrim(dest, DosName + 8, 3, ExtIsLow());
  if (dest == dest2)
    dest[-1] = 0;
  FatStringToUnicode(shortName, s);
}



// numWords != 0
static unsigned ParseLongName(UInt16 *buf, unsigned numWords)
{
  unsigned i;
  for (i = 0; i < numWords; i++)
  {
    const unsigned c = buf[i];
    if (c == 0)
      break;
    if (c == 0xFFFF)
      return 0;
  }
  if (i == 0)
    return 0;
  buf[i] = 0;
  numWords -= i;
  i++;
  if (numWords > 1)
  {
    numWords--;
    buf += i;
    do
      if (*buf++ != 0xFFFF)
        return 0;
    while (--numWords);
  }
  return i; // it includes NULL terminator
}


void CItem::GetName(UString &name) const
{
  if (LongName.Size() >= 2)
  {
    const Byte * const p = LongName;
    const unsigned numWords = ((unsigned)LongName.Size() - 2) / 2;
    wchar_t *dest = name.GetBuf(numWords);
    for (unsigned i = 0; i < numWords; i++)
      dest[i] = (wchar_t)Get16(p + (size_t)i * 2);
    name.ReleaseBuf_SetEnd(numWords);
  }
  else
    GetShortName(name);
  if (name.IsEmpty()) // it's unexpected
    name = '_';
  NItemName::NormalizeSlashes_in_FileName_for_OsPath(name);
}


static void GetVolName(const char dosName[11], NWindows::NCOM::CPropVariant &prop)
{
  char s[12];
  CopyAndTrim(s, dosName, 11, false);
  UString u;
  FatStringToUnicode(u, AString(s));
  prop = u;
}


struct CDatabase
{
  CObjectVector<CItem> Items;
  UInt32 *Fat;
  CHeader Header;
  CMyComPtr<IInStream> InStream;
  IArchiveOpenCallback *OpenCallback;
  CAlignedBuffer ByteBuf;
  CByteBuffer LfnBuf;

  UInt32 NumFreeClusters;
  UInt32 NumDirClusters;
  UInt64 NumCurUsedBytes;
  UInt64 PhySize;

  UInt32 Vol_MTime;
  char VolLabel[11];
  bool VolItem_Defined;

  CDatabase(): Fat(NULL) {}
  ~CDatabase() { ClearAndClose(); }

  void Clear();
  void ClearAndClose();
  HRESULT OpenProgressFat(bool changeTotal = true);
  HRESULT OpenProgress();

  void GetItemPath(UInt32 index, UString &s) const;
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
  return InStream_SeekSet(InStream, (UInt64)sector << Header.SectorSizeLog);
}

void CDatabase::Clear()
{
  PhySize = 0;
  VolItem_Defined = false;
  NumDirClusters = 0;
  NumCurUsedBytes = 0;

  Items.Clear();
  delete []Fat;
  Fat = NULL;
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
    const UInt64 numTotalBytes = (Header.CalcFatSizeInSectors() << Header.SectorSizeLog) +
        ((UInt64)(Header.FatSize - NumFreeClusters) << Header.ClusterSizeLog);
    RINOK(OpenCallback->SetTotal(NULL, &numTotalBytes))
  }
  return OpenCallback->SetCompleted(NULL, &NumCurUsedBytes);
}

HRESULT CDatabase::OpenProgress()
{
  if (!OpenCallback)
    return S_OK;
  const UInt64 numItems = Items.Size();
  return OpenCallback->SetCompleted(&numItems, &NumCurUsedBytes);
}

void CDatabase::GetItemPath(UInt32 index, UString &s) const
{
  UString name;
  for (;;)
  {
    const CItem &item = Items[index];
    item.GetName(name);
    if (item.Parent >= 0)
      name.InsertAtFront(WCHAR_PATH_SEPARATOR);
    s.Insert(0, name);
    index = (UInt32)item.Parent;
    if (item.Parent < 0)
      break;
  }
}


HRESULT CDatabase::ReadDir(Int32 parent, UInt32 cluster, unsigned level)
{
  const unsigned startIndex = Items.Size();
  if (startIndex >= (1 << 30) || level > 256)
    return S_FALSE;

  UInt32 blockSize = Header.ClusterSize();
  const bool clusterMode = (Header.IsFat32() || parent >= 0);
  if (!clusterMode)
  {
    blockSize = Header.SectorSize();
    RINOK(SeekToSector(Header.RootDirSector))
  }

  ByteBuf.Alloc(blockSize);

  const unsigned k_NumLfnRecords_MAX = 20; // 260 symbols limit (strict limit)
  // const unsigned k_NumLfnRecords_MAX = 0x40 - 1; // 1260 symbols limit (relaxed limit)
  const unsigned k_NumLfnBytes_in_Record = 13 * 2;
  // we reserve 2 additional bytes for NULL terminator
  LfnBuf.Alloc(k_NumLfnRecords_MAX * k_NumLfnBytes_in_Record + 2 * 1);
  
  UInt32 curDirBytes_read = 0;
  UInt32 sectorIndex = 0;
  unsigned num_lfn_records = 0;
  unsigned lfn_RecordIndex = 0;
  int checkSum = -1;
  bool is_record_error = false;

  for (UInt32 pos = blockSize;; pos += 32)
  {
    if (pos == blockSize)
    {
      pos = 0;

      if (clusterMode)
      {
        if (Header.IsEoc(cluster))
          break;
        if (!Header.IsValidCluster(cluster))
          return S_FALSE;
        PRF(printf("\nCluster = %4X", cluster));
        RINOK(SeekToCluster(cluster))
        const UInt32 newCluster = Fat[cluster];
        if (newCluster & kFatItemUsedByDirMask)
          return S_FALSE;
        Fat[cluster] |= kFatItemUsedByDirMask;
        cluster = newCluster;
        NumDirClusters++;
        if ((NumDirClusters & 0xFF) == 0)
        {
          RINOK(OpenProgress())
        }
        NumCurUsedBytes += Header.ClusterSize();
      }
      else if (sectorIndex++ >= Header.NumRootDirSectors)
        break;
      
      // if (curDirBytes_read > (1u << 28)) // do we need some relaxed limit for non-MS FATs?
      if (curDirBytes_read >= (1u << 21)) // 2MB limit from FAT specification.
        return S_FALSE;
      RINOK(ReadStream_FALSE(InStream, ByteBuf, blockSize))
      curDirBytes_read += blockSize;
    }
  
    if (is_record_error)
    {
      Header.HeadersWarning = true;
      num_lfn_records = 0;
      lfn_RecordIndex = 0;
      checkSum = -1;
    }

    const Byte * const p = ByteBuf + pos;
   
    if (p[0] == 0)
    {
      /*
      // FreeDOS formats FAT partition with cluster chain longer than required.
      if (clusterMode && !Header.IsEoc(cluster))
        return S_FALSE;
      */
      break;
    }

    is_record_error = true;

    if (p[0] == 0xE5)
    {
      // deleted entry
      if (num_lfn_records == 0)
        is_record_error = false;
      continue;
    }
    // else
    {
      const Byte attrib = p[11];
      // maybe we can use more strick check : (attrib == 0xF) ?
      if ((attrib & 0x3F) == 0xF)
      {
        // long file name (LFN) entry
        const unsigned longIndex = p[0] & 0x3F;
        if (longIndex == 0
            || longIndex > k_NumLfnRecords_MAX
            || p[0] > 0x7F
            || Get16a(p + 26) != 0 // LDIR_FstClusLO
            )
        {
          return S_FALSE;
          // break;
        }
        const bool isLast = (p[0] & 0x40) != 0;
        if (num_lfn_records == 0)
        {
          if (!isLast)
            continue; // orphan
          num_lfn_records = longIndex;
        }
        else if (isLast || longIndex != lfn_RecordIndex)
        {
          return S_FALSE;
          // break;
        }
        
        lfn_RecordIndex = longIndex - 1;
        
        if (p[12] == 0)
        {
          Byte * const dest = LfnBuf + k_NumLfnBytes_in_Record * lfn_RecordIndex;
          memcpy(dest,          p +  1, 5 * 2);
          memcpy(dest +  5 * 2, p + 14, 6 * 2);
          memcpy(dest + 11 * 2, p + 28, 2 * 2);
          if (isLast)
            checkSum = p[13];
          if (checkSum == p[13])
            is_record_error = false;
          // else return S_FALSE;
          continue;
        }
        // else
        checkSum = -1; // we will ignore LfnBuf in this case
        continue;
      }
      
      if (lfn_RecordIndex)
      {
        Header.HeadersWarning = true;
        // return S_FALSE;
      }
      // lfn_RecordIndex = 0;

      const unsigned type_in_attrib = attrib & 0x18;
      if (type_in_attrib == 0x18)
      {
        // invalid directory record (both flags are set: dir_flag and volume_flag)
        return S_FALSE;
        // break;
        // continue;
      }
      if (type_in_attrib == 8) // volume_flag
      {
        if (!VolItem_Defined && level == 0)
        {
          VolItem_Defined = true;
          memcpy(VolLabel, p, 11);
          Vol_MTime = Get32(p + 22);
          is_record_error = false;
        }
      }
      else if (memcmp(p, ".          ", 11) == 0
            || memcmp(p, "..         ", 11) == 0)
      {
        if (num_lfn_records == 0 && type_in_attrib == 0x10) // dir_flag
          is_record_error = false;
      }
      else
      {
        CItem &item = Items.AddNew();
        memcpy(item.DosName, p, 11);
        if (item.DosName[0] == 5)
          item.DosName[0] = (char)(Byte)0xE5; // 0xE5 is valid KANJI lead byte value.
        item.Attrib = attrib;
        item.Flags = p[12];
        item.Size = Get32a(p + 28);
        item.Cluster = Get16a(p + 26);
        if (Header.NumFatBits > 16)
          item.Cluster |= ((UInt32)Get16a(p + 20) << 16);
        else
        {
          // OS/2 and WinNT probably can store EA (extended atributes) in that field.
        }
        item.CTime = Get32(p + 14);
        item.CTime2 = p[13];
        item.ADate = Get16a(p + 18);
        item.MTime = Get32(p + 22);
        item.Parent = parent;
        {
          if (!item.IsDir())
            NumCurUsedBytes += Header.GetFilePackSize(item.Size);
          // PRF(printf("\n%7d: %S", Items.Size(), GetItemPath(Items.Size() - 1)));
          PRF(printf("\n%7d" /* ": %S" */, Items.Size() /* , item.GetShortName() */ );)
        }
        if (num_lfn_records == 0)
          is_record_error = false;
        else if (checkSum >= 0 && lfn_RecordIndex == 0)
        {
          Byte sum = 0;
          for (unsigned i = 0; i < 11; i++)
            sum = (Byte)((sum << 7) + (sum >> 1) + (Byte)item.DosName[i]);
          if (sum == checkSum)
          {
            const unsigned numWords = ParseLongName((UInt16 *)(void *)(Byte *)LfnBuf,
                num_lfn_records * k_NumLfnBytes_in_Record / 2);
            if (numWords > 1)
            {
              // numWords includes NULL terminator
              item.LongName.CopyFrom(LfnBuf, numWords * 2);
              is_record_error = false;
            }
          }
        }
        
        if (
            // item.LongName.Size() < 20 ||  // for debug
            item.LongName.Size() <= 2 * 1
            && memcmp(p, "           ", 11) == 0)
        {
          char s[16 + 16];
          const size_t numChars = (size_t)(ConvertUInt32ToString(
              Items.Size() - 1 - startIndex,
              MyStpCpy(s, "[NONAME]-")) - s) + 1;
          item.LongName.Alloc(numChars * 2);
          for (size_t i = 0; i < numChars; i++)
          {
            SetUi16a(item.LongName + i * 2, (Byte)s[i])
          }
          Header.HeadersWarning = true;
        }
      }
      num_lfn_records = 0;
    }
  }

  if (is_record_error)
    Header.HeadersWarning = true;

  const unsigned finishIndex = Items.Size();
  for (unsigned i = startIndex; i < finishIndex; i++)
  {
    const CItem &item = Items[i];
    if (item.IsDir())
    {
      PRF(printf("\n---- %c%c%c%c%c", item.DosName[0], item.DosName[1], item.DosName[2], item.DosName[3], item.DosName[4]));
      RINOK(ReadDir((int)i, item.Cluster, level + 1))
    }
  }
  return S_OK;
}



HRESULT CDatabase::Open()
{
  Clear();
  bool numFreeClusters_Defined = false;
  {
    UInt32 buf32[kHeaderSize / 4];
    RINOK(ReadStream_FALSE(InStream, buf32, kHeaderSize))
    if (!Header.Parse((Byte *)(void *)buf32))
      return S_FALSE;
    UInt64 fileSize;
    RINOK(InStream_GetSize_SeekToEnd(InStream, fileSize))

    /* we comment that check to support truncated images */
    /*
    if (fileSize < Header.GetPhySize())
      return S_FALSE;
    */

    if (Header.IsFat32())
    {
      if (((UInt32)Header.FsInfoSector << Header.SectorSizeLog) + kHeaderSize <= fileSize
          && SeekToSector(Header.FsInfoSector) == S_OK
          && ReadStream_FALSE(InStream, buf32, kHeaderSize) == S_OK
          && 0xaa550000 == Get32a(buf32 + 508 / 4)
          && 0x41615252 == Get32a(buf32)
          && 0x61417272 == Get32a(buf32 + 484 / 4))
      {
        NumFreeClusters = Get32a(buf32 + 488 / 4);
        numFreeClusters_Defined = (NumFreeClusters <= Header.FatSize);
      }
      else
        Header.HeadersWarning = true;
    }
  }

  // numFreeClusters_Defined = false; // to recalculate NumFreeClusters
  if (!numFreeClusters_Defined)
    NumFreeClusters = 0;

  CByteBuffer byteBuf;
  Fat = new UInt32[Header.FatSize];

  RINOK(OpenProgressFat())
  RINOK(SeekToSector(Header.GetFatSector()))
  if (Header.NumFatBits == 32)
  {
    const UInt32 kBufSize = 1 << 15;
    byteBuf.Alloc(kBufSize);
    for (UInt32 i = 0;;)
    {
      UInt32 size = Header.FatSize - i;
      if (size == 0)
        break;
      const UInt32 kBufSize32 = kBufSize / 4;
      if (size > kBufSize32)
        size = kBufSize32;
      const UInt32 readSize = Header.SizeToSectors(size * 4) << Header.SectorSizeLog;
      RINOK(ReadStream_FALSE(InStream, byteBuf, readSize))
      NumCurUsedBytes += readSize;

      const UInt32 *src = (const UInt32 *)(const void *)(const Byte *)byteBuf;
      UInt32 *dest = Fat + i;
      const UInt32 *srcLim = src + size;
      if (numFreeClusters_Defined)
        do
          *dest++ = Get32a(src) & 0x0FFFFFFF;
        while (++src != srcLim);
      else
      {
        UInt32 numFreeClusters = 0;
        do
        {
          const UInt32 v = Get32a(src) & 0x0FFFFFFF;
          *dest++ = v;
          numFreeClusters += (UInt32)(v - 1) >> 31;
        }
        while (++src != srcLim);
        NumFreeClusters += numFreeClusters;
      }
      i += size;
      if ((i & 0xFFFFF) == 0)
      {
        RINOK(OpenProgressFat(!numFreeClusters_Defined))
      }
    }
  }
  else
  {
    const UInt32 kBufSize = (UInt32)Header.CalcFatSizeInSectors() << Header.SectorSizeLog;
    NumCurUsedBytes += kBufSize;
    byteBuf.Alloc(kBufSize);
    Byte *p = byteBuf;
    RINOK(ReadStream_FALSE(InStream, p, kBufSize))
    const UInt32 fatSize = Header.FatSize;
    UInt32 *fat = &Fat[0];
    if (Header.NumFatBits == 16)
      for (UInt32 j = 0; j < fatSize; j++)
        fat[j] = Get16a(p + j * 2);
    else
      for (UInt32 j = 0; j < fatSize; j++)
        fat[j] = (Get16(p + j * 3 / 2) >> ((j & 1) << 2)) & 0xFFF;

    if (!numFreeClusters_Defined)
    {
      UInt32 numFreeClusters = 0;
      for (UInt32 i = 0; i < fatSize; i++)
        numFreeClusters += (UInt32)(fat[i] - 1) >> 31;
      NumFreeClusters = numFreeClusters;
    }
  }

  RINOK(OpenProgressFat())

  if ((Fat[0] & 0xFF) != Header.MediaType)
  {
    // that case can mean error in FAT,
    // but xdf file: (MediaType == 0xF0 && Fat[0] == 0xFF9)
    // 19.00: so we use non-strict check
    if ((Fat[0] & 0xFF) < 0xF0)
      return S_FALSE;
  }

  RINOK(ReadDir(-1, Header.RootCluster, 0))

  PhySize = Header.GetPhySize();
  return S_OK;
}



Z7_class_CHandler_final:
  public IInArchive,
  public IArchiveGetRawProps,
  public IInArchiveGetStream,
  public CMyUnknownImp,
  CDatabase
{
  Z7_IFACES_IMP_UNK_3(IInArchive, IArchiveGetRawProps, IInArchiveGetStream)
};

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  const CItem &item = Items[index];
  CClusterInStream *streamSpec = new CClusterInStream;
  CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
  streamSpec->Stream = InStream;
  streamSpec->StartOffset = Header.DataSector << Header.SectorSizeLog;
  streamSpec->BlockSizeLog = Header.ClusterSizeLog;
  streamSpec->Size = item.Size;

  const UInt32 numClusters = Header.GetNumClusters(item.Size);
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
    const UInt32 clusterSize = Header.ClusterSize();
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
  RINOK(streamSpec->InitAndSeek())
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
  // , kpidCharacts
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
  if (NWindows::NTime::DosTime_To_FileTime(dosTime, localFileTime))
    if (LocalFileTimeToFileTime(&localFileTime, &utc))
    {
      UInt64 t64 = (((UInt64)utc.dwHighDateTime) << 32) + utc.dwLowDateTime;
      t64 += ms10 * 100000;
      utc.dwLowDateTime = (DWORD)t64;
      utc.dwHighDateTime = (DWORD)(t64 >> 32);
      prop.SetAsTimeFrom_FT_Prec(utc, k_PropVar_TimePrec_Base + 2);
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

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
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
    case kpidMTime: if (VolItem_Defined) PropVariant_SetFrom_DosTime(prop, Vol_MTime); break;
    case kpidShortComment:
    case kpidVolumeName: if (VolItem_Defined) GetVolName(VolLabel, prop); break;
    case kpidNumFats: if (Header.NumFats != 2) prop = Header.NumFats; break;
    case kpidSectorSize: prop = (UInt32)1 << Header.SectorSizeLog; break;
    // case kpidSectorsPerTrack: prop = Header.SectorsPerTrack; break;
    // case kpidNumHeads: prop = Header.NumHeads; break;
    // case kpidOemName: STRING_TO_PROP(Header.OemName, prop); break;
    case kpidId: if (Header.VolFieldsDefined) prop = Header.VolId; break;
    case kpidIsTree: prop = true; break;
    // case kpidVolName: if (Header.VolFieldsDefined) STRING_TO_PROP(Header.VolName, prop); break;
    // case kpidFileSysType: if (Header.VolFieldsDefined) STRING_TO_PROP(Header.FileSys, prop); break;
    // case kpidHiddenSectors: prop = Header.NumHiddenSectors; break;
    case kpidWarningFlags:
    {
      UInt32 v = 0;
      if (Header.HeadersWarning) v |= kpv_ErrorFlags_HeadersError;
      if (v != 0)
        prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumRawProps(UInt32 *numProps))
{
  *numProps = 0;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetRawPropInfo(UInt32 /* index */ , BSTR *name, PROPID *propID))
{
  *name = NULL;
  *propID = 0;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType))
{
  *parentType = NParentType::kDir;
  int par = -1;
  if (index < Items.Size())
    par = Items[index].Parent;
  *parent = (UInt32)(Int32)par;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType))
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;

  if (index < Items.Size()
      && propID == kpidName)
  {
    CByteBuffer &buf = Items[index].LongName;
    const UInt32 size = (UInt32)buf.Size();
    if (size != 0)
    {
      *dataSize = size;
      *propType = NPropDataType::kUtf16z;
      *data = (const void *)(const Byte *)buf;
    }
  }
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CItem &item = Items[index];
  switch (propID)
  {
    case kpidPath:
    case kpidName:
    case kpidShortName:
    {
      UString s;
      if (propID == kpidPath)
        GetItemPath(index, s);
      else if (propID == kpidName)
        item.GetName(s);
      else
        item.GetShortName(s);
      prop = s;
      break;
    }
/*
    case kpidCharacts:
    {
      if (item.LongName.Size())
        prop = "LFN";
      break;
    }
*/
    case kpidIsDir: prop = item.IsDir(); break;
    case kpidMTime: PropVariant_SetFrom_DosTime(prop, item.MTime); break;
    case kpidCTime: FatTimeToProp(item.CTime, item.CTime2, prop); break;
    case kpidATime: PropVariant_SetFrom_DosTime(prop, ((UInt32)item.ADate << 16)); break;
    case kpidAttrib: prop = (UInt32)item.Attrib; break;
    case kpidSize: if (!item.IsDir()) prop = item.Size; break;
    case kpidPackSize: if (!item.IsDir()) prop = Header.GetFilePackSize(item.Size); break;
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
  if (numItems == (UInt32)(Int32)-1)
  {
    indices = NULL;
    numItems = Items.Size();
    if (numItems == 0)
      return S_OK;
  }
  else
  {
    if (numItems == 0)
      return S_OK;
    if (!indices)
      return E_INVALIDARG;
  }
  UInt64 totalSize = 0;
  {
    UInt32 i = 0;
    do
    {
      UInt32 index = i;
      if (indices)
        index = indices[i];
      const CItem &item = Items[index];
      if (!item.IsDir())
        totalSize += item.Size;
    }
    while (++i != numItems);
  }
  RINOK(extractCallback->SetTotal(totalSize))

  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;

  UInt32 i;
  for (i = 0;; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur())
    if (i == numItems)
      break;
    int res;
    {
      CMyComPtr<ISequentialOutStream> realOutStream;
      const Int32 askMode = testMode ?
          NExtract::NAskMode::kTest :
          NExtract::NAskMode::kExtract;
      UInt32 index = i;
      if (indices)
        index = indices[i];
      const CItem &item = Items[index];
      RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
        
      if (item.IsDir())
      {
        RINOK(extractCallback->PrepareOperation(askMode))
        RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
        continue;
      }
      
      totalPackSize += Header.GetFilePackSize(item.Size);
      totalSize += item.Size;
      
      if (!testMode && !realOutStream)
        continue;
      RINOK(extractCallback->PrepareOperation(askMode))
      res = NExtract::NOperationResult::kDataError;
      CMyComPtr<ISequentialInStream> inStream;
      const HRESULT hres = GetStream(index, &inStream);
      if (hres != S_FALSE)
      {
        RINOK(hres)
        if (inStream)
        {
          RINOK(copyCoder.Interface()->Code(inStream, realOutStream, NULL, NULL, lps))
          if (copyCoder->TotalSize == item.Size)
            res = NExtract::NOperationResult::kOK;
        }
      }
    }
    RINOK(extractCallback->SetOperationResult(res))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Items.Size();
  return S_OK;
}

static const Byte k_Signature[] = { 0x55, 0xAA };

REGISTER_ARC_I(
  "FAT", "fat img", NULL, 0xDA,
  k_Signature,
  0x1FE,
  0,
  IsArc_Fat)

}}
