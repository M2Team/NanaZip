// HfsHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/LimitedStreams.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/ZlibDecoder.h"

/* if HFS_SHOW_ALT_STREAMS is defined, the handler will show attribute files
   and resource forks. In most cases it looks useless. So we disable it. */
 
// #define HFS_SHOW_ALT_STREAMS

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
  int Index;

  int Compare(const CIdIndexPair &a) const;
};

#define RINOZ(x) { int __tt = (x); if (__tt != 0) return __tt; }

int CIdIndexPair::Compare(const CIdIndexPair &a) const
{
  RINOZ(MyCompare(ID, a.ID));
  return MyCompare(Index, a.Index);
}

static int FindItemIndex(const CRecordVector<CIdIndexPair> &items, UInt32 id)
{
  unsigned left = 0, right = items.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    UInt32 midVal = items[mid].ID;
    if (id == midVal)
      return items[mid].Index;
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
    unsigned mid = (left + right) / 2;
    UInt32 midVal = items[mid].ID;
    if (id == midVal)
      return mid;
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
  // UInt32 AttrMTime;
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

  // for compressed attribute
  UInt64 UnpackSize;
  size_t DataPos;
  UInt32 PackSize;
  unsigned Method;
  bool UseAttr;
  bool UseInlineData;

  CItem(): UseAttr(false), UseInlineData(false) {}
  bool IsDir() const { return Type == RECORD_TYPE_FOLDER; }
  const CFork &GetFork(bool isResource) const { return (const CFork & )*(isResource ? &ResourceFork: &DataFork ); }
};

struct CAttr
{
  UInt32 ID;
  UInt32 Size;
  size_t Pos;
  UString Name;
};

struct CRef
{
  unsigned ItemIndex;
  int AttrIndex;
  int Parent;
  bool IsResource;

  bool IsAltStream() const { return IsResource || AttrIndex >= 0; }
  CRef(): AttrIndex(-1), Parent(-1), IsResource(false)  {}
};

class CDatabase
{
  HRESULT ReadFile(const CFork &fork, CByteBuffer &buf, IInStream *inStream);
  HRESULT LoadExtentFile(const CFork &fork, IInStream *inStream, CObjectVector<CIdExtents> *overflowExtentsArray);
  HRESULT LoadAttrs(const CFork &fork, IInStream *inStream, IArchiveOpenCallback *progress);
  HRESULT LoadCatalog(const CFork &fork, const CObjectVector<CIdExtents> *overflowExtentsArray, IInStream *inStream, IArchiveOpenCallback *progress);
  bool Parse_decmpgfs(const CAttr &attr, CItem &item, bool &skip);
public:
  CRecordVector<CRef> Refs;
  CObjectVector<CItem> Items;
  CObjectVector<CAttr> Attrs;

  CByteBuffer AttrBuf;

  CVolHeader Header;
  bool HeadersError;
  bool ThereAreAltStreams;
  // bool CaseSensetive;
  UString ResFileName;

  UInt64 SpecOffset;
  UInt64 PhySize;
  UInt64 PhySize2;
  UInt64 ArcFileSize;

  void Clear()
  {
    SpecOffset = 0;
    PhySize = 0;
    PhySize2 = 0;
    ArcFileSize = 0;
    HeadersError = false;
    ThereAreAltStreams = false;
    // CaseSensetive = false;
    Refs.Clear();
    Items.Clear();
    Attrs.Clear();
    AttrBuf.Free();
  }

