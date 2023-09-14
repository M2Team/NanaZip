// SquashfsHandler.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"
#include "../../../C/LzmaDec.h"
#include "../../../C/Xz.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyLinux.h"
#include "../../Common/IntToString.h"
#include "../../Common/StringConvert.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/CWrappers.h"
#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/ZlibDecoder.h"
// #include "../Compress/LzmaDecoder.h"

namespace NArchive {
namespace NSquashfs {

static const UInt32 kNumFilesMax = (1 << 28);
static const unsigned kNumDirLevelsMax = (1 << 10);

// Layout: Header, Data, inodes, Directories, Fragments, UIDs, GIDs

/*
#define Get16(p) (be ? GetBe16(p) : GetUi16(p))
#define Get32(p) (be ? GetBe32(p) : GetUi32(p))
#define Get64(p) (be ? GetBe64(p) : GetUi64(p))
*/

static UInt16 Get16b(const Byte *p, bool be) { return be ? GetBe16(p) : GetUi16(p); }
static UInt32 Get32b(const Byte *p, bool be) { return be ? GetBe32(p) : GetUi32(p); }
static UInt64 Get64b(const Byte *p, bool be) { return be ? GetBe64(p) : GetUi64(p); }

#define Get16(p) Get16b(p, be)
#define Get32(p) Get32b(p, be)
#define Get64(p) Get64b(p, be)

#define LE_16(offs, dest) dest = GetUi16(p + (offs))
#define LE_32(offs, dest) dest = GetUi32(p + (offs))
#define LE_64(offs, dest) dest = GetUi64(p + (offs))

#define GET_16(offs, dest) dest = Get16(p + (offs))
#define GET_32(offs, dest) dest = Get32(p + (offs))
#define GET_64(offs, dest) dest = Get64(p + (offs))

static const UInt32 kSignature32_LE = 0x73717368;
static const UInt32 kSignature32_BE = 0x68737173;
static const UInt32 kSignature32_LZ = 0x71736873;
static const UInt32 kSignature32_B2 = 0x73687371;

#define kMethod_ZLIB 1
#define kMethod_LZMA 2
#define kMethod_LZO  3
#define kMethod_XZ   4

static const char * const k_Methods[] =
{
    "0"
  , "ZLIB"
  , "LZMA"
  , "LZO"
  , "XZ"
};

static const unsigned kMetadataBlockSizeLog = 13;
static const UInt32 kMetadataBlockSize = (1 << kMetadataBlockSizeLog);

enum
{
  kType_IPC,
  kType_DIR,
  kType_FILE,
  kType_LNK,
  kType_BLK,
  kType_CHR,
  kType_FIFO,
  kType_SOCK
};

static const UInt32 k_TypeToMode[] =
{
  0,
  MY_LIN_S_IFDIR, MY_LIN_S_IFREG, MY_LIN_S_IFLNK, MY_LIN_S_IFBLK, MY_LIN_S_IFCHR, MY_LIN_S_IFIFO, MY_LIN_S_IFSOCK,
  MY_LIN_S_IFDIR, MY_LIN_S_IFREG, MY_LIN_S_IFLNK, MY_LIN_S_IFBLK, MY_LIN_S_IFCHR, MY_LIN_S_IFIFO, MY_LIN_S_IFSOCK
};


enum
{
  kFlag_UNC_INODES,
  kFlag_UNC_DATA,
  kFlag_CHECK,
  kFlag_UNC_FRAGS,
  kFlag_NO_FRAGS,
  kFlag_ALWAYS_FRAG,
  kFlag_DUPLICATE,
  kFlag_EXPORT
};

static const char * const k_Flags[] =
{
    "UNCOMPRESSED_INODES"
  , "UNCOMPRESSED_DATA"
  , "CHECK"
  , "UNCOMPRESSED_FRAGMENTS"
  , "NO_FRAGMENTS"
  , "ALWAYS_FRAGMENTS"
  , "DUPLICATES_REMOVED"
  , "EXPORTABLE"
  , "UNCOMPRESSED_XATTRS"
  , "NO_XATTRS"
  , "COMPRESSOR_OPTIONS"
  , "UNCOMPRESSED_IDS"
};

static const UInt32 kNotCompressedBit16 = (1 << 15);
static const UInt32 kNotCompressedBit32 = (1 << 24);

#define GET_COMPRESSED_BLOCK_SIZE(size) ((size) & ~kNotCompressedBit32)
#define IS_COMPRESSED_BLOCK(size) (((size) & kNotCompressedBit32) == 0)

// static const UInt32 kHeaderSize1 = 0x33;
// static const UInt32 kHeaderSize2 = 0x3F;
static const UInt32 kHeaderSize3 = 0x77;
// static const UInt32 kHeaderSize4 = 0x60;

struct CHeader
{
  bool be;
  bool SeveralMethods;
  Byte NumUids;
  Byte NumGids;

  UInt32 NumInodes;
  UInt32 CTime;
  UInt32 BlockSize;
  UInt32 NumFrags;
  UInt16 Method;
  UInt16 BlockSizeLog;
  UInt16 Flags;
  UInt16 NumIDs;
  UInt16 Major;
  UInt16 Minor;
  UInt64 RootInode;
  UInt64 Size;
  UInt64 UidTable;
  UInt64 GidTable;
  UInt64 XattrIdTable;
  UInt64 InodeTable;
  UInt64 DirTable;
  UInt64 FragTable;
  UInt64 LookupTable;

  void Parse3(const Byte *p)
  {
    Method = kMethod_ZLIB;
    GET_32 (0x08, Size);
    GET_32 (0x0C, UidTable);
    GET_32 (0x10, GidTable);
    GET_32 (0x14, InodeTable);
    GET_32 (0x18, DirTable);
    GET_16 (0x20, BlockSize);
    GET_16 (0x22, BlockSizeLog);
    Flags   = p[0x24];
    NumUids = p[0x25];
    NumGids = p[0x26];
    GET_32 (0x27, CTime);
    GET_64 (0x2B, RootInode);
    NumFrags = 0;
    FragTable = UidTable;

    if (Major >= 2)
    {
      GET_32 (0x33, BlockSize);
      GET_32 (0x37, NumFrags);
      GET_32 (0x3B, FragTable);
      if (Major == 3)
      {
        GET_64 (0x3F, Size);
        GET_64 (0x47, UidTable);
        GET_64 (0x4F, GidTable);
        GET_64 (0x57, InodeTable);
        GET_64 (0x5F, DirTable);
        GET_64 (0x67, FragTable);
        GET_64 (0x6F, LookupTable);
      }
    }
  }

  void Parse4(const Byte *p)
  {
    LE_32 (0x08, CTime);
    LE_32 (0x0C, BlockSize);
    LE_32 (0x10, NumFrags);
    LE_16 (0x14, Method);
    LE_16 (0x16, BlockSizeLog);
    LE_16 (0x18, Flags);
    LE_16 (0x1A, NumIDs);
    LE_64 (0x20, RootInode);
    LE_64 (0x28, Size);
    LE_64 (0x30, UidTable);
    LE_64 (0x38, XattrIdTable);
    LE_64 (0x40, InodeTable);
    LE_64 (0x48, DirTable);
    LE_64 (0x50, FragTable);
    LE_64 (0x58, LookupTable);
    GidTable = 0;
  }

  bool Parse(const Byte *p)
  {
    be = false;
    SeveralMethods = false;
    switch (GetUi32(p))
    {
      case kSignature32_LE: break;
      case kSignature32_BE: be = true; break;
      case kSignature32_LZ: SeveralMethods = true; break;
      case kSignature32_B2: SeveralMethods = true; be = true; break;
      default: return false;
    }
    GET_32 (4, NumInodes);
    GET_16 (0x1C, Major);
    GET_16 (0x1E, Minor);
    if (Major <= 3)
      Parse3(p);
    else
    {
      if (be)
        return false;
      Parse4(p);
    }
    return
      InodeTable < DirTable &&
      DirTable <= FragTable &&
      FragTable <= Size &&
      UidTable <= Size &&
      BlockSizeLog >= 12 &&
      BlockSizeLog < 31 &&
      BlockSize == ((UInt32)1 << BlockSizeLog);
  }

