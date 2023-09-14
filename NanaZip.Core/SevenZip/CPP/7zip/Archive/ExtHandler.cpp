// ExtHandler.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO

// #include <stdio.h>
// #define PRF2(x) x

#define PRF2(x)

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#define PRF(x) x
#else
#define PRF(x)
#endif

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyLinux.h"
#include "../../Common/StringConvert.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

using namespace NWindows;

UInt32 LzhCrc16Update(UInt32 crc, const void *data, size_t size);

namespace NArchive {
namespace NExt {

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define LE_16(offs, dest) dest = Get16(p + (offs));
#define LE_32(offs, dest) dest = Get32(p + (offs));
#define LE_64(offs, dest) dest = Get64(p + (offs));

#define HI_16(offs, dest) dest |= (((UInt32)Get16(p + (offs))) << 16);
#define HI_32(offs, dest) dest |= (((UInt64)Get32(p + (offs))) << 32);

/*
static UInt32 g_Crc32CTable[256];

static struct CInitCrc32C
{
  CInitCrc32C()
  {
    for (unsigned i = 0; i < 256; i++)
    {
      UInt32 r = i;
      unsigned j;
      for (j = 0; j < 8; j++)
        r = (r >> 1) ^ (0x82F63B78 & ~((r & 1) - 1));
      g_Crc32CTable[i] = r;
    }
  }
} g_InitCrc32C;

#define CRC32C_INIT_VAL 0xFFFFFFFF
#define CRC32C_GET_DIGEST(crc) ((crc) ^ CRC_INIT_VAL)
#define CRC32C_UPDATE_BYTE(crc, b) (g_Crc32CTable[((crc) ^ (b)) & 0xFF] ^ ((crc) >> 8))

static UInt32 Crc32C_Update(UInt32 crc, Byte const *data, size_t size)
{
  for (size_t i = 0; i < size; i++)
    crc = CRC32C_UPDATE_BYTE(crc, data[i]);
  return crc;
}

static UInt32 Crc32C_Calc(Byte const *data, size_t size)
{
  return Crc32C_Update(CRC32C_INIT_VAL, data, size);
}
*/


#define CRC16_INIT_VAL 0xFFFF

#define Crc16Update(crc, data, size)  LzhCrc16Update(crc, data, size)

static UInt32 Crc16Calc(Byte const *data, size_t size)
{
  return Crc16Update(CRC16_INIT_VAL, data, size);
}


#define EXT4_GOOD_OLD_INODE_SIZE 128
#define EXT_NODE_SIZE_MIN 128


// inodes numbers
 
// #define k_INODE_BAD          1  // Bad blocks
#define k_INODE_ROOT         2  // Root dir
// #define k_INODE_USR_QUOTA    3  // User quota
// #define k_INODE_GRP_QUOTA    4  // Group quota
// #define k_INODE_BOOT_LOADER  5  // Boot loader
// #define k_INODE_UNDEL_DIR    6  // Undelete dir
#define k_INODE_RESIZE       7  // Reserved group descriptors
// #define k_INODE_JOURNAL      8  // Journal

// First non-reserved inode for old ext4 filesystems
#define k_INODE_GOOD_OLD_FIRST  11

static const char * const k_SysInode_Names[] =
{
    "0"
  , "Bad"
  , "Root"
  , "UserQuota"
  , "GroupQuota"
  , "BootLoader"
  , "Undelete"
  , "Resize"
  , "Journal"
  , "Exclude"
  , "Replica"
};

static const char * const kHostOS[] =
{
    "Linux"
  , "Hurd"
  , "Masix"
  , "FreeBSD"
  , "Lites"
};

static const char * const g_FeatureCompat_Flags[] =
{
    "DIR_PREALLOC"
  , "IMAGIC_INODES"
  , "HAS_JOURNAL"
  , "EXT_ATTR"
  , "RESIZE_INODE"
  , "DIR_INDEX"
  , "LAZY_BG" // not in Linux
  , NULL // { 7, "EXCLUDE_INODE" // not used
  , NULL // { 8, "EXCLUDE_BITMAP" // not in kernel
  , "SPARSE_SUPER2"
};
  

#define EXT4_FEATURE_INCOMPAT_FILETYPE (1 << 1)
#define EXT4_FEATURE_INCOMPAT_64BIT (1 << 7)

static const char * const g_FeatureIncompat_Flags[] =
{
    "COMPRESSION"
  , "FILETYPE"
  , "RECOVER" /* Needs recovery */
  , "JOURNAL_DEV" /* Journal device */
  , "META_BG"
  , NULL
  , "EXTENTS" /* extents support */
  , "64BIT"
  , "MMP"
  , "FLEX_BG"
  , "EA_INODE" /* EA in inode */
  , NULL
  , "DIRDATA" /* data in dirent */
  , "BG_USE_META_CSUM" /* use crc32c for bg */
  , "LARGEDIR" /* >2GB or 3-lvl htree */
  , "INLINE_DATA" /* data in inode */
  , "ENCRYPT" // 16
};


static const UInt32 RO_COMPAT_GDT_CSUM = 1 << 4;
static const UInt32 RO_COMPAT_METADATA_CSUM = 1 << 10;

static const char * const g_FeatureRoCompat_Flags[] =
{
    "SPARSE_SUPER"
  , "LARGE_FILE"
  , "BTREE_DIR"
  , "HUGE_FILE"
  , "GDT_CSUM"
  , "DIR_NLINK"
  , "EXTRA_ISIZE"
  , "HAS_SNAPSHOT"
  , "QUOTA"
  , "BIGALLOC"
  , "METADATA_CSUM"
  , "REPLICA"
  , "READONLY" // 12
};



static const UInt32 k_NodeFlags_HUGE = (UInt32)1 << 18;
static const UInt32 k_NodeFlags_EXTENTS = (UInt32)1 << 19;


static const char * const g_NodeFlags[] =
{
    "SECRM"
  , "UNRM"
  , "COMPR"
  , "SYNC"
  , "IMMUTABLE"
  , "APPEND"
  , "NODUMP"
  , "NOATIME"
  , "DIRTY"
  , "COMPRBLK"
  , "NOCOMPR"
  , "ENCRYPT"
  , "INDEX"
  , "IMAGIC"
  , "JOURNAL_DATA"
  , "NOTAIL"
  , "DIRSYNC"
  , "TOPDIR"
  , "HUGE_FILE"
  , "EXTENTS"
  , NULL
  , "EA_INODE"
  , "EOFBLOCKS"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , "INLINE_DATA" // 28
};


static inline char GetHex(unsigned t) { return (char)(((t < 10) ? ('0' + t) : ('A' + (t - 10)))); }

static inline void PrintHex(unsigned v, char *s)
{
  s[0] = GetHex((v >> 4) & 0xF);
  s[1] = GetHex(v & 0xF);
}


enum
{
  k_Type_UNKNOWN,
  k_Type_REG_FILE,
  k_Type_DIR,
  k_Type_CHRDEV,
  k_Type_BLKDEV,
  k_Type_FIFO,
  k_Type_SOCK,
  k_Type_SYMLINK
};

static const UInt16 k_TypeToMode[] =
{
  0,
  MY_LIN_S_IFREG,
  MY_LIN_S_IFDIR,
  MY_LIN_S_IFCHR,
  MY_LIN_S_IFBLK,
  MY_LIN_S_IFIFO,
  MY_LIN_S_IFSOCK,
  MY_LIN_S_IFLNK
};


#define EXT4_GOOD_OLD_REV 0  // old (original) format
// #define EXT4_DYNAMIC_REV 1  // V2 format with dynamic inode sizes
 
struct CHeader
{
  unsigned BlockBits;
  unsigned ClusterBits;

  UInt32 NumInodes;
  UInt64 NumBlocks;
  // UInt64 NumBlocksSuper;
  UInt64 NumFreeBlocks;
  UInt32 NumFreeInodes;
  // UInt32 FirstDataBlock;

  UInt32 BlocksPerGroup;
  UInt32 ClustersPerGroup;
  UInt32 InodesPerGroup;

  UInt32 MountTime;
  UInt32 WriteTime;

  // UInt16 NumMounts;
  // UInt16 NumMountsMax;
  
  // UInt16 State;
  // UInt16 Errors;
  // UInt16 MinorRevLevel;

  UInt32 LastCheckTime;
  // UInt32 CheckInterval;
  UInt32 CreatorOs;
  UInt32 RevLevel;

  // UInt16 DefResUid;
  // UInt16 DefResGid;

  UInt32 FirstInode;
  UInt16 InodeSize;
  UInt16 BlockGroupNr;
  UInt32 FeatureCompat;
  UInt32 FeatureIncompat;
  UInt32 FeatureRoCompat;
  Byte Uuid[16];
  char VolName[16];
  char LastMount[64];
  // UInt32 BitmapAlgo;

  UInt32 JournalInode;
  UInt16 GdSize; // = 64 if 64-bit();
  UInt32 CTime;
  UInt16 MinExtraISize;
  // UInt16 WantExtraISize;
  // UInt32 Flags;
  // Byte LogGroupsPerFlex;
  // Byte ChecksumType;

  UInt64 WrittenKB;

  bool IsOldRev() const { return RevLevel == EXT4_GOOD_OLD_REV; }

  UInt64 GetNumGroups() const { return (NumBlocks + BlocksPerGroup - 1) / BlocksPerGroup; }
  UInt64 GetNumGroups2() const { return ((UInt64)NumInodes + InodesPerGroup - 1) / InodesPerGroup; }

