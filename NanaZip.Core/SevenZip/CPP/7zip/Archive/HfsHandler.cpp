// HfsHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "HfsHandler.h"

/* if HFS_SHOW_ALT_STREAMS is defined, the handler will show attribute files
   and resource forks. In most cases it looks useless. So we disable it. */
 
#define HFS_SHOW_ALT_STREAMS

#define Get16(p) GetBe16(p)
#define Get32(p) GetBe32(p)
#define Get64(p) GetBe64(p)

namespace NArchive {
namespace NHfs {

static const char * const kResFileName = "rsrc"; // "com.apple.ResourceFork";

struct CExtent
{
  UInt32 Pos;
  UInt32 NumBlocks;
};

struct CIdExtents
{
  UInt32 ID;
  UInt32 StartBlock;
  CRecordVector<CExtent> Extents;
};

struct CFork
{
  UInt64 Size;
  UInt32 NumBlocks;
  // UInt32 ClumpSize;
  CRecordVector<CExtent> Extents;

  CFork(): Size(0), NumBlocks(0) {}

  void Parse(const Byte *p);

  bool IsEmpty() const { return Size == 0 && NumBlocks == 0 && Extents.Size() == 0; }

  UInt32 Calc_NumBlocks_from_Extents() const;
  bool Check_NumBlocks() const;

  bool Check_Size_with_NumBlocks(unsigned blockSizeLog) const
  {
    return Size <= ((UInt64)NumBlocks << blockSizeLog);
  }

  bool IsOk(unsigned blockSizeLog) const
  {
    // we don't check cases with extra (empty) blocks in last extent
    return Check_NumBlocks() && Check_Size_with_NumBlocks(blockSizeLog);
  }

  bool Upgrade(const CObjectVector<CIdExtents> &items, UInt32 id);
  bool UpgradeAndTest(const CObjectVector<CIdExtents> &items, UInt32 id, unsigned blockSizeLog)
  {
    if (!Upgrade(items, id))
      return false;
    return IsOk(blockSizeLog);
  }
};

static const unsigned kNumFixedExtents = 8;
static const unsigned kForkRecSize = 16 + kNumFixedExtents * 8;


void CFork::Parse(const Byte *p)
{
  Extents.Clear();
  Size = Get64(p);
  // ClumpSize = Get32(p + 8);
  NumBlocks = Get32(p + 12);
  p += 16;
  for (unsigned i = 0; i < kNumFixedExtents; i++, p += 8)
  {
    CExtent e;
    e.Pos = Get32(p);
    e.NumBlocks = Get32(p + 4);
    if (e.NumBlocks != 0)
      Extents.Add(e);
  }
}

UInt32 CFork::Calc_NumBlocks_from_Extents() const
{
  UInt32 num = 0;
  FOR_VECTOR (i, Extents)
  {
    num += Extents[i].NumBlocks;
  }
  return num;
}

bool CFork::Check_NumBlocks() const
{
  UInt32 num = 0;
  FOR_VECTOR (i, Extents)
  {
    UInt32 next = num + Extents[i].NumBlocks;
    if (next < num)
      return false;
    num = next;
  }
  return num == NumBlocks;
}

struct CIdIndexPair
{
  UInt32 ID;
  unsigned Index;

  int Compare(const CIdIndexPair &a) const;
};

#define RINOZ(x) { const int _t_ = (x); if (_t_ != 0) return _t_; }

int CIdIndexPair::Compare(const CIdIndexPair &a) const
{
  RINOZ(MyCompare(ID, a.ID))
  return MyCompare(Index, a.Index);
}

static int FindItemIndex(const CRecordVector<CIdIndexPair> &items, UInt32 id)
{
  unsigned left = 0, right = items.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const UInt32 midVal = items[mid].ID;
    if (id == midVal)
      return (int)items[mid].Index;
    if (id < midVal)
      right = mid;
    else
      left = mid + 1;
  }
  return -1;
}

static int Find_in_IdExtents(const CObjectVector<CIdExtents> &items, UInt32 id)
{
  unsigned left = 0, right = items.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const UInt32 midVal = items[mid].ID;
    if (id == midVal)
      return (int)mid;
    if (id < midVal)
      right = mid;
    else
      left = mid + 1;
  }
  return -1;
}

bool CFork::Upgrade(const CObjectVector<CIdExtents> &items, UInt32 id)
{
  int index = Find_in_IdExtents(items, id);
  if (index < 0)
    return true;
  const CIdExtents &item = items[index];
  if (Calc_NumBlocks_from_Extents() != item.StartBlock)
    return false;
  Extents += item.Extents;
  return true;
}


struct CVolHeader
{
  Byte Header[2];
  UInt16 Version;
  // UInt32 Attr;
  // UInt32 LastMountedVersion;
  // UInt32 JournalInfoBlock;

  UInt32 CTime;
  UInt32 MTime;
  // UInt32 BackupTime;
  // UInt32 CheckedTime;
  
  UInt32 NumFiles;
  UInt32 NumFolders;
  unsigned BlockSizeLog;
  UInt32 NumBlocks;
  UInt32 NumFreeBlocks;

  // UInt32 WriteCount;
  // UInt32 FinderInfo[8];
  // UInt64 VolID;

  UInt64 GetPhySize() const { return (UInt64)NumBlocks << BlockSizeLog; }
  UInt64 GetFreeSize() const { return (UInt64)NumFreeBlocks << BlockSizeLog; }
  bool IsHfsX() const { return Version > 4; }
};

inline void HfsTimeToFileTime(UInt32 hfsTime, FILETIME &ft)
{
  UInt64 v = ((UInt64)3600 * 24 * (365 * 303 + 24 * 3) + hfsTime) * 10000000;
  ft.dwLowDateTime = (DWORD)v;
  ft.dwHighDateTime = (DWORD)(v >> 32);
}

enum ERecordType
{
  RECORD_TYPE_FOLDER = 1,
  RECORD_TYPE_FILE,
  RECORD_TYPE_FOLDER_THREAD,
  RECORD_TYPE_FILE_THREAD
};



// static const UInt32 kMethod_1_NO_COMPRESSION = 1; // in xattr
static const UInt32 kMethod_ZLIB_ATTR = 3;
static const UInt32 kMethod_ZLIB_RSRC = 4;
// static const UInt32 kMethod_DEDUP = 5; // de-dup within the generation store
// macos 10.10
static const UInt32 kMethod_LZVN_ATTR = 7;
static const UInt32 kMethod_LZVN_RSRC = 8;
static const UInt32 kMethod_COPY_ATTR = 9;
static const UInt32 kMethod_COPY_RSRC = 10;
// macos 10.11
// static const UInt32 kMethod_LZFSE_ATTR = 11;
static const UInt32 kMethod_LZFSE_RSRC = 12;

// static const UInt32 kMethod_ZBM_RSRC = 14;

static const char * const g_Methods[] =
{
    NULL
  , NULL
  , NULL
  , "ZLIB-attr"
  , "ZLIB-rsrc"
  , NULL
  , NULL
  , "LZVN-attr"
  , "LZVN-rsrc"
  , "COPY-attr"
  , "COPY-rsrc"
  , "LZFSE-attr"
  , "LZFSE-rsrc"
  , NULL
  , "ZBM-rsrc"
};


static const Byte k_COPY_Uncompressed_Marker = 0xcc;
static const Byte k_LZVN_Uncompressed_Marker = 6;

void CCompressHeader::Parse(const Byte *p, size_t dataSize)
{
  Clear();

  if (dataSize < k_decmpfs_HeaderSize)
    return;
  if (GetUi32(p) != 0x636D7066) // magic == "fpmc"
    return;
  Method = GetUi32(p + 4);
  UnpackSize = GetUi64(p + 8);
  dataSize -= k_decmpfs_HeaderSize;
  IsCorrect = true;
  
  if (   Method == kMethod_ZLIB_RSRC
      || Method == kMethod_COPY_RSRC
      || Method == kMethod_LZVN_RSRC
      || Method == kMethod_LZFSE_RSRC
      // || Method == kMethod_ZBM_RSRC // for debug
      )
  {
    IsResource = true;
    if (dataSize == 0)
      IsSupported = (
          Method != kMethod_LZFSE_RSRC &&
          Method != kMethod_COPY_RSRC);
    return;
  }
  
  if (   Method == kMethod_ZLIB_ATTR
      || Method == kMethod_COPY_ATTR
      || Method == kMethod_LZVN_ATTR
      // || Method == kMethod_LZFSE_ATTR
    )
  {
    if (dataSize == 0)
      return;
    const Byte b = p[k_decmpfs_HeaderSize];
    if (   (Method == kMethod_ZLIB_ATTR && (b & 0xf) == 0xf)
        || (Method == kMethod_COPY_ATTR && b == k_COPY_Uncompressed_Marker)
        || (Method == kMethod_LZVN_ATTR && b == k_LZVN_Uncompressed_Marker))
    {
      dataSize--;
      // if (UnpackSize > dataSize)
      if (UnpackSize != dataSize)
        return;
      DataPos = k_decmpfs_HeaderSize + 1;
      IsSupported = true;
    }
    else
    {
      if (Method != kMethod_COPY_ATTR)
        IsSupported = true;
      DataPos = k_decmpfs_HeaderSize;
    }
  }
}


void CCompressHeader::MethodToProp(NWindows::NCOM::CPropVariant &prop) const
{
  if (!IsCorrect)
    return;
  const UInt32 method = Method;
  const char *p = NULL;
  if (method < Z7_ARRAY_SIZE(g_Methods))
    p = g_Methods[method];
  AString s;
  if (p)
    s = p;
  else
    s.Add_UInt32(method);
  // if (!IsSupported) s += "-unsuported";
  prop = s;
}

void MethodsMaskToProp(UInt32 methodsMask, NWindows::NCOM::CPropVariant &prop)
{
  FLAGS_TO_PROP(g_Methods, methodsMask, prop);
}