  bool IsSupported() const { return Major > 0 && Major <= 4 && BlockSizeLog <= 23; }
  bool IsOldVersion() const { return Major < 4; }
  bool NeedCheckData() const { return (Flags & (1 << kFlag_CHECK)) != 0; }
  unsigned GetFileNameOffset() const { return Major <= 2 ? 3 : (Major == 3 ? 5 : 8); }
  unsigned GetSymLinkOffset() const { return Major <= 1 ? 5: (Major <= 2 ? 6: (Major == 3 ? 18 : 24)); }
  unsigned GetSpecGuidIndex() const { return Major <= 1 ? 0xF: 0xFF; }
};

static const UInt32 kFrag_Empty = (UInt32)(Int32)-1;
// static const UInt32 kXattr_Empty = (UInt32)(Int32)-1;

struct CNode
{
  UInt16 Type;
  UInt16 Mode;
  UInt16 Uid;
  UInt16 Gid;
  UInt32 Frag;
  UInt32 Offset;
  // UInt32 MTime;
  // UInt32 Number;
  // UInt32 NumLinks;
  // UInt32 RDev;
  // UInt32 Xattr;
  // UInt32 Parent;
  
  UInt64 FileSize;
  UInt64 StartBlock;
  // UInt64 Sparse;
  
  UInt32 Parse1(const Byte *p, UInt32 size, const CHeader &_h);
  UInt32 Parse2(const Byte *p, UInt32 size, const CHeader &_h);
  UInt32 Parse3(const Byte *p, UInt32 size, const CHeader &_h);
  UInt32 Parse4(const Byte *p, UInt32 size, const CHeader &_h);
  
  bool IsDir() const { return (Type == kType_DIR || Type == kType_DIR + 7); }
  bool IsLink() const { return (Type == kType_LNK || Type == kType_LNK + 7); }
  UInt64 GetSize() const { return IsDir() ? 0 : FileSize; }
  
  bool ThereAreFrags() const { return Frag != kFrag_Empty; }
  UInt64 GetNumBlocks(const CHeader &_h) const
  {
    return (FileSize >> _h.BlockSizeLog) +
      (!ThereAreFrags() && (FileSize & (_h.BlockSize - 1)) != 0);
  }
};

UInt32 CNode::Parse1(const Byte *p, UInt32 size, const CHeader &_h)
{
  const bool be = _h.be;
  if (size < 4)
    return 0;
  {
    const UInt32 t = Get16(p);
    if (be)
    {
      Type = (UInt16)(t >> 12);
      Mode = (UInt16)(t & 0xFFF);
      Uid = (UInt16)(p[2] >> 4);
      Gid = (UInt16)(p[2] & 0xF);
    }
    else
    {
      Type = (UInt16)(t & 0xF);
      Mode = (UInt16)(t >> 4);
      Uid = (UInt16)(p[2] & 0xF);
      Gid = (UInt16)(p[2] >> 4);
    }
  }

  // Xattr = kXattr_Empty;
  // MTime = 0;
  FileSize = 0;
  StartBlock = 0;
  Frag = kFrag_Empty;

  if (Type == 0)
  {
    Byte t = p[3];
    if (be)
    {
      Type = (UInt16)(t >> 4);
      Offset = (UInt16)(t & 0xF);
    }
    else
    {
      Type = (UInt16)(t & 0xF);
      Offset = (UInt16)(t >> 4);
    }
    return (Type == kType_FIFO || Type == kType_SOCK) ? 4 : 0;
  }

  Type--;
  Uid = (UInt16)(Uid + (Type / 5) * 16);
  Type = (UInt16)((Type % 5) + 1);

  if (Type == kType_FILE)
  {
    if (size < 15)
      return 0;
    // GET_32 (3, MTime);
    GET_32 (7, StartBlock);
    UInt32 t;
    GET_32 (11, t);
    FileSize = t;
    UInt32 numBlocks = t >> _h.BlockSizeLog;
    if ((t & (_h.BlockSize - 1)) != 0)
      numBlocks++;
    UInt32 pos = numBlocks * 2 + 15;
    return (pos <= size) ? pos : 0;
  }
  
  if (Type == kType_DIR)
  {
    if (size < 14)
      return 0;
    UInt32 t = Get32(p + 3);
    if (be)
    {
      FileSize = t >> 13;
      Offset = t & 0x1FFF;
    }
    else
    {
      FileSize = t & 0x7FFFF;
      Offset = t >> 19;
    }
    // GET_32 (7, MTime);
    GET_32 (10, StartBlock);
    if (be)
      StartBlock &= 0xFFFFFF;
    else
      StartBlock >>= 8;
    return 14;
  }
  
  if (size < 5)
    return 0;

  if (Type == kType_LNK)
  {
    UInt32 len;
    GET_16 (3, len);
    FileSize = len;
    len += 5;
    return (len <= size) ? len : 0;
  }

  // GET_32 (3, RDev);
  return 5;
}

UInt32 CNode::Parse2(const Byte *p, UInt32 size, const CHeader &_h)
{
  const bool be = _h.be;
  if (size < 4)
    return 0;
  {
    const UInt32 t = Get16(p);
    if (be)
    {
      Type = (UInt16)(t >> 12);
      Mode = (UInt16)(t & 0xFFF);
    }
    else
    {
      Type = (UInt16)(t & 0xF);
      Mode = (UInt16)(t >> 4);
    }
  }

  Uid = p[2];
  Gid = p[3];

  // Xattr = kXattr_Empty;

  if (Type == kType_FILE)
  {
    if (size < 24)
      return 0;
    // GET_32 (4, MTime);
    GET_32 (8, StartBlock);
    GET_32 (12, Frag);
    GET_32 (16, Offset);
    UInt32 t;
    GET_32 (20, t);
    FileSize = t;
    UInt32 numBlocks = t >> _h.BlockSizeLog;
    if (!ThereAreFrags() && (t & (_h.BlockSize - 1)) != 0)
      numBlocks++;
    UInt32 pos = numBlocks * 4 + 24;
    return (pos <= size) ? (UInt32)pos : 0;
  }

  FileSize = 0;
  // MTime = 0;
  StartBlock = 0;
  Frag = kFrag_Empty;
  
  if (Type == kType_DIR)
  {
    if (size < 15)
      return 0;
    UInt32 t = Get32(p + 4);
    if (be)
    {
      FileSize = t >> 13;
      Offset = t & 0x1FFF;
    }
    else
    {
      FileSize = t & 0x7FFFF;
      Offset = t >> 19;
    }
    // GET_32 (8, MTime);
    GET_32 (11, StartBlock);
    if (be)
      StartBlock &= 0xFFFFFF;
    else
      StartBlock >>= 8;
    return 15;
  }
  
  if (Type == kType_DIR + 7)
  {
    if (size < 18)
      return 0;
    UInt32 t = Get32(p + 4);
    UInt32 t2 = Get16(p + 7);
    if (be)
    {
      FileSize = t >> 5;
      Offset = t2 & 0x1FFF;
    }
    else
    {
      FileSize = t & 0x7FFFFFF;
      Offset = t2 >> 3;
    }
    // GET_32 (9, MTime);
    GET_32 (12, StartBlock);
    if (be)
      StartBlock &= 0xFFFFFF;
    else
      StartBlock >>= 8;
    UInt32 iCount;
    GET_16 (16, iCount);
    UInt32 pos = 18;
    for (UInt32 i = 0; i < iCount; i++)
    {
      // 27 bits: index
      // 29 bits: startBlock
      if (pos + 8 > size)
        return 0;
      pos += 8 + (UInt32)p[pos + 7] + 1; // nameSize
      if (pos > size)
        return 0;
    }
    return pos;
  }

  if (Type == kType_FIFO || Type == kType_SOCK)
    return 4;

  if (size < 6)
    return 0;
  
  if (Type == kType_LNK)
  {
    UInt32 len;
    GET_16 (4, len);
    FileSize = len;
    len += 6;
    return (len <= size) ? len : 0;
  }
  
  if (Type == kType_BLK || Type == kType_CHR)
  {
    // GET_16 (4, RDev);
    return 6;
  }
  
  return 0;
}

UInt32 CNode::Parse3(const Byte *p, UInt32 size, const CHeader &_h)
{
  const bool be = _h.be;
  if (size < 12)
    return 0;
  
  {
    const UInt32 t = Get16(p);
    if (be)
    {
      Type = (UInt16)(t >> 12);
      Mode = (UInt16)(t & 0xFFF);
    }
    else
    {
      Type = (UInt16)(t & 0xF);
      Mode = (UInt16)(t >> 4);
    }
  }

  Uid = p[2];
  Gid = p[3];
  // GET_32 (4, MTime);
  // GET_32 (8, Number);
  // Xattr = kXattr_Empty;
  FileSize = 0;
  StartBlock = 0;
  
  if (Type == kType_FILE || Type == kType_FILE + 7)
  {
    UInt32 offset;
    if (Type == kType_FILE)
    {
      if (size < 32)
        return 0;
      GET_64 (12, StartBlock);
      GET_32 (20, Frag);
      GET_32 (24, Offset);
      GET_32 (28, FileSize);
      offset = 32;
    }
    else
    {
      if (size < 40)
        return 0;
      // GET_32 (12, NumLinks);
      GET_64 (16, StartBlock);
      GET_32 (24, Frag);
      GET_32 (28, Offset);
      GET_64 (32, FileSize);
      offset = 40;
    }
    UInt64 pos = GetNumBlocks(_h) * 4 + offset;
    return (pos <= size) ? (UInt32)pos : 0;
  }

  if (size < 16)
    return 0;
  // GET_32 (12, NumLinks);
 
  if (Type == kType_DIR)
  {
    if (size < 28)
      return 0;
    UInt32 t = Get32(p + 16);
    if (be)
    {
      FileSize = t >> 13;
      Offset = t & 0x1FFF;
    }
    else
    {
      FileSize = t & 0x7FFFF;
      Offset = t >> 19;
    }
    GET_32 (20, StartBlock);
    // GET_32 (24, Parent);
    return 28;
  }
  
  if (Type == kType_DIR + 7)
  {
    if (size < 31)
      return 0;
    UInt32 t = Get32(p + 16);
    UInt32 t2 = Get16(p + 19);
    if (be)
    {
      FileSize = t >> 5;
      Offset = t2 & 0x1FFF;
    }
    else
    {
      FileSize = t & 0x7FFFFFF;
      Offset = t2 >> 3;
    }
    GET_32 (21, StartBlock);
    UInt32 iCount;
    GET_16 (25, iCount);
    // GET_32 (27, Parent);
    UInt32 pos = 31;
    for (UInt32 i = 0; i < iCount; i++)
    {
      // UInt32 index
      // UInt32 startBlock
      if (pos + 9 > size)
        return 0;
      pos += 9 + (unsigned)p[pos + 8] + 1; // nameSize
      if (pos > size)
        return 0;
    }
    return pos;
  }

  if (Type == kType_FIFO || Type == kType_SOCK)
    return 16;
  
  if (size < 18)
    return 0;
  if (Type == kType_LNK)
  {
    UInt32 len;
    GET_16 (16, len);
    FileSize = len;
    len += 18;
    return (len <= size) ? len : 0;
  }

  if (Type == kType_BLK || Type == kType_CHR)
  {
    // GET_16 (16, RDev);
    return 18;
  }
  
  return 0;
}

UInt32 CNode::Parse4(const Byte *p, UInt32 size, const CHeader &_h)
{
  if (size < 20)
    return 0;
  LE_16 (0, Type);
  LE_16 (2, Mode);
  LE_16 (4, Uid);
  LE_16 (6, Gid);
  // LE_32 (8, MTime);
  // LE_32 (12, Number);
  
  // Xattr = kXattr_Empty;
  FileSize = 0;
  StartBlock = 0;
  
  if (Type == kType_FILE || Type == kType_FILE + 7)
  {
    UInt32 offset;
    if (Type == kType_FILE)
    {
      if (size < 32)
        return 0;
      LE_32 (16, StartBlock);
      LE_32 (20, Frag);
      LE_32 (24, Offset);
      LE_32 (28, FileSize);
      offset = 32;
    }
    else
    {
      if (size < 56)
        return 0;
      LE_64 (16, StartBlock);
      LE_64 (24, FileSize);
      // LE_64 (32, Sparse);
      // LE_32 (40, NumLinks);
      LE_32 (44, Frag);
      LE_32 (48, Offset);
      // LE_32 (52, Xattr);
      offset = 56;
    }
    UInt64 pos = GetNumBlocks(_h) * 4 + offset;
    return (pos <= size) ? (UInt32)pos : 0;
  }
  
  if (Type == kType_DIR)
  {
    if (size < 32)
      return 0;
    LE_32 (16, StartBlock);
    // LE_32 (20, NumLinks);
    LE_16 (24, FileSize);
    LE_16 (26, Offset);
    // LE_32 (28, Parent);
    return 32;
  }
  
  // LE_32 (16, NumLinks);

  if (Type == kType_DIR + 7)
  {
    if (size < 40)
      return 0;
    LE_32 (20, FileSize);
    LE_32 (24, StartBlock);
    // LE_32 (28, Parent);
    UInt32 iCount;
    LE_16 (32, iCount);
    LE_16 (34, Offset);
    // LE_32 (36, Xattr);

    UInt32 pos = 40;
    for (UInt32 i = 0; i < iCount; i++)
    {
      // UInt32 index
      // UInt32 startBlock
      if (pos + 12 > size)
        return 0;
      UInt32 nameLen = GetUi32(p + pos + 8);
      pos += 12 + nameLen + 1;
      if (pos > size || nameLen > (1 << 10))
        return 0;
    }
    return pos;
  }
  
  unsigned offset = 20;
  switch (Type)
  {
    case kType_FIFO: case kType_FIFO + 7:
    case kType_SOCK: case kType_SOCK + 7:
      break;
    case kType_LNK: case kType_LNK + 7:
    {
      if (size < 24)
        return 0;
      UInt32 len;
      LE_32 (20, len);
      FileSize = len;
      offset = len + 24;
      if (size < offset || len > (1 << 30))
        return 0;
      break;
    }
    case kType_BLK: case kType_BLK + 7:
    case kType_CHR: case kType_CHR + 7:
      if (size < 24)
        return 0;
      // LE_32 (20, RDev);
      offset = 24;
      break;
    default:
      return 0;
  }
  
  if (Type >= 8)
  {
    if (size < offset + 4)
      return 0;
    // LE_32 (offset, Xattr);
    offset += 4;
  }
  return offset;
}

struct CItem
{
  int Node;
  int Parent;
  UInt32 Ptr;