  bool IsThereFileType() const { return (FeatureIncompat & EXT4_FEATURE_INCOMPAT_FILETYPE) != 0; }
  bool Is64Bit() const { return (FeatureIncompat & EXT4_FEATURE_INCOMPAT_64BIT) != 0; }
  bool UseGdtChecksum() const { return (FeatureRoCompat & RO_COMPAT_GDT_CSUM) != 0; }
  bool UseMetadataChecksum() const { return (FeatureRoCompat & RO_COMPAT_METADATA_CSUM) != 0; }

  UInt64 GetPhySize() const { return NumBlocks << BlockBits; }

  bool Parse(const Byte *p);
};


static int inline GetLog(UInt32 num)
{
  for (unsigned i = 0; i < 32; i++)
    if (((UInt32)1 << i) == num)
      return (int)i;
  return -1;
}

static bool inline IsEmptyData(const Byte *data, unsigned size)
{
  for (unsigned i = 0; i < size; i++)
    if (data[i] != 0)
      return false;
  return true;
}


bool CHeader::Parse(const Byte *p)
{
  if (GetUi16(p + 0x38) != 0xEF53)
    return false;
  
  LE_32 (0x18, BlockBits)
  LE_32 (0x1C, ClusterBits)
  
  if (ClusterBits != 0 && BlockBits != ClusterBits)
    return false;

  if (BlockBits > 16 - 10)
    return false;
  BlockBits += 10;
  
  LE_32 (0x00, NumInodes)
  LE_32 (0x04, NumBlocks)
  // LE_32 (0x08, NumBlocksSuper);
  LE_32 (0x0C, NumFreeBlocks)
  LE_32 (0x10, NumFreeInodes)

  if (NumInodes < 2 || NumInodes <= NumFreeInodes)
    return false;

  UInt32 FirstDataBlock;
  LE_32 (0x14, FirstDataBlock)
  if (FirstDataBlock != (unsigned)(BlockBits == 10 ? 1 : 0))
    return false;
    
  LE_32 (0x20, BlocksPerGroup)
  LE_32 (0x24, ClustersPerGroup)

  if (BlocksPerGroup != ClustersPerGroup)
    return false;
  if (BlocksPerGroup == 0)
    return false;
  if (BlocksPerGroup != ((UInt32)1 << (BlockBits + 3)))
  {
    // it's allowed in ext2
    // return false;
  }

  LE_32 (0x28, InodesPerGroup)

  if (InodesPerGroup < 1 || InodesPerGroup > NumInodes)
    return false;
  
  LE_32 (0x2C, MountTime)
  LE_32 (0x30, WriteTime)
  
  // LE_16 (0x34, NumMounts);
  // LE_16 (0x36, NumMountsMax);
  
  // LE_16 (0x3A, State);
  // LE_16 (0x3C, Errors);
  // LE_16 (0x3E, MinorRevLevel);
  
  LE_32 (0x40, LastCheckTime)
  // LE_32 (0x44, CheckInterval);
  LE_32 (0x48, CreatorOs)
  LE_32 (0x4C, RevLevel)
  
  // LE_16 (0x50, DefResUid);
  // LE_16 (0x52, DefResGid);
  
  FirstInode = k_INODE_GOOD_OLD_FIRST;
  InodeSize = EXT4_GOOD_OLD_INODE_SIZE;

  if (!IsOldRev())
  {
    LE_32 (0x54, FirstInode)
    LE_16 (0x58, InodeSize)
    if (FirstInode < k_INODE_GOOD_OLD_FIRST)
      return false;
    if (InodeSize > ((UInt32)1 << BlockBits)
        || InodeSize < EXT_NODE_SIZE_MIN
        || GetLog(InodeSize) < 0)
      return false;
  }

  LE_16 (0x5A, BlockGroupNr)
  LE_32 (0x5C, FeatureCompat)
  LE_32 (0x60, FeatureIncompat)
  LE_32 (0x64, FeatureRoCompat)
  
  memcpy(Uuid, p + 0x68, sizeof(Uuid));
  memcpy(VolName, p + 0x78, sizeof(VolName));
  memcpy(LastMount, p + 0x88, sizeof(LastMount));
  
  // LE_32 (0xC8, BitmapAlgo);
  
  LE_32 (0xE0, JournalInode)
  
  LE_16 (0xFE, GdSize)
  
  LE_32 (0x108, CTime)

  if (Is64Bit())
  {
    HI_32(0x150, NumBlocks)
    // HI_32(0x154, NumBlocksSuper);
    HI_32(0x158, NumFreeBlocks)
  }

  if (NumBlocks >= (UInt64)1 << (63 - BlockBits))
    return false;


  LE_16(0x15C, MinExtraISize)
  // LE_16(0x15E, WantExtraISize);
  // LE_32(0x160, Flags);
  // LE_16(0x164, RaidStride);
  // LE_16(0x166, MmpInterval);
  // LE_64(0x168, MmpBlock);
  
  // LogGroupsPerFlex = p[0x174];
  // ChecksumType = p[0x175];

  LE_64 (0x178, WrittenKB)

  // LE_32(0x194, ErrorCount);
  // LE_32(0x198, ErrorTime);
  // LE_32(0x19C, ErrorINode);
  // LE_32(0x1A0, ErrorBlock);
  
  if (NumBlocks < 1)
    return false;
  if (NumFreeBlocks > NumBlocks)
    return false;

  if (GetNumGroups() != GetNumGroups2())
    return false;

  return true;
}


struct CGroupDescriptor
{
  UInt64 BlockBitmap;
  UInt64 InodeBitmap;
  UInt64 InodeTable;
  UInt32 NumFreeBlocks;
  UInt32 NumFreeInodes;
  UInt32 DirCount;
  
  UInt16 Flags;
  
  UInt64 ExcludeBitmap;
  UInt32 BlockBitmap_Checksum;
  UInt32 InodeBitmap_Checksum;
  UInt32 UnusedCount;
  UInt16 Checksum;

  void Parse(const Byte *p, unsigned size);
};

void CGroupDescriptor::Parse(const Byte *p, unsigned size)
{
  LE_32 (0x00, BlockBitmap)
  LE_32 (0x04, InodeBitmap)
  LE_32 (0x08, InodeTable)
  LE_16 (0x0C, NumFreeBlocks)
  LE_16 (0x0E, NumFreeInodes)
  LE_16 (0x10, DirCount)
  LE_16 (0x12, Flags)
  LE_32 (0x14, ExcludeBitmap)
  LE_16 (0x18, BlockBitmap_Checksum)
  LE_16 (0x1A, InodeBitmap_Checksum)
  LE_16 (0x1C, UnusedCount)
  LE_16 (0x1E, Checksum)
  
  if (size >= 64)
  {
    p += 0x20;
    HI_32 (0x00, BlockBitmap)
    HI_32 (0x04, InodeBitmap)
    HI_32 (0x08, InodeTable)
    HI_16 (0x0C, NumFreeBlocks)
    HI_16 (0x0E, NumFreeInodes)
    HI_16 (0x10, DirCount)
    HI_16 (0x12, UnusedCount) // instead of Flags
    HI_32 (0x14, ExcludeBitmap)
    HI_16 (0x18, BlockBitmap_Checksum)
    HI_16 (0x1A, InodeBitmap_Checksum)
    // HI_16 (0x1C, Reserved);
  }
}


static const unsigned kNodeBlockFieldSize = 60;

struct CExtentTreeHeader
{
  UInt16 NumEntries;
  UInt16 MaxEntries;
  UInt16 Depth;
  // UInt32 Generation;

  bool Parse(const Byte *p)
  {
    LE_16 (0x02, NumEntries)
    LE_16 (0x04, MaxEntries)
    LE_16 (0x06, Depth)
    // LE_32 (0x08, Generation);
    return Get16(p) == 0xF30A; // magic
  }
};

struct CExtentIndexNode
{
  UInt32 VirtBlock;
  UInt64 PhyLeaf;

  void Parse(const Byte *p)
  {
    LE_32 (0x00, VirtBlock)
    LE_32 (0x04, PhyLeaf)
    PhyLeaf |= (((UInt64)Get16(p + 8)) << 32);
    // unused 16-bit field (at offset 0x0A) can be not zero in some cases. Why?
  }
};

struct CExtent
{
  UInt32 VirtBlock;
  UInt16 Len;
  bool IsInited;
  UInt64 PhyStart;

  UInt32 GetVirtEnd() const { return VirtBlock + Len; }
  bool IsLenOK() const { return VirtBlock + Len >= VirtBlock; }

  void Parse(const Byte *p)
  {
    LE_32 (0x00, VirtBlock)
    LE_16 (0x04, Len)
    IsInited = true;
    if (Len > (UInt32)0x8000)
    {
      IsInited = false;
      Len = (UInt16)(Len - (UInt32)0x8000);
    }
    LE_32 (0x08, PhyStart)
    UInt16 hi;
    LE_16 (0x06, hi)
    PhyStart |= ((UInt64)hi << 32);
  }
};



struct CExtTime
{
  UInt32 Val;
  UInt32 Extra;
};

struct CNode
{
  Int32 ParentNode;   // in _refs[], -1 if not dir
  int ItemIndex;      // in _items[]   , (set as >=0 only for if (IsDir())
  int SymLinkIndex;   // in _symLinks[]
  int DirIndex;       // in _dirs[]

  UInt16 Mode;
  UInt32 Uid; // fixed 21.02
  UInt32 Gid; // fixed 21.02
  // UInt16 Checksum;
  
  UInt64 FileSize;
  CExtTime MTime;
  CExtTime ATime;
  CExtTime CTime;
  CExtTime ChangeTime;
  // CExtTime DTime;

  UInt64 NumBlocks;
  UInt32 NumLinks;
  UInt32 Flags;

  UInt32 NumLinksCalced;

  Byte Block[kNodeBlockFieldSize];
  
  CNode():
      ParentNode(-1),
      ItemIndex(-1),
      SymLinkIndex(-1),
      DirIndex(-1),
      NumLinksCalced(0)
        {}

