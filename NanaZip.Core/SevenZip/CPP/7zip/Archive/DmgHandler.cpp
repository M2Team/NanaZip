// DmgHandler.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/AutoPtr.h"
#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer2.h"
#include "../../Common/MyXml.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariant.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/BZip2Decoder.h"
#include "../Compress/CopyCoder.h"
#include "../Compress/LzfseDecoder.h"
#include "../Compress/XzDecoder.h"
#include "../Compress/ZlibDecoder.h"

#include "Common/OutStreamWithCRC.h"

// #define DMG_SHOW_RAW

// #define SHOW_DEBUG_INFO

/* for debug only: we can use block cache also for METHOD_COPY blocks.
   It can reduce the number of Stream->Read() requests.
   But it can increase memory usage.
   Note: METHOD_COPY blocks are not fused usually.
   But if METHOD_COPY blocks is fused in some dmg example,
   block size can exceed k_Chunk_Size_MAX.
   So we don't use cache for METHOD_COPY block, if (block_size > k_Chunk_Size_MAX)
*/
// for debug only:
// #define Z7_DMG_USE_CACHE_FOR_COPY_BLOCKS

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#define PRF(x) x
#else
#define PRF(x)
#endif


#define Get16(p) GetBe16(p)
#define Get32(p) GetBe32(p)
#define Get64(p) GetBe64(p)

#define Get32a(p) GetBe32a(p)
#define Get64a(p) GetBe64a(p)

Byte *Base64ToBin(Byte *dest, const char *src);

namespace NArchive {
namespace NDmg {

// allocation limits for compressed blocks for GetStream() interface:
static const unsigned k_NumChunks_MAX = 128;
static const size_t k_Chunk_Size_MAX = (size_t)1 << 28;
// 256 MB cache for 32-bit:
//   4 GB cache for 64-bit:
static const size_t k_Chunks_TotalSize_MAX = (size_t)1 << (sizeof(size_t) + 24);

// 2 GB limit for 32-bit:
// 4 GB limit for 64-bit:
// that limit can be increased for 64-bit mode, if there are such dmg files
static const size_t k_XmlSize_MAX =
    ((size_t)1 << (sizeof(size_t) / 4 + 30)) - 256;

static const UInt32  METHOD_ZERO_0  = 0; // sparse
static const UInt32  METHOD_COPY    = 1;
static const UInt32  METHOD_ZERO_2  = 2; // sparse : without file CRC calculation
static const UInt32  METHOD_ADC     = 0x80000004;
static const UInt32  METHOD_ZLIB    = 0x80000005;
static const UInt32  METHOD_BZIP2   = 0x80000006;
static const UInt32  METHOD_LZFSE   = 0x80000007;
static const UInt32  METHOD_XZ      = 0x80000008;
static const UInt32  METHOD_COMMENT = 0x7FFFFFFE; // is used to comment "+beg" and "+end" in extra field.
static const UInt32  METHOD_END     = 0xFFFFFFFF;


struct CBlock
{
  UInt32 Type;
  UInt64 UnpPos;
  // UInt64 UnpSize;
  UInt64 PackPos;
  UInt64 PackSize;
  
  bool NeedCrc() const { return Type != METHOD_ZERO_2; }
  bool IsZeroMethod() const
  {
    return (Type & ~(UInt32)METHOD_ZERO_2) == 0;
    // return Type == METHOD_ZERO_0 || Type == METHOD_ZERO_2;
  }

  bool IsClusteredMethod() const
  {
    // most of dmg files have non-fused COPY_METHOD blocks.
    // so we don't exclude COPY_METHOD blocks when we try to detect size of cluster.
    return !IsZeroMethod(); // include COPY_METHOD blocks.
    // Type > METHOD_ZERO_2; // for debug: exclude COPY_METHOD blocks.
  }

  bool NeedAllocateBuffer() const
  {
    return
#ifdef Z7_DMG_USE_CACHE_FOR_COPY_BLOCKS
        !IsZeroMethod();
#else
        Type > METHOD_ZERO_2;
        // !IsZeroMethod() && Type != METHOD_COPY;
#endif
  }
  // we don't store non-data blocks now
  // bool ThereAreDataInBlock() const { return Type != METHOD_COMMENT && Type != METHOD_END; }
};

static const UInt32 kCheckSumType_CRC = 2;
static const unsigned kChecksumSize_Max = 0x80;

struct CChecksum
{
  UInt32 Type;
  UInt32 NumBits;
  Byte Data[kChecksumSize_Max];

  bool IsCrc32() const { return Type == kCheckSumType_CRC && NumBits == 32; }
  UInt32 GetCrc32() const { return Get32(Data); }
  void Parse(const Byte *p);

  void PrintType(AString &s) const;
  void Print(AString &s) const;
  void Print_with_Name(AString &s) const;
  void AddToComment(AString &s, const char *name) const;
};

void CChecksum::Parse(const Byte *p)
{
  Type = Get32(p);
  NumBits = Get32(p + 4);
  memcpy(Data, p + 8, kChecksumSize_Max);
}


void CChecksum::PrintType(AString &s) const
{
  if (NumBits == 0)
    return;
  if (IsCrc32())
    s += "CRC";
  else
  {
    s += "Checksum";
    s.Add_UInt32(Type);
    s.Add_Minus();
    s.Add_UInt32(NumBits);
  }
}

void CChecksum::Print(AString &s) const
{
  if (NumBits == 0)
    return;
  char temp[kChecksumSize_Max * 2 + 2];
  /*
  if (IsCrc32())
    ConvertUInt32ToHex8Digits(GetCrc32(), temp);
  else
  */
  {
    UInt32 numBits = kChecksumSize_Max * 8;
    if (numBits > NumBits)
        numBits = NumBits;
    const unsigned numBytes = (numBits + 7) >> 3;
    if (numBytes <= 8)
      ConvertDataToHex_Upper(temp, Data, numBytes);
    else
      ConvertDataToHex_Lower(temp, Data, numBytes);
  }
  s += temp;
}

void CChecksum::Print_with_Name(AString &s) const
{
  if (NumBits == 0)
    return;
  PrintType(s);
  s += ": ";
  Print(s);
}

static void AddToComment_Prop(AString &s, const char *name, const char *val)
{
  s += name;
  s += ": ";
  s += val;
  s.Add_LF();
}

void CChecksum::AddToComment(AString &s, const char *name) const
{
  AString s2;
  Print_with_Name(s2);
  if (!s2.IsEmpty())
    AddToComment_Prop(s, name, s2);
}


struct CFile
{
  UInt64 Size;
  CRecordVector<CBlock> Blocks;
  UInt64 PackSize;
  UInt64 StartPackPos;
  UInt64 BlockSize_MAX;
  UInt64 StartUnpackSector;  // unpack sector position of this file from all files
  UInt64 NumUnpackSectors;
  Int32 Descriptor;
  bool IsCorrect;
  bool FullFileChecksum;
  AString Name;
  // AString Id;
  CChecksum Checksum;

  UInt64 GetUnpackSize_of_Block(unsigned blockIndex) const
  {
    return (blockIndex == Blocks.Size() - 1 ?
        Size : Blocks[blockIndex + 1].UnpPos) - Blocks[blockIndex].UnpPos;
  }

  CFile():
      Size(0),
      PackSize(0),
      StartPackPos(0),
      BlockSize_MAX(0),
      StartUnpackSector(0),
      NumUnpackSectors(0),
      Descriptor(0),
      IsCorrect(false),
      FullFileChecksum(false)
  {
    Checksum.Type = 0;
    Checksum.NumBits = 0;
  }
  HRESULT Parse(const Byte *p, UInt32 size);
};

#ifdef DMG_SHOW_RAW
struct CExtraFile
{
  CByteBuffer Data;
  AString Name;
};
#endif


static void AddToComment_UInt64(AString &s, UInt64 v, const char *name)
{
  s += name;
  s += ": ";
  s.Add_UInt64(v);
  s.Add_LF();
}

struct CForkPair
{
  UInt64 Offset;
  UInt64 Len;
  
  // (p) is aligned for 8-bytes
  void Parse(const Byte *p)
  {
    Offset = Get64a(p);
    Len = Get64a(p + 8);
  }

  bool GetEndPos(UInt64 &endPos)
  {
    endPos = Offset + Len;
    return endPos >= Offset;
  }

  bool UpdateTop(UInt64 limit, UInt64 &top)
  {
    if (Offset > limit || Len > limit - Offset)
      return false;
    const UInt64 top2 = Offset + Len;
    if (top <= top2)
      top = top2;
    return true;
  }