  UInt64 Get_UnpackSize_of_Ref(const CRef &ref) const
  {
    if (ref.AttrIndex >= 0)
      return Attrs[ref.AttrIndex].Size;
    const CItem &item = Items[ref.ItemIndex];
    if (item.IsDir())
      return 0;
    if (item.UseAttr)
      return item.UnpackSize;
    return item.GetFork(ref.IsResource).Size;
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
  int cur = index;
  unsigned i;
  
  for (i = 0; i < kNumLevelsMax; i++)
  {
    const CRef &ref = Refs[cur];
    const UString *s;

    if (ref.IsResource)
      s = &ResFileName;
    else if (ref.AttrIndex >= 0)
      s = &Attrs[ref.AttrIndex].Name;
    else
      s = &Items[ref.ItemIndex].Name;

    len += s->Len();
    len++;
    cur = ref.Parent;
    if (cur < 0)
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

    if (ref.IsResource)
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
    cur = ref.Parent;
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
    RINOK(inStream->Seek(SpecOffset + ((UInt64)e.Pos << Header.BlockSizeLog), STREAM_SEEK_SET, NULL));
    RINOK(ReadStream_FALSE(inStream,
        (Byte *)buf + ((size_t)curBlock << Header.BlockSizeLog),
        (size_t)e.NumBlocks << Header.BlockSizeLog));
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
  RINOK(ReadFile(fork, buf, inStream));
  const Byte *p = (const Byte *)buf;

  // CNodeDescriptor nodeDesc;
  // nodeDesc.Parse(p);
  CHeaderRec hr;
  RINOK(hr.Parse2(buf));

  UInt32 node = hr.FirstLeafNode;
  if (node == 0)
    return S_OK;

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
      const UInt32 nodeSize = (1 << hr.NodeSizeLog);
      const Byte *r = p + nodeOffset + nodeSize - i * 2;
      const UInt32 offs = Get16(r - 2);
      UInt32 recSize = Get16(r - 4) - offs;
      const unsigned kKeyLen = 10;

      if (recSize != 2 + kKeyLen + kNumFixedExtents * 8)
        return S_FALSE;

      r = p + nodeOffset + offs;
      if (Get16(r) != kKeyLen)
        return S_FALSE;
      
      Byte forkType = r[2];
      unsigned forkTypeIndex;
      if (forkType == kExtentForkType_Data)
        forkTypeIndex = 0;
      else if (forkType == kExtentForkType_Resource)
        forkTypeIndex = 1;
      else
        continue;
      CObjectVector<CIdExtents> &overflowExtents = overflowExtentsArray[forkTypeIndex];

      UInt32 id = Get32(r + 4);
      UInt32 startBlock = Get32(r + 8);
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
    wchar_t c = Get16(data + i * 2);
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
    char c = name[i];
    if (c == 0)
      return true;
    if (Get16(data + i * 2) != (Byte)c)
      return false;
  }
}

static const UInt32 kAttrRecordType_Inline = 0x10;
// static const UInt32 kAttrRecordType_Fork = 0x20;
// static const UInt32 kAttrRecordType_Extents = 0x30;

HRESULT CDatabase::LoadAttrs(const CFork &fork, IInStream *inStream, IArchiveOpenCallback *progress)
{
  if (fork.NumBlocks == 0)
    return S_OK;

  RINOK(ReadFile(fork, AttrBuf, inStream));
  const Byte *p = (const Byte *)AttrBuf;

  // CNodeDescriptor nodeDesc;
  // nodeDesc.Parse(p);
  CHeaderRec hr;
  RINOK(hr.Parse2(AttrBuf));
  
  // CaseSensetive = (Header.IsHfsX() && hr.KeyCompareType == 0xBC);

  UInt32 node = hr.FirstLeafNode;
  if (node == 0)
    return S_OK;
  
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
      const UInt32 nodeSize = (1 << hr.NodeSizeLog);
      const Byte *r = p + nodeOffset + nodeSize - i * 2;
      const UInt32 offs = Get16(r - 2);
      UInt32 recSize = Get16(r - 4) - offs;
      const unsigned kHeadSize = 14;
      if (recSize < kHeadSize)
        return S_FALSE;

      r = p + nodeOffset + offs;
      UInt32 keyLen = Get16(r);

      // UInt16 pad = Get16(r + 2);
      UInt32 fileID = Get32(r + 4);
      unsigned startBlock = Get32(r + 8);
      if (startBlock != 0)
      {
        // that case is still unsupported
        HeadersError = true;
        continue;
      }
      unsigned nameLen = Get16(r + 12);

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

      UInt32 recordType = Get32(r);
      if (recordType != kAttrRecordType_Inline)
      {
        // Probably only kAttrRecordType_Inline now is used in real HFS files
        HeadersError = true;
        continue;
      }

      const UInt32 kRecordHeaderSize = 16;
      if (recSize < kRecordHeaderSize)
        return S_FALSE;
      UInt32 dataSize = Get32(r + 12);

      r += kRecordHeaderSize;
      recSize -= kRecordHeaderSize;

      if (recSize < dataSize)
        return S_FALSE;

      CAttr &attr = Attrs.AddNew();
      attr.ID = fileID;
      attr.Pos = nodeOffset + offs + 2 + keyLen + kRecordHeaderSize;
      attr.Size = dataSize;
      LoadName(name, nameLen, attr.Name);

      if (progress && (i & 0xFFF) == 0)
      {
        const UInt64 numFiles = 0;
        RINOK(progress->SetCompleted(&numFiles, NULL));
      }
    }