  bool IsFlags_HUGE()    const { return (Flags & k_NodeFlags_HUGE) != 0; }
  bool IsFlags_EXTENTS() const { return (Flags & k_NodeFlags_EXTENTS) != 0; }

  bool IsDir()     const { return MY_LIN_S_ISDIR(Mode); }
  bool IsRegular() const { return MY_LIN_S_ISREG(Mode); }
  bool IsLink()    const { return MY_LIN_S_ISLNK(Mode); }

  bool Parse(const Byte *p, const CHeader &_h);
};


bool CNode::Parse(const Byte *p, const CHeader &_h)
{
  MTime.Extra = 0;
  ATime.Extra = 0;
  CTime.Extra = 0;
  CTime.Val = 0;
  ChangeTime.Extra = 0;
  // DTime.Extra = 0;

  LE_16 (0x00, Mode)
  LE_16 (0x02, Uid)
  LE_32 (0x04, FileSize)
  LE_32 (0x08, ATime.Val)
  LE_32 (0x0C, ChangeTime.Val)
  LE_32 (0x10, MTime.Val)
  // LE_32 (0x14, DTime.Val);
  LE_16 (0x18, Gid)
  LE_16 (0x1A, NumLinks)

  LE_32 (0x1C, NumBlocks)
  LE_32 (0x20, Flags)
  // LE_32 (0x24, Union osd1);
  
  memcpy(Block, p + 0x28, kNodeBlockFieldSize);
  
  // LE_32 (0x64, Generation);  // File version (for NFS)
  // LE_32 (0x68, ACL);
  
  {
    UInt32 highSize;
    LE_32 (0x6C, highSize) // In ext2/3 this field was named i_dir_acl
   
    if (IsRegular()) // do we need that check ?
      FileSize |= ((UInt64)highSize << 32);
  }

  // UInt32 fragmentAddress;
  // LE_32 (0x70, fragmentAddress);
  
  // osd2
  {
    // Linux;
    // ext2:
    // Byte FragmentNumber = p[0x74];
    // Byte FragmentSize = p[0x74 + 1];
    
    // ext4:
    UInt32 numBlocksHigh;
    LE_16 (0x74, numBlocksHigh)
    NumBlocks |= (UInt64)numBlocksHigh << 32;
    
    HI_16 (0x74 + 4, Uid)
    HI_16 (0x74 + 6, Gid)
    /*
    UInt32 checksum;
    LE_16 (0x74 + 8, checksum);
    checksum = checksum;
    */
  }

  // 0x74: Byte Union osd2[12];

  if (_h.InodeSize > 128)
  {
    // InodeSize is power of 2, so the following check is not required:
    // if (_h.InodeSize < 128 + 2) return false;
    UInt16 extra_isize;
    LE_16 (0x80, extra_isize)
    if (128 + extra_isize > _h.InodeSize)
      return false;
    if (extra_isize >= 0x1C)
    {
      // UInt16 checksumUpper;
      // LE_16 (0x82, checksumUpper);
      LE_32 (0x84, ChangeTime.Extra)
      LE_32 (0x88, MTime.Extra)
      LE_32 (0x8C, ATime.Extra)
      LE_32 (0x90, CTime.Val)
      LE_32 (0x94, CTime.Extra)
      // LE_32 (0x98, VersionHi)
    }
  }

  PRF(printf("size = %5d", (unsigned)FileSize));

  return true;
}


struct CItem
{
  unsigned Node;        // in _refs[]
  int ParentNode;       // in _refs[]
  int SymLinkItemIndex; // in _items[], if the Node contains SymLink to existing dir
  Byte Type;
  
  AString Name;

  CItem():
      Node(0),
      ParentNode(-1),
      SymLinkItemIndex(-1),
      Type(k_Type_UNKNOWN)
        {}
  
  void Clear()
  {
    Node = 0;
    ParentNode = -1;
    SymLinkItemIndex = -1;
    Type = k_Type_UNKNOWN;
    Name.Empty();
  }

  bool IsDir() const { return Type == k_Type_DIR; }
  // bool IsNotDir() const { return Type != k_Type_DIR && Type != k_Type_UNKNOWN; }
};



static const unsigned kNumTreeLevelsMax = 6; // must be >= 3


Z7_CLASS_IMP_CHandler_IInArchive_2(
  IArchiveGetRawProps,
  IInArchiveGetStream
)
  CObjectVector<CItem> _items;
  CIntVector _refs;                 // iNode -> (index in _nodes). if (_refs[iNode] < 0), that node is not filled
  CRecordVector<CNode> _nodes;
  CObjectVector<CUIntVector> _dirs; // each CUIntVector contains indexes in _items[] only for dir items;
  AStringVector _symLinks;
  AStringVector _auxItems;
  int _auxSysIndex;
  int _auxUnknownIndex;

  CMyComPtr<IInStream> _stream;
  UInt64 _phySize;
  bool _isArc;
  bool _headersError;
  bool _headersWarning;
  bool _linksError;
  
  bool _isUTF;
  
  CHeader _h;

  IArchiveOpenCallback *_openCallback;
  UInt64 _totalRead;
  UInt64 _totalReadPrev;

  CByteBuffer _tempBufs[kNumTreeLevelsMax];

  
  HRESULT CheckProgress2()
  {
    const UInt64 numFiles = _items.Size();
    return _openCallback->SetCompleted(&numFiles, &_totalRead);
  }

  HRESULT CheckProgress()
  {
    HRESULT res = S_OK;
    if (_openCallback)
    {
      if (_totalRead - _totalReadPrev >= ((UInt32)1 << 20))
      {
        _totalReadPrev = _totalRead;
        res = CheckProgress2();
      }
    }
    return res;
  }

  
  int GetParentAux(const CItem &item) const
  {
    if (item.Node < _h.FirstInode && _auxSysIndex >= 0)
      return _auxSysIndex;
    return _auxUnknownIndex;
  }

  HRESULT SeekAndRead(IInStream *inStream, UInt64 block, Byte *data, size_t size);
  HRESULT ParseDir(const Byte *data, size_t size, unsigned iNodeDir);
  int FindTargetItem_for_SymLink(unsigned dirNode, const AString &path) const;

  HRESULT FillFileBlocks2(UInt32 block, unsigned level, unsigned numBlocks, CRecordVector<UInt32> &blocks);
  HRESULT FillFileBlocks(const Byte *p, unsigned numBlocks, CRecordVector<UInt32> &blocks);
  HRESULT FillExtents(const Byte *p, size_t size, CRecordVector<CExtent> &extents, int parentDepth);

  HRESULT GetStream_Node(unsigned nodeIndex, ISequentialInStream **stream);
  HRESULT ExtractNode(unsigned nodeIndex, CByteBuffer &data);

  void ClearRefs();
  HRESULT Open2(IInStream *inStream);