struct CItem
{
  UString Name;
  
  UInt32 ParentID;

  UInt16 Type;
  UInt16 FileMode;
  // UInt16 Flags;
  // UInt32 Valence;
  UInt32 ID;
  UInt32 CTime;
  UInt32 MTime;
  UInt32 AttrMTime;
  UInt32 ATime;
  // UInt32 BackupDate;

  /*
  UInt32 OwnerID;
  UInt32 GroupID;
  Byte AdminFlags;
  Byte OwnerFlags;
  union
  {
    UInt32  iNodeNum;
    UInt32  LinkCount;
    UInt32  RawDevice;
  } special;

  UInt32 FileType;
  UInt32 FileCreator;
  UInt16 FinderFlags;
  UInt16 Point[2];
  */

  CFork DataFork;
  CFork ResourceFork;

  // for compressed attribute (decmpfs)
  int decmpfs_AttrIndex;
  CCompressHeader CompressHeader;

  CItem():
      decmpfs_AttrIndex(-1)
      {}
  bool IsDir() const { return Type == RECORD_TYPE_FOLDER; }
  // const CFork *GetFork(bool isResource) const { return (isResource ? &ResourceFork: &DataFork); }
};


struct CAttr
{
  UInt32 ID;
  bool Fork_defined;
  
  // UInt32 Size;    // for (Fork_defined == false) case
  // size_t DataPos; // for (Fork_defined == false) case
  CByteBuffer Data;

  CFork Fork;

  UString Name;

  UInt64 GetSize() const
  {
    if (Fork_defined)
      return Fork.Size;
    return Data.Size();
  }

  CAttr():
      Fork_defined(false)
      // Size(0),
      // DataPos(0),
      {}
};


static const int kAttrIndex_Item     = -1;
static const int kAttrIndex_Resource = -2;

struct CRef
{
  unsigned ItemIndex;
  int AttrIndex;
  int Parent;

  CRef(): AttrIndex(kAttrIndex_Item), Parent(-1) {}
  bool IsResource() const { return AttrIndex == kAttrIndex_Resource; }
  bool IsAltStream() const { return AttrIndex != kAttrIndex_Item; }
  bool IsItem() const { return AttrIndex == kAttrIndex_Item; }
};


class CDatabase
{
  HRESULT ReadFile(const CFork &fork, CByteBuffer &buf, IInStream *inStream);
  HRESULT LoadExtentFile(const CFork &fork, IInStream *inStream, CObjectVector<CIdExtents> *overflowExtentsArray);
  HRESULT LoadAttrs(const CFork &fork, IInStream *inStream, IArchiveOpenCallback *progress);
  HRESULT LoadCatalog(const CFork &fork, const CObjectVector<CIdExtents> *overflowExtentsArray, IInStream *inStream, IArchiveOpenCallback *progress);
  bool Parse_decmpgfs(unsigned attrIndex, CItem &item, bool &skip);
public:
  CRecordVector<CRef> Refs;
  CObjectVector<CItem> Items;
  CObjectVector<CAttr> Attrs;

  // CByteBuffer AttrBuf;

  CVolHeader Header;
  bool HeadersError;
  bool UnsupportedFeature;
  bool ThereAreAltStreams;
  // bool CaseSensetive;
  UString ResFileName;

  UInt64 SpecOffset;
  UInt64 PhySize;
  UInt64 PhySize2;
  UInt64 ArcFileSize;
  UInt32 MethodsMask;

  void Clear()
  {
    SpecOffset = 0;
    PhySize = 0;
    PhySize2 = 0;
    ArcFileSize = 0;
    MethodsMask = 0;
    HeadersError = false;
    UnsupportedFeature = false;
    ThereAreAltStreams = false;
    // CaseSensetive = false;

    Refs.Clear();
    Items.Clear();
    Attrs.Clear();
    // AttrBuf.Free();
  }

  UInt64 Get_UnpackSize_of_Ref(const CRef &ref) const
  {
    if (ref.AttrIndex >= 0)
      return Attrs[ref.AttrIndex].GetSize();
    const CItem &item = Items[ref.ItemIndex];
    if (ref.IsResource())
      return item.ResourceFork.Size;
    if (item.IsDir())
      return 0;
    else if (item.CompressHeader.IsCorrect)
      return item.CompressHeader.UnpackSize;
    return item.DataFork.Size;
  }

  void GetItemPath(unsigned index, NWindows::NCOM::CPropVariant &path) const;
  HRESULT Open2(IInStream *inStream, IArchiveOpenCallback *progress);
};

enum
{
  kHfsID_Root                  = 1,
  kHfsID_RootFolder            = 2,
  kHfsID_ExtentsFile           = 3,
  kHfsID_CatalogFile           = 4,
  kHfsID_BadBlockFile          = 5,
  kHfsID_AllocationFile        = 6,
  kHfsID_StartupFile           = 7,
  kHfsID_AttributesFile        = 8,
  kHfsID_RepairCatalogFile     = 14,
  kHfsID_BogusExtentFile       = 15,
  kHfsID_FirstUserCatalogNode  = 16
};

void CDatabase::GetItemPath(unsigned index, NWindows::NCOM::CPropVariant &path) const
{
  unsigned len = 0;
  const unsigned kNumLevelsMax = (1 << 10);
  unsigned cur = index;
  unsigned i;
  
  for (i = 0; i < kNumLevelsMax; i++)
  {
    const CRef &ref = Refs[cur];
    const UString *s;

    if (ref.IsResource())
      s = &ResFileName;
    else if (ref.AttrIndex >= 0)
      s = &Attrs[ref.AttrIndex].Name;
    else
      s = &Items[ref.ItemIndex].Name;

    len += s->Len();
    len++;
    cur = (unsigned)ref.Parent;
    if (ref.Parent < 0)
      break;
  }
  
  len--;
  wchar_t *p = path.AllocBstr(len);
  p[len] = 0;
  cur = index;
  
  for (;;)
  {
    const CRef &ref = Refs[cur];
    const UString *s;
    wchar_t delimChar = L':';

    if (ref.IsResource())
      s = &ResFileName;
    else if (ref.AttrIndex >= 0)
      s = &Attrs[ref.AttrIndex].Name;
    else
    {
      delimChar = WCHAR_PATH_SEPARATOR;
      s = &Items[ref.ItemIndex].Name;
    }

    unsigned curLen = s->Len();
    len -= curLen;
    
    const wchar_t *src = (const wchar_t *)*s;
    wchar_t *dest = p + len;
    for (unsigned j = 0; j < curLen; j++)
    {
      wchar_t c = src[j];
      // 18.06
      if (c == CHAR_PATH_SEPARATOR || c == '/')
        c = '_';
      dest[j] = c;
    }
    
    if (len == 0)
      break;
    p[--len] = delimChar;
    cur = (unsigned)ref.Parent;
  }
}

// Actually we read all blocks. It can be larger than fork.Size

HRESULT CDatabase::ReadFile(const CFork &fork, CByteBuffer &buf, IInStream *inStream)
{
  if (fork.NumBlocks >= Header.NumBlocks)
    return S_FALSE;
  if ((ArcFileSize >> Header.BlockSizeLog) + 1 < fork.NumBlocks)
    return S_FALSE;

  const size_t totalSize = (size_t)fork.NumBlocks << Header.BlockSizeLog;
  if ((totalSize >> Header.BlockSizeLog) != fork.NumBlocks)
    return S_FALSE;
  buf.Alloc(totalSize);
  UInt32 curBlock = 0;
  FOR_VECTOR (i, fork.Extents)
  {
    if (curBlock >= fork.NumBlocks)
      return S_FALSE;
    const CExtent &e = fork.Extents[i];
    if (e.Pos > Header.NumBlocks ||
        e.NumBlocks > fork.NumBlocks - curBlock ||
        e.NumBlocks > Header.NumBlocks - e.Pos)
      return S_FALSE;
    RINOK(InStream_SeekSet(inStream, SpecOffset + ((UInt64)e.Pos << Header.BlockSizeLog)))
    RINOK(ReadStream_FALSE(inStream,
        (Byte *)buf + ((size_t)curBlock << Header.BlockSizeLog),
        (size_t)e.NumBlocks << Header.BlockSizeLog))
    curBlock += e.NumBlocks;
  }
  return S_OK;
}

static const unsigned kNodeDescriptor_Size = 14;

struct CNodeDescriptor
{
  UInt32 fLink;
  // UInt32 bLink;
  Byte Kind;
  // Byte Height;
  unsigned NumRecords;

  bool Parse(const Byte *p, unsigned nodeSizeLog);
};


bool CNodeDescriptor::Parse(const Byte *p, unsigned nodeSizeLog)
{
  fLink = Get32(p);
  // bLink = Get32(p + 4);
  Kind = p[8];
  // Height = p[9];
  NumRecords = Get16(p + 10);

  const size_t nodeSize = (size_t)1 << nodeSizeLog;
  if (kNodeDescriptor_Size + ((UInt32)NumRecords + 1) * 2 > nodeSize)
    return false;
  const size_t limit = nodeSize - ((UInt32)NumRecords + 1) * 2;

  p += nodeSize - 2;

  for (unsigned i = 0; i < NumRecords; i++)
  {
    const UInt32 offs = Get16(p);
    p -= 2;
    const UInt32 offsNext = Get16(p);
    if (offs < kNodeDescriptor_Size
        || offs >= offsNext
        || offsNext > limit)
      return false;
  }
  return true;
}

struct CHeaderRec
{
  // UInt16 TreeDepth;
  // UInt32 RootNode;
  // UInt32 LeafRecords;
  UInt32 FirstLeafNode;
  // UInt32 LastLeafNode;
  unsigned NodeSizeLog;
  // UInt16 MaxKeyLength;
  UInt32 TotalNodes;
  // UInt32 FreeNodes;
  // UInt16 Reserved1;
  // UInt32 ClumpSize;
  // Byte BtreeType;
  // Byte KeyCompareType;
  // UInt32 Attributes;
  // UInt32 Reserved3[16];
  
