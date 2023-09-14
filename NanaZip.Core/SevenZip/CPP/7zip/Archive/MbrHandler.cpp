// MbrHandler.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#endif

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#ifdef SHOW_DEBUG_INFO
#define PRF(x) x
#else
#define PRF(x)
#endif

using namespace NWindows;

namespace NArchive {
namespace NMbr {

struct CChs
{
  Byte Head;
  Byte SectCyl;
  Byte Cyl8;
  
  UInt32 GetSector() const { return SectCyl & 0x3F; }
  UInt32 GetCyl() const { return ((UInt32)SectCyl >> 6 << 8) | Cyl8; }
  void ToString(NCOM::CPropVariant &prop) const;

  void Parse(const Byte *p)
  {
    Head = p[0];
    SectCyl = p[1];
    Cyl8 = p[2];
  }
  bool Check() const { return GetSector() > 0; }
};


// Chs in some MBRs contains only low bits of "Cyl number". So we disable check.
/*
#define RINOZ(x) { int _t_ = (x); if (_t_ != 0) return _t_; }
static int CompareChs(const CChs &c1, const CChs &c2)
{
  RINOZ(MyCompare(c1.GetCyl(), c2.GetCyl()));
  RINOZ(MyCompare(c1.Head, c2.Head));
  return MyCompare(c1.GetSector(), c2.GetSector());
}
*/

void CChs::ToString(NCOM::CPropVariant &prop) const
{
  AString s;
  s.Add_UInt32(GetCyl());
  s.Add_Minus();
  s.Add_UInt32(Head);
  s.Add_Minus();
  s.Add_UInt32(GetSector());
  prop = s;
}

struct CPartition
{
  Byte Status;
  CChs BeginChs;
  Byte Type;
  CChs EndChs;
  UInt32 Lba;
  UInt32 NumBlocks;

  CPartition() { memset (this, 0, sizeof(*this)); }
  
  bool IsEmpty() const { return Type == 0; }
  bool IsExtended() const { return Type == 5 || Type == 0xF; }
  UInt32 GetLimit() const { return Lba + NumBlocks; }
  // bool IsActive() const { return Status == 0x80; }
  UInt64 GetPos() const { return (UInt64)Lba * 512; }
  UInt64 GetSize() const { return (UInt64)NumBlocks * 512; }

  bool CheckLbaLimits() const { return (UInt32)0xFFFFFFFF - Lba >= NumBlocks; }
  bool Parse(const Byte *p)
  {
    Status = p[0];
    BeginChs.Parse(p + 1);
    Type = p[4];
    EndChs.Parse(p + 5);
    Lba = GetUi32(p + 8);
    NumBlocks = GetUi32(p + 12);
    if (Type == 0)
      return true;
    if (Status != 0 && Status != 0x80)
      return false;
    return BeginChs.Check()
       && EndChs.Check()
       // && CompareChs(BeginChs, EndChs) <= 0
       && NumBlocks > 0
       && CheckLbaLimits();
  }