  void GetPath(unsigned index, AString &s) const;
  bool GetPackSize(unsigned index, UInt64 &res) const;
};



HRESULT CHandler::ParseDir(const Byte *p, size_t size, unsigned iNodeDir)
{
  bool isThereSelfLink = false;

  PRF(printf("\n\n========= node = %5d    size = %5d", (unsigned)iNodeDir, (unsigned)size));

  CNode &nodeDir = _nodes[_refs[iNodeDir]];
  nodeDir.DirIndex = (int)_dirs.Size();
  CUIntVector &dir = _dirs.AddNew();
  int parentNode = -1;

  CItem item;

  for (;;)
  {
    if (size == 0)
      break;
    if (size < 8)
      return S_FALSE;
    UInt32 iNode;
    LE_32 (0x00, iNode)
    unsigned recLen;
    LE_16 (0x04, recLen)
    const unsigned nameLen = p[6];
    const Byte type = p[7];

    if (recLen > size)
      return S_FALSE;
    if (nameLen + 8 > recLen)
      return S_FALSE;

    if (iNode >= _refs.Size())
      return S_FALSE;

    item.Clear();

    if (_h.IsThereFileType())
      item.Type = type;
    else if (type != 0)
      return S_FALSE;

    item.ParentNode = (int)iNodeDir;
    item.Node = iNode;
    item.Name.SetFrom_CalcLen((const char *)(p + 8), nameLen);
  
    p += recLen;
    size -= recLen;
   
    if (item.Name.Len() != nameLen)
      return S_FALSE;
    
    if (_isUTF)
    {
      // 21.07 : we force UTF8
      // _isUTF = CheckUTF8_AString(item.Name);
    }

    if (iNode == 0)
    {
      /*
      ext3 deleted??
      if (item.Name.Len() != 0)
        return S_FALSE;
      */

      PRF(printf("\n EMPTY %6d %d %s", (unsigned)recLen, (unsigned)type, (const char *)item.Name));
      if (type == 0xDE)
      {
        // checksum
      }
      continue;
    }

    const int nodeIndex = _refs[iNode];
    if (nodeIndex < 0)
      return S_FALSE;
    CNode &node = _nodes[nodeIndex];

    if (_h.IsThereFileType() && type != 0)
    {
      if (type >= Z7_ARRAY_SIZE(k_TypeToMode))
        return S_FALSE;
      if (k_TypeToMode[type] != (node.Mode & MY_LIN_S_IFMT))
        return S_FALSE;
    }

    node.NumLinksCalced++;
    
    PRF(printf("\n%s %6d %s", item.IsDir() ? "DIR  " : "     ", (unsigned)item.Node, (const char *)item.Name));
    
    if (item.Name[0] == '.')
    {
      if (item.Name[1] == 0)
      {
        if (isThereSelfLink)
          return S_FALSE;
        isThereSelfLink = true;
        if (iNode != iNodeDir)
          return S_FALSE;
        continue;
      }
      
      if (item.Name[1] == '.' && item.Name[2] == 0)
      {
        if (parentNode >= 0)
          return S_FALSE;
        if (!node.IsDir())
          return S_FALSE;
        if (iNode == iNodeDir && iNode != k_INODE_ROOT)
          return S_FALSE;
        
        parentNode = (int)iNode;

        if (nodeDir.ParentNode < 0)
          nodeDir.ParentNode = (int)iNode;
        else if ((unsigned)nodeDir.ParentNode != iNode)
          return S_FALSE;

        continue;
      }
    }

    if (iNode == iNodeDir)
      return S_FALSE;

    if (parentNode < 0)
      return S_FALSE;

    if (node.IsDir())
    {
      if (node.ParentNode < 0)
        node.ParentNode = (int)iNodeDir;
      else if ((unsigned)node.ParentNode != iNodeDir)
        return S_FALSE;
      const unsigned itemIndex = _items.Size();
      dir.Add(itemIndex);
      node.ItemIndex = (int)itemIndex;
    }

    _items.Add(item);
  }

  if (parentNode < 0 || !isThereSelfLink)
    return S_FALSE;

  return S_OK;
}


static int CompareItemsNames(const unsigned *p1, const unsigned *p2, void *param)
{
  const CObjectVector<CItem> &items = *(const CObjectVector<CItem> *)param;
  return strcmp(items[*p1].Name, items[*p2].Name);
}


int CHandler::FindTargetItem_for_SymLink(unsigned iNode, const AString &path) const
{
  unsigned pos = 0;
  
  if (path.IsEmpty())
    return -1;
  
  if (path[0] == '/')
  {
    iNode = k_INODE_ROOT;
    if (iNode >= _refs.Size())
      return -1;
    pos = 1;
  }

  AString s;

  while (pos != path.Len())
  {
    const CNode &node = _nodes[_refs[iNode]];
    const int slash = path.Find('/', pos);
    
    if (slash < 0)
    {
      s = path.Ptr(pos);
      pos = path.Len();
    }
    else
    {
      s.SetFrom(path.Ptr(pos), (unsigned)slash - pos);
      pos = (unsigned)slash + 1;
    }
    
    if (s[0] == '.')
    {
      if (s[1] == 0)
        continue;
      else if (s[1] == '.' && s[2] == 0)
      {
        if (node.ParentNode < 0)
          return -1;
        if (iNode == k_INODE_ROOT)
          return -1;
        iNode = (unsigned)node.ParentNode;
        continue;
      }
    }
    
    if (node.DirIndex < 0)
      return -1;

    const CUIntVector &dir = _dirs[node.DirIndex];
    
    /*
    for (unsigned i = 0;; i++)
    {
      if (i >= dir.Size())
        return -1;
      const CItem &item = _items[dir[i]];
      if (item.Name == s)
      {
        iNode = item.Node;
        break;
      }
    }
    */

    unsigned left = 0, right = dir.Size();
    for (;;)
    {
      if (left == right)
        return -1;
      const unsigned mid = (unsigned)(((size_t)left + (size_t)right) / 2);
      const CItem &item = _items[dir[mid]];
      const int comp = strcmp(s, item.Name);
      if (comp == 0)
      {
        iNode = item.Node;
        break;
      }
      if (comp < 0)
        right = mid;
      else
        left = mid + 1;
    }
  }

  return _nodes[_refs[iNode]].ItemIndex;
}


HRESULT CHandler::SeekAndRead(IInStream *inStream, UInt64 block, Byte *data, size_t size)
{
  if (block == 0 || block >= _h.NumBlocks)
    return S_FALSE;
  if (((size + ((size_t)1 << _h.BlockBits) - 1) >> _h.BlockBits) > _h.NumBlocks - block)
    return S_FALSE;
  RINOK(InStream_SeekSet(inStream, (UInt64)block << _h.BlockBits))
  _totalRead += size;
  return ReadStream_FALSE(inStream, data, size);
}


static const unsigned kHeaderSize = 2 * 1024;
static const unsigned kHeaderDataOffset = 1024;

HRESULT CHandler::Open2(IInStream *inStream)
{
  {
    Byte buf[kHeaderSize];
    RINOK(ReadStream_FALSE(inStream, buf, kHeaderSize))
    if (!_h.Parse(buf + kHeaderDataOffset))
      return S_FALSE;
    if (_h.BlockGroupNr != 0)
      return S_FALSE; // it's just copy of super block
  }
  
  {
    // ---------- Read groups and nodes ----------
    
    unsigned numGroups;
    {
      const UInt64 numGroups64 = _h.GetNumGroups();
      if (numGroups64 > (UInt32)1 << 31)
        return S_FALSE;
      numGroups = (unsigned)numGroups64;
    }

    unsigned gdBits = 5;
    if (_h.Is64Bit())
    {
      if (_h.GdSize != 64)
        return S_FALSE;
      gdBits = 6;
    }

    _isArc = true;
    _phySize = _h.GetPhySize();

    if (_openCallback)
    {
      RINOK(_openCallback->SetTotal(NULL, &_phySize))
    }

    UInt64 fileSize = 0;
    RINOK(InStream_GetSize_SeekToEnd(inStream, fileSize))

    CRecordVector<CGroupDescriptor> groups;

    {
      // ---------- Read groups ----------

      CByteBuffer gdBuf;
      const size_t gdBufSize = (size_t)numGroups << gdBits;
      if ((gdBufSize >> gdBits) != numGroups)
        return S_FALSE;
      gdBuf.Alloc(gdBufSize);
      RINOK(SeekAndRead(inStream, (_h.BlockBits <= 10 ? 2 : 1), gdBuf, gdBufSize))

      for (unsigned i = 0; i < numGroups; i++)
      {
        CGroupDescriptor gd;
        
        const Byte *p = gdBuf + ((size_t)i << gdBits);
        const unsigned gd_Size = (unsigned)1 << gdBits;
        gd.Parse(p, gd_Size);
        
        if (_h.UseMetadataChecksum())
        {
          // use CRC32c
        }
        else if (_h.UseGdtChecksum())
        {
          UInt32 crc = Crc16Calc(_h.Uuid, sizeof(_h.Uuid));
          Byte i_le[4];
          SetUi32(i_le, i)
          crc = Crc16Update(crc, i_le, 4);
          crc = Crc16Update(crc, p, 32 - 2);
          if (gd_Size != 32)
            crc = Crc16Update(crc, p + 32, gd_Size - 32);
          if (crc != gd.Checksum)
            return S_FALSE;
        }
        
        groups.Add(gd);
      }
    }

    {
      // ---------- Read nodes ----------

      if (_h.NumInodes < _h.NumFreeInodes)
        return S_FALSE;

      UInt32 numNodes = _h.InodesPerGroup;
      if (numNodes > _h.NumInodes)
        numNodes = _h.NumInodes;
      const size_t nodesDataSize = (size_t)numNodes * _h.InodeSize;
      
      if (nodesDataSize / _h.InodeSize != numNodes)
        return S_FALSE;

      // that code to reduce false detecting cases
      if (nodesDataSize > fileSize)
      {
        if (numNodes > (1 << 24))
          return S_FALSE;
      }
      
      const UInt32 numReserveInodes = _h.NumInodes - _h.NumFreeInodes + 1;
      // numReserveInodes = _h.NumInodes + 1;
      if (numReserveInodes != 0)
      {
        _nodes.Reserve(numReserveInodes);
        _refs.Reserve(numReserveInodes);
      }
      
      CByteBuffer nodesData;
      nodesData.Alloc(nodesDataSize);
      
      CByteBuffer nodesMap;
      const size_t blockSize = (size_t)1 << _h.BlockBits;
      nodesMap.Alloc(blockSize);
      
      unsigned globalNodeIndex = 0;
      // unsigned numEmpty = 0;
      unsigned numEmpty_in_Maps = 0;

      FOR_VECTOR (gi, groups)
      {
        if (globalNodeIndex >= _h.NumInodes)
          break;

        const CGroupDescriptor &gd = groups[gi];
        
        PRF(printf("\n\ng%6d block = %6x\n", gi, (unsigned)gd.InodeTable));
        
        RINOK(SeekAndRead(inStream, gd.InodeBitmap, nodesMap, blockSize))
        RINOK(SeekAndRead(inStream, gd.InodeTable, nodesData, nodesDataSize))

        unsigned numEmpty_in_Map = 0;
        
        for (size_t n = 0; n < numNodes && globalNodeIndex < _h.NumInodes; n++, globalNodeIndex++)
        {
          if ((nodesMap[n >> 3] & ((unsigned)1 << (n & 7))) == 0)
          {
            numEmpty_in_Map++;
            continue;
          }

          const Byte *p = nodesData + (size_t)n * _h.InodeSize;
          if (IsEmptyData(p, _h.InodeSize))
          {
            if (globalNodeIndex + 1 >= _h.FirstInode)
            {
              _headersError = true;
              // return S_FALSE;
            }
            continue;
          }

          CNode node;
              
          PRF(printf("\nnode = %5d ", (unsigned)n));

          if (!node.Parse(p, _h))
            return S_FALSE;
  
          // PRF(printf("\n %6d", (unsigned)n));
          /*
            SetUi32(p + 0x7C, 0)
            SetUi32(p + 0x82, 0)
            
            UInt32 crc = Crc32C_Calc(_h.Uuid, sizeof(_h.Uuid));
            Byte i_le[4];
            SetUi32(i_le, n);
            crc = Crc32C_Update(crc, i_le, 4);
            crc = Crc32C_Update(crc, p, _h.InodeSize);
            if (crc != node.Checksum) return S_FALSE;
          */

          while (_refs.Size() < globalNodeIndex + 1)
          {
            // numEmpty++;
            _refs.Add(-1);
          }
          
          _refs.Add((int)_nodes.Add(node));
        }

        
        numEmpty_in_Maps += numEmpty_in_Map;
        
        if (numEmpty_in_Map != gd.NumFreeInodes)
        {
          _headersWarning = true;
          // return S_FALSE;
        }
      }
      
      if (numEmpty_in_Maps != _h.NumFreeInodes)
      {
        // some ext2 examples has incorrect value in _h.NumFreeInodes.
        // so we disable check;
        _headersWarning = true;
      }

      if (_refs.Size() <= k_INODE_ROOT)
        return S_FALSE;

      // printf("\n numReserveInodes = %6d, _refs.Size() = %d, numEmpty = %7d\n", numReserveInodes, _refs.Size(), (unsigned)numEmpty);
    }
  }

  _stream = inStream; // we need stream for dir nodes

  {
    // ---------- Read Dirs ----------

    CByteBuffer dataBuf;

    FOR_VECTOR (i, _refs)
    {
      const int nodeIndex = _refs[i];
      {
        if (nodeIndex < 0)
          continue;
        const CNode &node = _nodes[nodeIndex];
        if (!node.IsDir())
          continue;
      }
      RINOK(ExtractNode((unsigned)nodeIndex, dataBuf))
      if (dataBuf.Size() == 0)
      {
        // _headersError = true;
        return S_FALSE;
      }
      else
      {
        RINOK(ParseDir(dataBuf, dataBuf.Size(), i))
      }
      RINOK(CheckProgress())
    }

    const int ref = _refs[k_INODE_ROOT];
    if (ref < 0 || _nodes[ref].ParentNode != k_INODE_ROOT)
      return S_FALSE;
  }

  {
    // ---------- Check NumLinks and unreferenced dir nodes ----------
  
    FOR_VECTOR (i, _refs)
    {
      int nodeIndex = _refs[i];
      if (nodeIndex < 0)
        continue;
      const CNode &node = _nodes[nodeIndex];

      if (node.NumLinks != node.NumLinksCalced)
      {
        if (node.NumLinks != 1 || node.NumLinksCalced != 0
            // ) && i >= _h.FirstInode
            )
        {
          _linksError = true;
          // return S_FALSE;
        }
      }

      if (!node.IsDir())
        continue;

      if (node.ParentNode < 0)
      {
        if (i >= _h.FirstInode)
          return S_FALSE;
        continue;
      }
    }
  }

  {
    // ---------- Check that there is no loops in parents list ----------

    unsigned numNodes = _refs.Size();
    CIntArr UsedByNode(numNodes);
    {
      {
        for (unsigned i = 0; i < numNodes; i++)
          UsedByNode[i] = -1;
      }
    }

    FOR_VECTOR (i, _refs)
    {
      {
        int nodeIndex = _refs[i];
        if (nodeIndex < 0)
          continue;
        const CNode &node = _nodes[nodeIndex];
        if (node.ParentNode < 0 // not dir
            || i == k_INODE_ROOT)
          continue;
      }

      unsigned c = i;

      for (;;)
      {
        const int nodeIndex = _refs[c];
        if (nodeIndex < 0)
          return S_FALSE;
        CNode &node = _nodes[nodeIndex];
        
        if (UsedByNode[c] != -1)
        {
          if ((unsigned)UsedByNode[c] == i)
            return S_FALSE;
          break;
        }
        
        UsedByNode[c] = (int)i;
        if (node.ParentNode < 0 || node.ParentNode == k_INODE_ROOT)
          break;
        if ((unsigned)node.ParentNode == i)
          return S_FALSE;
        c = (unsigned)node.ParentNode;
      }
    }
  }
  
  {
    // ---------- Fill SymLinks data ----------

    AString s;
    CByteBuffer data;

    unsigned i;
    for (i = 0; i < _refs.Size(); i++)
    {
      const int nodeIndex = _refs[i];
      if (nodeIndex < 0)
        continue;
      CNode &node = _nodes[nodeIndex];
      if (!node.IsLink())
        continue;
      if (node.FileSize > ((UInt32)1 << 14))
        continue;
      if (ExtractNode((unsigned)nodeIndex, data) == S_OK && data.Size() != 0)
      {
        s.SetFrom_CalcLen((const char *)(const Byte *)data, (unsigned)data.Size());
        if (s.Len() == data.Size())
          node.SymLinkIndex = (int)_symLinks.Add(s);
        RINOK(CheckProgress())
      }
    }

    for (i = 0; i < _dirs.Size(); i++)
    {
      _dirs[i].Sort(CompareItemsNames, (void *)&_items);
    }

    unsigned prev = 0;
    unsigned complex = 0;

    for (i = 0; i < _items.Size(); i++)
    {
      CItem &item = _items[i];
      const int sym = _nodes[_refs[item.Node]].SymLinkIndex;
      if (sym >= 0 && item.ParentNode >= 0)
      {
        item.SymLinkItemIndex = FindTargetItem_for_SymLink((unsigned)item.ParentNode, _symLinks[sym]);
        if (_openCallback)
        {
          complex++;
          if (complex - prev >= (1 << 10))
          {
            prev = complex;
            RINOK(CheckProgress2())
          }
        }
      }
    }
  }

  {
    // ---------- Add items and aux folders for unreferenced files ----------

    bool useSys = false;
    bool useUnknown = false;

    FOR_VECTOR (i, _refs)
    {
      const int nodeIndex = _refs[i];
      if (nodeIndex < 0)
        continue;
      const CNode &node = _nodes[nodeIndex];
      
      if (node.NumLinksCalced == 0 /* || i > 100 && i < 150 */) // for debug
      {
        CItem item;
        item.Node = i;

        // we don't know how to work with k_INODE_RESIZE node (strange FileSize and Block values).
        // so we ignore it;
        
        if (i == k_INODE_RESIZE)
          continue;
        
        if (node.FileSize == 0)
          continue;

        if (i < _h.FirstInode)
        {
          if (item.Node < Z7_ARRAY_SIZE(k_SysInode_Names))
            item.Name = k_SysInode_Names[item.Node];
          useSys = true;
        }
        else
          useUnknown = true;
        
        if (item.Name.IsEmpty())
          item.Name.Add_UInt32(item.Node);

        _items.Add(item);
      }
    }

    if (useSys)
      _auxSysIndex = (int)_auxItems.Add((AString)"[SYS]");
    if (useUnknown)
      _auxUnknownIndex = (int)_auxItems.Add((AString)"[UNKNOWN]");
  }

  return S_OK;
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  {
    Close();
    HRESULT res;
    try
    {
      _openCallback = callback;
      res = Open2(stream);
    }
    catch(...)
    {
      ClearRefs();
      throw;
    }
    
    if (res != S_OK)
    {
      ClearRefs();
      return res;
    }
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}


void CHandler::ClearRefs()
{
  _stream.Release();
  _items.Clear();
  _nodes.Clear();
  _refs.Clear();
  _auxItems.Clear();
  _symLinks.Clear();
  _dirs.Clear();
  _auxSysIndex = -1;
  _auxUnknownIndex = -1;
}


Z7_COM7F_IMF(CHandler::Close())
{
  _totalRead = 0;
  _totalReadPrev = 0;
  _phySize = 0;
  _isArc = false;
  _headersError = false;
  _headersWarning = false;
  _linksError = false;
  _isUTF = true;

  ClearRefs();
  return S_OK;
}


static void ChangeSeparatorsInName(char *s, unsigned num)
{
  for (unsigned i = 0; i < num; i++)
  {
    char c = s[i];
    if (c == CHAR_PATH_SEPARATOR || c == '/')
      s[i] = '_';
  }
}


void CHandler::GetPath(unsigned index, AString &s) const
{
  s.Empty();

  if (index >= _items.Size())
  {
    s = _auxItems[index - _items.Size()];
    return;
  }
  
  for (;;)
  {
    const CItem &item = _items[index];
    if (!s.IsEmpty())
      s.InsertAtFront(CHAR_PATH_SEPARATOR);
    s.Insert(0, item.Name);
    // 18.06
    ChangeSeparatorsInName(s.GetBuf(), item.Name.Len());

    if (item.ParentNode == k_INODE_ROOT)
      return;

    if (item.ParentNode < 0)
    {
      int aux = GetParentAux(item);
      if (aux < 0)
        break;
      s.InsertAtFront(CHAR_PATH_SEPARATOR);
      s.Insert(0, _auxItems[aux]);
      return;
    }

    const CNode &node = _nodes[_refs[item.ParentNode]];
    if (node.ItemIndex < 0)
      return;
    index = (unsigned)node.ItemIndex;

    if (s.Len() > ((UInt32)1 << 16))
    {
      s.Insert(0, "[LONG]" STRING_PATH_SEPARATOR);
      return;
    }
  }
}


bool CHandler::GetPackSize(unsigned index, UInt64 &totalPack) const
{
  if (index >= _items.Size())
  {
    totalPack = 0;
    return false;
  }

  const CItem &item = _items[index];
  const CNode &node = _nodes[_refs[item.Node]];

  // if (!node.IsFlags_EXTENTS())
  {
    totalPack = (UInt64)node.NumBlocks << (node.IsFlags_HUGE() ? _h.BlockBits : 9);
    return true;
  }

  /*
  CExtentTreeHeader eth;
  if (!eth.Parse(node.Block))
    return false;
  if (eth.NumEntries > 3)
    return false;
  if (!eth.Depth == 0)
    return false;

  UInt64 numBlocks = 0;
  {
    for (unsigned i = 0; i < eth.NumEntries; i++)
    {
      CExtent e;
      e.Parse(node.Block + 12 + i * 12);
      // const CExtent &e = node.leafs[i];
      if (e.IsInited)
        numBlocks += e.Len;
    }
  }

  totalPack = numBlocks << _h.BlockBits;
  return true;
  */
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size() + _auxItems.Size();
  return S_OK;
}

enum
{
  kpidMountTime = kpidUserDefined,
  kpidLastCheckTime,
  kpidRevision,
  kpidINodeSize,
  kpidLastMount,
  kpidFeatureIncompat,
  kpidFeatureRoCompat,
  kpidWrittenKB

  // kpidGroupSize,

  // kpidChangeTime = kpidUserDefined + 256,
  // kpidDTime
};

static const UInt32 kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidPosixAttrib,
  kpidMTime,
  kpidCTime,
  kpidATime,
  // kpidChangeTime,
  // kpidDTime,
  kpidINode,
  kpidLinks,
  kpidSymLink,
  kpidCharacts,
  kpidUserId,
  kpidGroupId
};


static const CStatProp kArcProps[] =
{
  { NULL, kpidHeadersSize, VT_BSTR },
  // { NULL, kpidFileSystem, VT_BSTR },
  // kpidMethod,
  { NULL, kpidClusterSize, VT_UI4 },
  // { "Group Size", kpidGroupSize, VT_UI8 },
  { NULL, kpidFreeSpace, VT_UI8 },
  
  { NULL, kpidMTime, VT_FILETIME },
  { NULL, kpidCTime, VT_FILETIME },
  { "Mount Time", kpidMountTime, VT_FILETIME },
  { "Last Check Time", kpidLastCheckTime, VT_FILETIME },
  
  { NULL, kpidHostOS, VT_BSTR},
  { "Revision", kpidRevision, VT_UI4},
  { "inode Size", kpidINodeSize, VT_UI4},
  { NULL, kpidCodePage, VT_BSTR},
  { NULL, kpidVolumeName, VT_BSTR},
  { "Last Mounted", kpidLastMount, VT_BSTR},
  { NULL, kpidId, VT_BSTR},
  { NULL, kpidCharacts, VT_BSTR },
  { "Incompatible Features", kpidFeatureIncompat, VT_BSTR },
  { "Readonly-compatible Features", kpidFeatureRoCompat, VT_BSTR },
  { "Written KiB", kpidWrittenKB, VT_UI8 }
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME

static void StringToProp(bool isUTF, const char *s, unsigned size, NCOM::CPropVariant &prop)
{
  UString u;
  AString a;
  a.SetFrom_CalcLen(s, size);
  if (!isUTF || !ConvertUTF8ToUnicode(a, u))
    MultiByteToUnicodeString2(u, a);
  prop = u;
}

static void UnixTimeToProp(UInt32 val, NCOM::CPropVariant &prop)
{
  if (val != 0)
    PropVariant_SetFrom_UnixTime(prop, val);
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
 
  NCOM::CPropVariant prop;
  
  switch (propID)
  {
    /*
    case kpidFileSystem:
    {
      AString res = "Ext4";
      prop = res;
      break;
    }
    */
    
    case kpidIsTree: prop = true; break;
    case kpidIsAux: prop = true; break;
    case kpidINode: prop = true; break;

    case kpidClusterSize: prop = (UInt32)1 << _h.BlockBits; break;
    // case kpidGroupSize: prop = (UInt64)_h.BlocksPerGroup << _h.BlockBits; break;
    
    case kpidFreeSpace: prop = (UInt64)_h.NumFreeBlocks << _h.BlockBits; break;

    case kpidCTime: UnixTimeToProp(_h.CTime, prop); break;
    case kpidMTime: UnixTimeToProp(_h.WriteTime, prop); break;
    case kpidMountTime: UnixTimeToProp(_h.MountTime, prop); break;
    case kpidLastCheckTime: UnixTimeToProp(_h.LastCheckTime, prop); break;

    case kpidHostOS:
    {
      TYPE_TO_PROP(kHostOS, _h.CreatorOs, prop);
      break;
    }
    
    case kpidRevision: prop = _h.RevLevel; break;

    case kpidINodeSize: prop = (UInt32)_h.InodeSize; break;

    case kpidId:
    {
      if (!IsEmptyData(_h.Uuid, 16))
      {
        char s[16 * 2 + 2];
        for (unsigned i = 0; i < 16; i++)
          PrintHex(_h.Uuid[i], s + i * 2);
        s[16 * 2] = 0;
        prop = s;
      }
      break;
    }

    case kpidCodePage: if (_isUTF) prop = "UTF-8"; break;
    
    case kpidShortComment:
    case kpidVolumeName:
        StringToProp(_isUTF, _h.VolName, sizeof(_h.VolName), prop); break;
    
    case kpidLastMount: StringToProp(_isUTF, _h.LastMount, sizeof(_h.LastMount), prop); break;

    case kpidCharacts: FLAGS_TO_PROP(g_FeatureCompat_Flags, _h.FeatureCompat, prop); break;
    case kpidFeatureIncompat: FLAGS_TO_PROP(g_FeatureIncompat_Flags, _h.FeatureIncompat, prop); break;
    case kpidFeatureRoCompat: FLAGS_TO_PROP(g_FeatureRoCompat_Flags, _h.FeatureRoCompat, prop); break;
    case kpidWrittenKB: if (_h.WrittenKB != 0) prop = _h.WrittenKB; break;

    case kpidPhySize: prop = _phySize; break;

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_linksError) v |= kpv_ErrorFlags_HeadersError;
      if (_headersError) v |= kpv_ErrorFlags_HeadersError;
      if (!_stream && v == 0 && _isArc)
        v = kpv_ErrorFlags_HeadersError;
      if (v != 0)
        prop = v;
      break;
    }

    case kpidWarningFlags:
    {
      UInt32 v = 0;
      if (_headersWarning) v |= kpv_ErrorFlags_HeadersError;
      if (v != 0)
        prop = v;
      break;
    }
  }
  
