// ComHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"

#include "../../Windows/PropVariant.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/ItemNameUtils.h"

#define Get16(p) GetUi16a(p)
#define Get32(p) GetUi32a(p)

// we don't expect to get deleted files in real files
// define Z7_COMPOUND_SHOW_DELETED for debug
// #define Z7_COMPOUND_SHOW_DELETED

namespace NArchive {
namespace NCom {

static const unsigned k_Long_path_level_limit = 256;

static const Byte kSignature[] =
  { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };

// encoded "[!]MsiPatchSequence" name in "msp" file
static const Byte k_Sequence_msp[] =
  { 0x40, 0x48, 0x96, 0x45, 0x6c, 0x3e, 0xe4, 0x45,
    0xe6, 0x42, 0x16, 0x42, 0x37, 0x41, 0x27, 0x41,
    0x37, 0x41, 0, 0 };

// encoded "MergeModule.CABinet" name in "msm" file
static const Byte k_Sequence_msm[] =
  { 0x16, 0x42, 0xb5, 0x42, 0xa8, 0x3d, 0xf2, 0x41,
    0xf8, 0x43, 0xa8, 0x47, 0x8c, 0x3a, 0x0b, 0x43,
    0x31, 0x42, 0x37, 0x48, 0, 0 };

// static const Byte k_CLSID_AAF_V3[] = { 0x41, 0x41, 0x46, 0x42, 0x0d, 0x00, 0x4f, 0x4d, 0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0xff };
// static const Byte k_CLSID_AAF_V4[] = { 0x01, 0x02, 0x01, 0x0d, 0x00, 0x02, 0x00, 0x00, 0x06, 0x0e, 0x2b, 0x34, 0x03, 0x02, 0x01, 0x01 };

enum EArcType
{
  k_Type_Common,
  k_Type_Msi,
  k_Type_Msp,
  k_Type_Msm,
  k_Type_Doc,
  k_Type_Ppt,
  k_Type_Xls,
  k_Type_Aaf
};

static const char * const kExtensions[] =
{
    "compound"
  , "msi"
  , "msp"
  , "msm"
  , "doc"
  , "ppt"
  , "xls"
  , "aaf"
};

namespace NFatID
{
  static const UInt32 kFree       = 0xffffffff;
  static const UInt32 kEndOfChain = 0xfffffffe;
  static const UInt32 kFatSector  = 0xfffffffd;
  static const UInt32 k_DIF_SECT  = 0xfffffffc; // double-indirect file allocation table (DIFAT)
  static const UInt32 kMaxValue   = 0xfffffffa;
}

namespace NItemType
{
  static const unsigned kEmpty = 0;
  static const unsigned kStorage = 1;
  static const unsigned kStream = 2;
  // static const unsigned kLockBytes = 3;
  // static const unsigned kProperty = 4;
  static const unsigned kRootStorage = 5;
}

static const unsigned k_MiniSectorSizeBits = 6;
static const UInt32 k_LongStreamMinSize = 1 << 12;

static const unsigned k_Msi_NumBits = 6;
static const unsigned k_Msi_NumChars = 1 << k_Msi_NumBits;
static const unsigned k_Msi_CharMask = k_Msi_NumChars - 1;
static const unsigned k_Msi_UnicodeRange = k_Msi_NumChars * (k_Msi_NumChars + 1);
static const unsigned k_Msi_StartUnicodeChar = 0x3800;
static const unsigned k_Msi_SpecUnicodeChar = k_Msi_StartUnicodeChar + k_Msi_UnicodeRange;
// (k_Msi_SpecUnicodeChar == 0x4840) is used as special symbol that is used
// as first character in some names in dir entries
/*
static bool IsMsiName(const Byte *p)
{
  unsigned c = Get16(p);
  c -= k_Msi_StartUnicodeChar;
  return c <= k_Msi_UnicodeRange;
}
*/

Z7_FORCE_INLINE static bool IsLargeStream(UInt64 size)
{
  return size >= k_LongStreamMinSize;
}

static const unsigned kNameSizeMax = 64;
static const UInt32 k_Item_Level_Unused = (UInt32)0 - 1;

struct CItem
{
  Byte Name[kNameSizeMax]; // must be aligned for 2-bytes
  // UInt16 NameSize;
  // UInt32 Flags;
  FILETIME CTime;
  FILETIME MTime;
  UInt64 Size;
  UInt32 LeftDid;
  UInt32 RightDid;
  UInt32 SonDid;
  UInt32 Sid;
  unsigned Type;  // Byte : we use unsigned instead of Byte for alignment

  UInt32 Level;

  bool IsEmptyType() const { return Type == NItemType::kEmpty; }
  bool IsDir() const { return Type == NItemType::kStorage || Type == NItemType::kRootStorage; }
  bool IsStorage() const { return Type == NItemType::kStorage; }

  bool IsLevel_Unused() const { return Level == k_Item_Level_Unused; }

  // bool IsSpecMsiName() const { return Get16(Name) == k_Msi_SpecUnicodeChar; }
  bool AreMsiChars() const
  {
    for (unsigned i = 0; i < kNameSizeMax; i += 2)
    {
      unsigned c = Get16(Name + i);
      if (c == 0)
        break;
      c -= k_Msi_StartUnicodeChar;
      if (c <= k_Msi_UnicodeRange)
        return true;
    }
    return false;
  }
  bool Parse(const Byte *p, bool mode64bit);
};


static const UInt32 k_Ref_Parent_Root = 0xffffffff;

struct CRef
{
  UInt32 Parent;  // index in  Refs[]
  UInt32 Did;     // index in  Items[]
};


class CDatabase
{
public:
  CRecordVector<CRef> Refs;
  CObjectVector<CItem> Items;
  CObjArray<UInt32> Fat;
  CObjArray<UInt32> Mat;
  CObjArray<UInt32> MiniSids;