  #ifdef SHOW_DEBUG_INFO
  void Print() const
  {
    NCOM::CPropVariant prop, prop2;
    BeginChs.ToString(prop);
    EndChs.ToString(prop2);
    printf("   %2x %2x %8X %8X %12S %12S", (int)Status, (int)Type, Lba, NumBlocks, prop.bstrVal, prop2.bstrVal);
  }
  #endif
};

struct CPartType
{
  UInt32 Id;
  const char *Ext;
  const char *Name;
};

#define kFat "fat"

/*
if we use "format" command in Windows 10 for existing partition:
  - if we format to ExFAT, it sets type=7
  - if we format to UDF, it doesn't change type from previous value.
*/
  
static const unsigned kType_Windows_NTFS = 7;

static const CPartType kPartTypes[] =
{
  { 0x01, kFat, "FAT12" },
  { 0x04, kFat, "FAT16 DOS 3.0+" },
  { 0x05, NULL, "Extended" },
  { 0x06, kFat, "FAT16 DOS 3.31+" },
  { 0x07, "ntfs", "NTFS" },
  { 0x0B, kFat, "FAT32" },
  { 0x0C, kFat, "FAT32-LBA" },
  { 0x0E, kFat, "FAT16-LBA" },
  { 0x0F, NULL, "Extended-LBA" },
  { 0x11, kFat, "FAT12-Hidden" },
  { 0x14, kFat, "FAT16-Hidden < 32 MB" },
  { 0x16, kFat, "FAT16-Hidden >= 32 MB" },
  { 0x1B, kFat, "FAT32-Hidden" },
  { 0x1C, kFat, "FAT32-LBA-Hidden" },
  { 0x1E, kFat, "FAT16-LBA-WIN95-Hidden" },
  { 0x27, "ntfs", "NTFS-WinRE" },
  { 0x82, NULL, "Solaris x86 / Linux swap" },
  { 0x83, NULL, "Linux" },
  { 0x8E, "lvm", "Linux LVM" },
  { 0xA5, NULL, "BSD slice" },
  { 0xBE, NULL, "Solaris 8 boot" },
  { 0xBF, NULL, "New Solaris x86" },
  { 0xC2, NULL, "Linux-Hidden" },
  { 0xC3, NULL, "Linux swap-Hidden" },
  { 0xEE, NULL, "GPT" },
  { 0xEE, NULL, "EFI" }
};

static int FindPartType(UInt32 type)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kPartTypes); i++)
    if (kPartTypes[i].Id == type)
      return (int)i;
  return -1;
}

struct CItem
{
  bool IsReal;
  bool IsPrim;
  bool WasParsed;
  const char *FileSystem;
  UInt64 Size;
  CPartition Part;

  CItem():
      WasParsed(false),
      FileSystem(NULL)
      {}
};


Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  CObjectVector<CItem> _items;
  UInt64 _totalSize;
  CByteBuffer _buffer;

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const Z7_override Z7_final
  {
    const CItem &item = _items[index];
    pos = item.Part.GetPos();
    size = item.Size;
    return NExtract::NOperationResult::kOK;
  }

  HRESULT ReadTables(IInStream *stream, UInt32 baseLba, UInt32 lba, unsigned level);
};

HRESULT CHandler::ReadTables(IInStream *stream, UInt32 baseLba, UInt32 lba, unsigned level)
{
  if (level >= 128 || _items.Size() >= 128)
    return S_FALSE;

  const unsigned kNumHeaderParts = 4;
  CPartition parts[kNumHeaderParts];

  {
    const UInt32 kSectorSize = 512;
    _buffer.Alloc(kSectorSize);
    Byte *buf = _buffer;
    UInt64 newPos = (UInt64)lba << 9;
    if (newPos + 512 > _totalSize)
      return S_FALSE;
    RINOK(InStream_SeekSet(stream, newPos))
    RINOK(ReadStream_FALSE(stream, buf, kSectorSize))
    
    if (buf[0x1FE] != 0x55 || buf[0x1FF] != 0xAA)
      return S_FALSE;
    
    for (unsigned i = 0; i < kNumHeaderParts; i++)
      if (!parts[i].Parse(buf + 0x1BE + 16 * i))
        return S_FALSE;
  }

  PRF(printf("\n# %8X", lba));

  UInt32 limLba = lba + 1;
  if (limLba == 0)
    return S_FALSE;

  for (unsigned i = 0; i < kNumHeaderParts; i++)
  {
    CPartition &part = parts[i];
    
    if (part.IsEmpty())
      continue;
    PRF(printf("\n   %2d ", (unsigned)level));
    #ifdef SHOW_DEBUG_INFO
    part.Print();
    #endif
    
    unsigned numItems = _items.Size();
    UInt32 newLba = lba + part.Lba;
    
    if (part.IsExtended())
    {
      // if (part.Type == 5) // Check it!
      newLba = baseLba + part.Lba;
      if (newLba < limLba)
        return S_FALSE;
      HRESULT res = ReadTables(stream, level < 1 ? newLba : baseLba, newLba, level + 1);
      if (res != S_FALSE && res != S_OK)
        return res;
    }
    if (newLba < limLba)
      return S_FALSE;
    part.Lba = newLba;
    if (!part.CheckLbaLimits())
      return S_FALSE;

    CItem n;
    n.Part = part;
    bool addItem = false;
    if (numItems == _items.Size())
    {
      n.IsPrim = (level == 0);
      n.IsReal = true;
      addItem = true;
    }
    else
    {
      const CItem &back = _items.Back();
      UInt32 backLimit = back.Part.GetLimit();
      UInt32 partLimit = part.GetLimit();
      if (backLimit < partLimit)
      {
        n.IsReal = false;
        n.Part.Lba = backLimit;
        n.Part.NumBlocks = partLimit - backLimit;
        addItem = true;
      }
    }
    if (addItem)
    {
      if (n.Part.GetLimit() < limLba)
        return S_FALSE;
      limLba = n.Part.GetLimit();
      n.Size = n.Part.GetSize();
      _items.Add(n);
    }
  }
  return S_OK;
}