  prop.Detach(value);
  return S_OK;

  COM_TRY_END
}


/*
static const Byte kRawProps[] =
{
  // kpidSha1,
};
*/

Z7_COM7F_IMF(CHandler::GetNumRawProps(UInt32 *numProps))
{
  // *numProps = Z7_ARRAY_SIZE(kRawProps);
  *numProps = 0;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetRawPropInfo(UInt32 /* index */, BSTR *name, PROPID *propID))
{
  // *propID = kRawProps[index];
  *propID = 0;
  *name = NULL;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType))
{
  *parentType = NParentType::kDir;
  *parent = (UInt32)(Int32)-1;

  if (index >= _items.Size())
    return S_OK;

  const CItem &item = _items[index];
  
  if (item.ParentNode < 0)
  {
    const int aux = GetParentAux(item);
    if (aux >= 0)
      *parent = _items.Size() + (unsigned)aux;
  }
  else
  {
    const int itemIndex = _nodes[_refs[item.ParentNode]].ItemIndex;
    if (itemIndex >= 0)
      *parent = (unsigned)itemIndex;
  }
  
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType))
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;

  if (propID == kpidName && _isUTF)
  {
    if (index < _items.Size())
    {
      const AString &s = _items[index].Name;
      if (!s.IsEmpty())
      {
        *data = (void *)(const char *)s;
        *dataSize = (UInt32)s.Len() + 1;
        *propType = NPropDataType::kUtf8z;
      }
      return S_OK;
    }
    else
    {
      const AString &s = _auxItems[index - _items.Size()];
      {
        *data = (void *)(const char *)s;
        *dataSize = (UInt32)s.Len() + 1;
        *propType = NPropDataType::kUtf8z;
      }
      return S_OK;
    }
  }

  return S_OK;
}