  HRESULT Parse2(const CByteBuffer &buf);
};

HRESULT CHeaderRec::Parse2(const CByteBuffer &buf)
{
  if (buf.Size() < kNodeDescriptor_Size + 0x2A + 16 * 4)
    return S_FALSE;
  const Byte * p = (const Byte *)buf + kNodeDescriptor_Size;
  // TreeDepth = Get16(p);
  // RootNode = Get32(p + 2);
  // LeafRecords = Get32(p + 6);
  FirstLeafNode = Get32(p + 0xA);
  // LastLeafNode = Get32(p + 0xE);
  const UInt32 nodeSize = Get16(p + 0x12);

  unsigned i;
  for (i = 9; ((UInt32)1 << i) != nodeSize; i++)
    if (i == 16)
      return S_FALSE;
  NodeSizeLog = i;

  // MaxKeyLength = Get16(p + 0x14);
  TotalNodes = Get32(p + 0x16);
  // FreeNodes = Get32(p + 0x1A);
  // Reserved1 = Get16(p + 0x1E);
  // ClumpSize = Get32(p + 0x20);
  // BtreeType = p[0x24];
  // KeyCompareType = p[0x25];
  // Attributes = Get32(p + 0x26);
  /*
  for (int i = 0; i < 16; i++)
    Reserved3[i] = Get32(p + 0x2A + i * 4);
  */

  if ((buf.Size() >> NodeSizeLog) < TotalNodes)
    return S_FALSE;

  return S_OK;
}


static const Byte kNodeType_Leaf   = 0xFF;
// static const Byte kNodeType_Index  = 0;
// static const Byte kNodeType_Header = 1;
// static const Byte kNodeType_Mode   = 2;

static const Byte kExtentForkType_Data = 0;
static const Byte kExtentForkType_Resource = 0xFF;

/* It loads data extents from Extents Overflow File
   Most dmg installers are not fragmented. So there are no extents in Overflow File. */

HRESULT CDatabase::LoadExtentFile(const CFork &fork, IInStream *inStream, CObjectVector<CIdExtents> *overflowExtentsArray)
{
  if (fork.NumBlocks == 0)
    return S_OK;
  CByteBuffer buf;
  RINOK(ReadFile(fork, buf, inStream))
  const Byte *p = (const Byte *)buf;

  // CNodeDescriptor nodeDesc;
  // nodeDesc.Parse(p);
  CHeaderRec hr;
  RINOK(hr.Parse2(buf))

  UInt32 node = hr.FirstLeafNode;
  if (node == 0)
    return S_OK;
  if (hr.TotalNodes == 0)
    return S_FALSE;

  CByteArr usedBuf(hr.TotalNodes);
  memset(usedBuf, 0, hr.TotalNodes);

  while (node != 0)
  {
    if (node >= hr.TotalNodes || usedBuf[node] != 0)
      return S_FALSE;
    usedBuf[node] = 1;

    const size_t nodeOffset = (size_t)node << hr.NodeSizeLog;
    CNodeDescriptor desc;
    if (!desc.Parse(p + nodeOffset, hr.NodeSizeLog))
      return S_FALSE;
    if (desc.Kind != kNodeType_Leaf)
      return S_FALSE;
    
    UInt32 endBlock = 0;

    for (unsigned i = 0; i < desc.NumRecords; i++)
    {
      const UInt32 nodeSize = ((UInt32)1 << hr.NodeSizeLog);
      const Byte *r = p + nodeOffset + nodeSize - i * 2;
      const UInt32 offs = Get16(r - 2);
      UInt32 recSize = Get16(r - 4) - offs;
      const unsigned kKeyLen = 10;

      if (recSize != 2 + kKeyLen + kNumFixedExtents * 8)
        return S_FALSE;

      r = p + nodeOffset + offs;
      if (Get16(r) != kKeyLen)
        return S_FALSE;
      
      const Byte forkType = r[2];
      unsigned forkTypeIndex;
      if (forkType == kExtentForkType_Data)
        forkTypeIndex = 0;
      else if (forkType == kExtentForkType_Resource)
        forkTypeIndex = 1;
      else
        continue;
      CObjectVector<CIdExtents> &overflowExtents = overflowExtentsArray[forkTypeIndex];

      const UInt32 id = Get32(r + 4);
      const UInt32 startBlock = Get32(r + 8);
      r += 2 + kKeyLen;
      
      bool needNew = true;
      
      if (overflowExtents.Size() != 0)
      {
        CIdExtents &e = overflowExtents.Back();
        if (e.ID == id)
        {
          if (endBlock != startBlock)
            return S_FALSE;
          needNew = false;
        }
      }
      
      if (needNew)
      {
        CIdExtents &e = overflowExtents.AddNew();
        e.ID = id;
        e.StartBlock = startBlock;
        endBlock = startBlock;
      }
      
      CIdExtents &e = overflowExtents.Back();
      
      for (unsigned k = 0; k < kNumFixedExtents; k++, r += 8)
      {
        CExtent ee;
        ee.Pos = Get32(r);
        ee.NumBlocks = Get32(r + 4);
        if (ee.NumBlocks != 0)
        {
          e.Extents.Add(ee);
          endBlock += ee.NumBlocks;
        }
      }
    }

    node = desc.fLink;
  }
  return S_OK;
}

static void LoadName(const Byte *data, unsigned len, UString &dest)
{
  wchar_t *p = dest.GetBuf(len);
  unsigned i;
  for (i = 0; i < len; i++)
  {
    const wchar_t c = Get16(data + i * 2);
    if (c == 0)
      break;
    p[i] = c;
  }
  p[i] = 0;
  dest.ReleaseBuf_SetLen(i);
}

static bool IsNameEqualTo(const Byte *data, const char *name)
{
  for (unsigned i = 0;; i++)
  {
    const char c = name[i];
    if (c == 0)
      return true;
    if (Get16(data + i * 2) != (Byte)c)
      return false;
  }
}

static const UInt32 kAttrRecordType_Inline = 0x10;
static const UInt32 kAttrRecordType_Fork = 0x20;
// static const UInt32 kAttrRecordType_Extents = 0x30;

HRESULT CDatabase::LoadAttrs(const CFork &fork, IInStream *inStream, IArchiveOpenCallback *progress)
{
  if (fork.NumBlocks == 0)
    return S_OK;

  CByteBuffer AttrBuf;
  RINOK(ReadFile(fork, AttrBuf, inStream))
  const Byte *p = (const Byte *)AttrBuf;

  // CNodeDescriptor nodeDesc;
  // nodeDesc.Parse(p);
  CHeaderRec hr;
  RINOK(hr.Parse2(AttrBuf))
  
  // CaseSensetive = (Header.IsHfsX() && hr.KeyCompareType == 0xBC);

  UInt32 node = hr.FirstLeafNode;
  if (node == 0)
    return S_OK;
  if (hr.TotalNodes == 0)
    return S_FALSE;

  CByteArr usedBuf(hr.TotalNodes);
  memset(usedBuf, 0, hr.TotalNodes);

  CFork resFork;

  while (node != 0)
  {
    if (node >= hr.TotalNodes || usedBuf[node] != 0)
      return S_FALSE;
    usedBuf[node] = 1;
    
    const size_t nodeOffset = (size_t)node << hr.NodeSizeLog;
    CNodeDescriptor desc;
    if (!desc.Parse(p + nodeOffset, hr.NodeSizeLog))
      return S_FALSE;
    if (desc.Kind != kNodeType_Leaf)
      return S_FALSE;
    
    for (unsigned i = 0; i < desc.NumRecords; i++)
    {
      const UInt32 nodeSize = ((UInt32)1 << hr.NodeSizeLog);
      const Byte *r = p + nodeOffset + nodeSize - i * 2;
      const UInt32 offs = Get16(r - 2);
      UInt32 recSize = Get16(r - 4) - offs;
      const unsigned kHeadSize = 14;
      if (recSize < kHeadSize)
        return S_FALSE;

      r = p + nodeOffset + offs;
      const UInt32 keyLen = Get16(r);

      // UInt16 pad = Get16(r + 2);
      const UInt32 fileID = Get32(r + 4);
      const unsigned startBlock = Get32(r + 8);
      if (startBlock != 0)
      {
        // that case is still unsupported
        UnsupportedFeature = true;
        continue;
      }
      const unsigned nameLen = Get16(r + 12);

      if (keyLen + 2 > recSize ||
          keyLen != kHeadSize - 2 + nameLen * 2)
        return S_FALSE;
      r += kHeadSize;
      recSize -= kHeadSize;

      const Byte *name = r;
      r += nameLen * 2;
      recSize -= nameLen * 2;

      if (recSize < 4)
        return S_FALSE;

      const UInt32 recordType = Get32(r);

      if (progress && (Attrs.Size() & 0xFFF) == 0)
      {
        const UInt64 numFiles = 0;
        RINOK(progress->SetCompleted(&numFiles, NULL))
      }

      if (Attrs.Size() >= ((UInt32)1 << 31))
        return S_FALSE;

      CAttr &attr = Attrs.AddNew();
      attr.ID = fileID;
      LoadName(name, nameLen, attr.Name);

      if (recordType == kAttrRecordType_Fork)
      {
        // 22.00 : some hfs files contain it;
        /* spec: If the attribute has more than 8 extents, there will be additional
            records (of type kAttrRecordType_Extents) for this attribute. */
        if (recSize != 8 + kForkRecSize)
          return S_FALSE;
        if (Get32(r + 4) != 0) // reserved
          return S_FALSE;
        attr.Fork.Parse(r + 8);
        attr.Fork_defined = true;
        continue;
      }
      else if (recordType != kAttrRecordType_Inline)
      {
        UnsupportedFeature = true;
        continue;
      }

      const unsigned kRecordHeaderSize = 16;
      if (recSize < kRecordHeaderSize)
        return S_FALSE;
      if (Get32(r + 4) != 0 || Get32(r + 8) != 0) // reserved
        return S_FALSE;
      const UInt32 dataSize = Get32(r + 12);
      
      r += kRecordHeaderSize;
      recSize -= kRecordHeaderSize;
      
      if (recSize < dataSize)
        return S_FALSE;

      attr.Data.CopyFrom(r, dataSize);
      // attr.DataPos = nodeOffset + offs + 2 + keyLen + kRecordHeaderSize;
      // attr.Size = dataSize;
    }

    node = desc.fLink;
  }
  return S_OK;
}