  CItem(): Node(-1), Parent(-1), Ptr(0) {}
};

struct CData
{
  CByteBuffer Data;
  CRecordVector<UInt32> PackPos;
  CRecordVector<UInt32> UnpackPos; // additional item at the end contains TotalUnpackSize
  
  UInt32 GetNumBlocks() const { return PackPos.Size(); }
  void Clear()
  {
    Data.Free();
    PackPos.Clear();
    UnpackPos.Clear();
  }
};

struct CFrag
{
  UInt64 StartBlock;
  UInt32 Size;
};


Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  CRecordVector<CItem> _items;
  CRecordVector<CNode> _nodes;
  CRecordVector<UInt32> _nodesPos;
  CRecordVector<UInt32> _blockToNode;
  CData _inodesData;
  CData _dirs;
  CRecordVector<CFrag> _frags;
  CByteBuffer _uids;
  CByteBuffer _gids;
  CHeader _h;
  bool _noPropsLZMA;
  bool _needCheckLzma;
  
  UInt32 _openCodePage;

  CMyComPtr<IInStream> _stream;
  UInt64 _sizeCalculated;

  IArchiveOpenCallback *_openCallback;

  int _nodeIndex;
  CRecordVector<bool> _blockCompressed;
  CRecordVector<UInt64> _blockOffsets;
  
  CByteBuffer _cachedBlock;
  UInt64 _cachedBlockStartPos;
  UInt32 _cachedPackBlockSize;
  UInt32 _cachedUnpackBlockSize;

  CLimitedSequentialInStream *_limitedInStreamSpec;
  CMyComPtr<ISequentialInStream> _limitedInStream;

  CBufPtrSeqOutStream *_outStreamSpec;
  CMyComPtr<ISequentialOutStream> _outStream;

  // NCompress::NLzma::CDecoder *_lzmaDecoderSpec;
  // CMyComPtr<ICompressCoder> _lzmaDecoder;