static void ExtTimeToProp(const CExtTime &t, NCOM::CPropVariant &prop)
{
  if (t.Val == 0 && t.Extra == 0)
    return;

  FILETIME ft;
  unsigned low100ns = 0;
  // if (t.Extra != 0)
  {
    // 1901-2446 :
    Int64 v = (Int64)(Int32)t.Val;
    v += (UInt64)(t.Extra & 3) << 32;  // 2 low bits are offset for main timestamp
    UInt64 ft64 = NTime::UnixTime64_To_FileTime64(v);
    const UInt32 ns = (t.Extra >> 2);
    if (ns < 1000000000)
    {
      ft64 += ns / 100;
      low100ns = (unsigned)(ns % 100);
    }
    ft.dwLowDateTime = (DWORD)ft64;
    ft.dwHighDateTime = (DWORD)(ft64 >> 32);
  }
  /*
  else
  {
    // 1901-2038 : that code is good for ext4 and compatibility with Extra
    NTime::UnixTime64ToFileTime((Int32)t.Val, ft); // for

    // 1970-2106 : that code is good if timestamp is used as unsigned 32-bit
    // are there such systems?
    // NTime::UnixTimeToFileTime(t.Val, ft); // for
  }
  */
  prop.SetAsTimeFrom_FT_Prec_Ns100(ft, k_PropVar_TimePrec_1ns, low100ns);
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  if (index >= _items.Size())
  {
    switch (propID)
    {
      case kpidPath:
      case kpidName:
      {
        prop = _auxItems[index - _items.Size()];
        break;
      }
      case kpidIsDir: prop = true; break;
      case kpidIsAux: prop = true; break;
    }
  }
  else
  {

  const CItem &item = _items[index];
  const CNode &node = _nodes[_refs[item.Node]];
  bool isDir = node.IsDir();

  switch (propID)
  {
    case kpidPath:
    {
      UString u;
      {
        AString s;
        GetPath(index, s);
        if (!_isUTF || !ConvertUTF8ToUnicode(s, u))
          MultiByteToUnicodeString2(u, s);
      }
      prop = u;
      break;
    }
    
    case kpidName:
    {
      {
        UString u;
        {
          if (!_isUTF || !ConvertUTF8ToUnicode(item.Name, u))
            MultiByteToUnicodeString2(u, item.Name);
        }
        prop = u;
      }
      break;
    }
    
    case kpidIsDir:
    {
      bool isDir2 = isDir;
      if (item.SymLinkItemIndex >= 0)
        isDir2 = _nodes[_refs[_items[item.SymLinkItemIndex].Node]].IsDir();
      prop = isDir2;
      break;
    }

    case kpidSize: if (!isDir) prop = node.FileSize; break;
    
    case kpidPackSize:
      if (!isDir)
      {
        UInt64 size;
        if (GetPackSize(index, size))
          prop = size;
      }
      break;

    case kpidPosixAttrib:
    {
      /*
      if (node.Type != 0 && node.Type < Z7_ARRAY_SIZE(k_TypeToMode))
        prop = (UInt32)(node.Mode & 0xFFF) | k_TypeToMode[node.Type];
      */
      prop = (UInt32)(node.Mode);
      break;
    }

    case kpidMTime: ExtTimeToProp(node.MTime, prop); break;
    case kpidCTime: ExtTimeToProp(node.CTime, prop); break;
    case kpidATime: ExtTimeToProp(node.ATime, prop); break;
    // case kpidDTime: ExtTimeToProp(node.DTime, prop); break;
    case kpidChangeTime: ExtTimeToProp(node.ChangeTime, prop); break;
    case kpidUserId: prop = (UInt32)node.Uid; break;
    case kpidGroupId: prop = (UInt32)node.Gid; break;
    case kpidLinks: prop = node.NumLinks; break;
    case kpidINode: prop = (UInt32)item.Node; break;
    case kpidStreamId: if (!isDir) prop = (UInt32)item.Node; break;
    case kpidCharacts: FLAGS_TO_PROP(g_NodeFlags, (UInt32)node.Flags, prop); break;

    case kpidSymLink:
    {
      if (node.SymLinkIndex >= 0)
      {
        UString u;
        {
          const AString &s = _symLinks[node.SymLinkIndex];
          if (!_isUTF || !ConvertUTF8ToUnicode(s, u))
            MultiByteToUnicodeString2(u, s);
        }
        prop = u;
      }
      break;
    }
  }

  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_CLASS_IMP_IInStream(CClusterInStream2
)
  UInt64 _virtPos;
  UInt64 _physPos;
  UInt32 _curRem;
public:
  unsigned BlockBits;
  UInt64 Size;
  CMyComPtr<IInStream> Stream;
  CRecordVector<UInt32> Vector;

  HRESULT SeekToPhys() { return InStream_SeekSet(Stream, _physPos); }

  HRESULT InitAndSeek()
  {
    _curRem = 0;
    _virtPos = 0;
    _physPos = 0;
    if (Vector.Size() > 0)
    {
      _physPos = (Vector[0] << BlockBits);
      return SeekToPhys();
    }
    return S_OK;
  }
};


Z7_COM7F_IMF(CClusterInStream2::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  if (_virtPos >= Size)
    return S_OK;
  {
    UInt64 rem = Size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }
  if (size == 0)
    return S_OK;

  if (_curRem == 0)
  {
    const UInt32 blockSize = (UInt32)1 << BlockBits;
    const UInt32 virtBlock = (UInt32)(_virtPos >> BlockBits);
    const UInt32 offsetInBlock = (UInt32)_virtPos & (blockSize - 1);
    const UInt32 phyBlock = Vector[virtBlock];
    
    if (phyBlock == 0)
    {
      UInt32 cur = blockSize - offsetInBlock;
      if (cur > size)
        cur = size;
      memset(data, 0, cur);
      _virtPos += cur;
      if (processedSize)
        *processedSize = cur;
      return S_OK;
    }
    
    UInt64 newPos = ((UInt64)phyBlock << BlockBits) + offsetInBlock;
    if (newPos != _physPos)
    {
      _physPos = newPos;
      RINOK(SeekToPhys())
    }
    
    _curRem = blockSize - offsetInBlock;
    
    for (unsigned i = 1; i < 64 && (virtBlock + i) < (UInt32)Vector.Size() && phyBlock + i == Vector[virtBlock + i]; i++)
      _curRem += (UInt32)1 << BlockBits;
  }

  if (size > _curRem)
    size = _curRem;
  HRESULT res = Stream->Read(data, size, &size);
  if (processedSize)
    *processedSize = size;
  _physPos += size;
  _virtPos += size;
  _curRem -= size;
  return res;
}
 
Z7_COM7F_IMF(CClusterInStream2::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition))
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
    _curRem = 0;
  _virtPos = (UInt64)offset;
  if (newPosition)
    *newPosition = (UInt64)offset;
  return S_OK;
}