bool CDatabase::Parse_decmpgfs(unsigned attrIndex, CItem &item, bool &skip)
{
  const CAttr &attr = Attrs[attrIndex];
  skip = false;
  if (item.CompressHeader.IsCorrect || !item.DataFork.IsEmpty())
    return false;

  item.CompressHeader.Parse(attr.Data, attr.Data.Size());

  if (item.CompressHeader.IsCorrect)
  {
    item.decmpfs_AttrIndex = (int)attrIndex;
    skip = true;
    if (item.CompressHeader.Method < sizeof(MethodsMask) * 8)
      MethodsMask |= ((UInt32)1 << item.CompressHeader.Method);
  }

  return true;
}


HRESULT CDatabase::LoadCatalog(const CFork &fork, const CObjectVector<CIdExtents> *overflowExtentsArray, IInStream *inStream, IArchiveOpenCallback *progress)
{
  CByteBuffer buf;
  RINOK(ReadFile(fork, buf, inStream))
  const Byte *p = (const Byte *)buf;

  // CNodeDescriptor nodeDesc;
  // nodeDesc.Parse(p);
  CHeaderRec hr;
  RINOK(hr.Parse2(buf))

  CRecordVector<CIdIndexPair> IdToIndexMap;

  const unsigned reserveSize = (unsigned)(Header.NumFolders + 1 + Header.NumFiles);

  const unsigned kBasicRecSize = 0x58;
  const unsigned kMinRecSize = kBasicRecSize + 10;

  if ((UInt64)reserveSize * kMinRecSize < buf.Size())
  {
    Items.ClearAndReserve(reserveSize);
    Refs.ClearAndReserve(reserveSize);
    IdToIndexMap.ClearAndReserve(reserveSize);
  }
 
  // CaseSensetive = (Header.IsHfsX() && hr.KeyCompareType == 0xBC);

  CByteArr usedBuf(hr.TotalNodes);
  if (hr.TotalNodes != 0)
    memset(usedBuf, 0, hr.TotalNodes);

  CFork resFork;

  UInt32 node = hr.FirstLeafNode;
  UInt32 numFiles = 0;
  UInt32 numFolders = 0;
  
  while (node != 0)
  {
    if (node >= hr.TotalNodes || usedBuf[node] != 0)
      return S_FALSE;
    usedBuf[node] = 1;
    
    const size_t nodeOffset = (size_t)node << hr.NodeSizeLog;
    CNodeDescriptor desc;
    if (!desc.Parse(p + nodeOffset, hr.NodeSizeLog))
      return S_FALSE;
    if (desc.Kind != kNodeType_Leaf)
      return S_FALSE;
    
    for (unsigned i = 0; i < desc.NumRecords; i++)
    {
      const UInt32 nodeSize = (1 << hr.NodeSizeLog);
      const Byte *r = p + nodeOffset + nodeSize - i * 2;
      const UInt32 offs = Get16(r - 2);
      UInt32 recSize = Get16(r - 4) - offs;
      if (recSize < 6)
        return S_FALSE;

      r = p + nodeOffset + offs;
      UInt32 keyLen = Get16(r);
      UInt32 parentID = Get32(r + 2);
      if (keyLen < 6 || (keyLen & 1) != 0 || keyLen + 2 > recSize)
        return S_FALSE;
      r += 6;
      recSize -= 6;
      keyLen -= 6;
      
      unsigned nameLen = Get16(r);
      if (nameLen * 2 != (unsigned)keyLen)
        return S_FALSE;
      r += 2;
      recSize -= 2;
     
      r += nameLen * 2;
      recSize -= nameLen * 2;

      if (recSize < 2)
        return S_FALSE;
      UInt16 type = Get16(r);

      if (type != RECORD_TYPE_FOLDER &&
          type != RECORD_TYPE_FILE)
        continue;

      if (recSize < kBasicRecSize)
        return S_FALSE;

      CItem &item = Items.AddNew();
      item.ParentID = parentID;
      item.Type = type;
      // item.Flags = Get16(r + 2);
      // item.Valence = Get32(r + 4);
      item.ID = Get32(r + 8);
      {
        const Byte *name = r - (nameLen * 2);
        LoadName(name, nameLen, item.Name);
        if (item.Name.Len() <= 1)
        {
          if (item.Name.IsEmpty() && nameLen == 21)
          {
            if (GetUi32(name) == 0 &&
                GetUi32(name + 4) == 0 &&
                IsNameEqualTo(name + 8, "HFS+ Private Data"))
            {
              // it's folder for "Hard Links" files
              item.Name = "[HFS+ Private Data]";
            }
          }

          // Some dmg files have ' ' folder item.
          if (item.Name.IsEmpty() || item.Name[0] == L' ')
            item.Name = "[]";
        }
      }

      item.CTime = Get32(r + 0xC);
      item.MTime = Get32(r + 0x10);
      item.AttrMTime = Get32(r + 0x14);
      item.ATime = Get32(r + 0x18);
      // item.BackupDate = Get32(r + 0x1C);
      
      /*
      item.OwnerID = Get32(r + 0x20);
      item.GroupID = Get32(r + 0x24);
      item.AdminFlags = r[0x28];
      item.OwnerFlags = r[0x29];
      */
      item.FileMode = Get16(r + 0x2A);
      /*
      item.special.iNodeNum = Get16(r + 0x2C); // or .linkCount
      item.FileType = Get32(r + 0x30);
      item.FileCreator = Get32(r + 0x34);
      item.FinderFlags = Get16(r + 0x38);
      item.Point[0] = Get16(r + 0x3A); // v
      item.Point[1] = Get16(r + 0x3C); // h
      */

      // const refIndex = Refs.Size();
      CIdIndexPair pair;
      pair.ID = item.ID;
      pair.Index = Items.Size() - 1;
      IdToIndexMap.Add(pair);

      recSize -= kBasicRecSize;
      r += kBasicRecSize;
      if (item.IsDir())
      {
        numFolders++;
        if (recSize != 0)
          return S_FALSE;
      }
      else
      {
        numFiles++;
        if (recSize != kForkRecSize * 2)
          return S_FALSE;
        
        item.DataFork.Parse(r);

        if (!item.DataFork.UpgradeAndTest(overflowExtentsArray[0], item.ID, Header.BlockSizeLog))
          HeadersError = true;
  
        item.ResourceFork.Parse(r + kForkRecSize);
        if (!item.ResourceFork.IsEmpty())
        {
          if (!item.ResourceFork.UpgradeAndTest(overflowExtentsArray[1], item.ID, Header.BlockSizeLog))
            HeadersError = true;
          // ThereAreAltStreams = true;
        }
      }
      if (progress && (Items.Size() & 0xFFF) == 0)
      {
        const UInt64 numItems = Items.Size();
        RINOK(progress->SetCompleted(&numItems, NULL))
      }
    }
    node = desc.fLink;
  }

  if (Header.NumFiles != numFiles ||
      Header.NumFolders + 1 != numFolders)
    HeadersError = true;

  IdToIndexMap.Sort2();
  {
    for (unsigned i = 1; i < IdToIndexMap.Size(); i++)
      if (IdToIndexMap[i - 1].ID == IdToIndexMap[i].ID)
        return S_FALSE;
  }

 
  CBoolArr skipAttr(Attrs.Size());
  {
    for (unsigned i = 0; i < Attrs.Size(); i++)
      skipAttr[i] = false;
  }

  {
    FOR_VECTOR (i, Attrs)
    {
      const CAttr &attr = Attrs[i];

      const int itemIndex = FindItemIndex(IdToIndexMap, attr.ID);
      if (itemIndex < 0)
      {
        HeadersError = true;
        continue;
      }
  
      if (attr.Name.IsEqualTo("com.apple.decmpfs"))
      {
        if (!Parse_decmpgfs(i, Items[itemIndex], skipAttr[i]))
          HeadersError = true;
      }
    }
  }

  IdToIndexMap.ClearAndReserve(Items.Size());

  {
    FOR_VECTOR (i, Items)
    {
      const CItem &item = Items[i];

      CIdIndexPair pair;
      pair.ID = item.ID;
      pair.Index = Refs.Size();
      IdToIndexMap.Add(pair);

      CRef ref;
      ref.ItemIndex = i;
      Refs.Add(ref);
      
      #ifdef HFS_SHOW_ALT_STREAMS
     
      if (item.ResourceFork.IsEmpty())
        continue;
      if (item.CompressHeader.IsSupported && item.CompressHeader.IsMethod_Resource())
        continue;

      ThereAreAltStreams = true;
      ref.AttrIndex = kAttrIndex_Resource;
      ref.Parent = (int)(Refs.Size() - 1);
      Refs.Add(ref);
      
      #endif
    }
  }

  IdToIndexMap.Sort2();

  {
    FOR_VECTOR (i, Refs)
    {
      CRef &ref = Refs[i];
      if (ref.IsResource())
        continue;
      const CItem &item = Items[ref.ItemIndex];
      ref.Parent = FindItemIndex(IdToIndexMap, item.ParentID);
      if (ref.Parent >= 0)
      {
        if (!Items[Refs[ref.Parent].ItemIndex].IsDir())
        {
          ref.Parent = -1;
          HeadersError = true;
        }
      }
    }
  }

  #ifdef HFS_SHOW_ALT_STREAMS
  {
    FOR_VECTOR (i, Attrs)
    {
      if (skipAttr[i])
        continue;
      const CAttr &attr = Attrs[i];

      const int refIndex = FindItemIndex(IdToIndexMap, attr.ID);
      if (refIndex < 0)
      {
        HeadersError = true;
        continue;
      }

      ThereAreAltStreams = true;

      CRef ref;
      ref.AttrIndex = (int)i;
      ref.Parent = refIndex;
      ref.ItemIndex = Refs[refIndex].ItemIndex;
      Refs.Add(ref);
    }
  }
  #endif

  return S_OK;
}