  UInt32 FatSize;
  UInt32 MatSize;
  UInt32 NumSectors_in_MiniStream;

  // UInt32 LongStreamMinSize;
  unsigned SectorSizeBits;

  Int32 MainSubfile;
  EArcType Type;

  bool IsArc;
  bool HeadersError;
  // bool IsMsi;

  UInt64 PhySize;
  UInt64 PhySize_Unaligned;
  // UInt64 FreeSize;

  IArchiveOpenCallback *OpenCallback;
  UInt32 Callback_Cur;

private:
  /*
  HRESULT IncreaseOpenTotal(UInt32 numSects)
  {
    if (!OpenCallback)
      return S_OK;
    const UInt64 total = (UInt64)(Callback_Cur + numSects) << SectorSizeBits;
    return OpenCallback->SetTotal(NULL, &total);
  }
  */
  HRESULT AddNodes();
  HRESULT ReadSector(IInStream *inStream, Byte *buf, UInt32 sid);
  HRESULT ReadIDs(IInStream *inStream, Byte *buf, UInt32 sid, UInt32 *dest);
  HRESULT Check_Item(unsigned index);

public:
  bool IsNotArcType() const
  {
    return
      Type != k_Type_Msi &&
      Type != k_Type_Msp &&
      Type != k_Type_Msm;
  }

  void Clear();
  UString GetItemPath(UInt32 index) const;

  UInt64 GetItemPackSize(UInt64 size) const
  {
    const UInt64 mask = ((UInt32)1 << (IsLargeStream(size) ? SectorSizeBits : k_MiniSectorSizeBits)) - 1;
    return (size + mask) & ~(UInt64)mask;
  }