Z7_CLASS_IMP_IInStream(
  CExtInStream
)
  UInt64 _virtPos;
  UInt64 _phyPos;
public:
  unsigned BlockBits;
  UInt64 Size;
  CMyComPtr<IInStream> Stream;
  CRecordVector<CExtent> Extents;

  HRESULT StartSeek()
  {
    _virtPos = 0;
    _phyPos = 0;
    return InStream_SeekSet(Stream, _phyPos);
  }
};

Z7_COM7F_IMF(CExtInStream::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  if (_virtPos >= Size)
    return S_OK;
  {
    UInt64 rem = Size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }
  if (size == 0)
    return S_OK;

  UInt32 blockIndex = (UInt32)(_virtPos >> BlockBits);
  
  unsigned left = 0, right = Extents.Size();
  for (;;)
  {
    unsigned mid = (left + right) / 2;
    if (mid == left)
      break;
    if (blockIndex < Extents[mid].VirtBlock)
      right = mid;
    else
      left = mid;
  }

  {
    const CExtent &extent = Extents[left];
    if (blockIndex < extent.VirtBlock)
      return E_FAIL;
    UInt32 bo = blockIndex - extent.VirtBlock;
    if (bo >= extent.Len)
      return E_FAIL;

    UInt32 offset = ((UInt32)_virtPos & (((UInt32)1 << BlockBits) - 1));
    UInt32 remBlocks = extent.Len - bo;
    UInt64 remBytes = ((UInt64)remBlocks << BlockBits);
    remBytes -= offset;

    if (size > remBytes)
      size = (UInt32)remBytes;

    if (!extent.IsInited)
    {
      memset(data, 0, size);
      _virtPos += size;
      if (processedSize)
        *processedSize = size;
      return S_OK;
    }

    const UInt64 phyBlock = extent.PhyStart + bo;
    const UInt64 phy = (phyBlock << BlockBits) + offset;
    
    if (phy != _phyPos)
    {
      RINOK(InStream_SeekSet(Stream, phy))
      _phyPos = phy;
    }
    
    UInt32 realProcessSize = 0;
    
    const HRESULT res = Stream->Read(data, size, &realProcessSize);
    
    _phyPos += realProcessSize;
    _virtPos += realProcessSize;
    if (processedSize)
      *processedSize = realProcessSize;
    return res;
  }
}


Z7_COM7F_IMF(CExtInStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition))
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



HRESULT CHandler::FillFileBlocks2(UInt32 block, unsigned level, unsigned numBlocks, CRecordVector<UInt32> &blocks)
{
  const size_t blockSize = (size_t)1 << _h.BlockBits;
  CByteBuffer &tempBuf = _tempBufs[level];
  tempBuf.Alloc(blockSize);

  PRF2(printf("\n level = %d, block = %7d", level, (unsigned)block));

  RINOK(SeekAndRead(_stream, block, tempBuf, blockSize))

  const Byte *p = tempBuf;
  size_t num = (size_t)1 << (_h.BlockBits - 2);

  for (size_t i = 0; i < num; i++)
  {
    if (blocks.Size() == numBlocks)
      break;
    UInt32 val = GetUi32(p + 4 * i);
    if (val >= _h.NumBlocks)
      return S_FALSE;

    if (level != 0)
    {
      if (val == 0)
      {
        /*
        size_t num = (size_t)1 << ((_h.BlockBits - 2) * (level));
        PRF2(printf("\n num empty = %3d", (unsigned)num));
        for (size_t k = 0; k < num; k++)
        {
          blocks.Add(0);
          if (blocks.Size() == numBlocks)
            return S_OK;
        }
        continue;
        */
        return S_FALSE;
      }
      
      RINOK(FillFileBlocks2(val, level - 1, numBlocks, blocks))
      continue;
    }
    
    PRF2(printf("\n i = %3d,  blocks.Size() = %6d, block = %5d ", i, blocks.Size(), (unsigned)val));

    PRF(printf("\n i = %3d,  start = %5d ", (unsigned)i, (unsigned)val));
    
    blocks.Add(val);
  }

  return S_OK;
}


static const unsigned kNumDirectNodeBlocks = 12;

HRESULT CHandler::FillFileBlocks(const Byte *p, unsigned numBlocks, CRecordVector<UInt32> &blocks)
{
  // ext2 supports zero blocks (blockIndex == 0).

  blocks.ClearAndReserve(numBlocks);

  for (unsigned i = 0; i < kNumDirectNodeBlocks; i++)
  {
    if (i == numBlocks)
      return S_OK;
    UInt32 val = GetUi32(p + 4 * i);
    if (val >= _h.NumBlocks)
      return S_FALSE;
    blocks.Add(val);
  }
  
  for (unsigned level = 0; level < 3; level++)
  {
    if (blocks.Size() == numBlocks)
      break;
    UInt32 val = GetUi32(p + 4 * (kNumDirectNodeBlocks + level));
    if (val >= _h.NumBlocks)
      return S_FALSE;

    if (val == 0)
    {
      /*
      size_t num = (size_t)1 << ((_h.BlockBits - 2) * (level + 1));
      for (size_t k = 0; k < num; k++)
      {
        blocks.Add(0);
        if (blocks.Size() == numBlocks)
          return S_OK;
      }
      continue;
      */
      return S_FALSE;
    }

    RINOK(FillFileBlocks2(val, level, numBlocks, blocks))
  }
  
  return S_OK;
}