static const unsigned kHeaderPadSize = (1 << 10);
static const unsigned kMainHeaderSize = 512;
static const unsigned kHfsHeaderSize = kHeaderPadSize + kMainHeaderSize;

API_FUNC_static_IsArc IsArc_HFS(const Byte *p, size_t size)
{
  if (size < kHfsHeaderSize)
    return k_IsArc_Res_NEED_MORE;
  p += kHeaderPadSize;
  if (p[0] == 'B' && p[1] == 'D')
  {
    if (p[0x7C] != 'H' || p[0x7C + 1] != '+')
      return k_IsArc_Res_NO;
  }
  else
  {
    if (p[0] != 'H' || (p[1] != '+' && p[1] != 'X'))
      return k_IsArc_Res_NO;
    UInt32 version = Get16(p + 2);
    if (version < 4 || version > 5)
      return k_IsArc_Res_NO;
  }
  return k_IsArc_Res_YES;
}
}

HRESULT CDatabase::Open2(IInStream *inStream, IArchiveOpenCallback *progress)
{
  Clear();
  Byte buf[kHfsHeaderSize];
  RINOK(ReadStream_FALSE(inStream, buf, kHfsHeaderSize))
  {
    for (unsigned i = 0; i < kHeaderPadSize; i++)
      if (buf[i] != 0)
        return S_FALSE;
  }
  const Byte *p = buf + kHeaderPadSize;
  CVolHeader &h = Header;

  h.Header[0] = p[0];
  h.Header[1] = p[1];

  if (p[0] == 'B' && p[1] == 'D')
  {
    /*
    It's header for old HFS format.
    We don't support old HFS format, but we support
    special HFS volume that contains embedded HFS+ volume
    */

    if (p[0x7C] != 'H' || p[0x7C + 1] != '+')
      return S_FALSE;

    /*
    h.CTime = Get32(p + 0x2);
    h.MTime = Get32(p + 0x6);
    
    h.NumFiles = Get32(p + 0x54);
    h.NumFolders = Get32(p + 0x58);
    
    if (h.NumFolders > ((UInt32)1 << 29) ||
        h.NumFiles > ((UInt32)1 << 30))
      return S_FALSE;
    if (progress)
    {
      UInt64 numFiles = (UInt64)h.NumFiles + h.NumFolders + 1;
      RINOK(progress->SetTotal(&numFiles, NULL))
    }
    h.NumFreeBlocks = Get16(p + 0x22);
    */
    
    UInt32 blockSize = Get32(p + 0x14);
    
    {
      unsigned i;
      for (i = 9; ((UInt32)1 << i) != blockSize; i++)
        if (i == 31)
          return S_FALSE;
      h.BlockSizeLog = i;
    }
    
    h.NumBlocks = Get16(p + 0x12);
    /*
    we suppose that it has the follwing layout
    {
      start block with header
      [h.NumBlocks]
      end block with header
    }
    */
    PhySize2 = ((UInt64)h.NumBlocks + 2) << h.BlockSizeLog;

    UInt32 startBlock = Get16(p + 0x7C + 2);
    UInt32 blockCount = Get16(p + 0x7C + 4);
    SpecOffset = (UInt64)(1 + startBlock) << h.BlockSizeLog;
    UInt64 phy = SpecOffset + ((UInt64)blockCount << h.BlockSizeLog);
    if (PhySize2 < phy)
      PhySize2 = phy;
    RINOK(InStream_SeekSet(inStream, SpecOffset))
    RINOK(ReadStream_FALSE(inStream, buf, kHfsHeaderSize))
  }

  if (p[0] != 'H' || (p[1] != '+' && p[1] != 'X'))
    return S_FALSE;
  h.Version = Get16(p + 2);
  if (h.Version < 4 || h.Version > 5)
    return S_FALSE;

  // h.Attr = Get32(p + 4);
  // h.LastMountedVersion = Get32(p + 8);
  // h.JournalInfoBlock = Get32(p + 0xC);

  h.CTime = Get32(p + 0x10);
  h.MTime = Get32(p + 0x14);
  // h.BackupTime = Get32(p + 0x18);
  // h.CheckedTime = Get32(p + 0x1C);

  h.NumFiles = Get32(p + 0x20);
  h.NumFolders = Get32(p + 0x24);
  
  if (h.NumFolders > ((UInt32)1 << 29) ||
      h.NumFiles > ((UInt32)1 << 30))
    return S_FALSE;

  RINOK(InStream_GetSize_SeekToEnd(inStream, ArcFileSize))

  if (progress)
  {
    const UInt64 numFiles = (UInt64)h.NumFiles + h.NumFolders + 1;
    RINOK(progress->SetTotal(&numFiles, NULL))
  }

  UInt32 blockSize = Get32(p + 0x28);

  {
    unsigned i;
    for (i = 9; ((UInt32)1 << i) != blockSize; i++)
      if (i == 31)
        return S_FALSE;
    h.BlockSizeLog = i;
  }

  h.NumBlocks = Get32(p + 0x2C);
  h.NumFreeBlocks = Get32(p + 0x30);

  /*
  h.NextCalatlogNodeID = Get32(p + 0x40);
  h.WriteCount = Get32(p + 0x44);
  for (i = 0; i < 6; i++)
    h.FinderInfo[i] = Get32(p + 0x50 + i * 4);
  h.VolID = Get64(p + 0x68);
  */

  ResFileName = kResFileName;

  CFork extentsFork, catalogFork, attrFork;
  // allocationFork.Parse(p + 0x70 + 0x50 * 0);
  extentsFork.Parse(p + 0x70 + 0x50 * 1);
  catalogFork.Parse(p + 0x70 + 0x50 * 2);
  attrFork.Parse   (p + 0x70 + 0x50 * 3);
  // startupFork.Parse(p + 0x70 + 0x50 * 4);

  CObjectVector<CIdExtents> overflowExtents[2];
  if (!extentsFork.IsOk(Header.BlockSizeLog))
    HeadersError = true;
  else
  {
    HRESULT res = LoadExtentFile(extentsFork, inStream, overflowExtents);
    if (res == S_FALSE)
      HeadersError = true;
    else if (res != S_OK)
      return res;
  }

  if (!catalogFork.UpgradeAndTest(overflowExtents[0], kHfsID_CatalogFile, Header.BlockSizeLog))
    return S_FALSE;
  
  if (!attrFork.UpgradeAndTest(overflowExtents[0], kHfsID_AttributesFile, Header.BlockSizeLog))
    HeadersError = true;
  else
  {
    if (attrFork.Size != 0)
      RINOK(LoadAttrs(attrFork, inStream, progress))
  }
  
  RINOK(LoadCatalog(catalogFork, overflowExtents, inStream, progress))

  PhySize = Header.GetPhySize();
  return S_OK;
}



Z7_class_CHandler_final:
  public IInArchive,
  public IArchiveGetRawProps,
  public IInArchiveGetStream,
  public CMyUnknownImp,
  public CDatabase
{
  Z7_IFACES_IMP_UNK_3(
      IInArchive,
      IArchiveGetRawProps,
      IInArchiveGetStream)

  CMyComPtr<IInStream> _stream;
  HRESULT GetForkStream(const CFork &fork, ISequentialInStream **stream);
};

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidCTime,
  kpidMTime,
  kpidATime,
  kpidChangeTime,
  kpidPosixAttrib,
  /*
  kpidUserId,
  kpidGroupId,
  */
#ifdef HFS_SHOW_ALT_STREAMS
  kpidIsAltStream,
#endif
  kpidMethod
};