  HRESULT Open(IInStream *inStream);
};


HRESULT CDatabase::ReadSector(IInStream *inStream, Byte *buf, UInt32 sid)
{
  const unsigned sb = SectorSizeBits;
  RINOK(InStream_SeekSet(inStream, ((UInt64)sid + 1) << sb))
  RINOK(ReadStream_FALSE(inStream, buf, (size_t)1 << sb))
  if (OpenCallback)
  {
    if ((++Callback_Cur & 0xfff) == 0)
    {
      const UInt64 processed = (UInt64)Callback_Cur << sb;
      const UInt64 numFiles = Items.Size();
      RINOK(OpenCallback->SetCompleted(&numFiles, &processed))
    }
  }
  return S_OK;
}

HRESULT CDatabase::ReadIDs(IInStream *inStream, Byte *buf, UInt32 sid, UInt32 *dest)
{
  RINOK(ReadSector(inStream, buf, sid))
  const size_t sectorSize = (size_t)1 << SectorSizeBits;
  for (size_t t = 0; t < sectorSize; t += 4)
    *dest++ = Get32(buf + t);
  return S_OK;
}


Z7_FORCE_INLINE
static void GetFileTimeFromMem(const Byte *p, FILETIME *ft)
{
  ft->dwLowDateTime = Get32(p);
  ft->dwHighDateTime = Get32(p + 4);
}

bool CItem::Parse(const Byte *p, bool mode64bit)
{
  memcpy(Name, p, kNameSizeMax);
  unsigned i;
  for (i = 0; i < kNameSizeMax; i += 2)
    if (*(const UInt16 *)(const void *)(p + i) == 0)
      break;
#if 0 // 1 : for debug : for more strict field check
  {
    for (unsigned k = i; k < kNameSizeMax; k += 2)
      if (*(const UInt16 *)(const void *)(p + k) != 0)
        return false;
  }
#endif
  Type = p[66];
  // DOC: names are limited to 32 UTF-16 code points, including the terminating null character.
  if (!IsEmptyType())
    if (i == kNameSizeMax || i + 2 != Get16(p + 64)) //  NameLength
      return false;
  if (p[67] >= 2)  // Color: 0 (red) or 1 (black)
    return false;
  LeftDid = Get32(p + 68);
  RightDid = Get32(p + 72);
  SonDid = Get32(p + 76);
  // if (Get32(p + 96) == 0) return false; // State / Flags
  GetFileTimeFromMem(p + 100, &CTime);
  GetFileTimeFromMem(p + 108, &MTime);
  Sid = Get32(p + 116);
  Size = Get32(p + 120);
  /* MS DOC: it is recommended that parsers ignore the most
     significant 32 bits of this field in version 3 compound files */
  if (mode64bit)
    Size |= ((UInt64)Get32(p + 124) << 32);
  return true;
}


void CDatabase::Clear()
{
  Type = k_Type_Common;
  MainSubfile = -1;
  IsArc = false;
  HeadersError = false;
  // IsMsi = false;
  PhySize = 0;
  PhySize_Unaligned = 0;
  // FreeSize = 0;
  Callback_Cur = 0;
  // OpenCallback = NULL;

  FatSize = 0;
  MatSize = 0;
  NumSectors_in_MiniStream = 0;
  
  Fat.Free();
  Mat.Free();
  MiniSids.Free();
  Items.Clear();
  Refs.Clear();
}


static const UInt32 kNoDid = 0xffffffff;

HRESULT CDatabase::AddNodes()
{
  UInt32 index = Items[0].SonDid; // Items[0] is root item
  if (index == kNoDid) // no files case
    return S_OK;
  if (index == 0 || index >= Items.Size())
    return S_FALSE;

  CObjArray<UInt32> itemParents(Items.Size());
  CByteArr states(Items.Size());
  memset(itemParents, 0, (size_t)Items.Size() * sizeof(itemParents[0])); // optional
  memset(states, 0, Items.Size());

#if 1 // 0 : for debug
  const UInt32 k_exitParent = 0;
  const UInt32 k_startLevel = 1;
  // we don't show "Root Entry" dir
  states[0] = 3; // we mark root node as processed, also we block any cycle links to root node
  // itemParents[0] = 0xffffffff; // optional / unused value
#else
  // we show item[0] "Root Entry" dir
  const UInt32 k_exitParent = 0xffffffff;
  const UInt32 k_startLevel = 0;
  index = 0;
#endif
 
  UInt32 level = k_startLevel; // directory level
  unsigned state = 0;
  UInt32 parent = k_exitParent; // in Items[], itemParents[], states[]
  UInt32 refParent = k_Ref_Parent_Root; // in Refs[]

  for (;;)
  {
    if (state >= 3)
    {
      // we return to parent node
      if (state != 3)
        return E_FAIL;
      index = parent;
      if (index == k_exitParent)
        break;
      if (index >= Items.Size())
        return E_FAIL; // (index) was checked already
      parent = itemParents[index];
      state = states[index];
      if (state == 0)
        return E_FAIL;
      if (state == 2)
      {
        // we return to parent Dir node
        if (refParent >= Refs.Size())
          return E_FAIL;
        refParent = Refs[refParent].Parent;
        level--;
      }
      continue;
    }

    if (index >= Items.Size())
      return S_FALSE;
    CItem &item = Items[index];
    if (item.IsEmptyType())
      return S_FALSE;
    item.Level = level;
    state++;
    states[index] = (Byte)state; // we mark current (index) node as used node

    UInt32 newIndex;
    if (state != 2)
      newIndex = (state < 2) ? item.LeftDid : item.RightDid;
    else
    {
      CRef ref;
      ref.Parent = refParent;
      ref.Did = index;
      const unsigned refIndex = Refs.Add(ref);
      if (!item.IsDir())
        continue;
      newIndex = item.SonDid;
      if (newIndex != kNoDid)
      {
        level++;
        refParent = refIndex;
      }
    }

    if (newIndex != kNoDid)
    {
      itemParents[index] = parent;
      state = 0;
      parent = index;
      index = newIndex;
      if (index >= Items.Size() || states[index])
        return S_FALSE;
    }
  }
  
  if (level != k_startLevel || refParent != k_Ref_Parent_Root)
    return E_FAIL;
#if 1 // 1 : optional
  // we check that all non-empty items were processed correctly
  FOR_VECTOR(i, Items)
  {
    const unsigned st = states[i];
    if (Items[i].IsEmptyType())
    {
      if (st)
        return E_FAIL;
    }
    else if (st == 3)
      continue;
    else if (st)
      return E_FAIL;
    else
      return S_FALSE; // there is unused directory item
  }
#endif
  return S_OK;
}


static const char k_Msi_Chars[] =
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz._";
static const char k_Msi_SpecChar_Replace = '!';

static bool AreEqualNames(const Byte *rawName, const char *asciiName)
{
  for (;;)
  {
    const unsigned c = Get16(rawName);
    rawName += 2;
    const unsigned c2 = (Byte)*asciiName;
    asciiName++;
    if (c != c2)
      return false;
    if (c2 == 0)
      return true;
  }
}


static void MsiName_To_FileName(const Byte *p, UString &res)
{
  res.Empty();
  for (unsigned i = 0; i < kNameSizeMax; i += 2)
  {
    unsigned c = Get16(p + i);
    if (c == 0)
      break;
    if (c <= k_Msi_SpecUnicodeChar)
    {
      if (c < k_Msi_StartUnicodeChar)
      {
        if (c < 0x20)
        {
          res.Add_Char('[');
          res.Add_UInt32((UInt32)c);
          c = ']';
        }
      }
      else
      {
#if 0 // 1 : for debug
        if (i == 0) res += "{msi}";
#endif
        c -= k_Msi_StartUnicodeChar;
        const unsigned c1 = (unsigned)c >> k_Msi_NumBits;
        if (c1 <= k_Msi_NumChars)
        {
          res.Add_Char(k_Msi_Chars[(unsigned)c & k_Msi_CharMask]);
          if (c1 == k_Msi_NumChars)
            continue;
          c = (Byte)k_Msi_Chars[c1];
        }
        else
          c = k_Msi_SpecChar_Replace;
      }
    }
    res += (wchar_t)c;
  }
}


UString CDatabase::GetItemPath(UInt32 index) const
{
  UString s;
  UString name;
  unsigned level = 0;
  while (index != k_Ref_Parent_Root)
  {
    const CRef &ref = Refs[index];
    const CItem &item = Items[ref.Did];
    if (!s.IsEmpty())
      s.InsertAtFront(WCHAR_PATH_SEPARATOR);
    // if (IsMsi)
    MsiName_To_FileName(item.Name, name);
    // else NonMsiName_To_FileName(item.Name, name);
    NItemName::NormalizeSlashes_in_FileName_for_OsPath(name);
    if (name.IsEmpty())
      name = "[]";
    s.Insert(0, name);
    index = ref.Parent;
#ifdef Z7_COMPOUND_SHOW_DELETED
    if (item.IsLevel_Unused())
    {
      s.Insert(0, L"[DELETED]" WSTRING_PATH_SEPARATOR);
      break;
    }
#endif
    if (item.Level >= k_Long_path_level_limit && level)
    {
      s.Insert(0, L"[LONG_PATH]" WSTRING_PATH_SEPARATOR);
      break;
    }
    level = 1; // level++;
  }
  return s;
}


HRESULT CDatabase::Check_Item(unsigned index)
{
  const CItem &item = Items[index];
  if (item.IsEmptyType() || item.IsStorage())
    return S_OK;
  UInt64 size = item.Size;
  const bool isLargeStream = (index == 0 || IsLargeStream(size));
  if (!isLargeStream)
  {
    const unsigned bsLog = k_MiniSectorSizeBits;
    const UInt32 clusterSize = (UInt32)1 << bsLog;
    const UInt64 numClusters = (size + clusterSize - 1) >> bsLog;
    if (numClusters > MatSize)
      return S_FALSE;
    UInt32 sid = item.Sid;
    if (size != 0)
    {
      for (;; size -= clusterSize)
      {
        if (sid >= MatSize)
          return S_FALSE;
        const unsigned subBits = SectorSizeBits - k_MiniSectorSizeBits;
        const UInt32 fid = sid >> subBits;
        if (fid >= NumSectors_in_MiniStream)
          return false;
        sid = Mat[sid];
        if (size <= clusterSize)
          break;
      }
    }
    if (sid != NFatID::kEndOfChain)
      return S_FALSE;
  }
  else
  {
    const unsigned bsLog = SectorSizeBits;
    const UInt32 clusterSize = (UInt32)1 << bsLog;
    const UInt64 numClusters = (size + clusterSize - 1) >> bsLog;
    if (numClusters > FatSize)
      return S_FALSE;
    UInt32 sid = item.Sid;
    if (size != 0)
    {
      for (;; size -= clusterSize)
      {
        if (sid >= FatSize)
          return S_FALSE;
        const UInt32 sidPrev = sid;
        sid = Fat[sid];
        if (size <= clusterSize)
        {
          const UInt64 phySize = (((UInt64)sidPrev + 1) << SectorSizeBits) + size;
          if (PhySize_Unaligned < phySize)
              PhySize_Unaligned = phySize;
          break;
        }
      }
    }
    if (sid != NFatID::kEndOfChain)
      return S_FALSE;
  }
  return S_OK;
}


HRESULT CDatabase::Open(IInStream *inStream)
{
  const unsigned kHeaderSize = 512;
  UInt32 p32[kHeaderSize / 4];
  RINOK(ReadStream_FALSE(inStream, p32, kHeaderSize))
  const Byte *p = (const Byte *)(const void *)p32;
  if (memcmp(p, kSignature, Z7_ARRAY_SIZE(kSignature)))
    return S_FALSE;
  /*
  if (memcmp(p + 8, k_CLSID_AAF_V3, Z7_ARRAY_SIZE(k_CLSID_AAF_V3)) == 0 ||
      memcmp(p + 8, k_CLSID_AAF_V4, Z7_ARRAY_SIZE(k_CLSID_AAF_V4)) == 0)
  */
  if (Get32(p32 + 4) == 0x342b0e06) // simplified AAF signature check
    Type = k_Type_Aaf;
  if (Get16(p + 0x18) != 0x3e) // minorVer
    return S_FALSE;
  const unsigned ver = Get16(p + 0x1a); // majorVer
  if (ver < 3 || ver > 4)
    return S_FALSE;
  if (Get16(p + 0x1c) != 0xfffe) // Little-endian
    return S_FALSE;
  const unsigned sectorSizeBits = Get16(p + 0x1e);
  if (sectorSizeBits != ver * 3) // (ver == 3 ? 9 : 12)
    return S_FALSE;
  SectorSizeBits = sectorSizeBits;
  if (Get16(p + 0x20) != k_MiniSectorSizeBits)
    return S_FALSE;

  IsArc = true;
  HeadersError = true;

  const bool mode64bit = (sectorSizeBits >= 12); // (ver == 4)
  if (Get16(p + 0x22) || p32[9]) // reserved
    return S_FALSE;

  const UInt32 numDirSectors = Get32(p32 + 10);
  // If (ver==3), the Number of Directory Sectors MUST be zero.
  if (ver != 3 + (unsigned)(numDirSectors != 0))
    return S_FALSE;
  if (numDirSectors > ((1u << (32 - 2)) >> (sectorSizeBits - (7 + 2))))
    return S_FALSE;

  const UInt32 numSectorsForFAT = Get32(p32 + 11); // SAT

  // MSDOC: A 512-byte sector compound file MUST be no greater than 2 GB in size for compatibility reasons.
  // but actual restriction for windows compond creation code can be more strict:
  // (numSectorsForFAT <  (1 << 15)) : actual restriction in win10 for compound creation code
  // (numSectorsForFAT <= (1 << 15)) : relaxed restriction to allow 2 GB files.
  if (sectorSizeBits == 9 &&
      numSectorsForFAT >= (1u << (31 - (9 + 9 - 2)))) // we use most strict check
    return S_FALSE;

  // const UInt32 TransactionSignatureNumber = Get32(p32 + 13);
  if (Get32(p32 + 14) != k_LongStreamMinSize)
    return S_FALSE;

  const unsigned ssb2 = sectorSizeBits - 2;
  const UInt32 numSidsInSec = (UInt32)1 << ssb2;
  const UInt32 numFatItems = numSectorsForFAT << ssb2;
  if (numFatItems == 0 || (numFatItems >> ssb2) != numSectorsForFAT)
    return S_FALSE;

  const size_t sectSize = (size_t)1 << sectorSizeBits;
  CByteArr sect(sectSize);
  CByteArr used(numFatItems);
  // don't change these const values. These values use same order as (0xffffffff - NFatID::const)
  // const Byte k_Used_Free = 0;
  const Byte k_Used_ChainTo = 1;
  const Byte k_Used_FAT     = 2;
  const Byte k_Used_DIFAT   = 3;
  memset(used, 0, numFatItems);
  UInt32 *fat;
  {
    // ========== READ FAT ==========
    const UInt32 numSectorsForBat = Get32(p32 + 18); // master sector allocation table
    const unsigned ssb2_m1 = ssb2 - 1;
    if (numSectorsForBat > ((1u << 30) >> ssb2_m1 >> ssb2_m1))
      return S_FALSE;
    const unsigned kNumHeaderBatItems = 109;
    UInt32 numBatItems = kNumHeaderBatItems + (numSectorsForBat << ssb2); // real size can be smaller
    CObjArray<UInt32> bat(numBatItems);
    size_t i;
    for (i = 0; i < kNumHeaderBatItems; i++)
      bat[i] = Get32(p32 + 19 + i);
    {
      UInt32 sid = Get32(p32 + 17);
      for (UInt32 s = 0; s < numSectorsForBat; s++)
      {
        if (sid >= numFatItems || used[sid])
          return S_FALSE;
        used[sid] = k_Used_DIFAT;
        RINOK(ReadIDs(inStream, sect, sid, bat + i))
        i += numSidsInSec - 1;
        sid = bat[i];
      }
      if (sid != NFatID::kEndOfChain // NFatID::kEndOfChain is expected value for most files
         && sid != NFatID::kFree)    // NFatID::kFree is used in some AAF files
        return S_FALSE;
    }
    numBatItems = (UInt32)i; // corrected value
    if (numSectorsForFAT > numBatItems)
      return S_FALSE;
    for (i = numSectorsForFAT; i < numBatItems; i++)
      if (bat[i] != NFatID::kFree)
        return S_FALSE;

    // RINOK(IncreaseOpenTotal(numSectorsForFAT + numDirSectors))

    Fat.Alloc(numFatItems);
    fat = Fat;
    for (i = 0; i < numSectorsForFAT; i++)
    {
      const UInt32 sectorIndex = bat[i];
      if (sectorIndex >= numFatItems)
        return S_FALSE;
      if (used[sectorIndex])
        return S_FALSE;
      used[sectorIndex] = k_Used_FAT;
      UInt32 *fat2 = fat + ((size_t)i << ssb2);
      RINOK(ReadIDs(inStream, sect, sectorIndex, fat2))
      for (size_t k = 0; k < numSidsInSec; k++)
      {
        const UInt32 sid = fat2[k];
        if (sid > NFatID::kMaxValue)
        {
          if (sid == NFatID::k_DIF_SECT
            && used[((size_t)i << ssb2) + k] != k_Used_DIFAT)
              return S_FALSE;
          continue;
        }
        if (sid >= numFatItems || used[sid])
          return S_FALSE; // strict error check
        used[sid] = k_Used_ChainTo;
      }
    }
    {
      for (i = 0; i < numSectorsForFAT; i++)
        if (fat[bat[i]] != NFatID::kFatSector)
          return S_FALSE;
    }
    FatSize = numFatItems;
  }

  {
    size_t i = numFatItems;
    do
      if (fat[i - 1] != NFatID::kFree)
        break;
    while (--i);
    PhySize = ((UInt64)i + 1) << sectorSizeBits;
    /*
    if (i)
    {
      const UInt32 *lim = fat + i;
      UInt32 num = 0;
      do
        if (*fat++ == NFatID::kFree)
          num++;
      while (fat != lim);
      FreeSize = num << sectorSizeBits;
    }
    */
  }

  UInt32 numMatItems;
  {
    // ========== READ MAT ==========
    const UInt32 numSectorsForMat = Get32(p32 + 16);
    numMatItems = (UInt32)numSectorsForMat << ssb2;
    if ((numMatItems >> ssb2) != numSectorsForMat)
      return S_FALSE;
    Mat.Alloc(numMatItems);
    UInt32 sid = Get32(p32 + 15); // short-sector table SID
    if (numMatItems)
    {
      if (sid >= numFatItems || used[sid])
        return S_FALSE;
      used[sid] = k_Used_ChainTo;
    }
    for (UInt32 i = 0; i < numMatItems; i += numSidsInSec)
    {
      if (sid >= numFatItems)
        return S_FALSE;
      RINOK(ReadIDs(inStream, sect, sid, Mat + i))
      sid = fat[sid];
    }
    if (sid != NFatID::kEndOfChain)
      return S_FALSE;
  }

  {
    // ========== READ DIR ITEMS ==========
    UInt32 sid = Get32(p32 + 12); // directory stream SID
    UInt32 numDirSectors_Processed = 0;
    if (sid >= numFatItems || used[sid])
      return S_FALSE;
    used[sid] = k_Used_ChainTo;
    do
    {
      // we need to check sid here becase kEndOfChain sid < numFatItems is required
      if (sid >= numFatItems)
        return S_FALSE;
      if (numDirSectors && numDirSectors_Processed >= numDirSectors)
        return S_FALSE;
      numDirSectors_Processed++;
      RINOK(ReadSector(inStream, sect, sid))
      for (size_t i = 0; i < sectSize; i += (1 << 7))
      {
        CItem item;
        item.Level = k_Item_Level_Unused;
        if (!item.Parse(sect + i, mode64bit))
          return S_FALSE;
        // we use (item.Size) check here.
        // so we don't need additional overflow checks for (item.Size +) in another code
        if ((UInt32)(item.Size >> 32) >= sectSize) // it's because FAT size is limited by (1 << 32) items.
          return S_FALSE;

        if (Items.IsEmpty())
        {
          if (item.Type != NItemType::kRootStorage
              || item.LeftDid != kNoDid
              || item.RightDid != kNoDid
              || item.SonDid == 0)
            return S_FALSE;
          if (item.Sid != NFatID::kEndOfChain)
          {
            if (item.Sid >= numFatItems || used[item.Sid])
              return S_FALSE;
            used[item.Sid] = k_Used_ChainTo;
          }
        }
        else if (item.IsStorage())
        {
          if (item.Size != 0) // by specification
            return S_FALSE;
          if (item.Sid != 0   // by specification
              && item.Sid != NFatID::kFree) // NFatID::kFree is used in some AAF files
            return S_FALSE;
        }
        // else if (item.Type == NItemType::kRootStorage) return S_FALSE;
        else if (item.IsEmptyType())
        {
          // kNoDid is expected in *Did fileds, but rare case MSI contains zero in all fields
          if ((item.Sid != 0  // expected value
                && item.Sid != NFatID::kFree  // NFatID::kFree is used in some AAF files
                && item.Sid != NFatID::kEndOfChain) // used by some MSI file
              || (item.LeftDid  != kNoDid && item.LeftDid)
              || (item.RightDid != kNoDid && item.RightDid)
              || (item.SonDid   != kNoDid && item.SonDid)
            // || item.Size != 0         // the check is disabled because some MSI file contains non zero
            // || Get16(item.Name) != 0  // the check is disabled because some MSI file contains some name
              )
            return S_FALSE;
        }
        else
        {
          if (item.Type != NItemType::kStream)
            return S_FALSE;
          // NItemType::kStream case
          if (item.SonDid != kNoDid) // optional check
            return S_FALSE;
          if (item.Size == 0)
          {
            if (item.Sid != NFatID::kEndOfChain)
              return S_FALSE;
          }
          else if (IsLargeStream(item.Size))
          {
            if (item.Sid >= numFatItems || used[item.Sid])
              return S_FALSE;
            used[item.Sid] = k_Used_ChainTo;
          }
        }

        Items.Add(item);
      }
      sid = fat[sid];
    }
    while (sid != NFatID::kEndOfChain);
  }

  {
    // root stream contains all data that stored with mini Sectors
    const CItem &root = Items[0];
    UInt32 numSectorsInMiniStream;
    {
      const UInt64 numSatSects64 = (root.Size + sectSize - 1) >> sectorSizeBits;
      if (numSatSects64 > NFatID::kMaxValue + 1)
        return S_FALSE;
      numSectorsInMiniStream = (UInt32)numSatSects64;
    }
    {
      const UInt64 matSize64 = (root.Size + (1 << k_MiniSectorSizeBits) - 1) >> k_MiniSectorSizeBits;
      if (matSize64 > numMatItems)
        return S_FALSE;
      MatSize = (UInt32)matSize64;
    }
    MiniSids.Alloc(numSectorsInMiniStream);
    UInt32 * const miniSids = MiniSids;
    UInt32 sid = root.Sid;
    for (UInt32 i = 0; ; i++)
    {
      if (sid == NFatID::kEndOfChain)
      {
        if (i != numSectorsInMiniStream)
          return S_FALSE;
        break;
      }
      if (i >= numSectorsInMiniStream)
        return S_FALSE;
      if (sid >= numFatItems)
        return S_FALSE;
      miniSids[i] = sid;
      sid = fat[sid];
    }
    NumSectors_in_MiniStream = numSectorsInMiniStream;
  }


  {
/*
  MS DOCs:
    The range lock sector covers file offsets 0x7FFFFF00-0x7FFFFFFF.
    These offsets are reserved for byte-range locking to support
    concurrency, transactions, and other compound file features.
    The range lock sector MUST be allocated in the FAT and marked with
    ENDOFCHAIN (0xFFFFFFFE), when the compound file grows beyond 2 GB.
    If the compound file is greater than 2 GB and then shrinks to below 2 GB,
    the range lock sector SHOULD be marked as FREESECT (0xFFFFFFFF) in the FAT.
*/
    {
      const UInt32 lockSector = (0x7fffffff >> sectorSizeBits) - 1;
      if (lockSector < numFatItems)
      {
        if (used[lockSector])
          return S_FALSE;
        const UInt32 f = fat[lockSector];
        if (f == NFatID::kEndOfChain)
          used[lockSector] = k_Used_ChainTo; // we use fake state to pass the check in loop below
        else if (f != NFatID::kFree)
          return S_FALSE;
      }
    }
    for (size_t i = 0; i < numFatItems; i++)
    {
      UInt32 f = fat[i];
      const UInt32 u = ~(UInt32)used[i]; // (0xffffffff - used[i])
      if (f < NFatID::kMaxValue + 1)
        f = NFatID::kEndOfChain;
      if (f != u)
        return S_FALSE;
    }
  }

  {
    // Don't move that code up, becase Check_Item uses Mat[] array.
    FOR_VECTOR(t, Items)
    {
      RINOK(Check_Item(t))
    }
  }

  RINOK(AddNodes())

  {
    // some msi (in rare cases) have unaligned size of archive,
    // unaligned size of compond files is also possible if we create just one stream
    // where there is no padding data after payload data in last cluster of archive
    UInt64 fileSize;
    RINOK(InStream_GetSize_SeekToEnd(inStream, fileSize))
    if (   fileSize < PhySize
        && fileSize > PhySize - sectSize
        && fileSize >= PhySize_Unaligned
        && PhySize_Unaligned > PhySize - sectSize)
      PhySize = PhySize_Unaligned;
  }
    
  bool isMsi = false;
  {
    FOR_VECTOR (i, Refs)
    {
      const CItem &item = Items[Refs[i].Did];
      if (item.IsDir())
        continue;
      if (item.AreMsiChars())
      // if (item.IsSpecMsiName())
      {
        isMsi = true;
        break;
      }
    }
  }

  // IsMsi = isMsi;
  if (isMsi)
  {
    unsigned numCabs = 0;
    UString name;
    FOR_VECTOR (i, Refs)
    {
      const CItem &item = Items[Refs[i].Did];
      if (item.IsDir() /* || item.IsSpecMsiName() */)
        continue;
      MsiName_To_FileName(item.Name, name);
      if ( (name.Len() >= 4 && StringsAreEqualNoCase_Ascii(name.RightPtr(4), ".cab"))
        || (name.Len() >= 3 && StringsAreEqualNoCase_Ascii(name.RightPtr(3), "exe"))
        )
      {
        numCabs++;
        if (numCabs > 1)
        {
          MainSubfile = -1;
          break;
        }
        MainSubfile = (int)i;
      }
    }
  }

  if (isMsi) // we provide msi priority over AAF
    Type = k_Type_Msi;
  if (Type != k_Type_Aaf)
  {
    FOR_VECTOR (i, Refs)
    {
      const CItem &item = Items[Refs[i].Did];
      if (item.IsDir())
        continue;
      const Byte *name = item.Name;
      // if (IsMsiName(name))
      if (isMsi)
      {
        if (memcmp(name, k_Sequence_msp, sizeof(k_Sequence_msp)) == 0)
        {
          Type = k_Type_Msp;
          break;
        }
        if (memcmp(name, k_Sequence_msm, sizeof(k_Sequence_msm)) == 0)
        {
          Type = k_Type_Msm;
          break;
        }
      }
      else
      {
        if (AreEqualNames(name, "WordDocument"))
        {
          Type = k_Type_Doc;
          break;
        }
        if (AreEqualNames(name, "PowerPoint Document"))
        {
          Type = k_Type_Ppt;
          break;
        }
        if (AreEqualNames(name, "Workbook"))
        {
          Type = k_Type_Xls;
          break;
        }
      }
    }
  }

#ifdef Z7_COMPOUND_SHOW_DELETED
  {
    // we skip Items[0] that is root item
    for (unsigned t = 1; t < Items.Size(); t++)
    {
      const CItem &item = Items[t];
      if (
#if 1 // 0 for debug to show empty files
          item.IsEmptyType() ||
#endif
          !item.IsLevel_Unused())
        continue;
      CRef ref;
      ref.Parent = k_Ref_Parent_Root;
      ref.Did = t;
      Refs.Add(ref);
    }
  }
#endif

  HeadersError = false;
  return S_OK;
}



Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  CMyComPtr<IInStream> _stream;
  CDatabase _db;
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidCTime,
  kpidMTime
  // , kpidCharacts // for debug
};

static const Byte kArcProps[] =
{
  kpidExtension,
  kpidClusterSize
  // , kpidSectorSize
  // , kpidFreeSpace
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidExtension: prop = kExtensions[(unsigned)_db.Type]; break;
    case kpidPhySize: prop = _db.PhySize; break;
    case kpidClusterSize: prop = (UInt32)1 << _db.SectorSizeBits; break;
    // case kpidSectorSize: prop = (UInt32)1 << _db.MiniSectorSizeBits; break;
    case kpidMainSubfile: if (_db.MainSubfile >= 0) prop = (UInt32)_db.MainSubfile; break;
    // case kpidFreeSpace: prop = _db.FreeSize; break;
    case kpidIsNotArcType: if (_db.IsNotArcType()) prop = true; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_db.IsArc)
        v |= kpv_ErrorFlags_IsNotArc;
      if (_db.HeadersError)
        v |= kpv_ErrorFlags_HeadersError;
      prop = v;
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
  const CRef &ref = _db.Refs[index];
  const CItem &item = _db.Items[ref.Did];
    