  NCompress::NZlib::CDecoder *_zlibDecoderSpec;
  CMyComPtr<ICompressCoder> _zlibDecoder;
  
  CXzUnpacker _xz;

  CByteBuffer _inputBuffer;

  CDynBufSeqOutStream *_dynOutStreamSpec;
  CMyComPtr<ISequentialOutStream> _dynOutStream;

  void ClearCache()
  {
    _cachedBlockStartPos = 0;
    _cachedPackBlockSize = 0;
    _cachedUnpackBlockSize = 0;
  }

  HRESULT Seek2(UInt64 offset)
  {
    return InStream_SeekSet(_stream, offset);
  }

  HRESULT Decompress(ISequentialOutStream *outStream, Byte *outBuf, bool *outBufWasWritten, UInt32 *outBufWasWrittenSize,
      UInt32 inSize, UInt32 outSizeMax);
  HRESULT ReadMetadataBlock(UInt32 &packSize);
  HRESULT ReadMetadataBlock2();
  HRESULT ReadData(CData &data, UInt64 start, UInt64 end);

  HRESULT OpenDir(int parent, UInt32 startBlock, UInt32 offset, unsigned level, int &nodeIndex);
  HRESULT ScanInodes(UInt64 ptr);
  HRESULT ReadUids(UInt64 start, UInt32 num, CByteBuffer &ids);
  HRESULT Open2(IInStream *inStream);
  AString GetPath(unsigned index) const;
  bool GetPackSize(unsigned index, UInt64 &res, bool fillOffsets);

public:
  CHandler();
  ~CHandler()
  {
    XzUnpacker_Free(&_xz);
  }

  HRESULT ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize);
};


CHandler::CHandler()
{
  XzUnpacker_Construct(&_xz, &g_Alloc);

  _limitedInStreamSpec = new CLimitedSequentialInStream;
  _limitedInStream = _limitedInStreamSpec;

  _outStreamSpec = new CBufPtrSeqOutStream();
  _outStream = _outStreamSpec;

  _dynOutStreamSpec = new CDynBufSeqOutStream;
  _dynOutStream = _dynOutStreamSpec;
}

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidPosixAttrib,
  kpidUserId,
  kpidGroupId
  // kpidLinks,
  // kpidOffset
};