static const Byte kArcProps[] =
{
  kpidMethod,
  kpidCharacts,
  kpidClusterSize,
  kpidFreeSpace,
  kpidCTime,
  kpidMTime
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static void HfsTimeToProp(UInt32 hfsTime, NWindows::NCOM::CPropVariant &prop)
{
  if (hfsTime == 0)
    return;
  FILETIME ft;
  HfsTimeToFileTime(hfsTime, ft);
  prop.SetAsTimeFrom_FT_Prec(ft, k_PropVar_TimePrec_Base);
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidExtension: prop = Header.IsHfsX() ? "hfsx" : "hfs"; break;
    case kpidMethod: prop = Header.IsHfsX() ? "HFSX" : "HFS+"; break;
    case kpidCharacts: MethodsMaskToProp(MethodsMask, prop); break;
    case kpidPhySize:
    {
      UInt64 v = SpecOffset + PhySize;
      if (v < PhySize2)
        v = PhySize2;
      prop = v;
      break;
    }
    case kpidClusterSize: prop = (UInt32)1 << Header.BlockSizeLog; break;
    case kpidFreeSpace: prop = (UInt64)Header.GetFreeSize(); break;
    case kpidMTime: HfsTimeToProp(Header.MTime, prop); break;
    case kpidCTime:
    {
      if (Header.CTime != 0)
      {
        FILETIME localFt, ft;
        HfsTimeToFileTime(Header.CTime, localFt);
        if (LocalFileTimeToFileTime(&localFt, &ft))
          prop.SetAsTimeFrom_FT_Prec(ft, k_PropVar_TimePrec_Base);
      }
      break;
    }
    case kpidIsTree: prop = true; break;
    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (HeadersError) flags |= kpv_ErrorFlags_HeadersError;
      if (UnsupportedFeature) flags |= kpv_ErrorFlags_UnsupportedFeature;
      if (flags != 0)
        prop = flags;
      break;
    }
    case kpidIsAltStream: prop = ThereAreAltStreams; break;
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

Z7_COM7F_IMF(CHandler::GetRawPropInfo(UInt32 /* index */, BSTR *name, PROPID *propID))
{
  *name = NULL;
  *propID = 0;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType))
{
  const CRef &ref = Refs[index];
  *parentType = ref.IsAltStream() ?
      NParentType::kAltStream :
      NParentType::kDir;
  *parent = (UInt32)(Int32)ref.Parent;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType))
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;
  #ifdef MY_CPU_LE
  if (propID == kpidName)
  {
    const CRef &ref = Refs[index];
    const UString *s;
    if (ref.IsResource())
      s = &ResFileName;
    else if (ref.AttrIndex >= 0)
      s = &Attrs[ref.AttrIndex].Name;
    else
      s = &Items[ref.ItemIndex].Name;
    *data = (const wchar_t *)(*s);
    *dataSize = (s->Len() + 1) * (UInt32)sizeof(wchar_t);
    *propType = PROP_DATA_TYPE_wchar_t_PTR_Z_LE;
    return S_OK;
  }
  #else
  UNUSED_VAR(index);
  UNUSED_VAR(propID);
  #endif
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CRef &ref = Refs[index];
  const CItem &item = Items[ref.ItemIndex];
  switch (propID)
  {
    case kpidPath: GetItemPath(index, prop); break;
    case kpidName:
    {
      const UString *s;
      if (ref.IsResource())
        s = &ResFileName;
      else if (ref.AttrIndex >= 0)
        s = &Attrs[ref.AttrIndex].Name;
      else
        s = &item.Name;
      prop = *s;
      break;
    }
    case kpidPackSize:
      {
        UInt64 size;
        if (ref.AttrIndex >= 0)
          size = Attrs[ref.AttrIndex].GetSize();
        else if (ref.IsResource())
          size = (UInt64)item.ResourceFork.NumBlocks << Header.BlockSizeLog;
        else if (item.IsDir())
          break;
        else if (item.CompressHeader.IsCorrect)
        {
          if (item.CompressHeader.IsMethod_Resource())
            size = (UInt64)item.ResourceFork.NumBlocks << Header.BlockSizeLog;
          else if (item.decmpfs_AttrIndex >= 0)
          {
            // size = item.PackSize;
            const CAttr &attr = Attrs[item.decmpfs_AttrIndex];
            size = attr.Data.Size() - item.CompressHeader.DataPos;
          }
          else
            size = 0;
        }
        else
          size = (UInt64)item.DataFork.NumBlocks << Header.BlockSizeLog;
        prop = size;
        break;
      }
    case kpidSize:
      {
        UInt64 size;
        if (ref.AttrIndex >= 0)
          size = Attrs[ref.AttrIndex].GetSize();
        else if (ref.IsResource())
          size = item.ResourceFork.Size;
        else if (item.IsDir())
          break;
        else if (item.CompressHeader.IsCorrect)
          size = item.CompressHeader.UnpackSize;
        else
          size = item.DataFork.Size;
        prop = size;
        break;
      }
    case kpidIsDir: prop = (ref.IsItem() && item.IsDir()); break;
    case kpidIsAltStream: prop = ref.IsAltStream(); break;
    case kpidCTime: HfsTimeToProp(item.CTime, prop); break;
    case kpidMTime: HfsTimeToProp(item.MTime, prop); break;
    case kpidATime: HfsTimeToProp(item.ATime, prop); break;
    case kpidChangeTime: HfsTimeToProp(item.AttrMTime, prop); break;
    case kpidPosixAttrib: if (ref.IsItem()) prop = (UInt32)item.FileMode; break;
    /*
    case kpidUserId: prop = (UInt32)item.OwnerID; break;
    case kpidGroupId: prop = (UInt32)item.GroupID; break;
    */

    case kpidMethod:
      if (ref.IsItem())
        item.CompressHeader.MethodToProp(prop);
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(inStream, callback))
  _stream = inStream;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _stream.Release();
  Clear();
  return S_OK;
}

static const UInt32 kCompressionBlockSize = 1 << 16;

CDecoder::CDecoder()
{
  _zlibDecoderSpec = new NCompress::NZlib::CDecoder();
  _zlibDecoder = _zlibDecoderSpec;
  
  _lzfseDecoderSpec = new NCompress::NLzfse::CDecoder();
  _lzfseDecoder = _lzfseDecoderSpec;
  _lzfseDecoderSpec->LzvnMode = true;
}

HRESULT CDecoder::ExtractResourceFork_ZLIB(
    ISequentialInStream *inStream, ISequentialOutStream *outStream,
    UInt64 forkSize, UInt64 unpackSize,
    UInt64 progressStart, IArchiveExtractCallback *extractCallback)
{
  const unsigned kHeaderSize = 0x100 + 8;

  const size_t kBufSize = kCompressionBlockSize;
  _buf.Alloc(kBufSize + 0x10); // we need 1 additional bytes for uncompressed chunk header

  RINOK(ReadStream_FALSE(inStream, _buf, kHeaderSize))
  Byte *buf = _buf;
  const UInt32 dataPos = Get32(buf);
  const UInt32 mapPos = Get32(buf + 4);
  const UInt32 dataSize = Get32(buf + 8);
  const UInt32 mapSize = Get32(buf + 12);

  const UInt32 kResMapSize = 50;

  if (mapSize != kResMapSize
      || dataPos > mapPos
      || dataSize != mapPos - dataPos
      || mapSize > forkSize
      || mapPos != forkSize - mapSize)
    return S_FALSE;
  
  const UInt32 dataSize2 = Get32(buf + 0x100);
  if (4 + dataSize2 != dataSize
      || dataSize2 < 8
      || dataSize2 > dataSize)
    return S_FALSE;
  
  const UInt32 numBlocks = GetUi32(buf + 0x100 + 4);
  if (((dataSize2 - 4) >> 3) < numBlocks)
    return S_FALSE;
  {
    const UInt64 up = unpackSize + kCompressionBlockSize - 1;
    if (up < unpackSize || up / kCompressionBlockSize != numBlocks)
      return S_FALSE;
  }

  const UInt32 tableSize = (numBlocks << 3);

  _tableBuf.AllocAtLeast(tableSize);

  RINOK(ReadStream_FALSE(inStream, _tableBuf, tableSize))
  const Byte *tableBuf = _tableBuf;
  
  UInt32 prev = 4 + tableSize;

  UInt32 i;
  for (i = 0; i < numBlocks; i++)
  {
    const UInt32 offs = GetUi32(tableBuf + i * 8);
    const UInt32 size = GetUi32(tableBuf + i * 8 + 4);
    if (size == 0
        || prev != offs
        || offs > dataSize2
        || size > dataSize2 - offs)
      return S_FALSE;
    prev = offs + size;
  }

  if (prev != dataSize2)
    return S_FALSE;

  CBufInStream *bufInStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> bufInStream = bufInStreamSpec;

  // bool padError = false;
  UInt64 outPos = 0;

  for (i = 0; i < numBlocks; i++)
  {
    const UInt64 rem = unpackSize - outPos;
    if (rem == 0)
      return S_FALSE;
    UInt32 blockSize = kCompressionBlockSize;
    if (rem < kCompressionBlockSize)
      blockSize = (UInt32)rem;

    const UInt32 size = GetUi32(tableBuf + i * 8 + 4);

    if (size > kCompressionBlockSize + 1)
      return S_FALSE;

    RINOK(ReadStream_FALSE(inStream, buf, size))

    if ((buf[0] & 0xF) == 0xF)
    {
      // (buf[0] = 0xff) is marker of uncompressed block in APFS
      // that code was not tested in HFS
      if (size - 1 != blockSize)
        return S_FALSE;

      if (outStream)
      {
        RINOK(WriteStream(outStream, buf + 1, blockSize))
      }
    }
    else
    {
      const UInt64 blockSize64 = blockSize;
      bufInStreamSpec->Init(buf, size);
      RINOK(_zlibDecoder->Code(bufInStream, outStream, NULL, &blockSize64, NULL))
      if (_zlibDecoderSpec->GetOutputProcessedSize() != blockSize)
        return S_FALSE;
      const UInt64 inSize = _zlibDecoderSpec->GetInputProcessedSize();
      if (inSize != size)
      {
        if (inSize > size)
          return S_FALSE;
        // apfs file can contain junk (non-zeros) after data block.
        /*
        if (!padError)
        {
          const Byte *p = buf + (UInt32)inSize;
          const Byte *e = p + (size - (UInt32)inSize);
          do
          {
            if (*p != 0)
            {
              padError = true;
              break;
            }
          }
          while (++p != e);
        }
        */
      }
    }
    
    outPos += blockSize;
    if ((i & 0xFF) == 0)
    {
      const UInt64 progressPos = progressStart + outPos;
      RINOK(extractCallback->SetCompleted(&progressPos))
    }
  }

  if (outPos != unpackSize)
    return S_FALSE;

  // if (padError) return S_FALSE;

  /* We check Resource Map
     Are there HFS files with another values in Resource Map ??? */

  RINOK(ReadStream_FALSE(inStream, buf, mapSize))
  const UInt32 types = Get16(buf + 24);
  const UInt32 names = Get16(buf + 26);
  const UInt32 numTypes = Get16(buf + 28);
  if (numTypes != 0 || types != 28 || names != kResMapSize)
    return S_FALSE;
  const UInt32 resType = Get32(buf + 30);
  const UInt32 numResources = Get16(buf + 34);
  const UInt32 resListOffset = Get16(buf + 36);
  if (resType != 0x636D7066) // cmpf
    return S_FALSE;
  if (numResources != 0 || resListOffset != 10)
    return S_FALSE;

  const UInt32 entryId = Get16(buf + 38);
  const UInt32 nameOffset = Get16(buf + 40);
  // Byte attrib = buf[42];
  const UInt32 resourceOffset = Get32(buf + 42) & 0xFFFFFF;
  if (entryId != 1 || nameOffset != 0xFFFF || resourceOffset != 0)
    return S_FALSE;

  return S_OK;
}