  void Print(AString &s, const char *name) const;
};

void CForkPair::Print(AString &s, const char *name) const
{
  if (Offset != 0 || Len != 0)
  {
    s += name;  s.Add_Minus();  AddToComment_UInt64(s, Offset, "offset");
    s += name;  s.Add_Minus();  AddToComment_UInt64(s, Len,    "length");
  }
}


Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  bool _masterCrcError;
  bool _headersError;
  bool _dataForkError;
  bool _rsrcMode_wasUsed;

  CMyComPtr<IInStream> _inStream;
  CObjectVector<CFile> _files;

  // UInt32 _dataStartOffset;
  UInt64 _startPos;
  UInt64 _phySize;

  AString _name;

  CForkPair _dataForkPair;
  CForkPair rsrcPair, xmlPair, blobPair;

  // UInt64 _runningDataForkOffset;
  // UInt32 _segmentNumber;
  // UInt32 _segmentCount;
  UInt64 _numSectors; // total unpacked number of sectors
  Byte _segmentGUID[16];

  CChecksum _dataForkChecksum;
  CChecksum _masterChecksum;

#ifdef DMG_SHOW_RAW
  CObjectVector<CExtraFile> _extras;
#endif

  HRESULT ReadData(IInStream *stream, const CForkPair &pair, CByteBuffer &buf);
  bool ParseBlob(const CByteBuffer &data);
  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openArchiveCallback);
  HRESULT Extract(IInStream *stream);
};


struct CMethods
{
  CRecordVector<UInt32> Types;

  void Update(const CFile &file);
  void AddToString(AString &s) const;
};

void CMethods::Update(const CFile &file)
{
  FOR_VECTOR (i, file.Blocks)
  {
    if (Types.Size() >= (1 << 8))
      break;
    Types.AddToUniqueSorted(file.Blocks[i].Type);
  }
}

void CMethods::AddToString(AString &res) const
{
  FOR_VECTOR (i, Types)
  {
    const UInt32 type = Types[i];
    /*
    if (type == METHOD_COMMENT || type == METHOD_END)
      continue;
    */
    char buf[16];
    const char *s;
    switch (type)
    {
      // case METHOD_COMMENT: s = "Comment"; break;
      case METHOD_ZERO_0: s = "Zero0"; break;
      case METHOD_ZERO_2: s = "Zero2"; break;
      case METHOD_COPY:   s = "Copy";  break;
      case METHOD_ADC:    s = "ADC";   break;
      case METHOD_ZLIB:   s = "ZLIB";  break;
      case METHOD_BZIP2:  s = "BZip2"; break;
      case METHOD_LZFSE:  s = "LZFSE"; break;
      case METHOD_XZ:     s = "XZ";    break;
      default: ConvertUInt32ToHex(type, buf); s = buf;
    }
    res.Add_OptSpaced(s);
  }
}



struct CAppleName
{
  bool IsFs;
  const char *Ext;
  const char *AppleName;
};

static const CAppleName k_Names[] =
{
  { true,  "hfs",  "Apple_HFS" },
  { true,  "hfsx", "Apple_HFSX" },
  { true,  "ufs",  "Apple_UFS" },
  { true,  "apfs", "Apple_APFS" },
  { true,  "iso",  "Apple_ISO" },

  // efi_sys partition is FAT32, but it's not main file. So we use (IsFs = false)
  { false,  "efi_sys", "C12A7328-F81F-11D2-BA4B-00A0C93EC93B" },

  { false, "free", "Apple_Free" },
  { false, "ddm",  "DDM" },
  { false, NULL,   "Apple_partition_map" },
  { false, NULL,   " GPT " },
    /* " GPT " is substring of full name entry in dmg that contains
         some small metadata GPT entry. It's not real FS entry:
      "Primary GPT Header",
      "Primary GPT Table"
      "Backup GPT Header",
      "Backup GPT Table",
    */
  { false, NULL,   "MBR" },
  { false, NULL,   "Driver" },
  { false, NULL,   "Patches" }
};
  
static const unsigned kNumAppleNames = Z7_ARRAY_SIZE(k_Names);

const char *Find_Apple_FS_Ext(const AString &name);
const char *Find_Apple_FS_Ext(const AString &name)
{
  for (unsigned i = 0; i < kNumAppleNames; i++)
  {
    const CAppleName &a = k_Names[i];
    if (a.Ext)
      if (name == a.AppleName)
        return a.Ext;
  }
  return NULL;
}


bool Is_Apple_FS_Or_Unknown(const AString &name);
bool Is_Apple_FS_Or_Unknown(const AString &name)
{
  for (unsigned i = 0; i < kNumAppleNames; i++)
  {
    const CAppleName &a = k_Names[i];
    // if (name.Find(a.AppleName) >= 0)
    if (strstr(name, a.AppleName))
      return a.IsFs;
  }
  return true;
}



// define it for debug only:
// #define Z7_DMG_SINGLE_FILE_MODE

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidComment,
  kpidMethod,
  kpidNumBlocks,
  kpidClusterSize,
  kpidChecksum,
  kpidCRC,
  kpidId
  // kpidOffset
#ifdef Z7_DMG_SINGLE_FILE_MODE
  , kpidPosition
#endif
};

IMP_IInArchive_Props

static const Byte kArcProps[] =
{
  kpidMethod,
  kpidNumBlocks,
  kpidClusterSize,
  kpidComment
};


Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMethod:
    {
      CMethods m;
      CRecordVector<UInt32> ChecksumTypes;
      {
        FOR_VECTOR (i, _files)
        {
          const CFile &file = _files[i];
          m.Update(file);
          if (ChecksumTypes.Size() < (1 << 8))
            ChecksumTypes.AddToUniqueSorted(file.Checksum.Type);
        }
      }
      AString s;
      m.AddToString(s);

      FOR_VECTOR (i, ChecksumTypes)
      {
        const UInt32 type = ChecksumTypes[i];
        switch (type)
        {
          case kCheckSumType_CRC:
            s.Add_OptSpaced("CRC");
            break;
          default:
            s.Add_OptSpaced("Checksum");
            s.Add_UInt32(type);
        }
      }

      if (!s.IsEmpty())
        prop = s;
      break;
    }
    case kpidNumBlocks:
    {
      UInt64 numBlocks = 0;
      FOR_VECTOR (i, _files)
        numBlocks += _files[i].Blocks.Size();
      prop = numBlocks;
      break;
    }
    case kpidClusterSize:
    {
      UInt64 blockSize_MAX = 0;
      FOR_VECTOR (i, _files)
      {
        const UInt64 a = _files[i].BlockSize_MAX;
        if (blockSize_MAX < a)
            blockSize_MAX = a;
      }
      prop = blockSize_MAX;
      break;
    }
    case kpidMainSubfile:
    {
      int mainIndex = -1;
      FOR_VECTOR (i, _files)
      {
        if (Is_Apple_FS_Or_Unknown(_files[i].Name))
        {
          if (mainIndex != -1)
          {
            mainIndex = -1;
            break;
          }
          mainIndex = (int)i;
        }
      }
      if (mainIndex != -1)
        prop = (UInt32)(Int32)mainIndex;
      break;
    }
    case kpidWarning:
      if (_masterCrcError)
        prop = "Master CRC error";
      break;

    case kpidWarningFlags:
    {
      UInt32 v = 0;
      if (_headersError)  v |= kpv_ErrorFlags_HeadersError;
      if (_dataForkError) v |= kpv_ErrorFlags_CrcError;
      if (v != 0)
        prop = v;
      break;
    }

    case kpidOffset: prop = _startPos; break;
    case kpidPhySize: prop = _phySize; break;

    case kpidComment:
    {
      AString s;
      if (!_name.IsEmpty())
        AddToComment_Prop(s, "Name", _name);
      AddToComment_UInt64(s, _numSectors << 9, "unpack-size");
      {
        char temp[sizeof(_segmentGUID) * 2 + 2];
        ConvertDataToHex_Lower(temp, _segmentGUID, sizeof(_segmentGUID));
        AddToComment_Prop(s, "ID", temp);
      }
      _masterChecksum.AddToComment(s, "master-checksum");
      _dataForkChecksum.AddToComment(s, "pack-checksum");
      {
        /*
        if (_dataStartOffset != 0)
          AddToComment_UInt64(s, _dataStartOffset,    "payload-start-offset");
        */
        // if (_dataForkPair.Offset != 0)
        _dataForkPair.Print(s, "pack");
        rsrcPair.Print(s, "rsrc");
        xmlPair.Print(s, "xml");
        blobPair.Print(s, "blob");
      }
      if (_rsrcMode_wasUsed)
        s += "RSRC_MODE\n";
      if (!s.IsEmpty())
        prop = s;
    }
    break;

    case kpidName:
      if (!_name.IsEmpty())
        prop = _name + ".dmg";
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