    node = desc.fLink;
  }
  return S_OK;
}

static const UInt32 kMethod_Attr     = 3; // data stored in attribute file
static const UInt32 kMethod_Resource = 4; // data stored in resource fork

bool CDatabase::Parse_decmpgfs(const CAttr &attr, CItem &item, bool &skip)
{
  skip = false;
  if (!attr.Name.IsEqualTo("com.apple.decmpfs"))
    return true;
  if (item.UseAttr || !item.DataFork.IsEmpty())
    return false;

  const UInt32 k_decmpfs_headerSize = 16;
  UInt32 dataSize = attr.Size;
  if (dataSize < k_decmpfs_headerSize)
    return false;
  const Byte *r = AttrBuf + attr.Pos;
  if (GetUi32(r) != 0x636D7066) // magic == "fpmc"
    return false;
  item.Method = GetUi32(r + 4);
  item.UnpackSize = GetUi64(r + 8);
  dataSize -= k_decmpfs_headerSize;
  r += k_decmpfs_headerSize;
  if (item.Method == kMethod_Resource)
  {
    if (dataSize != 0)
      return false;
    item.UseAttr = true;
  }
  else if (item.Method == kMethod_Attr)
  {
    if (dataSize == 0)
      return false;
    Byte b = r[0];
    if ((b & 0xF) == 0xF)
    {
      dataSize--;
      if (item.UnpackSize > dataSize)
        return false;
      item.DataPos = attr.Pos + k_decmpfs_headerSize + 1;
      item.PackSize = dataSize;
      item.UseAttr = true;
      item.UseInlineData = true;
    }
    else
    {
      item.DataPos = attr.Pos + k_decmpfs_headerSize;
      item.PackSize = dataSize;
      item.UseAttr = true;
    }
  }
  else
    return false;
  skip = true;
  return true;
}