HRESULT CDecoder::ExtractResourceFork_LZFSE(
    ISequentialInStream *inStream, ISequentialOutStream *outStream,
    UInt64 forkSize, UInt64 unpackSize,
    UInt64 progressStart, IArchiveExtractCallback *extractCallback)
{
  const UInt32 kNumBlocksMax = (UInt32)1 << 29;
  if (unpackSize >= (UInt64)kNumBlocksMax * kCompressionBlockSize)
    return S_FALSE;
  const UInt32 numBlocks = (UInt32)((unpackSize + kCompressionBlockSize - 1) / kCompressionBlockSize);
  const UInt32 numBlocks2 = numBlocks + 1;
  const UInt32 tableSize = (numBlocks2 << 2);
  if (tableSize > forkSize)
    return S_FALSE;
  _tableBuf.AllocAtLeast(tableSize);
  RINOK(ReadStream_FALSE(inStream, _tableBuf, tableSize))
  const Byte *tableBuf = _tableBuf;
  
  {
    UInt32 prev = GetUi32(tableBuf);
    if (prev != tableSize)
      return S_FALSE;
    for (UInt32 i = 1; i < numBlocks2; i++)
    {
      const UInt32 offs = GetUi32(tableBuf + i * 4);
      if (offs <= prev)
        return S_FALSE;
      prev = offs;
    }
    if (prev != forkSize)
      return S_FALSE;
  }

  const size_t kBufSize = kCompressionBlockSize;
  _buf.Alloc(kBufSize + 0x10); // we need 1 additional bytes for uncompressed chunk header

  CBufInStream *bufInStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> bufInStream = bufInStreamSpec;

  UInt64 outPos = 0;

  for (UInt32 i = 0; i < numBlocks; i++)
  {
    const UInt64 rem = unpackSize - outPos;
    if (rem == 0)
      return S_FALSE;
    UInt32 blockSize = kCompressionBlockSize;
    if (rem < kCompressionBlockSize)
      blockSize = (UInt32)rem;

    const UInt32 size =
        GetUi32(tableBuf + i * 4 + 4) -
        GetUi32(tableBuf + i * 4);

    if (size > kCompressionBlockSize + 1)
      return S_FALSE;

    RINOK(ReadStream_FALSE(inStream, _buf, size))
    const Byte *buf = _buf;

    if (buf[0] == k_LZVN_Uncompressed_Marker)
    {
      if (size - 1 != blockSize)
        return S_FALSE;
      if (outStream)
      {
        RINOK(WriteStream(outStream, buf + 1, blockSize))
      }
    }
    else
    {
      const UInt64 blockSize64 = blockSize;
      const UInt64 packSize64 = size;
      bufInStreamSpec->Init(buf, size);
      RINOK(_lzfseDecoder->Code(bufInStream, outStream, &packSize64, &blockSize64, NULL))
      // in/out sizes were checked in Code()
    }
    
    outPos += blockSize;
    if ((i & 0xFF) == 0)
    {
      const UInt64 progressPos = progressStart + outPos;
      RINOK(extractCallback->SetCompleted(&progressPos))
    }
  }

  return S_OK;
}


/*
static UInt32 GetUi24(const Byte *p)
{
  return p[0] + ((UInt32)p[1] << 8) + ((UInt32)p[2] << 24);
}

HRESULT CDecoder::ExtractResourceFork_ZBM(
    ISequentialInStream *inStream, ISequentialOutStream *outStream,
    UInt64 forkSize, UInt64 unpackSize,
    UInt64 progressStart, IArchiveExtractCallback *extractCallback)
{
  const UInt32 kNumBlocksMax = (UInt32)1 << 29;
  if (unpackSize >= (UInt64)kNumBlocksMax * kCompressionBlockSize)
    return S_FALSE;
  const UInt32 numBlocks = (UInt32)((unpackSize + kCompressionBlockSize - 1) / kCompressionBlockSize);
  const UInt32 numBlocks2 = numBlocks + 1;
  const UInt32 tableSize = (numBlocks2 << 2);
  if (tableSize > forkSize)
    return S_FALSE;
  _tableBuf.AllocAtLeast(tableSize);
  RINOK(ReadStream_FALSE(inStream, _tableBuf, tableSize));
  const Byte *tableBuf = _tableBuf;
  
  {
    UInt32 prev = GetUi32(tableBuf);
    if (prev != tableSize)
      return S_FALSE;
    for (UInt32 i = 1; i < numBlocks2; i++)
    {
      const UInt32 offs = GetUi32(tableBuf + i * 4);
      if (offs <= prev)
        return S_FALSE;
      prev = offs;
    }
    if (prev != forkSize)
      return S_FALSE;
  }

  const size_t kBufSize = kCompressionBlockSize;
  _buf.Alloc(kBufSize + 0x10); // we need 1 additional bytes for uncompressed chunk header

  CBufInStream *bufInStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> bufInStream = bufInStreamSpec;

  UInt64 outPos = 0;

  for (UInt32 i = 0; i < numBlocks; i++)
  {
    const UInt64 rem = unpackSize - outPos;
    if (rem == 0)
      return S_FALSE;
    UInt32 blockSize = kCompressionBlockSize;
    if (rem < kCompressionBlockSize)
      blockSize = (UInt32)rem;

    const UInt32 size =
        GetUi32(tableBuf + i * 4 + 4) -
        GetUi32(tableBuf + i * 4);

    // if (size > kCompressionBlockSize + 1)
    if (size > blockSize + 1)
      return S_FALSE; // we don't expect it, because encode will use uncompressed chunk

    RINOK(ReadStream_FALSE(inStream, _buf, size));
    const Byte *buf = _buf;

    // (size != 0)
    // if (size == 0) return S_FALSE;

    if (buf[0] == 0xFF) // uncompressed marker
    {
      if (size != blockSize + 1)
        return S_FALSE;
      if (outStream)
      {
        RINOK(WriteStream(outStream, buf + 1, blockSize));
      }
    }
    else
    {
      if (size < 4)
        return S_FALSE;
      if (buf[0] != 'Z' ||
          buf[1] != 'B' ||
          buf[2] != 'M' ||
          buf[3] != 9)
        return S_FALSE;
      // for debug:
      unsigned packPos = 4;
      unsigned unpackPos = 0;
      unsigned packRem = size - packPos;
      for (;;)
      {
        if (packRem < 6)
          return S_FALSE;
        const UInt32 packSize = GetUi24(buf + packPos);
        const UInt32 chunkUnpackSize = GetUi24(buf + packPos + 3);
        if (packSize < 6)
          return S_FALSE;
        if (packSize > packRem)
          return S_FALSE;
        if (chunkUnpackSize > blockSize - unpackPos)
          return S_FALSE;
        packPos += packSize;
        packRem -= packSize;
        unpackPos += chunkUnpackSize;
        if (packSize == 6)
        {
          if (chunkUnpackSize != 0)
            return S_FALSE;
          break;
        }
        if (packSize >= chunkUnpackSize + 6)
        {
          if (packSize > chunkUnpackSize + 6)
            return S_FALSE;
          // uncompressed chunk;
        }
        else
        {
          // compressed chunk
          const Byte *t = buf + packPos - packSize + 6;
          UInt32 r = packSize - 6;
          if (r < 9)
            return S_FALSE;
          const UInt32 v0 = GetUi24(t);
          const UInt32 v1 = GetUi24(t + 3);
          const UInt32 v2 = GetUi24(t + 6);
          if (v0 > v1 || v1 > v2 || v2 > packSize)
            return S_FALSE;
          // here we need the code that will decompress ZBM chunk
        }
      }

      if (unpackPos != blockSize)
        return S_FALSE;

      UInt32 size1 = size;
      if (size1 > kCompressionBlockSize)
      {
        size1 = kCompressionBlockSize;
        // return S_FALSE;
      }
      if (outStream)
      {
        RINOK(WriteStream(outStream, buf, size1))

        const UInt32 kTempSize = 1 << 16;
        Byte temp[kTempSize];
        memset(temp, 0, kTempSize);

        for (UInt32 k = size1; k < kCompressionBlockSize; k++)
        {
          UInt32 cur = kCompressionBlockSize - k;
          if (cur > kTempSize)
            cur = kTempSize;
          RINOK(WriteStream(outStream, temp, cur))
          k += cur;
        }
      }

      // const UInt64 blockSize64 = blockSize;
      // const UInt64 packSize64 = size;
      // bufInStreamSpec->Init(buf, size);
      // RINOK(_zbmDecoderSpec->Code(bufInStream, outStream, &packSize64, &blockSize64, NULL));
      // in/out sizes were checked in Code()
    }
    
    outPos += blockSize;
    if ((i & 0xFF) == 0)
    {
      const UInt64 progressPos = progressStart + outPos;
      RINOK(extractCallback->SetCompleted(&progressPos));
    }
  }

  return S_OK;
}
*/