IMP_IInArchive_ArcProps



static const UInt64 kSectorNumber_LIMIT = (UInt64)1 << (63 - 9);

HRESULT CFile::Parse(const Byte *p, UInt32 size)
{
  // CFile was initialized to default values: 0 in size variables and (IsCorrect == false)
  const unsigned kHeadSize = 0xCC;
  if (size < kHeadSize)
    return S_FALSE;
  if (Get32(p) != 0x6D697368) // "mish" signature
    return S_FALSE;
  if (Get32(p + 4) != 1) // version
    return S_FALSE;
  
  StartUnpackSector = Get64(p + 8);
  NumUnpackSectors = Get64(p + 0x10);
  StartPackPos = Get64(p + 0x18);

#ifdef SHOW_DEBUG_INFO
  /* the number of sectors that must be allocated.
     ==  0x208 for 256KB clusters
     ==  0x808 for   1MB clusters
     == 0x1001 for   1MB clusters in some example
  */
  const UInt32 decompressedBufRequested = Get32(p + 0x20);
#endif
  
  // Descriptor is file index. usually started from -1
  // in one dmg it was started from 0
  Descriptor = (Int32)Get32(p + 0x24);
  // char Reserved1[24];
  
  Checksum.Parse(p + 0x40);
  PRF(printf("\n" " Checksum Type = %2u"
        "\n StartUnpackSector = %8x"
        "\n NumUnpackSectors  = %8x"
        "\n StartPos = %8x"
        "\n decompressedBufRequested=%8x"
        "\n blocksDescriptor=%8x"
        , (unsigned)Checksum.Type
        , (unsigned)StartUnpackSector
        , (unsigned)NumUnpackSectors
        , (unsigned)StartPackPos
        , (unsigned)decompressedBufRequested
        , (unsigned)Descriptor
    );)

  const UInt32 numBlocks = Get32(p + 0xC8);
  const unsigned kRecordSize = 40;
  if ((UInt64)numBlocks * kRecordSize + kHeadSize != size)
    return S_FALSE;

  Blocks.ClearAndReserve(numBlocks);
  FullFileChecksum = true;
  /* IsCorrect = false; by default
     So we return S_OK, if we can ignore some error in headers.
  */

  p += kHeadSize;
  UInt32 i;
 
  for (i = 0; i < numBlocks; i++, p += kRecordSize)
  {
    CBlock b;
    b.Type = Get32(p);
    {
      const UInt64 a = Get64(p + 0x08);
      if (a >= kSectorNumber_LIMIT)
        return S_OK;
      b.UnpPos = a << 9;
    }
    UInt64 unpSize;
    {
      const UInt64 a = Get64(p + 0x10);
      if (a >= kSectorNumber_LIMIT)
        return S_OK;
      unpSize = a << 9;
    }
    const UInt64 newSize = b.UnpPos + unpSize;
    if (newSize >= ((UInt64)1 << 63))
      return S_OK;

    b.PackPos  = Get64(p + 0x18);
    b.PackSize = Get64(p + 0x20);
    // b.PackPos can be 0 for some types. So we don't check it
    if (b.UnpPos != Size)
      return S_OK;
    
    PRF(printf("\nType=%8x  comment=%8x  uPos=%8x  uSize=%7x  pPos=%8x  pSize=%7x",
        (unsigned)b.Type, Get32(p + 4), (unsigned)b.UnpPos, (unsigned)unpSize, (unsigned)b.PackPos, (unsigned)b.PackSize));
    
    if (b.Type == METHOD_COMMENT)
    {
      // some files contain 2 comment block records:
      // record[0]     : Type=METHOD_COMMENT, comment_field = "+beg"
      // record[num-2] : Type=METHOD_COMMENT, comment_field = "+end"
      // we skip these useless records.
      continue;
    }
    if (b.Type == METHOD_END)
      break;
 
    // we add only blocks that have non empty unpacked data:
    if (unpSize != 0)
    {
      const UInt64 k_max_pos = (UInt64)1 << 63;
      if (b.PackPos >= k_max_pos ||
          b.PackSize >= k_max_pos - b.PackPos)
        return S_OK;

      /* we don't count non-ZERO blocks here, because
         ZERO blocks in dmg files are not limited by some cluster size.
         note: COPY blocks also sometimes are fused to larger blocks.
      */
      if (b.IsClusteredMethod())
      if (BlockSize_MAX < unpSize)
          BlockSize_MAX = unpSize;

      PackSize += b.PackSize;
      if (!b.NeedCrc())
        FullFileChecksum = false;
      Blocks.AddInReserved(b);
      Size = newSize;
    }
  }

  PRF(printf("\n");)

  if (i != numBlocks - 1)
  {
    // return S_FALSE;
    return S_OK;
  }
    
  if ((Size >> 9) == NumUnpackSectors)
    IsCorrect = true;
  return S_OK;
}


static const CXmlItem *FindKeyPair(const CXmlItem &item, const char *key, const char *nextTag)
{
  for (unsigned i = 0; i + 1 < item.SubItems.Size(); i++)
  {
    const CXmlItem &si = item.SubItems[i];
    if (si.IsTagged("key") && si.GetSubString() == key)
    {
      const CXmlItem *si_1 = &item.SubItems[i + 1];
      if (si_1->IsTagged(nextTag))
        return si_1;
    }
  }
  return NULL;
}

static const AString *GetStringFromKeyPair(const CXmlItem &item, const char *key, const char *nextTag)
{
  const CXmlItem *si_1 = FindKeyPair(item, key, nextTag);
  if (si_1)
    return si_1->GetSubStringPtr();
  return NULL;
}

static const Byte k_Signature[] = { 'k','o','l','y', 0, 0, 0, 4, 0, 0, 2, 0 };

static inline bool IsKoly(const Byte *p)
{
  return memcmp(p, k_Signature, Z7_ARRAY_SIZE(k_Signature)) == 0;
  /*
  if (Get32(p) != 0x6B6F6C79) // "koly" signature
    return false;
  if (Get32(p + 4) != 4) // version
    return false;
  if (Get32(p + 8) != HEADER_SIZE)
    return false;
  return true;
  */
}


HRESULT CHandler::ReadData(IInStream *stream, const CForkPair &pair, CByteBuffer &buf)
{
  size_t size = (size_t)pair.Len;
  if (size != pair.Len)
    return E_OUTOFMEMORY;
  buf.Alloc(size);
  RINOK(InStream_SeekSet(stream, _startPos + pair.Offset))
  return ReadStream_FALSE(stream, buf, size);
}



bool CHandler::ParseBlob(const CByteBuffer &data)
{
  const unsigned kHeaderSize = 3 * 4;
  if (data.Size() < kHeaderSize)
    return false;
  const Byte * const p = (const Byte *)data;
  if (Get32a(p) != 0xfade0cc0) // CSMAGIC_EMBEDDED_SIGNATURE
    return true;
  const UInt32 size = Get32a(p + 4);
  if (size != data.Size())
    return false;
  const UInt32 num = Get32a(p + 8);
  if (num > (size - kHeaderSize) / 8)
    return false;
  
  const UInt32 limit = num * 8 + kHeaderSize;
  for (size_t i = kHeaderSize; i < limit; i += 8)
  {
    // type == 0 == CSSLOT_CODEDIRECTORY for CSMAGIC_CODEDIRECTORY item
    // UInt32 type = Get32(p + i);
    const UInt32 offset = Get32a(p + i + 4);
    if (offset < limit || offset > size - 8)
      return false;
    // offset is not aligned for 4 here !!!
    const Byte * const p2 = p + offset;
    const UInt32 magic = Get32(p2);
    const UInt32 len = Get32(p2 + 4);
    if (size - offset < len || len < 8)
      return false;

#ifdef DMG_SHOW_RAW
    CExtraFile &extra = _extras.AddNew();
    extra.Name = "_blob_";
    extra.Data.CopyFrom(p2, len);
#endif

    if (magic == 0xfade0c02) // CSMAGIC_CODEDIRECTORY
    {
#ifdef DMG_SHOW_RAW
      extra.Name += "codedir";
#endif
    
      if (len < 11 * 4)
        return false;
      const UInt32 idOffset = Get32(p2 + 5 * 4);
      if (idOffset >= len)
        return false;
      UInt32 len2 = len - idOffset;
      const UInt32 kNameLenMax = 1 << 8;
      if (len2 > kNameLenMax)
          len2 = kNameLenMax;
      _name.SetFrom_CalcLen((const char *)(p2 + idOffset), len2);
      /*
      // #define kSecCodeSignatureHashSHA1     1
      // #define kSecCodeSignatureHashSHA256   2
      const UInt32 hashOffset    = Get32(p2 + 4 * 4);
      const UInt32 nSpecialSlots = Get32(p2 + 6 * 4);
      const UInt32 nCodeSlots    = Get32(p2 + 7 * 4);
      const unsigned hashSize = p2[36];
      const unsigned hashType = p2[37];
      // const unsigned unused = p2[38];
      const unsigned pageSize = p2[39];
      */
    }
#ifdef DMG_SHOW_RAW
    else if (magic == 0xfade0c01) extra.Name += "requirements";
    else if (magic == 0xfade0b01) extra.Name += "signed";
    else
    {
      char temp[16];
      ConvertUInt32ToHex8Digits(magic, temp);
      extra.Name += temp;
    }
#endif
  }

  return true;
}




HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *openArchiveCallback)
{
  /*
  - usual dmg contains Koly Header at the end:
  - rare case old dmg contains Koly Header at the begin.
  */

  // _dataStartOffset = 0;
  UInt64 fileSize;
  RINOK(InStream_GetPos_GetSize(stream, _startPos, fileSize))

  const unsigned HEADER_SIZE = 0x200;
  UInt64 buf[HEADER_SIZE / sizeof(UInt64)];
  RINOK(ReadStream_FALSE(stream, buf, HEADER_SIZE))

  UInt64 headerPos;
  bool front_Koly_Mode = false;

  /*
  _dataForkChecksum.Offset ==   0 for koly-at-the-end
  _dataForkChecksum.Offset == 512 for koly-at-the-start
  so we can use (_dataForkChecksum.Offset) to detect "koly-at-the-start" mode
  */

  if (IsKoly((const Byte *)(const void *)buf))
  {
    // it can be normal koly-at-the-end or koly-at-the-start
    headerPos = _startPos;
    /*
    if (_startPos <= (1 << 8))
    {
      // we want to support front_Koly_Mode, even if there is
      // some data before dmg file, like 128 bytes MacBin header
      _dataStartOffset = HEADER_SIZE;
      front_Koly_Mode = true;
    }
    */
  }
  else
  {
    /* we try to open in backward mode only for first attempt
       when (_startPos == 0) */
    if (_startPos != 0)
      return S_FALSE;
    headerPos = fileSize;
    if (headerPos < HEADER_SIZE)
      return S_FALSE;
    headerPos -= HEADER_SIZE;
    RINOK(InStream_SeekSet(stream, headerPos))
    RINOK(ReadStream_FALSE(stream, buf, HEADER_SIZE))
    if (!IsKoly((const Byte *)(const void *)buf))
      return S_FALSE;
  }

  // UInt32 flags = Get32a((const Byte *)(const void *)buf + 12);
  // _runningDataForkOffset = Get64a((const Byte *)(const void *)buf + 0x10);
  _dataForkPair.Parse((const Byte *)(const void *)buf + 0x18);
  rsrcPair.Parse((const Byte *)(const void *)buf + 0x28);
  // _segmentNumber = Get32a(buf + 0x38); // 0 or 1
  // _segmentCount = Get32a(buf + 0x3C);  // 0 (if not set) or 1
  memcpy(_segmentGUID, (const Byte *)(const void *)buf + 0x40, 16);
  _dataForkChecksum.Parse((const Byte *)(const void *)buf + 0x50);
  xmlPair.Parse((const Byte *)(const void *)buf + 0xD8);
  // Byte resereved[]
  blobPair.Parse((const Byte *)(const void *)buf + 0x128);
  _masterChecksum.Parse((const Byte *)(const void *)buf + 0x160);
  // UInt32 imageVariant = Get32a((const Byte *)(const void *)buf + 0x1E8); imageVariant = imageVariant;
  _numSectors = Get64((const Byte *)(const void *)buf + 0x1EC);  // it's not aligned for 8-bytes
  // Byte resereved[12];

  if (_dataForkPair.Offset == HEADER_SIZE
      && headerPos + HEADER_SIZE < fileSize)
    front_Koly_Mode = true;

  const UInt64 limit = front_Koly_Mode ? fileSize : headerPos;
  UInt64 top = 0;
  if (!_dataForkPair.UpdateTop(limit, top)) return S_FALSE;
  if (!xmlPair.UpdateTop(limit, top)) return S_FALSE;
  if (!rsrcPair.UpdateTop(limit, top)) return S_FALSE;

  /* Some old dmg files contain garbage data in blobPair field.
     So we need to ignore such garbage case;
     And we still need to detect offset of start of archive for "parser" mode. */
  const bool useBlob = blobPair.UpdateTop(limit, top);

  if (front_Koly_Mode)
    _phySize = top;
  else
  {
    _phySize = headerPos + HEADER_SIZE;
    _startPos = 0;
    if (top != headerPos)
    {
      /*
      if expected absolute offset is not equal to real header offset,
      2 cases are possible:
        - additional (unknown) headers
        - archive with offset.
       So we try to read XML with absolute offset to select from these two ways.
      */
      CForkPair xmlPair2 = xmlPair;
      const char *sz = "<?xml version";
      const unsigned len = (unsigned)strlen(sz);
      if (xmlPair2.Len > len)
        xmlPair2.Len = len;
      CByteBuffer buf2;
      if (xmlPair2.Len < len
        || ReadData(stream, xmlPair2, buf2) != S_OK
        || memcmp(buf2, sz, len) != 0)
      {
        // if absolute offset is not OK, probably it's archive with offset
        _startPos = headerPos - top;
        _phySize = top + HEADER_SIZE;
      }
    }
  }

  if (useBlob
      && blobPair.Len != 0
      && blobPair.Len <= (1u << 24)) // we don't want parsing of big blobs
  {
#ifdef DMG_SHOW_RAW
    CExtraFile &extra = _extras.AddNew();
    extra.Name = "_blob.bin";
    CByteBuffer &blobBuf = extra.Data;
#else
    CByteBuffer blobBuf;
#endif
    RINOK(ReadData(stream, blobPair, blobBuf))
    if (!ParseBlob(blobBuf))
      _headersError = true;
  }


  UInt64 openTotal = 0;
  UInt64 openCur = 0;
  if (_dataForkChecksum.IsCrc32())
    openTotal = _dataForkPair.Len;

  const UInt32 RSRC_HEAD_SIZE = 0x100;

  /* we have 2 ways to read files and blocks metadata:
     via Xml or via Rsrc.
     But some images have no Rsrc fork.
     Is it possible that there is no Xml?
     Rsrc method will not work for big files. */
  // v23.02: we use xml mode by default
  // if (rsrcPair.Len >= RSRC_HEAD_SIZE && rsrcPair.Len <= ((UInt32)1 << 24)) // for debug
  if (xmlPair.Len == 0)
  {
    // We don't know the size of the field "offset" in Rsrc.
    // We suppose that it uses 24 bits. So we use Rsrc, only if the rsrcPair.Len <= (1 << 24).
    const bool canUseRsrc = (rsrcPair.Len >= RSRC_HEAD_SIZE && rsrcPair.Len <= ((UInt32)1 << 24));
    if (!canUseRsrc)
      return S_FALSE;

    _rsrcMode_wasUsed = true;
#ifdef DMG_SHOW_RAW
    CExtraFile &extra = _extras.AddNew();
    extra.Name = "rsrc.bin";
    CByteBuffer &rsrcBuf = extra.Data;
#else
    CByteBuffer rsrcBuf;
#endif

    RINOK(ReadData(stream, rsrcPair, rsrcBuf))

    const Byte *p = rsrcBuf;
    const UInt32 headSize     = Get32a(p + 0);
    const UInt32 footerOffset = Get32a(p + 4);
    const UInt32 mainDataSize = Get32a(p + 8);
    const UInt32 footerSize   = Get32a(p + 12);
    if (headSize != RSRC_HEAD_SIZE
        || footerOffset >= rsrcPair.Len
        || mainDataSize >= rsrcPair.Len
        || footerOffset < mainDataSize
        || footerOffset != headSize + mainDataSize)
      return S_FALSE;

    {
      const UInt32 footerEnd = footerOffset + footerSize;
      if (footerEnd != rsrcPair.Len)
      {
        // there is rare case dmg example, where there are 4 additional bytes
        const UInt64 rem = rsrcPair.Len - footerOffset;
        if (rem < footerSize
          || rem - footerSize != 4
          || Get32(p + footerEnd) != 0)
          return S_FALSE;
      }
    }
  
    if (footerSize < 0x1e)
      return S_FALSE;
    if (memcmp(p, p + footerOffset, 16) != 0)
      return S_FALSE;

    p += footerOffset;

    if ((UInt32)Get16(p + 0x18) != 0x1c)
      return S_FALSE;
    const UInt32 namesOffset = Get16(p + 0x1a);
    if (namesOffset > footerSize)
      return S_FALSE;
    
    const UInt32 numItems = (UInt32)Get16(p + 0x1c) + 1;
    if (numItems * 8 + 0x1e > namesOffset)
      return S_FALSE;
    
    for (UInt32 i = 0; i < numItems; i++)
    {
      const Byte *p2 = p + 0x1e + (size_t)i * 8;
      const UInt32 typeId = Get32(p2);
      const UInt32 k_typeId_blkx = 0x626c6b78; // blkx
#ifndef DMG_SHOW_RAW
      if (typeId != k_typeId_blkx)
        continue;
#endif
      const UInt32 numFiles = (UInt32)Get16(p2 + 4) + 1;
      const UInt32 offs = Get16(p2 + 6);
      if (0x1c + offs + 12 * numFiles > namesOffset)
        return S_FALSE;

      for (UInt32 k = 0; k < numFiles; k++)
      {
        const Byte *p3 = p + 0x1c + offs + k * 12;
        // UInt32 id = Get16(p3);
        const UInt32 namePos = Get16(p3 + 2);
        // Byte attributes = p3[4]; // = 0x50 for blkx #define (ATTRIBUTE_HDIUTIL)
        // we don't know how many bits we can use. So we use 24 bits only
        UInt32 blockOffset = Get32(p3 + 4);
        blockOffset &= ((UInt32)1 << 24) - 1;
        // UInt32 unknown2 = Get32(p3 + 8); // ???
        if (blockOffset + 4 >= mainDataSize)
          return S_FALSE;
        const Byte *pBlock = rsrcBuf + headSize + blockOffset;
        const UInt32 blockSize = Get32(pBlock);
        if (mainDataSize - (blockOffset + 4) < blockSize)
          return S_FALSE;
        
        AString name;
        
        if (namePos != 0xffff)
        {
          const UInt32 namesBlockSize = footerSize - namesOffset;
          if (namePos >= namesBlockSize)
            return S_FALSE;
          const Byte *namePtr = p + namesOffset + namePos;
          const UInt32 nameLen = *namePtr;
          if (namesBlockSize - namePos <= nameLen)
            return S_FALSE;
          for (UInt32 r = 1; r <= nameLen; r++)
          {
            const Byte c = namePtr[r];
            if (c < 0x20 || c >= 0x80)
              break;
            name += (char)c;
          }
        }
        
        if (typeId == k_typeId_blkx)
        {
          CFile &file = _files.AddNew();
          file.Name = name;
          RINOK(file.Parse(pBlock + 4, blockSize))
          if (!file.IsCorrect)
            _headersError = true;
        }
        
#ifdef DMG_SHOW_RAW
        {
          AString name2;
          name2.Add_UInt32(i);
          name2 += '_';
          {
            char temp[4 + 1] = { 0 };
            memcpy(temp, p2, 4);
            name2 += temp;
          }
          name2.Trim();
          name2 += '_';
          name2.Add_UInt32(k);
          if (!name.IsEmpty())
          {
            name2 += '_';
            name2 += name;
          }
          CExtraFile &extra2 = _extras.AddNew();
          extra2.Name = name2;
          extra2.Data.CopyFrom(pBlock + 4, blockSize);
        }
#endif
      }
    }
  }
  else
  {
    if (xmlPair.Len > k_XmlSize_MAX)
      return S_FALSE;
    // if (!canUseXml) return S_FALSE;
    const size_t size = (size_t)xmlPair.Len;
    // if (size + 1 <= xmlPair.Len) return S_FALSE; // optional check
    RINOK(InStream_SeekSet(stream, _startPos + xmlPair.Offset))
    CXml xml;
    {
      openTotal += size;
      if (openArchiveCallback)
      {
        RINOK(openArchiveCallback->SetTotal(NULL, &openTotal))
      }
      size_t pos = 0;
      CAlignedBuffer1 xmlStr(size + 1);
      for (;;)
      {
        const size_t k_OpenStep = 1 << 24;
        const size_t cur = MyMin(k_OpenStep, size - pos);
        RINOK(ReadStream_FALSE(stream, xmlStr + pos, cur))
        pos += cur;
        openCur += cur;
        if (pos == size)
          break;
        if (openArchiveCallback)
        {
          RINOK(openArchiveCallback->SetCompleted(NULL, &openCur))
        }
      }
      xmlStr[size] = 0;
      // if (strlen((const char *)(const void *)(const Byte *)xmlStr) != size) return S_FALSE;
      if (!xml.Parse((char *)(void *)(Byte *)xmlStr))
        return S_FALSE;

#ifdef DMG_SHOW_RAW
      CExtraFile &extra = _extras.AddNew();
      extra.Name = "a.xml";
      extra.Data.CopyFrom(xmlStr, size);
#endif
    }
    
    if (xml.Root.Name != "plist")
      return S_FALSE;
    
    const CXmlItem *dictItem = xml.Root.FindSubTag_GetPtr("dict");
    if (!dictItem)
      return S_FALSE;
    
    const CXmlItem *rfDictItem = FindKeyPair(*dictItem, "resource-fork", "dict");
    if (!rfDictItem)
      return S_FALSE;
    
    const CXmlItem *arrItem = FindKeyPair(*rfDictItem, "blkx", "array");
    if (!arrItem)
      return S_FALSE;
    
    FOR_VECTOR (i, arrItem->SubItems)
    {
      const CXmlItem &item = arrItem->SubItems[i];
      if (!item.IsTagged("dict"))
        continue;
      
      CByteBuffer rawBuf;
      unsigned destLen = 0;
      {
        const AString *dataString = GetStringFromKeyPair(item, "Data", "data");
        if (!dataString)
          return S_FALSE;
        destLen = dataString->Len() / 4 * 3 + 4;
        rawBuf.Alloc(destLen);
        {
          const Byte *endPtr = Base64ToBin(rawBuf, *dataString);
          if (!endPtr)
            return S_FALSE;
          destLen = (unsigned)(endPtr - (const Byte *)rawBuf);
        }
        
#ifdef DMG_SHOW_RAW
        CExtraFile &extra = _extras.AddNew();
        extra.Name.Add_UInt32(_files.Size());
        extra.Data.CopyFrom(rawBuf, destLen);
#endif
      }
      CFile &file = _files.AddNew();
      {
        /* xml code removes front space for such string:
             <string> (Apple_Free : 3)</string>
           maybe we shoud fix xml code and return full string with space.
        */
        const AString *name = GetStringFromKeyPair(item, "Name", "string");
        if (!name || name->IsEmpty())
          name = GetStringFromKeyPair(item, "CFName", "string");
        if (name)
          file.Name = *name;
      }
      /*
      {
        const AString *s = GetStringFromKeyPair(item, "ID", "string");
        if (s)
          file.Id = *s;
      }
      */
      RINOK(file.Parse(rawBuf, destLen))
      if (!file.IsCorrect)
        _headersError = true;
    }
  }

  if (_masterChecksum.IsCrc32())
  {
    UInt32 crc = CRC_INIT_VAL;
    unsigned i;
    for (i = 0; i < _files.Size(); i++)
    {
      const CChecksum &cs = _files[i].Checksum;
      if ((cs.NumBits & 0x7) != 0)
        break;
      const UInt32 len = cs.NumBits >> 3;
      if (len > kChecksumSize_Max)
        break;
      crc = CrcUpdate(crc, cs.Data, (size_t)len);
    }
    if (i == _files.Size())
      _masterCrcError = (CRC_GET_DIGEST(crc) != _masterChecksum.GetCrc32());
  }

  {
    UInt64 sec = 0;
    FOR_VECTOR (i, _files)
    {
      const CFile &file = _files[i];
      /*
      if (file.Descriptor != (Int32)i - 1)
        _headersError = true;
      */
      if (file.StartUnpackSector != sec)
        _headersError = true;
      if (file.NumUnpackSectors >= kSectorNumber_LIMIT)
        _headersError = true;
      sec += file.NumUnpackSectors;
      if (sec >= kSectorNumber_LIMIT)
        _headersError = true;
    }
    if (sec != _numSectors)
      _headersError = true;
  }

  // data checksum calculation can be slow for big dmg file
  if (_dataForkChecksum.IsCrc32())
  {
    UInt64 endPos;
    if (!_dataForkPair.GetEndPos(endPos)
        || _dataForkPair.Offset >= ((UInt64)1 << 63))
      _headersError = true;
    else
    {
      const UInt64 seekPos = _startPos + _dataForkPair.Offset;
      if (seekPos > fileSize
        || endPos > fileSize - _startPos)
      {
        _headersError = true;
        // kpv_ErrorFlags_UnexpectedEnd
      }
      else
      {
        const size_t kBufSize = 1 << 15;
        CAlignedBuffer1 buf2(kBufSize);
        RINOK(InStream_SeekSet(stream, seekPos))
        if (openArchiveCallback)
        {
          RINOK(openArchiveCallback->SetTotal(NULL, &openTotal))
        }
        UInt32 crc = CRC_INIT_VAL;
        UInt64 pos = 0;
        for (;;)
        {
          const UInt64 rem = _dataForkPair.Len - pos;
          size_t cur = kBufSize;
          if (cur > rem)
            cur = (UInt32)rem;
          if (cur == 0)
            break;
          RINOK(ReadStream_FALSE(stream, buf2, cur))
          crc = CrcUpdate(crc, buf2, cur);
          pos += cur;
          openCur += cur;
          if ((pos & ((1 << 24) - 1)) == 0 && openArchiveCallback)
          {
            RINOK(openArchiveCallback->SetCompleted(NULL, &openCur))
          }
        }
        if (_dataForkChecksum.GetCrc32() != CRC_GET_DIGEST(crc))
          _dataForkError = true;
      }
    }
  }

  return S_OK;
}



Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *openArchiveCallback))
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(stream, openArchiveCallback))
  _inStream = stream;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _masterCrcError = false;
  _headersError = false;
  _dataForkError = false;
  _rsrcMode_wasUsed = false;
  _phySize = 0;
  _startPos = 0;
  _name.Empty();
  _inStream.Release();
  _files.Clear();