static void AddSkipExtents(CRecordVector<CExtent> &extents, UInt32 virtBlock, UInt32 numBlocks)
{
  while (numBlocks != 0)
  {
    UInt32 len = numBlocks;
    const UInt32 kLenMax = (UInt32)1 << 15;
    if (len > kLenMax)
      len = kLenMax;
    CExtent e;
    e.VirtBlock = virtBlock;
    e.Len = (UInt16)len;
    e.IsInited = false;
    e.PhyStart = 0;
    extents.Add(e);
    virtBlock += len;
    numBlocks -= len;
  }
}

static bool UpdateExtents(CRecordVector<CExtent> &extents, UInt32 block)
{
  if (extents.IsEmpty())
  {
    if (block == 0)
      return true;
    AddSkipExtents(extents, 0, block);
    return true;
  }

  const CExtent &prev = extents.Back();
  if (block < prev.VirtBlock)
    return false;
  UInt32 prevEnd = prev.GetVirtEnd();
  if (block == prevEnd)
    return true;
  AddSkipExtents(extents, prevEnd, block - prevEnd);
  return true;
}


HRESULT CHandler::FillExtents(const Byte *p, size_t size, CRecordVector<CExtent> &extents, int parentDepth)
{
  CExtentTreeHeader eth;
  if (!eth.Parse(p))
    return S_FALSE;

  if (parentDepth >= 0 && eth.Depth != parentDepth - 1) // (eth.Depth >= parentDepth)
    return S_FALSE;

  if (12 + 12 * (size_t)eth.NumEntries > size)
    return S_FALSE;

  if (eth.Depth >= kNumTreeLevelsMax)
    return S_FALSE;

  if (eth.Depth == 0)
  {
    for (unsigned i = 0; i < eth.NumEntries; i++)
    {
      CExtent e;
      e.Parse(p + 12 + i * 12);
      if (e.PhyStart == 0
          || e.PhyStart > _h.NumBlocks
          || e.PhyStart + e.Len > _h.NumBlocks
          || !e.IsLenOK())
        return S_FALSE;
      if (!UpdateExtents(extents, e.VirtBlock))
        return S_FALSE;
      extents.Add(e);
    }
    
    return S_OK;
  }

  const size_t blockSize = (size_t)1 << _h.BlockBits;
  CByteBuffer &tempBuf = _tempBufs[eth.Depth];
  tempBuf.Alloc(blockSize);

  for (unsigned i = 0; i < eth.NumEntries; i++)
  {
    CExtentIndexNode e;
    e.Parse(p + 12 + i * 12);

    if (e.PhyLeaf == 0 || e.PhyLeaf >= _h.NumBlocks)
      return S_FALSE;

    if (!UpdateExtents(extents, e.VirtBlock))
      return S_FALSE;

    RINOK(SeekAndRead(_stream, e.PhyLeaf, tempBuf, blockSize))
    RINOK(FillExtents(tempBuf, blockSize, extents, eth.Depth))
  }

  return S_OK;
}


HRESULT CHandler::GetStream_Node(unsigned nodeIndex, ISequentialInStream **stream)
{
  COM_TRY_BEGIN

  *stream = NULL;

  const CNode &node = _nodes[nodeIndex];

  if (!node.IsFlags_EXTENTS())
  {
    // maybe sparse file can have (node.NumBlocks == 0) ?

    /* The following code doesn't work correctly for some CentOS images,
       where there are nodes with inline data and (node.NumBlocks != 0).
       If you know better way to detect inline data, please notify 7-Zip developers. */

    if (node.NumBlocks == 0 && node.FileSize < kNodeBlockFieldSize)
    {
      Create_BufInStream_WithNewBuffer(node.Block, (size_t)node.FileSize, stream);
      return S_OK;
    }
  }

  if (node.FileSize >= ((UInt64)1 << 63))
    return S_FALSE;

  CMyComPtr<IInStream> streamTemp;
  
  UInt64 numBlocks64 = (node.FileSize + (UInt64)(((UInt32)1 << _h.BlockBits) - 1)) >> _h.BlockBits;
  
  if (node.IsFlags_EXTENTS())
  {
    if ((UInt32)numBlocks64 != numBlocks64)
      return S_FALSE;

    CExtInStream *streamSpec = new CExtInStream;
    streamTemp = streamSpec;
    
    streamSpec->BlockBits = _h.BlockBits;
    streamSpec->Size = node.FileSize;
    streamSpec->Stream = _stream;
    
    RINOK(FillExtents(node.Block, kNodeBlockFieldSize, streamSpec->Extents, -1))

    UInt32 end = 0;
    if (!streamSpec->Extents.IsEmpty())
      end = streamSpec->Extents.Back().GetVirtEnd();
    if (end < numBlocks64)
    {
      AddSkipExtents(streamSpec->Extents, end, (UInt32)(numBlocks64 - end));
      // return S_FALSE;
    }

    RINOK(streamSpec->StartSeek())
  }
  else
  {
    {
      UInt64 numBlocks2 = numBlocks64;

      if (numBlocks64 > kNumDirectNodeBlocks)
      {
        UInt64 rem = numBlocks64 - kNumDirectNodeBlocks;
        const unsigned refBits = (_h.BlockBits - 2);
        const size_t numRefsInBlocks = (size_t)1 << refBits;
        numBlocks2++;
        if (rem > numRefsInBlocks)
        {
          numBlocks2++;
          const UInt64 numL2 = (rem - 1) >> refBits;
          numBlocks2 += numL2;
          if (numL2 > numRefsInBlocks)
          {
            numBlocks2++;
            numBlocks2 += (numL2 - 1) >> refBits;
          }
        }
      }

      const unsigned specBits = (node.IsFlags_HUGE() ? 0 : _h.BlockBits - 9);
      const UInt32 specMask = ((UInt32)1 << specBits) - 1;
      if ((node.NumBlocks & specMask) != 0)
        return S_FALSE;
      const UInt64 numBlocks64_from_header = node.NumBlocks >> specBits;
      if (numBlocks64_from_header < numBlocks2)
      {
        // why (numBlocks64_from_header > numBlocks2) in some cases?
        // return S_FALSE;
      }
    }

    unsigned numBlocks = (unsigned)numBlocks64;
    if (numBlocks != numBlocks64)
      return S_FALSE;

    CClusterInStream2 *streamSpec = new CClusterInStream2;
    streamTemp = streamSpec;

    streamSpec->BlockBits = _h.BlockBits;
    streamSpec->Size = node.FileSize;
    streamSpec->Stream = _stream;

    RINOK(FillFileBlocks(node.Block, numBlocks, streamSpec->Vector))
    streamSpec->InitAndSeek();
  }

  *stream = streamTemp.Detach();

  return S_OK;

  COM_TRY_END
}


HRESULT CHandler::ExtractNode(unsigned nodeIndex, CByteBuffer &data)
{
  data.Free();
  const CNode &node = _nodes[nodeIndex];
  size_t size = (size_t)node.FileSize;
  if (size != node.FileSize)
    return S_FALSE;
  CMyComPtr<ISequentialInStream> inSeqStream;
  RINOK(GetStream_Node(nodeIndex, &inSeqStream))
  if (!inSeqStream)
    return S_FALSE;
  data.Alloc(size);
  _totalRead += size;
  return ReadStream_FALSE(inSeqStream, data, size);
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size() + _auxItems.Size();
  if (numItems == 0)
    return S_OK;
  
  UInt64 totalSize = 0;
  UInt32 i;

  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = allFilesMode ? i : indices[i];
    if (index >= _items.Size())
      continue;
    const CItem &item = _items[index];
    const CNode &node = _nodes[_refs[item.Node]];
    if (!node.IsDir())
      totalSize += node.FileSize;
  }
  
  extractCallback->SetTotal(totalSize);

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0;; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur())

    if (i == numItems)
      break;

    CMyComPtr<ISequentialOutStream> outStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    
    const UInt32 index = allFilesMode ? i : indices[i];
    
    RINOK(extractCallback->GetStream(index, &outStream, askMode))

    if (index >= _items.Size())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }

    const CItem &item = _items[index];
    const CNode &node = _nodes[_refs[item.Node]];

    if (node.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }

    UInt64 unpackSize = node.FileSize;
    totalSize += unpackSize;
    UInt64 packSize;
    if (GetPackSize(index, packSize))
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
  *stream = NULL;
  if (index >= _items.Size())
    return S_FALSE;
  return GetStream_Node((unsigned)_refs[_items[index].Node], stream);
}


API_FUNC_IsArc IsArc_Ext_PhySize(const Byte *p, size_t size, UInt64 *phySize);
API_FUNC_IsArc IsArc_Ext_PhySize(const Byte *p, size_t size, UInt64 *phySize)
{
  if (phySize)
    *phySize = 0;
  if (size < kHeaderSize)
    return k_IsArc_Res_NEED_MORE;
  CHeader h;
  if (!h.Parse(p + kHeaderDataOffset))
    return k_IsArc_Res_NO;
  if (phySize)
    *phySize = h.GetPhySize();
  return k_IsArc_Res_YES;
}


API_FUNC_IsArc IsArc_Ext(const Byte *p, size_t size);
API_FUNC_IsArc IsArc_Ext(const Byte *p, size_t size)
{
  return IsArc_Ext_PhySize(p, size, NULL);
}


static const Byte k_Signature[] = { 0x53, 0xEF };

REGISTER_ARC_I(
  "Ext", "ext ext2 ext3 ext4 img", NULL, 0xC7,
  k_Signature,
  0x438,
  0,
  IsArc_Ext)

}}