HRESULT CDecoder::Extract(
    ISequentialInStream *inStreamFork, ISequentialOutStream *realOutStream,
    UInt64 forkSize,
    const CCompressHeader &compressHeader,
    const CByteBuffer *data,
    UInt64 progressStart, IArchiveExtractCallback *extractCallback,
    int &opRes)
{
  opRes = NExtract::NOperationResult::kDataError;

  if (compressHeader.IsMethod_Uncompressed_Inline())
  {
    const size_t packSize = data->Size() - compressHeader.DataPos;
    if (realOutStream)
    {
      RINOK(WriteStream(realOutStream, *data + compressHeader.DataPos, packSize))
    }
    opRes = NExtract::NOperationResult::kOK;
    return S_OK;
  }

  if (compressHeader.Method == kMethod_ZLIB_ATTR ||
      compressHeader.Method == kMethod_LZVN_ATTR)
  {
    CBufInStream *bufInStreamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> bufInStream = bufInStreamSpec;
    const size_t packSize = data->Size() - compressHeader.DataPos;
    bufInStreamSpec->Init(*data + compressHeader.DataPos, packSize);
    
    if (compressHeader.Method == kMethod_ZLIB_ATTR)
    {
      const HRESULT hres = _zlibDecoder->Code(bufInStream, realOutStream,
          NULL, &compressHeader.UnpackSize, NULL);
      if (hres == S_OK)
        if (_zlibDecoderSpec->GetOutputProcessedSize() == compressHeader.UnpackSize
            && _zlibDecoderSpec->GetInputProcessedSize() == packSize)
          opRes = NExtract::NOperationResult::kOK;
      return hres;
    }
    {
      const UInt64 packSize64 = packSize;
      const HRESULT hres = _lzfseDecoder->Code(bufInStream, realOutStream,
          &packSize64, &compressHeader.UnpackSize, NULL);
      if (hres == S_OK)
      {
        // in/out sizes were checked in Code()
        opRes = NExtract::NOperationResult::kOK;
      }
      return hres;
    }
  }

  HRESULT hres;
  if (compressHeader.Method == NHfs::kMethod_ZLIB_RSRC)
  {
    hres = ExtractResourceFork_ZLIB(
        inStreamFork, realOutStream,
        forkSize, compressHeader.UnpackSize,
        progressStart, extractCallback);
  }
  else if (compressHeader.Method == NHfs::kMethod_LZVN_RSRC)
  {
    hres = ExtractResourceFork_LZFSE(
        inStreamFork, realOutStream,
        forkSize, compressHeader.UnpackSize,
        progressStart, extractCallback);
  }
  /*
  else if (compressHeader.Method == NHfs::kMethod_ZBM_RSRC)
  {
    hres = ExtractResourceFork_ZBM(
        inStreamFork, realOutStream,
        forkSize, compressHeader.UnpackSize,
        progressStart, extractCallback);
  }
  */
  else
  {
    opRes = NExtract::NOperationResult::kUnsupportedMethod;
    hres = S_FALSE;
  }

  if (hres == S_OK)
    opRes = NExtract::NOperationResult::kOK;
  return hres;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = Refs.Size();
  if (numItems == 0)
    return S_OK;
  UInt32 i;
  UInt64 totalSize = 0;
  for (i = 0; i < numItems; i++)
  {
    const CRef &ref = Refs[allFilesMode ? i : indices[i]];
    totalSize += Get_UnpackSize_of_Ref(ref);
  }
  RINOK(extractCallback->SetTotal(totalSize))

  UInt64 currentTotalSize = 0, currentItemSize = 0;
  
  const size_t kBufSize = kCompressionBlockSize;
  CByteBuffer buf(kBufSize + 0x10); // we need 1 additional bytes for uncompressed chunk header

  CDecoder decoder;

  for (i = 0;; i++, currentTotalSize += currentItemSize)
  {
    RINOK(extractCallback->SetCompleted(&currentTotalSize))
    if (i == numItems)
      break;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CRef &ref = Refs[index];
    const CItem &item = Items[ref.ItemIndex];
    currentItemSize = Get_UnpackSize_of_Ref(ref);

    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))

    if (ref.IsItem() && item.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    if (!testMode && !realOutStream)
      continue;
    
    RINOK(extractCallback->PrepareOperation(askMode))
    
    UInt64 pos = 0;
    int opRes = NExtract::NOperationResult::kDataError;
    const CFork *fork = NULL;
    
    if (ref.AttrIndex >= 0)
    {
      const CAttr &attr = Attrs[ref.AttrIndex];
      if (attr.Fork_defined && attr.Data.Size() == 0)
        fork = &attr.Fork;
      else
      {
        opRes = NExtract::NOperationResult::kOK;
        if (realOutStream)
        {
          RINOK(WriteStream(realOutStream,
              // AttrBuf + attr.Pos, attr.Size
              attr.Data, attr.Data.Size()
              ))
        }
      }
    }
    else if (ref.IsResource())
      fork = &item.ResourceFork;
    else if (item.CompressHeader.IsSupported)
    {
      CMyComPtr<ISequentialInStream> inStreamFork;
      UInt64 forkSize = 0;
      const CByteBuffer *decmpfs_Data = NULL;

      if (item.CompressHeader.IsMethod_Resource())
      {
        const CFork &resourceFork = item.ResourceFork;
        forkSize = resourceFork.Size;
        GetForkStream(resourceFork, &inStreamFork);
      }
      else
      {
        const CAttr &attr = Attrs[item.decmpfs_AttrIndex];
        decmpfs_Data = &attr.Data;
      }

      if (inStreamFork || decmpfs_Data)
      {
        const HRESULT hres = decoder.Extract(
            inStreamFork, realOutStream,
            forkSize,
            item.CompressHeader,
            decmpfs_Data,
            currentTotalSize, extractCallback,
            opRes);
        if (hres != S_FALSE && hres != S_OK)
          return hres;
      }
    }
    else if (item.CompressHeader.IsCorrect)
      opRes = NExtract::NOperationResult::kUnsupportedMethod;
    else
      fork = &item.DataFork;
    
    if (fork)
    {
      if (fork->IsOk(Header.BlockSizeLog))
      {
        opRes = NExtract::NOperationResult::kOK;
        unsigned extentIndex;
        for (extentIndex = 0; extentIndex < fork->Extents.Size(); extentIndex++)
        {
          if (opRes != NExtract::NOperationResult::kOK)
            break;
          if (fork->Size == pos)
            break;
          const CExtent &e = fork->Extents[extentIndex];
          RINOK(InStream_SeekSet(_stream, SpecOffset + ((UInt64)e.Pos << Header.BlockSizeLog)))
          UInt64 extentRem = (UInt64)e.NumBlocks << Header.BlockSizeLog;
          while (extentRem != 0)
          {
            const UInt64 rem = fork->Size - pos;
            if (rem == 0)
            {
              // Here we check that there are no extra (empty) blocks in last extent.
              if (extentRem >= ((UInt64)1 << Header.BlockSizeLog))
                opRes = NExtract::NOperationResult::kDataError;
              break;
            }
            size_t cur = kBufSize;
            if (cur > rem)
              cur = (size_t)rem;
            if (cur > extentRem)
              cur = (size_t)extentRem;
            RINOK(ReadStream(_stream, buf, &cur))
            if (cur == 0)
            {
              opRes = NExtract::NOperationResult::kDataError;
              break;
            }
            if (realOutStream)
            {
              RINOK(WriteStream(realOutStream, buf, cur))
            }
            pos += cur;
            extentRem -= cur;
            const UInt64 processed = currentTotalSize + pos;
            RINOK(extractCallback->SetCompleted(&processed))
          }
        }
        if (extentIndex != fork->Extents.Size() || fork->Size != pos)
          opRes = NExtract::NOperationResult::kDataError;
      }
    }
    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(opRes))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Refs.Size();
  return S_OK;
}

HRESULT CHandler::GetForkStream(const CFork &fork, ISequentialInStream **stream)
{
  *stream = NULL;
  
  if (!fork.IsOk(Header.BlockSizeLog))
    return S_FALSE;

  CExtentsStream *extentStreamSpec = new CExtentsStream();
  CMyComPtr<ISequentialInStream> extentStream = extentStreamSpec;

  UInt64 rem = fork.Size;
  UInt64 virt = 0;

  FOR_VECTOR (i, fork.Extents)
  {
    const CExtent &e = fork.Extents[i];
    if (e.NumBlocks == 0)
      continue;
    UInt64 cur = ((UInt64)e.NumBlocks << Header.BlockSizeLog);
    if (cur > rem)
    {
      cur = rem;
      if (i != fork.Extents.Size() - 1)
        return S_FALSE;
    }
    CSeekExtent se;
    se.Phy = (UInt64)e.Pos << Header.BlockSizeLog;
    se.Virt = virt;
    virt += cur;
    rem -= cur;
    extentStreamSpec->Extents.Add(se);
  }
  
  if (rem != 0)
    return S_FALSE;
  
  CSeekExtent se;
  se.Phy = 0;
  se.Virt = virt;
  extentStreamSpec->Extents.Add(se);
  extentStreamSpec->Stream = _stream;
  extentStreamSpec->Init();
  *stream = extentStream.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  *stream = NULL;

  const CRef &ref = Refs[index];
  const CFork *fork = NULL;
  if (ref.AttrIndex >= 0)
  {
    const CAttr &attr = Attrs[ref.AttrIndex];
    if (!attr.Fork_defined || attr.Data.Size() != 0)
      return S_FALSE;
    fork = &attr.Fork;
  }
  else
  {
    const CItem &item = Items[ref.ItemIndex];
    if (ref.IsResource())
      fork = &item.ResourceFork;
    else if (item.IsDir())
      return S_FALSE;
    else if (item.CompressHeader.IsCorrect)
      return S_FALSE;
    else
      fork = &item.DataFork;
  }
  return GetForkStream(*fork, stream);
}

static const Byte k_Signature[] = {
    2, 'B', 'D',
    4, 'H', '+', 0, 4,
    4, 'H', 'X', 0, 5 };

REGISTER_ARC_I(
  "HFS", "hfs hfsx", NULL, 0xE3,
  k_Signature,
  kHeaderPadSize,
  NArcInfoFlags::kMultiSignature,
  IsArc_HFS)

}}