static const Byte kArcProps[] =
{
  kpidHeadersSize,
  kpidFileSystem,
  kpidMethod,
  kpidClusterSize,
  kpidBigEndian,
  kpidCTime,
  kpidCharacts,
  kpidCodePage
  // kpidNumBlocks
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static HRESULT LzoDecode(Byte *dest, SizeT *destLen, const Byte *src, SizeT *srcLen)
{
  SizeT destRem = *destLen;
  SizeT srcRem = *srcLen;
  *destLen = 0;
  *srcLen = 0;
  const Byte *destStart = dest;
  const Byte *srcStart = src;
  unsigned mode = 0;

  {
    if (srcRem == 0)
      return S_FALSE;
    UInt32 b = *src;
    if (b > 17)
    {
      src++;
      srcRem--;
      b -= 17;
      mode = (b < 4 ? 1 : 4);
      if (b > srcRem || b > destRem)
        return S_FALSE;
      srcRem -= b;
      destRem -= b;
      do
        *dest++ = *src++;
      while (--b);
    }
  }

  for (;;)
  {
    if (srcRem < 3)
      return S_FALSE;
    UInt32 b = *src++;
    srcRem--;
    UInt32 len, back;
    
    if (b >= 64)
    {
      srcRem--;
      back = ((b >> 2) & 7) + ((UInt32)*src++ << 3);
      len = (b >> 5) + 1;
    }
    else if (b < 16)
    {
      if (mode == 0)
      {
        if (b == 0)
        {
          for (b = 15;; b += 255)
          {
            if (srcRem == 0)
              return S_FALSE;
            UInt32 b2 = *src++;
            srcRem--;
            if (b2 != 0)
            {
              b += b2;
              break;
            }
          }
        }
      
        b += 3;
        if (b > srcRem || b > destRem)
          return S_FALSE;
        srcRem -= b;
        destRem -= b;
        mode = 4;
        do
          *dest++ = *src++;
        while (--b);
        continue;
      }
      
      srcRem--;
      back = (b >> 2) + ((UInt32)*src++ << 2);
      len = 2;
      if (mode == 4)
      {
        back += (1 << 11);
        len = 3;
      }
    }
    else
    {
      UInt32 bOld = b;
      b = (b < 32 ? 7 : 31);
      len = bOld & b;
      
      if (len == 0)
      {
        for (len = b;; len += 255)
        {
          if (srcRem == 0)
            return S_FALSE;
          UInt32 b2 = *src++;
          srcRem--;
          if (b2 != 0)
          {
            len += b2;
            break;
          }
        }
      }
      
      len += 2;
      if (srcRem < 2)
        return S_FALSE;
      b = *src;
      back = (b >> 2) + ((UInt32)src[1] << 6);
      src += 2;
      srcRem -= 2;
      if (bOld < 32)
      {
        back += ((bOld & 8) << 11);
        if (back == 0)
        {
          *destLen = (size_t)(dest - destStart);
          *srcLen = (size_t)(src - srcStart);
          return S_OK;
        }
        back += (1 << 14) - 1;
      }
    }
    
    back++;
    if (len > destRem || (size_t)(dest - destStart) < back)
      return S_FALSE;
    destRem -= len;
    Byte *destTemp = dest - back;
    dest += len;
    
    do
    {
      *(destTemp + back) = *destTemp;
      destTemp++;
    }
    while (--len);
    
    b &= 3;
    mode = b;
    if (b == 0)
      continue;
    if (b > srcRem || b > destRem)
      return S_FALSE;
    srcRem -= b;
    destRem -= b;
    *dest++ = *src++;
    if (b > 1)
    {
      *dest++ = *src++;
      if (b > 2)
        *dest++ = *src++;
    }
  }
}

HRESULT CHandler::Decompress(ISequentialOutStream *outStream, Byte *outBuf, bool *outBufWasWritten, UInt32 *outBufWasWrittenSize, UInt32 inSize, UInt32 outSizeMax)
{
  if (outBuf)
  {
    *outBufWasWritten = false;
    *outBufWasWrittenSize = 0;
  }
  UInt32 method = _h.Method;
  if (_h.SeveralMethods)
  {
    Byte b;
    RINOK(ReadStream_FALSE(_stream, &b, 1))
    RINOK(_stream->Seek(-1, STREAM_SEEK_CUR, NULL))
    method = (b == 0x5D ? kMethod_LZMA : kMethod_ZLIB);
  }

  if (method == kMethod_ZLIB && _needCheckLzma)
  {
    Byte b;
    RINOK(ReadStream_FALSE(_stream, &b, 1))
    RINOK(_stream->Seek(-1, STREAM_SEEK_CUR, NULL))
    if (b == 0)
    {
      _noPropsLZMA = true;
      method = _h.Method = kMethod_LZMA;
    }
    _needCheckLzma = false;
  }
  
  if (method == kMethod_ZLIB)
  {
    if (!_zlibDecoder)
    {
      _zlibDecoderSpec = new NCompress::NZlib::CDecoder();
      _zlibDecoder = _zlibDecoderSpec;
    }
    RINOK(_zlibDecoder->Code(_limitedInStream, outStream, NULL, NULL, NULL))
    if (inSize != _zlibDecoderSpec->GetInputProcessedSize())
      return S_FALSE;
  }
  /*
  else if (method == kMethod_LZMA)
  {
    if (!_lzmaDecoder)
    {
      _lzmaDecoderSpec = new NCompress::NLzma::CDecoder();
      // _lzmaDecoderSpec->FinishStream = true;
      _lzmaDecoder = _lzmaDecoderSpec;
    }
    const UInt32 kPropsSize = LZMA_PROPS_SIZE + 8;
    Byte props[kPropsSize];
    UInt32 propsSize;
    UInt64 outSize;
    if (_noPropsLZMA)
    {
      props[0] = 0x5D;
      SetUi32(&props[1], _h.BlockSize);
      propsSize = 0;
      outSize = outSizeMax;
    }
    else
    {
      RINOK(ReadStream_FALSE(_limitedInStream, props, kPropsSize));
      propsSize = kPropsSize;
      outSize = GetUi64(&props[LZMA_PROPS_SIZE]);
      if (outSize > outSizeMax)
        return S_FALSE;
    }
    RINOK(_lzmaDecoderSpec->SetDecoderProperties2(props, LZMA_PROPS_SIZE));
    RINOK(_lzmaDecoder->Code(_limitedInStream, outStream, NULL, &outSize, NULL));
    if (inSize != propsSize + _lzmaDecoderSpec->GetInputProcessedSize())
      return S_FALSE;
  }
  */
  else
  {
    if (_inputBuffer.Size() < inSize)
      _inputBuffer.Alloc(inSize);
    RINOK(ReadStream_FALSE(_stream, _inputBuffer, inSize))

    Byte *dest = outBuf;
    if (!outBuf)
    {
      dest = _dynOutStreamSpec->GetBufPtrForWriting(outSizeMax);
      if (!dest)
        return E_OUTOFMEMORY;
    }
    
    SizeT destLen = outSizeMax, srcLen = inSize;

    if (method == kMethod_LZO)
    {
      RINOK(LzoDecode(dest, &destLen, _inputBuffer, &srcLen))
    }
    else if (method == kMethod_LZMA)
    {
      Byte props[5];
      const Byte *src = _inputBuffer;

      if (_noPropsLZMA)
      {
        props[0] = 0x5D;
        SetUi32(&props[1], _h.BlockSize)
      }
      else
      {
        const UInt32 kPropsSize = LZMA_PROPS_SIZE + 8;
        if (inSize < kPropsSize)
          return S_FALSE;
        memcpy(props, src, LZMA_PROPS_SIZE);
        UInt64 outSize = GetUi64(src + LZMA_PROPS_SIZE);
        if (outSize > outSizeMax)
          return S_FALSE;
        destLen = (SizeT)outSize;
        src += kPropsSize;
        inSize -= kPropsSize;
        srcLen = inSize;
      }

      ELzmaStatus status;
      SRes res = LzmaDecode(dest, &destLen,
          src, &srcLen,
          props, LZMA_PROPS_SIZE,
          LZMA_FINISH_END,
          &status, &g_Alloc);
      if (res != 0)
        return SResToHRESULT(res);
      if (status != LZMA_STATUS_FINISHED_WITH_MARK
          && status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
        return S_FALSE;
    }
    else
    {
      ECoderStatus status;
      SRes res = XzUnpacker_CodeFull(&_xz,
          dest, &destLen,
          _inputBuffer, &srcLen,
          CODER_FINISH_END, &status);
      if (res != 0)
        return SResToHRESULT(res);
      if (status != CODER_STATUS_NEEDS_MORE_INPUT || !XzUnpacker_IsStreamWasFinished(&_xz))
        return S_FALSE;
    }
    
    if (inSize != srcLen)
      return S_FALSE;
    if (outBuf)
    {
      *outBufWasWritten = true;
      *outBufWasWrittenSize = (UInt32)destLen;
    }
    else
      _dynOutStreamSpec->UpdateSize(destLen);
  }
  return S_OK;
}

HRESULT CHandler::ReadMetadataBlock(UInt32 &packSize)
{
  Byte temp[3];
  const unsigned offset = _h.NeedCheckData() ? 3 : 2;
  if (offset > packSize)
    return S_FALSE;
  RINOK(ReadStream_FALSE(_stream, temp, offset))
  // if (NeedCheckData && Major < 4) checkByte must be = 0xFF
  const bool be = _h.be;
  UInt32 size = Get16(temp);
  const bool isCompressed = ((size & kNotCompressedBit16) == 0);
  if (size != kNotCompressedBit16)
    size &= ~kNotCompressedBit16;

  if (size > kMetadataBlockSize || offset + size > packSize)
    return S_FALSE;
  packSize = offset + size;
  if (isCompressed)
  {
    _limitedInStreamSpec->Init(size);
    RINOK(Decompress(_dynOutStream, NULL, NULL, NULL, size, kMetadataBlockSize))
  }
  else
  {
    // size != 0 here
    Byte *buf = _dynOutStreamSpec->GetBufPtrForWriting(size);
    if (!buf)
      return E_OUTOFMEMORY;
    RINOK(ReadStream_FALSE(_stream, buf, size))
    _dynOutStreamSpec->UpdateSize(size);
  }
  return S_OK;
}


HRESULT CHandler::ReadMetadataBlock2()
{
  _dynOutStreamSpec->Init();
  UInt32 packSize = kMetadataBlockSize + 3; // check it
  return ReadMetadataBlock(packSize);
}

HRESULT CHandler::ReadData(CData &data, UInt64 start, UInt64 end)
{
  if (end < start || end - start >= ((UInt64)1 << 32))
    return S_FALSE;
  const UInt32 size = (UInt32)(end - start);
  RINOK(Seek2(start))
  _dynOutStreamSpec->Init();
  UInt32 packPos = 0;
  while (packPos != size)
  {
    data.PackPos.Add(packPos);
    data.UnpackPos.Add((UInt32)_dynOutStreamSpec->GetSize());
    if (packPos > size)
      return S_FALSE;
    UInt32 packSize = size - packPos;
    RINOK(ReadMetadataBlock(packSize))
    {
      const size_t tSize = _dynOutStreamSpec->GetSize();
      if (tSize != (UInt32)tSize)
        return S_FALSE;
    }
    packPos += packSize;
  }
  data.UnpackPos.Add((UInt32)_dynOutStreamSpec->GetSize());
  _dynOutStreamSpec->CopyToBuffer(data.Data);
  return S_OK;
}

struct CTempItem
{
  UInt32 StartBlock;
  // UInt32 iNodeNumber1;
  UInt32 Offset;
  // UInt16 iNodeNumber2;
  UInt16 Type;
};
  
HRESULT CHandler::OpenDir(int parent, UInt32 startBlock, UInt32 offset, unsigned level, int &nodeIndex)
{
  if (level > kNumDirLevelsMax)
    return S_FALSE;

  int blockIndex = _inodesData.PackPos.FindInSorted(startBlock);
  if (blockIndex < 0)
    return S_FALSE;
  UInt32 unpackPos = _inodesData.UnpackPos[blockIndex] + offset;
  if (unpackPos < offset)
    return S_FALSE;

  nodeIndex = _nodesPos.FindInSorted(unpackPos, _blockToNode[blockIndex], _blockToNode[blockIndex + 1]);
  // nodeIndex = _nodesPos.FindInSorted(unpackPos);
  if (nodeIndex < 0)
    return S_FALSE;
  
  const CNode &n = _nodes[nodeIndex];
  if (!n.IsDir())
    return S_OK;
  blockIndex = _dirs.PackPos.FindInSorted((UInt32)n.StartBlock);
  if (blockIndex < 0)
    return S_FALSE;
  unpackPos = _dirs.UnpackPos[blockIndex] + n.Offset;
  if (unpackPos < n.Offset || unpackPos > _dirs.Data.Size())
    return S_FALSE;

  UInt32 rem = (UInt32)_dirs.Data.Size() - unpackPos;
  const Byte *p = _dirs.Data + unpackPos;
  UInt32 fileSize = (UInt32)n.FileSize;

  // for some squashfs files: fileSize = rem + 3  !!!
  if (_h.Major >= 3)
  {
    if (fileSize < 3)
      return S_FALSE;
    fileSize -= 3;
  }
  if (fileSize > rem)
    return S_FALSE;
  rem = fileSize;

  AString tempString;

  CRecordVector<CTempItem> tempItems;
  while (rem != 0)
  {
    const bool be = _h.be;
    UInt32 count;
    CTempItem tempItem;
    if (_h.Major <= 2)
    {
      if (rem < 4)
        return S_FALSE;
      count = p[0];
      tempItem.StartBlock = Get32(p);
      if (be)
        tempItem.StartBlock &= 0xFFFFFF;
      else
        tempItem.StartBlock >>= 8;
      p += 4;
      rem -= 4;
    }
    else
    {
      if (_h.Major == 3)
      {
        if (rem < 9)
          return S_FALSE;
        count = p[0];
        p += 1;
        rem -= 1;
      }
      else
      {
        if (rem < 12)
          return S_FALSE;
        count = GetUi32(p);
        p += 4;
        rem -= 4;
      }
      GET_32 (0, tempItem.StartBlock);
      // GET_32 (4, tempItem.iNodeNumber1);
      p += 8;
      rem -= 8;
    }
    count++;
    
    for (UInt32 i = 0; i < count; i++)
    {
      if (rem == 0)
        return S_FALSE;

      UInt32 nameOffset = _h.GetFileNameOffset();
      if (rem < nameOffset)
        return S_FALSE;

      if ((UInt32)_items.Size() >= kNumFilesMax)
        return S_FALSE;
      if (_openCallback)
      {
        UInt64 numFiles = _items.Size();
        if ((numFiles & 0xFFFF) == 0)
        {
          RINOK(_openCallback->SetCompleted(&numFiles, NULL))
        }
      }
      
      CItem item;
      item.Ptr = (UInt32)(p - (const Byte *)_dirs.Data);

      UInt32 size;
      if (_h.IsOldVersion())
      {
        UInt32 t = Get16(p);
        if (be)
        {
          tempItem.Offset = t >> 3;
          tempItem.Type = (UInt16)(t & 0x7);
        }
        else
        {
          tempItem.Offset = t & 0x1FFF;
          tempItem.Type = (UInt16)(t >> 13);
        }
        size = (UInt32)p[2];
        /*
        if (_h.Major > 2)
          tempItem.iNodeNumber2 = Get16(p + 3);
        */
      }
      else
      {
        GET_16 (0, tempItem.Offset);
        // GET_16 (2, tempItem.iNodeNumber2);
        GET_16 (4, tempItem.Type);
        GET_16 (6, size);
      }
      p += nameOffset;
      rem -= nameOffset;
      size++;
      if (rem < size)
        return S_FALSE;

      if (_openCodePage == CP_UTF8)
      {
        tempString.SetFrom_CalcLen((const char *)p, size);
        if (!CheckUTF8_AString(tempString))
          _openCodePage = CP_OEMCP;
      }

      p += size;
      rem -= size;
      item.Parent = parent;
      _items.Add(item);
      tempItems.Add(tempItem);
    }
  }

  const unsigned startItemIndex = _items.Size() - tempItems.Size();
  FOR_VECTOR (i, tempItems)
  {
    const CTempItem &tempItem = tempItems[i];
    const unsigned index = startItemIndex + i;
    CItem &item = _items[index];
    RINOK(OpenDir((int)index, tempItem.StartBlock, tempItem.Offset, level + 1, item.Node))
  }

  return S_OK;
}

HRESULT CHandler::ReadUids(UInt64 start, UInt32 num, CByteBuffer &ids)
{
  const size_t size = (size_t)num * 4;
  ids.Alloc(size);
  if (num == 0)
    return S_OK;
  RINOK(Seek2(start))
  return ReadStream_FALSE(_stream, ids, size);
}

HRESULT CHandler::Open2(IInStream *inStream)
{
  {
    Byte buf[kHeaderSize3];
    RINOK(ReadStream_FALSE(inStream, buf, kHeaderSize3))
    if (!_h.Parse(buf))
      return S_FALSE;
    if (!_h.IsSupported())
      return E_NOTIMPL;
    
    _noPropsLZMA = false;
    _needCheckLzma = false;
    switch (_h.Method)
    {
      case kMethod_ZLIB: _needCheckLzma = true; break;
      case kMethod_LZMA:
      case kMethod_LZO:
      case kMethod_XZ:
        break;
      default:
        return E_NOTIMPL;
    }
  }

  _stream = inStream;

  if (_h.NumFrags != 0)
  {
    if (_h.NumFrags > kNumFilesMax)
      return S_FALSE;
    _frags.ClearAndReserve(_h.NumFrags);
    const unsigned bigFrag = (_h.Major > 2);
    
    const unsigned fragPtrsInBlockLog = kMetadataBlockSizeLog - (3 + bigFrag);
    const UInt32 numBlocks = (_h.NumFrags + (1 << fragPtrsInBlockLog) - 1) >> fragPtrsInBlockLog;
    const size_t numBlocksBytes = (size_t)numBlocks << (2 + bigFrag);
    CByteBuffer data(numBlocksBytes);
    RINOK(Seek2(_h.FragTable))
    RINOK(ReadStream_FALSE(inStream, data, numBlocksBytes))
    const bool be = _h.be;
    
    for (UInt32 i = 0; i < numBlocks; i++)
    {
      const UInt64 offset = bigFrag ? Get64(data + i * 8) : Get32(data + i * 4);
      RINOK(Seek2(offset))
      RINOK(ReadMetadataBlock2())
      const UInt32 unpackSize = (UInt32)_dynOutStreamSpec->GetSize();
      if (unpackSize != kMetadataBlockSize)
        if (i != numBlocks - 1 || unpackSize != ((_h.NumFrags << (3 + bigFrag)) & (kMetadataBlockSize - 1)))
          return S_FALSE;
      const Byte *buf = _dynOutStreamSpec->GetBuffer();
      for (UInt32 j = 0; j < kMetadataBlockSize && j < unpackSize;)
      {
        CFrag frag;
        if (bigFrag)
        {
          frag.StartBlock = Get64(buf + j);
          frag.Size = Get32(buf + j + 8);
          // some archives contain nonzero in unused (buf + j + 12)
          j += 16;
        }
        else
        {
          frag.StartBlock = Get32(buf + j);
          frag.Size = Get32(buf + j + 4);
          j += 8;
        }
        _frags.Add(frag);
      }
    }
    if ((UInt32)_frags.Size() != _h.NumFrags)
      return S_FALSE;
  }

  RINOK(ReadData(_inodesData, _h.InodeTable, _h.DirTable))
  RINOK(ReadData(_dirs, _h.DirTable, _h.FragTable))

  UInt64 absOffset = _h.RootInode >> 16;
  if (absOffset >= ((UInt64)1 << 32))
    return S_FALSE;
  {
    UInt32 pos = 0;
    UInt32 totalSize = (UInt32)_inodesData.Data.Size();
    const unsigned kMinNodeParseSize = 4;
    if (_h.NumInodes > totalSize / kMinNodeParseSize)
      return S_FALSE;
    _nodesPos.ClearAndReserve(_h.NumInodes);
    _nodes.ClearAndReserve(_h.NumInodes);
    // we use _blockToNode for binary search seed optimizations
    _blockToNode.ClearAndReserve(_inodesData.GetNumBlocks() + 1);
    unsigned curBlock = 0;
    for (UInt32 i = 0; i < _h.NumInodes; i++)
    {
      CNode n;
      const Byte *p = _inodesData.Data + pos;
      UInt32 size = totalSize - pos;

      switch (_h.Major)
      {
        case 1:  size = n.Parse1(p, size, _h); break;
        case 2:  size = n.Parse2(p, size, _h); break;
        case 3:  size = n.Parse3(p, size, _h); break;
        default: size = n.Parse4(p, size, _h); break;
      }
      if (size == 0)
        return S_FALSE;
      while (pos >= _inodesData.UnpackPos[curBlock])
      {
        _blockToNode.Add(_nodesPos.Size());
        curBlock++;
      }
      _nodesPos.AddInReserved(pos);
      _nodes.AddInReserved(n);
      pos += size;
    }
    _blockToNode.Add(_nodesPos.Size());
    if (pos != totalSize)
      return S_FALSE;
  }
  int rootNodeIndex;
  RINOK(OpenDir(-1, (UInt32)absOffset, (UInt32)_h.RootInode & 0xFFFF, 0, rootNodeIndex))

  if (_h.Major < 4)
  {
    RINOK(ReadUids(_h.UidTable, _h.NumUids, _uids))
    RINOK(ReadUids(_h.GidTable, _h.NumGids, _gids))
  }
  else
  {
    const UInt32 size = (UInt32)_h.NumIDs * 4;
    _uids.Alloc(size);

    const UInt32 numBlocks = (size + kMetadataBlockSize - 1) / kMetadataBlockSize;
    const UInt32 numBlocksBytes = numBlocks << 3;
    CByteBuffer data(numBlocksBytes);
    RINOK(Seek2(_h.UidTable))
    RINOK(ReadStream_FALSE(inStream, data, numBlocksBytes))

    for (UInt32 i = 0; i < numBlocks; i++)
    {
      const UInt64 offset = GetUi64(data + i * 8);
      RINOK(Seek2(offset))
      // RINOK(ReadMetadataBlock(NULL, _uids + kMetadataBlockSize * i, packSize, unpackSize));
      RINOK(ReadMetadataBlock2())
      const size_t unpackSize = _dynOutStreamSpec->GetSize();
      const UInt32 remSize = (i == numBlocks - 1)  ?
          (size & (kMetadataBlockSize - 1)) : kMetadataBlockSize;
      if (unpackSize != remSize)
        return S_FALSE;
      memcpy(_uids + kMetadataBlockSize * i, _dynOutStreamSpec->GetBuffer(), remSize);
    }
  }

  {
    const UInt32 alignSize = 1 << 12;
    Byte buf[alignSize];
    RINOK(Seek2(_h.Size))
    UInt32 rem = (UInt32)(0 - _h.Size) & (alignSize - 1);
    _sizeCalculated = _h.Size;
    if (rem != 0)
    {
      if (ReadStream_FALSE(_stream, buf, rem) == S_OK)
      {
        size_t i;
        for (i = 0; i < rem && buf[i] == 0; i++);
        if (i == rem)
          _sizeCalculated = _h.Size + rem;
      }
    }
  }
  return S_OK;
}

AString CHandler::GetPath(unsigned index) const
{
  unsigned len = 0;
  const unsigned indexMem = index;
  const bool be = _h.be;
  for (;;)
  {
    const CItem &item = _items[index];
    const Byte *p = _dirs.Data + item.Ptr;
    const unsigned size = (_h.IsOldVersion() ? (unsigned)p[2] : (unsigned)Get16(p + 6)) + 1;
    p += _h.GetFileNameOffset();
    unsigned i;
    for (i = 0; i < size && p[i]; i++);
    len += i + 1;
    index = (unsigned)item.Parent;
    if (item.Parent < 0)
      break;
  }
  len--;

  AString path;
  char *dest = path.GetBuf_SetEnd(len) + len;
  index = indexMem;
  for (;;)
  {
    const CItem &item = _items[index];
    const Byte *p = _dirs.Data + item.Ptr;
    const unsigned size = (_h.IsOldVersion() ? (unsigned)p[2] : (unsigned)Get16(p + 6)) + 1;
    p += _h.GetFileNameOffset();
    unsigned i;
    for (i = 0; i < size && p[i]; i++);
    dest -= i;
    memcpy(dest, p, i);
    index = (unsigned)item.Parent;
    if (item.Parent < 0)
      break;
    *(--dest) = CHAR_PATH_SEPARATOR;
  }
  return path;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  {
    Close();
    _limitedInStreamSpec->SetStream(stream);
    HRESULT res;
    try
    {
      _openCallback = callback;
      res = Open2(stream);
    }
    catch(...)
    {
      Close();
      throw;
    }
    if (res != S_OK)
    {
      Close();
      return res;
    }
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _openCodePage = CP_UTF8;
  _sizeCalculated = 0;

  _limitedInStreamSpec->ReleaseStream();
  _stream.Release();

  _items.Clear();
  _nodes.Clear();
  _nodesPos.Clear();
  _blockToNode.Clear();
  _frags.Clear();
  _inodesData.Clear();
  _dirs.Clear();

  _uids.Free();
  _gids.Free();

  _cachedBlock.Free();
  ClearCache();

  return S_OK;
}

bool CHandler::GetPackSize(unsigned index, UInt64 &totalPack, bool fillOffsets)
{
  totalPack = 0;
  const CItem &item = _items[index];
  const CNode &node = _nodes[item.Node];
  const UInt32 ptr = _nodesPos[item.Node];
  const Byte *p = _inodesData.Data + ptr;
  const bool be = _h.be;

  const UInt32 type = node.Type;
  UInt32 offset;
  if (node.IsLink() || node.FileSize == 0)
  {
    totalPack = node.FileSize;
    return true;
  }

  const UInt32 numBlocks = (UInt32)node.GetNumBlocks(_h);

  if (fillOffsets)
  {
    _blockOffsets.Clear();
    _blockCompressed.Clear();
    _blockOffsets.Add(totalPack);
  }

  if (_h.Major <= 1)
  {
    offset = 15;
    p += offset;
    
    for (UInt32 i = 0; i < numBlocks; i++)
    {
      UInt32 t = Get16(p + i * 2);
      if (fillOffsets)
        _blockCompressed.Add((t & kNotCompressedBit16) == 0);
      if (t != kNotCompressedBit16)
        t &= ~kNotCompressedBit16;
      totalPack += t;
      if (fillOffsets)
        _blockOffsets.Add(totalPack);
    }
  }
  else
  {
    if (_h.Major <= 2)
      offset = 24;
    else if (type == kType_FILE)
      offset = 32;
    else if (type == kType_FILE + 7)
      offset = (_h.Major <= 3 ? 40 : 56);
    else
      return false;
    
    p += offset;
    
    for (UInt64 i = 0; i < numBlocks; i++)
    {
      UInt32 t = Get32(p + i * 4);
      if (fillOffsets)
        _blockCompressed.Add(IS_COMPRESSED_BLOCK(t));
      UInt32 size = GET_COMPRESSED_BLOCK_SIZE(t);
      if (size > _h.BlockSize)
        return false;
      totalPack += size;
      if (fillOffsets)
        _blockOffsets.Add(totalPack);
    }

    if (node.ThereAreFrags())
    {
      if (node.Frag >= (UInt32)_frags.Size())
        return false;
      const CFrag &frag = _frags[node.Frag];
      if (node.Offset == 0)
      {
        UInt32 size = GET_COMPRESSED_BLOCK_SIZE(frag.Size);
        if (size > _h.BlockSize)
          return false;
        totalPack += size;
      }
    }
  }
  return true;
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMethod:
    {
      char sz[16];
      const char *s;
      if (_noPropsLZMA)
        s = "LZMA Spec";
      else if (_h.SeveralMethods)
        s = "LZMA ZLIB";
      else
      {
        s = NULL;
        if (_h.Method < Z7_ARRAY_SIZE(k_Methods))
          s = k_Methods[_h.Method];
        if (!s)
        {
          ConvertUInt32ToString(_h.Method, sz);
          s = sz;
        }
      }
      prop = s;
      break;
    }
    case kpidFileSystem:
    {
      AString res ("SquashFS");
      if (_h.SeveralMethods)
        res += "-LZMA";
      res.Add_Space();
      res.Add_UInt32(_h.Major);
      res.Add_Dot();
      res.Add_UInt32(_h.Minor);
      prop = res;
      break;
    }
    case kpidClusterSize: prop = _h.BlockSize; break;
    case kpidBigEndian: prop = _h.be; break;
    case kpidCTime:
      if (_h.CTime != 0)
        PropVariant_SetFrom_UnixTime(prop, _h.CTime);
      break;
    case kpidCharacts: FLAGS_TO_PROP(k_Flags, _h.Flags, prop); break;
    // case kpidNumBlocks: prop = _h.NumFrags; break;
    case kpidPhySize: prop = _sizeCalculated; break;
    case kpidHeadersSize:
      if (_sizeCalculated >= _h.InodeTable)
        prop = _sizeCalculated - _h.InodeTable;
      break;

    case kpidCodePage:
    {
      char sz[16];
      const char *name = NULL;
      switch (_openCodePage)
      {
        case CP_OEMCP: name = "OEM"; break;
        case CP_UTF8: name = "UTF-8"; break;
      }
      if (!name)
      {
        ConvertUInt32ToString(_openCodePage, sz);
        name = sz;
      }
      prop = name;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CItem &item = _items[index];
  const CNode &node = _nodes[item.Node];
  const bool isDir = node.IsDir();
  const bool be = _h.be;

  switch (propID)
  {
    case kpidPath:
    {
      AString path (GetPath(index));
      UString s;
      if (_openCodePage == CP_UTF8)
        ConvertUTF8ToUnicode(path, s);
      else
        MultiByteToUnicodeString2(s, path, _openCodePage);
      prop = s;
      break;
    }
    case kpidIsDir: prop = isDir; break;
    // case kpidOffset: if (!node.IsLink()) prop = (UInt64)node.StartBlock; break;
    case kpidSize: if (!isDir) prop = node.GetSize(); break;
    case kpidPackSize:
      if (!isDir)
      {
        UInt64 size;
        if (GetPackSize(index, size, false))
          prop = size;
      }
      break;
    case kpidMTime:
    {
      UInt32 offset = 0;
      switch (_h.Major)
      {
        case 1:
          if (node.Type == kType_FILE)
            offset = 3;
          else if (node.Type == kType_DIR)
            offset = 7;
          break;
        case 2:
          if (node.Type == kType_FILE)
            offset = 4;
          else if (node.Type == kType_DIR)
            offset = 8;
          else if (node.Type == kType_DIR + 7)
            offset = 9;
          break;
        case 3: offset = 4; break;
        case 4: offset = 8; break;
      }
      if (offset != 0)
      {
        const Byte *p = _inodesData.Data + _nodesPos[item.Node] + offset;
        PropVariant_SetFrom_UnixTime(prop, Get32(p));
      }
      break;
    }
    case kpidPosixAttrib:
    {
      if (node.Type != 0 && node.Type < Z7_ARRAY_SIZE(k_TypeToMode))
        prop = (UInt32)(node.Mode & 0xFFF) | k_TypeToMode[node.Type];
      break;
    }
    case kpidUserId:
    case kpidGroupId:
    {
      UInt32 id = node.Uid;
      const CByteBuffer *ids = &_uids;
      if (propID == kpidGroupId)
      {
        id = node.Gid;
        if (_h.Major < 4)
        {
          if (id == _h.GetSpecGuidIndex())
            id = node.Uid;
          else
            ids = &_gids;
        }
      }
      const UInt32 offset = (UInt32)id * 4;
      if (offset < ids->Size())
        prop = (UInt32)Get32(*ids + offset);
      break;
    }
    /*
    case kpidLinks:
      if (_h.Major >= 3 && node.Type != kType_FILE)
        prop = node.NumLinks;
      break;
    */
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

class CSquashfsInStream: public CCachedInStream
{
  HRESULT ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize) Z7_override;
public:
  CHandler *Handler;
};

HRESULT CSquashfsInStream::ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize)
{
  return Handler->ReadBlock(blockIndex, dest, blockSize);
}

HRESULT CHandler::ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize)
{
  const CNode &node = _nodes[_nodeIndex];
  UInt64 blockOffset;
  UInt32 packBlockSize;
  UInt32 offsetInBlock = 0;
  bool compressed;
  if (blockIndex < _blockCompressed.Size())
  {
    compressed = _blockCompressed[(unsigned)blockIndex];
    blockOffset = _blockOffsets[(unsigned)blockIndex];
    packBlockSize = (UInt32)(_blockOffsets[(unsigned)blockIndex + 1] - blockOffset);
    blockOffset += node.StartBlock;
  }
  else
  {
    if (!node.ThereAreFrags())
      return S_FALSE;
    const CFrag &frag = _frags[node.Frag];
    offsetInBlock = node.Offset;
    blockOffset = frag.StartBlock;
    packBlockSize = GET_COMPRESSED_BLOCK_SIZE(frag.Size);
    compressed = IS_COMPRESSED_BLOCK(frag.Size);
  }

  if (packBlockSize == 0)
  {
    // sparse file ???
    memset(dest, 0, blockSize);
    return S_OK;
  }

  if (blockOffset != _cachedBlockStartPos ||
      packBlockSize != _cachedPackBlockSize)
  {
    ClearCache();
    RINOK(Seek2(blockOffset))
    _limitedInStreamSpec->Init(packBlockSize);
    
    if (compressed)
    {
      _outStreamSpec->Init((Byte *)_cachedBlock, _h.BlockSize);
      bool outBufWasWritten;
      UInt32 outBufWasWrittenSize;
      HRESULT res = Decompress(_outStream, _cachedBlock, &outBufWasWritten, &outBufWasWrittenSize, packBlockSize, _h.BlockSize);
      RINOK(res)
      if (outBufWasWritten)
        _cachedUnpackBlockSize = outBufWasWrittenSize;
      else
        _cachedUnpackBlockSize = (UInt32)_outStreamSpec->GetPos();
    }
    else
    {
      if (packBlockSize > _h.BlockSize)
        return S_FALSE;
      RINOK(ReadStream_FALSE(_limitedInStream, _cachedBlock, packBlockSize))
      _cachedUnpackBlockSize = packBlockSize;
    }
    _cachedBlockStartPos = blockOffset;
    _cachedPackBlockSize = packBlockSize;
  }
  if (offsetInBlock + blockSize > _cachedUnpackBlockSize)
    return S_FALSE;
  if (blockSize != 0)
    memcpy(dest, _cachedBlock + offsetInBlock, blockSize);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const CItem &item = _items[allFilesMode ? i : indices[i]];
    const CNode &node = _nodes[item.Node];
    totalSize += node.GetSize();
  }
  extractCallback->SetTotal(totalSize);

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur())
    CMyComPtr<ISequentialOutStream> outStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item = _items[index];
    const CNode &node = _nodes[item.Node];
    RINOK(extractCallback->GetStream(index, &outStream, askMode))
    // const Byte *p = _data + item.Offset;

    if (node.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    UInt64 unpackSize = node.GetSize();
    totalSize += unpackSize;
    UInt64 packSize;
    if (GetPackSize(index, packSize, false))
      totalPackSize += packSize;

    if (!testMode && !outStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))

    int res = NExtract::NOperationResult::kDataError;
    {
      CMyComPtr<ISequentialInStream> inSeqStream;
      HRESULT hres = GetStream(index, &inSeqStream);
      if (hres == S_FALSE || !inSeqStream)
      {
        if (hres == E_OUTOFMEMORY)
          return hres;
        res = NExtract::NOperationResult::kUnsupportedMethod;
      }
      else
      {
        RINOK(hres)
        {
          hres = copyCoder->Code(inSeqStream, outStream, NULL, NULL, progress);
          if (hres == S_OK)
          {
            if (copyCoderSpec->TotalSize == unpackSize)
              res = NExtract::NOperationResult::kOK;
          }
          else if (hres == E_NOTIMPL)
          {
            res = NExtract::NOperationResult::kUnsupportedMethod;
          }
          else if (hres != S_FALSE)
          {
            RINOK(hres)
          }
        }
      }
    }

    RINOK(extractCallback->SetOperationResult(res))
  }
  
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN

  const CItem &item = _items[index];
  const CNode &node = _nodes[item.Node];

  if (node.IsDir())
    return E_FAIL;

  const Byte *p = _inodesData.Data + _nodesPos[item.Node];

  if (node.FileSize == 0 || node.IsLink())
  {
    CBufInStream *streamSpec = new CBufInStream;
    CMyComPtr<IInStream> streamTemp = streamSpec;
    if (node.IsLink())
      streamSpec->Init(p + _h.GetSymLinkOffset(), (size_t)node.FileSize);
    else
      streamSpec->Init(NULL, 0);
    *stream = streamTemp.Detach();
    return S_OK;
  }

  UInt64 packSize;
  if (!GetPackSize(index, packSize, true))
    return S_FALSE;

  _nodeIndex = item.Node;

  size_t cacheSize = _h.BlockSize;
  if (_cachedBlock.Size() != cacheSize)
  {
    ClearCache();
    _cachedBlock.Alloc(cacheSize);
  }

  CSquashfsInStream *streamSpec = new CSquashfsInStream;
  CMyComPtr<IInStream> streamTemp = streamSpec;
  streamSpec->Handler = this;
  unsigned cacheSizeLog = 22;
  if (cacheSizeLog <= _h.BlockSizeLog)
    cacheSizeLog = _h.BlockSizeLog + 1;
  if (!streamSpec->Alloc(_h.BlockSizeLog, cacheSizeLog - _h.BlockSizeLog))
    return E_OUTOFMEMORY;
  streamSpec->Init(node.FileSize);
  *stream = streamTemp.Detach();

  return S_OK;

  COM_TRY_END
}

static const Byte k_Signature[] = {
    4, 'h', 's', 'q', 's',
    4, 's', 'q', 's', 'h',
    4, 's', 'h', 's', 'q',
    4, 'q', 's', 'h', 's' };

REGISTER_ARC_I(
  "SquashFS", "squashfs", NULL, 0xD2,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature,
  NULL)

}}