static bool Is_Ntfs(const Byte *p)
{
  if (p[0x1FE] != 0x55 || p[0x1FF] != 0xAA)
    return false;
  // int codeOffset = 0;
  switch (p[0])
  {
    case 0xE9: /* codeOffset = 3 + (Int16)Get16(p + 1); */ break;
    case 0xEB: if (p[2] != 0x90) return false; /* codeOffset = 2 + (int)(signed char)p[1]; */ break;
    default: return false;
  }
  return memcmp(p + 3, "NTFS    ", 8) == 0;
}

static bool Is_ExFat(const Byte *p)
{
  if (p[0x1FE] != 0x55 || p[0x1FF] != 0xAA)
    return false;
  if (p[0] != 0xEB || p[1] != 0x76 || p[2] != 0x90)
    return false;
  return memcmp(p + 3, "EXFAT   ", 8) == 0;
}

static bool AllAreZeros(const Byte *p, size_t size)
{
  for (size_t i = 0; i < size; i++)
    if (p[i] != 0)
      return false;
  return true;
}

static const UInt32 k_Udf_StartPos = 0x8000;
static const Byte k_Udf_Signature[] = { 0, 'B', 'E', 'A', '0', '1', 1, 0 };

static bool Is_Udf(const Byte *p)
{
  return memcmp(p + k_Udf_StartPos, k_Udf_Signature, sizeof(k_Udf_Signature)) == 0;
}


static const char *GetFileSystem(
    ISequentialInStream *stream, UInt64 partitionSize)
{
  const size_t kHeaderSize = 1 << 9;
  if (partitionSize >= kHeaderSize)
  {
    Byte buf[kHeaderSize];
    if (ReadStream_FAIL(stream, buf, kHeaderSize) == S_OK)
    {
      // NTFS is expected default filesystem for (Type == 7)
      if (Is_Ntfs(buf))
        return "NTFS";
      if (Is_ExFat(buf))
        return "exFAT";
      const size_t kHeaderSize2 = k_Udf_StartPos + (1 << 9);
      if (partitionSize >= kHeaderSize2)
      {
        if (AllAreZeros(buf, kHeaderSize))
        {
          CByteBuffer buffer(kHeaderSize2);
          // memcpy(buffer, buf, kHeaderSize);
          if (ReadStream_FAIL(stream, buffer + kHeaderSize, kHeaderSize2 - kHeaderSize) == S_OK)
            if (Is_Udf(buffer))
              return "UDF";
        }
      }
    }
  }
  return NULL;
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  RINOK(InStream_GetSize_SeekToEnd(stream, _totalSize))
  RINOK(ReadTables(stream, 0, 0, 0))
  if (_items.IsEmpty())
    return S_FALSE;
  UInt32 lbaLimit = _items.Back().Part.GetLimit();
  UInt64 lim = (UInt64)lbaLimit << 9;
  if (lim < _totalSize)
  {
    CItem n;
    n.Part.Lba = lbaLimit;
    n.Size = _totalSize - lim;
    n.IsReal = false;
    _items.Add(n);
  }

  FOR_VECTOR (i, _items)
  {
    CItem &item = _items[i];
    if (item.Part.Type != kType_Windows_NTFS)
      continue;
    if (InStream_SeekSet(stream, item.Part.GetPos()) != S_OK)
      continue;
    item.FileSystem = GetFileSystem(stream, item.Size);
    item.WasParsed = true;
  }

  _stream = stream;
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _totalSize = 0;
  _items.Clear();
  _stream.Release();
  return S_OK;
}