HRESULT CDatabase::LoadCatalog(const CFork &fork, const CObjectVector<CIdExtents> *overflowExtentsArray, IInStream *inStream, IArchiveOpenCallback *progress)
{
  CByteBuffer buf;
  RINOK(ReadFile(fork, buf, inStream));
  const Byte *p = (const Byte *)buf;

  // CNodeDescriptor nodeDesc;
  // nodeDesc.Parse(p);
  CHeaderRec hr;
  RINOK(hr.Parse2(buf));

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
      // item.AttrMTime = Get32(r + 0x14);
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
        const unsigned kForkRecSize = 16 + kNumFixedExtents * 8;
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
          ThereAreAltStreams = true;
        }
      }
      if (progress && (Items.Size() & 0xFFF) == 0)
      {
        const UInt64 numItems = Items.Size();
        RINOK(progress->SetCompleted(&numItems, NULL));
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

      int itemIndex = FindItemIndex(IdToIndexMap, attr.ID);
      if (itemIndex < 0)
      {
        HeadersError = true;
        continue;
      }
      if (!Parse_decmpgfs(attr, Items[itemIndex], skipAttr[i]))
        HeadersError = true;
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
      if (item.UseAttr && item.Method == kMethod_Resource)
        continue;
      CRef resRef;
      resRef.ItemIndex = i;
      resRef.IsResource = true;
      resRef.Parent = Refs.Size() - 1;
      Refs.Add(resRef);
      
      #endif
    }
  }

  IdToIndexMap.Sort2();

  {
    FOR_VECTOR (i, Refs)
    {
      CRef &ref = Refs[i];
      if (ref.IsResource)
        continue;
      CItem &item = Items[ref.ItemIndex];
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

      int refIndex = FindItemIndex(IdToIndexMap, attr.ID);
      if (refIndex < 0)
      {
        HeadersError = true;
        continue;
      }

      CRef ref;
      ref.AttrIndex = i;
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
  RINOK(ReadStream_FALSE(inStream, buf, kHfsHeaderSize));
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
      RINOK(progress->SetTotal(&numFiles, NULL));
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
    RINOK(inStream->Seek(SpecOffset, STREAM_SEEK_SET, NULL));
    RINOK(ReadStream_FALSE(inStream, buf, kHfsHeaderSize));
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

  RINOK(inStream->Seek(0, STREAM_SEEK_END, &ArcFileSize));

  if (progress)
  {
    const UInt64 numFiles = (UInt64)h.NumFiles + h.NumFolders + 1;
    RINOK(progress->SetTotal(&numFiles, NULL));
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
      RINOK(LoadAttrs(attrFork, inStream, progress));
  }
  
  RINOK(LoadCatalog(catalogFork, overflowExtents, inStream, progress));

  PhySize = Header.GetPhySize();
  return S_OK;
}



class CHandler:
  public IInArchive,
  public IArchiveGetRawProps,
  public IInArchiveGetStream,
  public CMyUnknownImp,
  public CDatabase
{
  CMyComPtr<IInStream> _stream;

  HRESULT GetForkStream(const CFork &fork, ISequentialInStream **stream);

  HRESULT ExtractZlibFile(
      ISequentialOutStream *realOutStream,
      const CItem &item,
      NCompress::NZlib::CDecoder *_zlibDecoderSpec,
      CByteBuffer &buf,
      UInt64 progressStart,
      IArchiveExtractCallback *extractCallback);
public:
  MY_UNKNOWN_IMP3(IInArchive, IArchiveGetRawProps, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  INTERFACE_IArchiveGetRawProps(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
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
  kpidPosixAttrib
};

static const Byte kArcProps[] =
{
  kpidMethod,
  kpidClusterSize,
  kpidFreeSpace,
  kpidCTime,
  kpidMTime
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static void HfsTimeToProp(UInt32 hfsTime, NWindows::NCOM::CPropVariant &prop)
{
  FILETIME ft;
  HfsTimeToFileTime(hfsTime, ft);
  prop = ft;
}

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidExtension: prop = Header.IsHfsX() ? "hfsx" : "hfs"; break;
    case kpidMethod: prop = Header.IsHfsX() ? "HFSX" : "HFS+"; break;
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
      FILETIME localFt, ft;
      HfsTimeToFileTime(Header.CTime, localFt);
      if (LocalFileTimeToFileTime(&localFt, &ft))
        prop = ft;
      break;
    }
    case kpidIsTree: prop = true; break;
    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (HeadersError) flags |= kpv_ErrorFlags_HeadersError;
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

STDMETHODIMP CHandler::GetNumRawProps(UInt32 *numProps)
{
  *numProps = 0;
  return S_OK;
}

STDMETHODIMP CHandler::GetRawPropInfo(UInt32 /* index */, BSTR *name, PROPID *propID)
{
  *name = NULL;
  *propID = 0;
  return S_OK;
}

STDMETHODIMP CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType)
{
  const CRef &ref = Refs[index];
  *parentType = ref.IsAltStream() ?
      NParentType::kAltStream :
      NParentType::kDir;
  *parent = (UInt32)(Int32)ref.Parent;
  return S_OK;
}

STDMETHODIMP CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType)
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;
  #ifdef MY_CPU_LE
  if (propID == kpidName)
  {
    const CRef &ref = Refs[index];
    const UString *s;
    if (ref.IsResource)
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
  #endif
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CRef &ref = Refs[index];
  const CItem &item = Items[ref.ItemIndex];
  switch (propID)
  {
    case kpidPath: GetItemPath(index, prop); break;
    case kpidName:
      const UString *s;
      if (ref.IsResource)
        s = &ResFileName;
      else if (ref.AttrIndex >= 0)
        s = &Attrs[ref.AttrIndex].Name;
      else
        s = &item.Name;
      prop = *s;
      break;
    case kpidPackSize:
      {
        UInt64 size;
        if (ref.AttrIndex >= 0)
          size = Attrs[ref.AttrIndex].Size;
        else if (item.IsDir())
          break;
        else if (item.UseAttr)
        {
          if (item.Method == kMethod_Resource)
            size = item.ResourceFork.NumBlocks << Header.BlockSizeLog;
          else
            size = item.PackSize;
        }
        else
          size = item.GetFork(ref.IsResource).NumBlocks << Header.BlockSizeLog;
        prop = size;
        break;
      }
    case kpidSize:
      {
        UInt64 size;
        if (ref.AttrIndex >= 0)
          size = Attrs[ref.AttrIndex].Size;
        else if (item.IsDir())
          break;
        else if (item.UseAttr)
          size = item.UnpackSize;
        else
          size = item.GetFork(ref.IsResource).Size;
        prop = size;
        break;
      }
    case kpidIsDir: prop = item.IsDir(); break;
    case kpidIsAltStream: prop = ref.IsAltStream(); break;

    case kpidCTime: HfsTimeToProp(item.CTime, prop); break;
    case kpidMTime: HfsTimeToProp(item.MTime, prop); break;
    case kpidATime: HfsTimeToProp(item.ATime, prop); break;

    case kpidPosixAttrib: if (ref.AttrIndex < 0) prop = (UInt32)item.FileMode; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *callback)
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(inStream, callback));
  _stream = inStream;
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _stream.Release();
  Clear();
  return S_OK;
}