#ifdef DMG_SHOW_RAW
  _extras.Clear();
#endif
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _files.Size()
#ifdef DMG_SHOW_RAW
    + _extras.Size()
#endif
    ;
  return S_OK;
}

#ifdef DMG_SHOW_RAW
#define RAW_PREFIX "raw" STRING_PATH_SEPARATOR
#endif

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  
#ifdef DMG_SHOW_RAW
  if (index >= _files.Size())
  {
    const CExtraFile &extra = _extras[index - _files.Size()];
    switch (propID)
    {
      case kpidPath:
        prop = (AString)RAW_PREFIX + extra.Name;
        break;
      case kpidSize:
      case kpidPackSize:
        prop = (UInt64)extra.Data.Size();
        break;
    }
  }
  else
#endif
  {
    const CFile &item = _files[index];
    switch (propID)
    {
      case kpidSize:  prop = item.Size; break;
      case kpidPackSize:  prop = item.PackSize; break;
      case kpidCRC:
      {
        if (item.Checksum.IsCrc32() && item.FullFileChecksum)
          prop = item.Checksum.GetCrc32();
        break;
      }
      case kpidChecksum:
      {
        AString s;
        item.Checksum.Print(s);
        if (!s.IsEmpty())
          prop = s;
        break;
      }

      /*
      case kpidOffset:
      {
        prop = item.StartPackPos;
        break;
      }
      */

      case kpidNumBlocks:
          prop = (UInt32)item.Blocks.Size();
          break;
      case kpidClusterSize:
          prop = item.BlockSize_MAX;
          break;

      case kpidMethod:
      {
        AString s;
        if (!item.IsCorrect)
          s.Add_OptSpaced("CORRUPTED");
        CMethods m;
        m.Update(item);
        m.AddToString(s);
        {
          AString s2;
          item.Checksum.PrintType(s2);
          if (!s2.IsEmpty())
            s.Add_OptSpaced(s2);
        }
        if (!s.IsEmpty())
          prop = s;
        break;
      }
      
      case kpidPath:
      {
#ifdef Z7_DMG_SINGLE_FILE_MODE
        prop = "a.img";
#else
        UString name;
        name.Add_UInt32(index);
        unsigned num = 10;
        unsigned numDigits;
        for (numDigits = 1; num < _files.Size(); numDigits++)
          num *= 10;
        while (name.Len() < numDigits)
          name.InsertAtFront(L'0');

        AString subName;
        int pos1 = item.Name.Find('(');
        if (pos1 >= 0)
        {
          pos1++;
          const int pos2 = item.Name.Find(')', pos1);
          if (pos2 >= 0)
          {
            subName.SetFrom(item.Name.Ptr(pos1), pos2 - pos1);
            pos1 = subName.Find(':');
            if (pos1 >= 0)
              subName.DeleteFrom(pos1);
          }
        }
        else
          subName = item.Name; // new apfs dmg can be without braces
        subName.Trim();
        if (!subName.IsEmpty())
        {
          const char *ext = Find_Apple_FS_Ext(subName);
          if (ext)
            subName = ext;
          UString name2;
          ConvertUTF8ToUnicode(subName, name2);
          name.Add_Dot();
          name += name2;
        }
        else
        {
          UString name2;
          ConvertUTF8ToUnicode(item.Name, name2);
          if (!name2.IsEmpty())
            name += "_";
          name += name2;
        }
        prop = name;
#endif

        break;
      }
      
      case kpidComment:
      {
        UString name;
        ConvertUTF8ToUnicode(item.Name, name);
        prop = name;
        break;
      }
      case kpidId:
      {
        prop.Set_Int32((Int32)item.Descriptor);
        /*
        if (!item.Id.IsEmpty())
        {
          UString s;
          ConvertUTF8ToUnicode(item.Id, s);
          prop = s;
        }
        */
        break;
      }
#ifdef Z7_DMG_SINGLE_FILE_MODE
      case kpidPosition:
        prop = item.StartUnpackSector << 9;
        break;
#endif
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


class CAdcDecoder
{
  CLzOutWindow m_OutWindowStream;
  CInBuffer m_InStream;

  class CCoderReleaser Z7_final
  {
    CAdcDecoder *m_Coder;
  public:
    bool NeedFlush;
    CCoderReleaser(CAdcDecoder *coder): m_Coder(coder), NeedFlush(true) {}
    ~CCoderReleaser()
    {
      if (NeedFlush)
        m_Coder->m_OutWindowStream.Flush();
    }
  };
  friend class CCoderReleaser;

public:
  HRESULT Code(ISequentialInStream * const inStream,
    ISequentialOutStream *outStream,
    const UInt64 * const inSize,
    const UInt64 * const outSize,
    ICompressProgressInfo * const progress);
};


HRESULT CAdcDecoder::Code(ISequentialInStream * const inStream,
    ISequentialOutStream *outStream,
    const UInt64 * const inSize,
    const UInt64 * const outSizePtr,
    ICompressProgressInfo * const progress)
{
  try {

  if (!m_OutWindowStream.Create(1 << 18)) // at least (1 << 16) is required here
    return E_OUTOFMEMORY;
  if (!m_InStream.Create(1 << 18))
    return E_OUTOFMEMORY;

  m_OutWindowStream.SetStream(outStream);
  m_OutWindowStream.Init(false);
  m_InStream.SetStream(inStream);
  m_InStream.Init();
  
  CCoderReleaser coderReleaser(this);

  const UInt32 kStep = 1 << 22;
  UInt64 nextLimit = kStep;
  const UInt64 outSize = *outSizePtr;
  UInt64 pos = 0;
  /* match sequences and literal sequences do not cross 64KB range
     in some dmg archive examples. But is it so for any Adc stream? */

  while (pos < outSize)
  {
    if (pos >= nextLimit && progress)
    {
      nextLimit += kStep;
      const UInt64 packSize = m_InStream.GetProcessedSize();
      RINOK(progress->SetRatioInfo(&packSize, &pos))
    }
    Byte b;
    if (!m_InStream.ReadByte(b))
      return S_FALSE;
    const UInt64 rem = outSize - pos;
    if (b & 0x80)
    {
      unsigned num = (unsigned)b - 0x80 + 1;
      if (num > rem)
        return S_FALSE;
      pos += num;
      do
      {
        if (!m_InStream.ReadByte(b))
          return S_FALSE;
        m_OutWindowStream.PutByte(b);
      }
      while (--num);
      continue;
    }
    Byte b1;
    if (!m_InStream.ReadByte(b1))
      return S_FALSE;

    UInt32 len, dist;

    if (b & 0x40)
    {
      len = (UInt32)b - 0x40 + 4;
      Byte b2;
      if (!m_InStream.ReadByte(b2))
        return S_FALSE;
      dist = ((UInt32)b1 << 8) + b2;
    }
    else
    {
      len = ((UInt32)b >> 2) + 3;
      dist = (((UInt32)b & 3) << 8) + b1;
    }

    if (/* dist >= pos || */ len > rem)
      return S_FALSE;
    if (!m_OutWindowStream.CopyBlock(dist, len))
      return S_FALSE;
    pos += len;
  }
  if (*inSize != m_InStream.GetProcessedSize())
    return S_FALSE;
  coderReleaser.NeedFlush = false;
  return m_OutWindowStream.Flush();

  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}



struct CDecoders
{
  CMyComPtr2<ICompressCoder, NCompress::NZlib::CDecoder> zlib;
  CMyComPtr2<ICompressCoder, NCompress::NBZip2::CDecoder> bzip2;
  CMyComPtr2<ICompressCoder, NCompress::NLzfse::CDecoder> lzfse;
  CMyUniquePtr<NCompress::NXz::CDecoder> xz;
  CMyUniquePtr<CAdcDecoder> adc;

  HRESULT Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const CBlock &block, const UInt64 *unpSize, ICompressProgressInfo *progress);
};

HRESULT CDecoders::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const CBlock &block, const UInt64 *unpSize, ICompressProgressInfo *progress)
{
  HRESULT hres;
  UInt64 processed;
  switch (block.Type)
  {
    case METHOD_ADC:
      adc.Create_if_Empty();
      return adc->Code(inStream, outStream, &block.PackSize, unpSize, progress);
    case METHOD_LZFSE:
      lzfse.Create_if_Empty();
      return lzfse.Interface()->Code(inStream, outStream, &block.PackSize, unpSize, progress);
    case METHOD_ZLIB:
      zlib.Create_if_Empty();
      hres = zlib.Interface()->Code(inStream, outStream, NULL, unpSize, progress);
      processed = zlib->GetInputProcessedSize();
      break;
    case METHOD_BZIP2:
      bzip2.Create_if_Empty();
      hres = bzip2.Interface()->Code(inStream, outStream, NULL, unpSize, progress);
      processed = bzip2->GetInputProcessedSize();
      break;
    case METHOD_XZ:
      xz.Create_if_Empty();
      hres = xz->Decode(inStream, outStream, unpSize, true, progress);
      processed = xz->Stat.InSize;
      break;
    default:
      return E_NOTIMPL;
  }
  if (hres == S_OK && processed != block.PackSize)
    hres = S_FALSE;
  return hres;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _files.Size()
#ifdef DMG_SHOW_RAW
    + _extras.Size()
#endif
  ;
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  
  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = allFilesMode ? i : indices[i];
#ifdef DMG_SHOW_RAW
    if (index >= _files.Size())
      totalSize += _extras[index - _files.Size()].Data.Size();
    else
#endif
      totalSize += _files[index].Size;
  }
  RINOK(extractCallback->SetTotal(totalSize))

  const size_t kZeroBufSize = 1 << 14;
  CAlignedBuffer1 zeroBuf(kZeroBufSize);
  memset(zeroBuf, 0, kZeroBufSize);
  
  CDecoders decoders;
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CMyComPtr2_Create<ISequentialInStream, CLimitedSequentialInStream> inStream;
  inStream->SetStream(_inStream);

  UInt64 total_PackSize = 0;
  UInt64 total_UnpackSize = 0;
  UInt64 cur_PackSize = 0;
  UInt64 cur_UnpackSize = 0;

  for (i = 0;; i++,
        total_PackSize   += cur_PackSize,
        total_UnpackSize += cur_UnpackSize)
  {
    lps->InSize = total_PackSize;
    lps->OutSize = total_UnpackSize;
    cur_PackSize = 0;
    cur_UnpackSize = 0;
    RINOK(lps->SetCur())
    if (i >= numItems)
      return S_OK;
    
    Int32 opRes = NExtract::NOperationResult::kOK;
   {
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))

#ifdef DMG_SHOW_RAW
    if (index >= _files.Size())
    {
      const CByteBuffer &buf = _extras[index - _files.Size()].Data;
      if (realOutStream)
        RINOK(WriteStream(realOutStream, buf, buf.Size()))
      cur_PackSize = cur_UnpackSize = buf.Size();
    }
    else
#endif
    {
      const CFile &item = _files[index];
      cur_PackSize = item.PackSize;
      cur_UnpackSize = item.Size;

      if (!item.IsCorrect)
        opRes = NExtract::NOperationResult::kHeadersError;
      else
      {
        CMyComPtr2_Create<ISequentialOutStream, COutStreamWithCRC> outCrcStream;
        outCrcStream->SetStream(realOutStream);
        // realOutStream.Release();
        const bool needCrc = item.Checksum.IsCrc32();
        outCrcStream->Init(needCrc);
        
        CMyComPtr2_Create<ISequentialOutStream, CLimitedSequentialOutStream> outStream;
        outStream->SetStream(outCrcStream);
        
        UInt64 unpPos = 0;
        UInt64 packPos = 0;

        FOR_VECTOR (blockIndex, item.Blocks)
        {
          lps->InSize = total_PackSize + packPos;
          lps->OutSize = total_UnpackSize + unpPos;
          RINOK(lps->SetCur())

          const CBlock &block = item.Blocks[blockIndex];
          // if (!block.ThereAreDataInBlock()) continue;

          packPos += block.PackSize;
          if (block.UnpPos != unpPos)
          {
            opRes = NExtract::NOperationResult::kHeadersError;
            break;
          }

          RINOK(InStream_SeekSet(_inStream, _startPos + _dataForkPair.Offset + item.StartPackPos + block.PackPos))
          inStream->Init(block.PackSize);

          const UInt64 unpSize = item.GetUnpackSize_of_Block(blockIndex);

          outStream->Init(unpSize);
          HRESULT res = S_OK;

          outCrcStream->EnableCalc(needCrc && block.NeedCrc());

          if (block.IsZeroMethod())
          {
            if (block.PackSize != 0)
              opRes = NExtract::NOperationResult::kUnsupportedMethod;
          }
          else if (block.Type == METHOD_COPY)
          {
            if (unpSize != block.PackSize)
              opRes = NExtract::NOperationResult::kUnsupportedMethod;
            else
              res = copyCoder.Interface()->Code(inStream, outStream, NULL, NULL, lps);
          }
          else
            res = decoders.Code(inStream, outStream, block, &unpSize, lps);

          if (res != S_OK)
          {
            if (res != S_FALSE)
            {
              if (res != E_NOTIMPL)
                return res;
              opRes = NExtract::NOperationResult::kUnsupportedMethod;
            }
            if (opRes == NExtract::NOperationResult::kOK)
              opRes = NExtract::NOperationResult::kDataError;
          }
          
          unpPos += unpSize;
          
          if (!outStream->IsFinishedOK())
          {
            if (!block.IsZeroMethod() && opRes == NExtract::NOperationResult::kOK)
              opRes = NExtract::NOperationResult::kDataError;

            for (unsigned k = 0;;)
            {
              const UInt64 rem = outStream->GetRem();
              if (rem == 0)
                break;
              size_t size = kZeroBufSize;
              if (size > rem)
                size = (size_t)rem;
              RINOK(WriteStream(outStream, zeroBuf, size))
              k++;
              if ((k & 0xfff) == 0)
              {
                lps->OutSize = total_UnpackSize + unpPos - outStream->GetRem();
                RINOK(lps->SetCur())
              }
            }
          }
        }
        if (needCrc && opRes == NExtract::NOperationResult::kOK)
        {
          if (outCrcStream->GetCRC() != item.Checksum.GetCrc32())
            opRes = NExtract::NOperationResult::kCRCError;
        }
      }
    }
   }
    RINOK(extractCallback->SetOperationResult(opRes))
  }

  COM_TRY_END
}