  switch (propID)
  {
    case kpidPath:  prop = _db.GetItemPath(index); break;
    case kpidIsDir:  prop = item.IsDir(); break;
    case kpidCTime:  prop = item.CTime; break;
    case kpidMTime:  prop = item.MTime; break;
    case kpidPackSize:  if (!item.IsDir()) prop = _db.GetItemPackSize(item.Size); break;
    case kpidSize:  if (!item.IsDir()) prop = item.Size; break;
    // case kpidCharacts:  prop = item.Level; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *openArchiveCallback))
{
  COM_TRY_BEGIN
  Close();
  _db.OpenCallback = openArchiveCallback;
  // try
  {
    RINOK(_db.Open(inStream))
    _stream = inStream;
  }
  // catch(...) { return S_FALSE; }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _db.Clear();
  _stream.Release();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _db.Refs.Size();
  if (numItems == 0)
    return S_OK;
  UInt32 i;
  UInt64 totalSize = 0;
  for (i = 0; i < numItems; i++)
  {
    const CItem &item = _db.Items[_db.Refs[allFilesMode ? i : indices[i]].Did];
    if (!item.IsDir())
      totalSize += item.Size;
  }
  RINOK(extractCallback->SetTotal(totalSize))

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);

  for (i = 0;; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;

    const UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item = _db.Items[_db.Refs[index].Did];
    Int32 res;
    {
      CMyComPtr<ISequentialOutStream> outStream;
      const Int32 askMode = testMode ?
          NExtract::NAskMode::kTest :
          NExtract::NAskMode::kExtract;
      RINOK(extractCallback->GetStream(index, &outStream, askMode))
        
      if (item.IsDir())
      {
        RINOK(extractCallback->PrepareOperation(askMode))
        RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
        continue;
      }
      
      totalPackSize += _db.GetItemPackSize(item.Size);
      totalSize += item.Size;
      
      if (!testMode && !outStream)
        continue;
      RINOK(extractCallback->PrepareOperation(askMode))
      res = NExtract::NOperationResult::kDataError;
      CMyComPtr<ISequentialInStream> inStream;
      const HRESULT hres = GetStream(index, &inStream);
      if (hres == S_FALSE)
        res = NExtract::NOperationResult::kDataError;
      /*
      else if (hres == E_NOTIMPL)
        res = NExtract::NOperationResult::kUnsupportedMethod;
      */
      else
      {
        RINOK(hres)
        if (inStream)
        {
          RINOK(copyCoder.Interface()->Code(inStream, outStream, NULL, NULL, lps))
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
  *numItems = _db.Refs.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  const UInt32 itemIndex = _db.Refs[index].Did;
  const CItem &item = _db.Items[itemIndex];
  if (item.IsDir())
    return S_FALSE;
  const bool isLargeStream = (itemIndex == 0 || IsLargeStream(item.Size));
  if (!isLargeStream)
  {
    CBufferInStream *streamSpec = new CBufferInStream;
    CMyComPtr<IInStream> streamTemp = streamSpec;

    UInt32 size = (UInt32)item.Size;
    streamSpec->Buf.Alloc(size);
    streamSpec->Init();

    UInt32 sid = item.Sid;
    Byte *dest = streamSpec->Buf;

    UInt64 phyPos = 0;
    while (size)
    {
      if (sid >= _db.MatSize)
        return S_FALSE;
      const unsigned subBits = _db.SectorSizeBits - k_MiniSectorSizeBits;
      const UInt32 fid = sid >> subBits;
      if (fid >= _db.NumSectors_in_MiniStream)
        return false;
      const UInt64 offset = (((UInt64)_db.MiniSids[fid] + 1) << _db.SectorSizeBits) +
          ((sid & ((1u << subBits) - 1)) << k_MiniSectorSizeBits);
      if (phyPos != offset)
      {
        RINOK(InStream_SeekSet(_stream, offset))
        phyPos = offset;
      }
      UInt32 readSize = (UInt32)1 << k_MiniSectorSizeBits;
      if (readSize > size)
          readSize = size;
      RINOK(ReadStream_FALSE(_stream, dest, readSize))
      phyPos += readSize;
      dest += readSize;
      sid = _db.Mat[sid];
      size -= readSize;
    }
    if (sid != NFatID::kEndOfChain)
      return S_FALSE;
    *stream = streamTemp.Detach();
    return S_OK;
  }

  CClusterInStream *streamSpec = new CClusterInStream;
  CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
  streamSpec->Stream = _stream;
  streamSpec->StartOffset = 0;
  const unsigned bsLog = _db.SectorSizeBits;
  streamSpec->BlockSizeLog = bsLog;
  streamSpec->Size = item.Size;

  const UInt32 clusterSize = (UInt32)1 << bsLog;
  const UInt64 numClusters64 = (item.Size + clusterSize - 1) >> bsLog;
  if (numClusters64 > _db.FatSize)
    return S_FALSE;
  streamSpec->Vector.ClearAndReserve((unsigned)numClusters64);
  UInt32 sid = item.Sid;
  UInt64 size = item.Size;

  if (size != 0)
  {
    for (;; size -= clusterSize)
    {
      if (sid >= _db.FatSize)
        return S_FALSE;
      streamSpec->Vector.AddInReserved(sid + 1);
      sid = _db.Fat[sid];
      if (size <= clusterSize)
        break;
    }
  }
  if (sid != NFatID::kEndOfChain)
    return S_FALSE;
  RINOK(streamSpec->InitAndSeek())
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I(
  "Compound", "msi msp msm doc xls ppt aaf", NULL, 0xe5,
  kSignature,
  0,
  0,
  NULL)

}}