enum
{
  kpidPrimary = kpidUserDefined,
  kpidBegChs,
  kpidEndChs
};

static const CStatProp kProps[] =
{
  { NULL, kpidPath, VT_BSTR},
  { NULL, kpidSize, VT_UI8},
  { NULL, kpidFileSystem, VT_BSTR},
  { NULL, kpidOffset, VT_UI8},
  { "Primary", kpidPrimary, VT_BOOL},
  { "Begin CHS", kpidBegChs, VT_BSTR},
  { "End CHS", kpidEndChs, VT_BSTR}
};

IMP_IInArchive_Props_WITH_NAME
IMP_IInArchive_ArcProps_NO_Table

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile:
    {
      int mainIndex = -1;
      FOR_VECTOR (i, _items)
        if (_items[i].IsReal)
        {
          if (mainIndex >= 0)
          {
            mainIndex = -1;
            break;
          }
          mainIndex = (int)i;
        }
      if (mainIndex >= 0)
        prop = (UInt32)(Int32)mainIndex;
      break;
    }
    case kpidPhySize: prop = _totalSize; break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  const CItem &item = _items[index];
  const CPartition &part = item.Part;
  switch (propID)
  {
    case kpidPath:
    {
      AString s;
      s.Add_UInt32(index);
      if (item.IsReal)
      {
        s.Add_Dot();
        const char *ext = NULL;
        if (item.FileSystem)
        {
          ext = "";
          AString fs (item.FileSystem);
          fs.MakeLower_Ascii();
          s += fs;
        }
        else if (!item.WasParsed)
        {
          const int typeIndex = FindPartType(part.Type);
          if (typeIndex >= 0)
            ext = kPartTypes[(unsigned)typeIndex].Ext;
        }
        if (!ext)
          ext = "img";
        s += ext;
      }
      prop = s;
      break;
    }
    case kpidFileSystem:
      if (item.IsReal)
      {
        char s[32];
        ConvertUInt32ToString(part.Type, s);
        const char *res = s;
        if (item.FileSystem)
        {
          // strcat(s, "-");
          // strcpy(s, item.FileSystem);
          res = item.FileSystem;
        }
        else if (!item.WasParsed)
        {
          const int typeIndex = FindPartType(part.Type);
          if (typeIndex >= 0 && kPartTypes[(unsigned)typeIndex].Name)
            res = kPartTypes[(unsigned)typeIndex].Name;
        }
        prop = res;
      }
      break;
    case kpidSize:
    case kpidPackSize: prop = item.Size; break;
    case kpidOffset: prop = part.GetPos(); break;
    case kpidPrimary: if (item.IsReal) prop = item.IsPrim; break;
    case kpidBegChs: if (item.IsReal) part.BeginChs.ToString(prop); break;
    case kpidEndChs: if (item.IsReal) part.EndChs.ToString(prop); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


  // 3, { 1, 1, 0 },
  // 2, { 0x55, 0x1FF },

REGISTER_ARC_I_NO_SIG(
  "MBR", "mbr", NULL, 0xDB,
  0,
  NArcInfoFlags::kPureStartOpen,
  NULL)

}}