struct CChunk
{
  int BlockIndex;
  UInt64 AccessMark;
  Byte *Buf;
  size_t BufSize;

  void Free()
  {
    z7_AlignedFree(Buf);
    Buf = NULL;
    BufSize = 0;
  }
  void Alloc(size_t size)
  {
    Buf = (Byte *)z7_AlignedAlloc(size);
  }
};


Z7_CLASS_IMP_IInStream(
  CInStream
)
  bool _errorMode;
  UInt64 _virtPos;
  int _latestChunk;
  int _latestBlock;
  UInt64 _accessMark;
  UInt64 _chunks_TotalSize;
  CRecordVector<CChunk> _chunks;

public:
  CMyComPtr<IInStream> Stream;
  const CFile *File;
  UInt64 Size;
private:
  UInt64 _startPos;

  ~CInStream();
  CMyComPtr2<ISequentialOutStream, CBufPtrSeqOutStream> outStream;
  CMyComPtr2<ISequentialInStream, CLimitedSequentialInStream> inStream;
  CDecoders decoders;
public:

  // HRESULT
  void Init(UInt64 startPos)
  {
    _errorMode = false;
    _startPos = startPos;
    _virtPos = 0;
    _latestChunk = -1;
    _latestBlock = -1;
    _accessMark = 0;
    _chunks_TotalSize = 0;

    inStream.Create_if_Empty();
    inStream->SetStream(Stream);

    outStream.Create_if_Empty();
    // return S_OK;
  }
};