static const UInt32 kCompressionBlockSize = 1 << 16;

HRESULT CHandler::ExtractZlibFile(
    ISequentialOutStream *outStream,
    const CItem &item,
    NCompress::NZlib::CDecoder *_zlibDecoderSpec,
    CByteBuffer &buf,
    UInt64 progressStart,
    IArchiveExtractCallback *extractCallback)
{
  CMyComPtr<ISequentialInStream> inStream;
  const CFork &fork = item.ResourceFork;
  RINOK(GetForkStream(fork, &inStream));
  const unsigned kHeaderSize = 0x100 + 8;
  RINOK(ReadStream_FALSE(inStream, buf, kHeaderSize));
  UInt32 dataPos = Get32(buf);
  UInt32 mapPos = Get32(buf + 4);
  UInt32 dataSize = Get32(buf + 8);
  UInt32 mapSize = Get32(buf + 12);

  const UInt32 kResMapSize = 50;

  if (mapSize != kResMapSize
      || dataPos + dataSize != mapPos
      || mapPos + mapSize != fork.Size)
    return S_FALSE;
  
  UInt32 dataSize2 = Get32(buf + 0x100);
  if (4 + dataSize2 != dataSize || dataSize2 < 8)
    return S_FALSE;
  
  UInt32 numBlocks = GetUi32(buf + 0x100 + 4);
  if (((dataSize2 - 4) >> 3) < numBlocks)
    return S_FALSE;
  if (item.UnpackSize > (UInt64)numBlocks * kCompressionBlockSize)
    return S_FALSE;

  if (item.UnpackSize + kCompressionBlockSize < (UInt64)numBlocks * kCompressionBlockSize)
    return S_FALSE;

  UInt32 tableSize = (numBlocks << 3);

  CByteBuffer tableBuf(tableSize);

  RINOK(ReadStream_FALSE(inStream, tableBuf, tableSize));
  
  UInt32 prev = 4 + tableSize;

  UInt32 i;
  for (i = 0; i < numBlocks; i++)
  {
    UInt32 offset = GetUi32(tableBuf + i * 8);
    UInt32 size = GetUi32(tableBuf + i * 8 + 4);
    if (size == 0)
      return S_FALSE;
    if (prev != offset)
      return S_FALSE;
    if (offset > dataSize2 ||
        size > dataSize2 - offset)
      return S_FALSE;
    prev = offset + size;
  }

  if (prev != dataSize2)
    return S_FALSE;

  CBufInStream *bufInStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> bufInStream = bufInStreamSpec;

  UInt64 outPos = 0;
  for (i = 0; i < numBlocks; i++)
  {
    UInt64 rem = item.UnpackSize - outPos;
    if (rem == 0)
      return S_FALSE;
    UInt32 blockSize = kCompressionBlockSize;
    if (rem < kCompressionBlockSize)
      blockSize = (UInt32)rem;

    UInt32 size = GetUi32(tableBuf + i * 8 + 4);

    if (size > buf.Size() || size > kCompressionBlockSize + 1)
      return S_FALSE;

    RINOK(ReadStream_FALSE(inStream, buf, size));

    if ((buf[0] & 0xF) == 0xF)
    {
      // that code was not tested. Are there HFS archives with uncompressed block
      if (size - 1 != blockSize)
        return S_FALSE;

      if (outStream)
      {
        RINOK(WriteStream(outStream, buf, blockSize));
      }
    }
    else
    {
      UInt64 blockSize64 = blockSize;
      bufInStreamSpec->Init(buf, size);
      RINOK(_zlibDecoderSpec->Code(bufInStream, outStream, NULL, &blockSize64, NULL));
      if (_zlibDecoderSpec->GetOutputProcessedSize() != blockSize ||
          _zlibDecoderSpec->GetInputProcessedSize() != size)
        return S_FALSE;
    }
    
    outPos += blockSize;
    const UInt64 progressPos = progressStart + outPos;
    RINOK(extractCallback->SetCompleted(&progressPos));
  }

  if (outPos != item.UnpackSize)
    return S_FALSE;

  /* We check Resource Map
     Are there HFS files with another values in Resource Map ??? */

  RINOK(ReadStream_FALSE(inStream, buf, mapSize));
  UInt32 types = Get16(buf + 24);
  UInt32 names = Get16(buf + 26);
  UInt32 numTypes = Get16(buf + 28);
  if (numTypes != 0 || types != 28 || names != kResMapSize)
    return S_FALSE;
  UInt32 resType = Get32(buf + 30);
  UInt32 numResources = Get16(buf + 34);
  UInt32 resListOffset = Get16(buf + 36);
  if (resType != 0x636D7066) // cmpf
    return S_FALSE;
  if (numResources != 0 || resListOffset != 10)
    return S_FALSE;

  UInt32 entryId = Get16(buf + 38);
  UInt32 nameOffset = Get16(buf + 40);
  // Byte attrib = buf[42];
  UInt32 resourceOffset = Get32(buf + 42) & 0xFFFFFF;
  if (entryId != 1 || nameOffset != 0xFFFF || resourceOffset != 0)
    return S_FALSE;

  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
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
  RINOK(extractCallback->SetTotal(totalSize));

  UInt64 currentTotalSize = 0, currentItemSize = 0;
  
  const size_t kBufSize = kCompressionBlockSize;
  CByteBuffer buf(kBufSize + 0x10); // we need 1 additional bytes for uncompressed chunk header

  NCompress::NZlib::CDecoder *_zlibDecoderSpec = NULL;
  CMyComPtr<ICompressCoder> _zlibDecoder;

  for (i = 0; i < numItems; i++, currentTotalSize += currentItemSize)
  {
    RINOK(extractCallback->SetCompleted(&currentTotalSize));
    UInt32 index = allFilesMode ? i : indices[i];
    const CRef &ref = Refs[index];
    const CItem &item = Items[ref.ItemIndex];
    currentItemSize = Get_UnpackSize_of_Ref(ref);

    CMyComPtr<ISequentialOutStream> realOutStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    if (ref.AttrIndex < 0 && item.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode));
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }
    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));
    UInt64 pos = 0;
    int res = NExtract::NOperationResult::kDataError;
    if (ref.AttrIndex >= 0)
    {
      res = NExtract::NOperationResult::kOK;
      if (realOutStream)
      {
        const CAttr &attr = Attrs[ref.AttrIndex];
        RINOK(WriteStream(realOutStream, AttrBuf + attr.Pos, attr.Size));
      }
    }
    else if (item.UseAttr)
    {
      if (item.UseInlineData)
      {
        res = NExtract::NOperationResult::kOK;
        if (realOutStream)
        {
          RINOK(WriteStream(realOutStream, AttrBuf + item.DataPos, (size_t)item.UnpackSize));
        }
      }
      else
      {
        if (!_zlibDecoder)
        {
          _zlibDecoderSpec = new NCompress::NZlib::CDecoder();
          _zlibDecoder = _zlibDecoderSpec;
        }
        
        if (item.Method == kMethod_Attr)
        {
          CBufInStream *bufInStreamSpec = new CBufInStream;
          CMyComPtr<ISequentialInStream> bufInStream = bufInStreamSpec;
          bufInStreamSpec->Init(AttrBuf + item.DataPos, item.PackSize);
          
          HRESULT hres = _zlibDecoder->Code(bufInStream, realOutStream, NULL, &item.UnpackSize, NULL);
          if (hres != S_FALSE)
          {
            if (hres != S_OK)
              return hres;
            if (_zlibDecoderSpec->GetOutputProcessedSize() == item.UnpackSize &&
                _zlibDecoderSpec->GetInputProcessedSize() == item.PackSize)
              res = NExtract::NOperationResult::kOK;
          }
        }
        else
        {
          HRESULT hres = ExtractZlibFile(realOutStream, item, _zlibDecoderSpec, buf,
            currentTotalSize, extractCallback);
          if (hres != S_FALSE)
          {
            if (hres != S_OK)
              return hres;
            res = NExtract::NOperationResult::kOK;
          }
        }
      }
    }
    else
    {
      const CFork &fork = item.GetFork(ref.IsResource);
      if (fork.IsOk(Header.BlockSizeLog))
      {
        res = NExtract::NOperationResult::kOK;
        unsigned extentIndex;
        for (extentIndex = 0; extentIndex < fork.Extents.Size(); extentIndex++)
        {
          if (res != NExtract::NOperationResult::kOK)
            break;
          if (fork.Size == pos)
            break;
          const CExtent &e = fork.Extents[extentIndex];
          RINOK(_stream->Seek(SpecOffset + ((UInt64)e.Pos << Header.BlockSizeLog), STREAM_SEEK_SET, NULL));
          UInt64 extentRem = (UInt64)e.NumBlocks << Header.BlockSizeLog;
          while (extentRem != 0)
          {
            UInt64 rem = fork.Size - pos;
            if (rem == 0)
            {
              // Here we check that there are no extra (empty) blocks in last extent.
              if (extentRem >= ((UInt64)1 << Header.BlockSizeLog))
                res = NExtract::NOperationResult::kDataError;
              break;
            }
            size_t cur = kBufSize;
            if (cur > rem)
              cur = (size_t)rem;
            if (cur > extentRem)
              cur = (size_t)extentRem;
            RINOK(ReadStream(_stream, buf, &cur));
            if (cur == 0)
            {
              res = NExtract::NOperationResult::kDataError;
              break;
            }
            if (realOutStream)
            {
              RINOK(WriteStream(realOutStream, buf, cur));
            }
            pos += cur;
            extentRem -= cur;
            const UInt64 processed = currentTotalSize + pos;
            RINOK(extractCallback->SetCompleted(&processed));
          }
        }
        if (extentIndex != fork.Extents.Size() || fork.Size != pos)
          res = NExtract::NOperationResult::kDataError;
      }
    }
    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(res));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = Refs.Size();
  return S_OK;
}

HRESULT CHandler::GetForkStream(const CFork &fork, ISequentialInStream **stream)
{
  *stream = 0;
  
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

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  *stream = 0;

  const CRef &ref = Refs[index];
  if (ref.AttrIndex >= 0)
    return S_FALSE;
  const CItem &item = Items[ref.ItemIndex];
  if (item.IsDir() || item.UseAttr)
    return S_FALSE;
  
  return GetForkStream(item.GetFork(ref.IsResource), stream);
}

static const Byte k_Signature[] = {
    2, 'B', 'D',
    4, 'H', '+', 0, 4,
    4, 'H', 'X', 0, 5 };

REGISTER_ARC_I(
  "HFS", "hfs hfsx", 0, 0xE3,
  k_Signature,
  kHeaderPadSize,
  NArcInfoFlags::kMultiSignature,
  IsArc_HFS)

}}