CInStream::~CInStream()
{
  unsigned i = _chunks.Size();
  while (i)
    _chunks[--i].Free();
}

static unsigned FindBlock(const CRecordVector<CBlock> &blocks, UInt64 pos)
{
  unsigned left = 0, right = blocks.Size();
  for (;;)
  {
    const unsigned mid = (left + right) / 2;
    if (mid == left)
      return left;
    if (pos < blocks[mid].UnpPos)
      right = mid;
    else
      left = mid;
  }
}

Z7_COM7F_IMF(CInStream::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  // COM_TRY_BEGIN
  try {

  if (_errorMode)
    return E_OUTOFMEMORY;

  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;
  if (_virtPos >= Size)
    return S_OK; // (Size == _virtPos) ? S_OK: E_FAIL;
  {
    const UInt64 rem = Size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }

  if (_latestBlock >= 0)
  {
    const CBlock &block = File->Blocks[(unsigned)_latestBlock];
    if (_virtPos < block.UnpPos ||
        _virtPos - block.UnpPos >= File->GetUnpackSize_of_Block((unsigned)_latestBlock))
      _latestBlock = -1;
  }
  
  if (_latestBlock < 0)
  {
    _latestChunk = -1;
    const unsigned blockIndex = FindBlock(File->Blocks, _virtPos);
    const CBlock &block = File->Blocks[blockIndex];
    const UInt64 unpSize = File->GetUnpackSize_of_Block(blockIndex);
    
    if (block.NeedAllocateBuffer()
        && unpSize <= k_Chunk_Size_MAX)
    {
      unsigned i = 0;
      {
        unsigned numChunks = _chunks.Size();
        if (numChunks)
        {
          const CChunk *chunk = _chunks.ConstData();
          do
          {
            if (chunk->BlockIndex == (int)blockIndex)
              break;
            chunk++;
          }
          while (--numChunks);
          i = _chunks.Size() - numChunks;
        }
      }
      if (i != _chunks.Size())
        _latestChunk = (int)i;
      else
      {
        unsigned chunkIndex;
        for (;;)
        {
          if (_chunks.IsEmpty() ||
              (_chunks.Size() < k_NumChunks_MAX
              && _chunks_TotalSize + unpSize <= k_Chunks_TotalSize_MAX))
          {
            CChunk chunk;
            chunk.Buf = NULL;
            chunk.BufSize = 0;
            chunk.BlockIndex = -1;
            chunk.AccessMark = 0;
            chunkIndex = _chunks.Add(chunk);
            break;
          }
          chunkIndex = 0;
          if (_chunks.Size() == 1)
            break;
          {
            const CChunk *chunks = _chunks.ConstData();
            UInt64 accessMark_min = chunks[chunkIndex].AccessMark;
            const unsigned numChunks = _chunks.Size();
            for (i = 1; i < numChunks; i++)
            {
              if (chunks[i].AccessMark < accessMark_min)
              {
                chunkIndex = i;
                accessMark_min = chunks[i].AccessMark;
              }
            }
          }
          {
            CChunk &chunk = _chunks[chunkIndex];
            const UInt64 newTotalSize = _chunks_TotalSize - chunk.BufSize;
            if (newTotalSize + unpSize <= k_Chunks_TotalSize_MAX)
              break;
            _chunks_TotalSize = newTotalSize;
            chunk.Free();
          }
          // we have called chunk.Free() before, because
          // _chunks.Delete() doesn't call chunk.Free().
          _chunks.Delete(chunkIndex);
          PRF(printf("\n++num_chunks=%u, _chunks_TotalSize = %u\n", (unsigned)_chunks.Size(), (unsigned)_chunks_TotalSize);)
        }
        
        CChunk &chunk = _chunks[chunkIndex];
        chunk.BlockIndex = -1;
        chunk.AccessMark = 0;
        
        if (chunk.BufSize < unpSize)
        {
          _chunks_TotalSize -= chunk.BufSize;
          chunk.Free();
          // if (unpSize > k_Chunk_Size_MAX) return E_FAIL;
          chunk.Alloc((size_t)unpSize);
          if (!chunk.Buf)
            return E_OUTOFMEMORY;
          chunk.BufSize = (size_t)unpSize;
          _chunks_TotalSize += chunk.BufSize;
        }
        
        RINOK(InStream_SeekSet(Stream, _startPos + File->StartPackPos + block.PackPos))

        inStream->Init(block.PackSize);
#ifdef Z7_DMG_USE_CACHE_FOR_COPY_BLOCKS
        if (block.Type == METHOD_COPY)
        {
          if (block.PackSize != unpSize)
            return E_FAIL;
          RINOK(ReadStream_FAIL(inStream, chunk.Buf, (size_t)unpSize))
        }
        else
#endif
        {
          outStream->Init(chunk.Buf, (size_t)unpSize);
          RINOK(decoders.Code(inStream, outStream, block, &unpSize, NULL))
          if (outStream->GetPos() != unpSize)
            return E_FAIL;
        }
        chunk.BlockIndex = (int)blockIndex;
        _latestChunk = (int)chunkIndex;
      }
      
      _chunks[_latestChunk].AccessMark = _accessMark++;
    }
  
    _latestBlock = (int)blockIndex;
  }

  const CBlock &block = File->Blocks[(unsigned)_latestBlock];
  const UInt64 offset = _virtPos - block.UnpPos;
  {
    const UInt64 rem = File->GetUnpackSize_of_Block((unsigned)_latestBlock) - offset;
    if (size > rem)
      size = (UInt32)rem;
    if (size == 0) // it's unexpected case. but we check it.
      return S_OK;
  }
  HRESULT res = S_OK;
  
  if (block.IsZeroMethod())
    memset(data, 0, size);
  else if (_latestChunk >= 0)
    memcpy(data, _chunks[_latestChunk].Buf + (size_t)offset, size);
  else
  {
    if (block.Type != METHOD_COPY)
      return E_FAIL;
    RINOK(InStream_SeekSet(Stream, _startPos + File->StartPackPos + block.PackPos + offset))
    res = Stream->Read(data, size, &size);
  }
  
  _virtPos += size;
  if (processedSize)
    *processedSize = size;
  return res;
  // COM_TRY_END
  }
  catch(...)
  {
    _errorMode = true;
    return E_OUTOFMEMORY;
  }
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
  _virtPos = (UInt64)offset;
  if (newPosition)
    *newPosition = (UInt64)offset;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  
#ifdef DMG_SHOW_RAW
  if (index >= _files.Size())
    return S_FALSE;
#endif
  
  CMyComPtr2<ISequentialInStream, CInStream> spec;
  spec.Create_if_Empty();
  spec->File = &_files[index];
  const CFile &file = *spec->File;

  if (!file.IsCorrect)
    return S_FALSE;
  
  FOR_VECTOR (i, file.Blocks)
  {
    const CBlock &block = file.Blocks[i];
    if (!block.NeedAllocateBuffer())
      continue;

    switch (block.Type)
    {
#ifdef Z7_DMG_USE_CACHE_FOR_COPY_BLOCKS
      case METHOD_COPY:
        break;
#endif
      case METHOD_ADC:
      case METHOD_ZLIB:
      case METHOD_BZIP2:
      case METHOD_LZFSE:
      case METHOD_XZ:
      // case METHOD_END:
        if (file.GetUnpackSize_of_Block(i) > k_Chunk_Size_MAX)
          return S_FALSE;
        break;
      default:
        return S_FALSE;
    }
  }
  
  spec->Stream = _inStream;
  spec->Size = spec->File->Size;
  // RINOK(
  spec->Init(_startPos + _dataForkPair.Offset);
  *stream = spec.Detach();
  return S_OK;
  
  COM_TRY_END
}

REGISTER_ARC_I(
  "Dmg", "dmg", NULL, 0xE4,
  k_Signature,
  0,
  NArcInfoFlags::kBackwardOpen |
  NArcInfoFlags::kUseGlobalOffset,
  NULL)

}}
