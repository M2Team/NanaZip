// GptHandler.cpp

#include "StdAfx.h"

#include "../../../C/7zCrc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

using namespace NWindows;

namespace NArchive {

namespace NMbr {
const char *GetFileSystem(ISequentialInStream *stream, UInt64 partitionSize);
}

namespace NFat {
API_FUNC_IsArc IsArc_Fat(const Byte *p, size_t size);
}

namespace NGpt {

static const unsigned k_SignatureSize = 12;
static const Byte k_Signature[k_SignatureSize] =
    { 'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T', 0, 0, 1, 0 };

static const CUInt32PCharPair g_PartitionFlags[] =
{
  { 0, "Sys" },
  { 1, "Ignore" },
  { 2, "Legacy" },
  { 60, "Win-Read-only" },
  { 62, "Win-Hidden" },
  { 63, "Win-Not-Automount" }
};

static const unsigned kNameLen = 36;

struct CPartition
{
  Byte Type[16];
  Byte Id[16];
  UInt64 FirstLba;
  UInt64 LastLba;
  UInt64 Flags;
  const char *Ext; // detected later
  Byte Name[kNameLen * 2];

  bool IsUnused() const
  {
    for (unsigned i = 0; i < 16; i++)
      if (Type[i] != 0)
        return false;
    return true;
  }

  UInt64 GetSize(unsigned sectorSizeLog) const { return (LastLba - FirstLba + 1) << sectorSizeLog; }
  UInt64 GetPos(unsigned sectorSizeLog) const { return FirstLba << sectorSizeLog; }
  UInt64 GetEnd(unsigned sectorSizeLog) const { return (LastLba + 1) << sectorSizeLog; }

  void Parse(const Byte *p)
  {
    memcpy(Type, p, 16);
    memcpy(Id, p + 16, 16);
    FirstLba = Get64(p + 32);
    LastLba = Get64(p + 40);
    Flags = Get64(p + 48);
    memcpy(Name, p + 56, kNameLen * 2);
    Ext = NULL;
  }
};


struct CPartType
{
  UInt32 Id;
  const char *Ext;
  const char *Type;
};

static const CPartType kPartTypes[] =
{
  // { 0x0, NULL, "Unused" },

  { 0x21686148, NULL, "BIOS Boot" },

  { 0xC12A7328, NULL, "EFI System" },
  { 0x024DEE41, NULL, "MBR" },
      
  { 0xE3C9E316, NULL, "Windows MSR" },
  { 0xEBD0A0A2, NULL, "Windows BDP" },
  { 0x5808C8AA, NULL, "Windows LDM Metadata" },
  { 0xAF9B60A0, NULL, "Windows LDM Data" },
  { 0xDE94BBA4, NULL, "Windows Recovery" },
  // { 0x37AFFC90, NULL, "IBM GPFS" },
  // { 0xE75CAF8F, NULL, "Windows Storage Spaces" },

  { 0x0FC63DAF, NULL, "Linux Data" },
  { 0x0657FD6D, NULL, "Linux Swap" },
  { 0x44479540, NULL, "Linux root (x86)" },
  { 0x4F68BCE3, NULL, "Linux root (x86-64)" },
  { 0x69DAD710, NULL, "Linux root (ARM)" },
  { 0xB921B045, NULL, "Linux root (ARM64)" },
  { 0x993D8D3D, NULL, "Linux root (IA-64)" },
  

  { 0x83BD6B9D, NULL, "FreeBSD Boot" },
  { 0x516E7CB4, NULL, "FreeBSD Data" },
  { 0x516E7CB5, NULL, "FreeBSD Swap" },
  { 0x516E7CB6, "ufs", "FreeBSD UFS" },
  { 0x516E7CB8, NULL, "FreeBSD Vinum" },
  { 0x516E7CB8, "zfs", "FreeBSD ZFS" },

  { 0x48465300, "hfsx", "HFS+" },
  { 0x7C3457EF, "apfs", "APFS" },
};

static int FindPartType(const Byte *guid)
{
  const UInt32 val = Get32(guid);
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kPartTypes); i++)
    if (kPartTypes[i].Id == val)
      return (int)i;
  return -1;
}


static void RawLeGuidToString_Upper(const Byte *g, char *s)
{
  RawLeGuidToString(g, s);
  // MyStringUpper_Ascii(s);
}


Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  CRecordVector<CPartition> _items;
  UInt64 _totalSize;
  unsigned _sectorSizeLog;
  Byte Guid[16];

  CByteBuffer _buffer;

  HRESULT Open2(IInStream *stream);

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const Z7_override
  {
    const CPartition &item = _items[index];
    pos = item.GetPos(_sectorSizeLog);
    size = item.GetSize(_sectorSizeLog);
    return NExtract::NOperationResult::kOK;
  }
};


HRESULT CHandler::Open2(IInStream *stream)
{
  const unsigned kBufSize = 2 << 12;
  _buffer.Alloc(kBufSize);
  RINOK(ReadStream_FALSE(stream, _buffer, kBufSize))
  const Byte *buf = _buffer;
  if (buf[0x1FE] != 0x55 || buf[0x1FF] != 0xAA)
    return S_FALSE;
  {
    for (unsigned sectorSizeLog = 9;; sectorSizeLog += 3)
    {
      if (sectorSizeLog > 12)
        return S_FALSE;
      if (memcmp(buf + ((size_t)1 << sectorSizeLog), k_Signature, k_SignatureSize) == 0)
      {
        buf += ((size_t)1 << sectorSizeLog);
        _sectorSizeLog = sectorSizeLog;
        break;
      }
    }
  }
  const UInt32 kSectorSize = 1u << _sectorSizeLog;
  {
    // if (Get32(buf + 8) != 0x10000) return S_FALSE; // revision
    const UInt32 headerSize = Get32(buf + 12); // = 0x5C usually
    if (headerSize > kSectorSize)
      return S_FALSE;
    const UInt32 crc = Get32(buf + 0x10);
    SetUi32(_buffer + kSectorSize + 0x10, 0)
    if (CrcCalc(_buffer + kSectorSize, headerSize) != crc)
      return S_FALSE;
  }
  // UInt32 reserved = Get32(buf + 0x14);
  const UInt64 curLba = Get64(buf + 0x18);
  if (curLba != 1)
    return S_FALSE;
  const UInt64 backupLba = Get64(buf + 0x20);
  // UInt64 firstUsableLba = Get64(buf + 0x28);
  // UInt64 lastUsableLba = Get64(buf + 0x30);
  memcpy(Guid, buf + 0x38, 16);
  const UInt64 tableLba = Get64(buf + 0x48);
  if (tableLba < 2 || (tableLba >> (63 - _sectorSizeLog)) != 0)
    return S_FALSE;
  const UInt32 numEntries = Get32(buf + 0x50);
  if (numEntries > (1 << 16))
    return S_FALSE;
  const UInt32 entrySize = Get32(buf + 0x54); // = 128 usually
  if (entrySize < 128 || entrySize > (1 << 12))
    return S_FALSE;
  const UInt32 entriesCrc = Get32(buf + 0x58);
  
  const UInt32 tableSize = entrySize * numEntries;
  const UInt32 tableSizeAligned = (tableSize + kSectorSize - 1) & ~(kSectorSize - 1);
  _buffer.Alloc(tableSizeAligned);
  const UInt64 tableOffset = tableLba * kSectorSize;
  RINOK(InStream_SeekSet(stream, tableOffset))
  RINOK(ReadStream_FALSE(stream, _buffer, tableSizeAligned))
  
  if (CrcCalc(_buffer, tableSize) != entriesCrc)
    return S_FALSE;
  
  _totalSize = tableOffset + tableSizeAligned;
  
  for (UInt32 i = 0; i < numEntries; i++)
  {
    CPartition item;
    item.Parse(_buffer + i * entrySize);
    if (item.IsUnused())
      continue;
    if (item.LastLba < item.FirstLba)
      return S_FALSE;
    if ((item.LastLba >> (63 - _sectorSizeLog)) != 0)
      return S_FALSE;
    const UInt64 endPos = item.GetEnd(_sectorSizeLog);
    if (_totalSize < endPos)
      _totalSize = endPos;
    _items.Add(item);
  }

  _buffer.Free();
  {
    if ((backupLba >> (63 - _sectorSizeLog)) != 0)
      return S_FALSE;
    const UInt64 end = (backupLba + 1) * kSectorSize;
    if (_totalSize < end)
      _totalSize = end;
  }
  {
    UInt64 fileEnd;
    RINOK(InStream_GetSize_SeekToEnd(stream, fileEnd))
    
    if (_totalSize < fileEnd)
    {
      const UInt64 rem = fileEnd - _totalSize;
      const UInt64 kRemMax = 1 << 22;
      if (rem <= kRemMax)
      {
        RINOK(InStream_SeekSet(stream, _totalSize))
        bool areThereNonZeros = false;
        UInt64 numZeros = 0;
        if (ReadZeroTail(stream, areThereNonZeros, numZeros, kRemMax) == S_OK)
          if (!areThereNonZeros)
            _totalSize += numZeros;
      }
    }
  }

  return S_OK;
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(stream))
  _stream = stream;

  FOR_VECTOR (fileIndex, _items)
  {
    CPartition &item = _items[fileIndex];
    const int typeIndex = FindPartType(item.Type);
    if (typeIndex < 0)
      continue;
    const CPartType &t = kPartTypes[(unsigned)typeIndex];
    if (t.Ext)
    {
      item.Ext = t.Ext;
      continue;
    }
    if (t.Type && IsString1PrefixedByString2_NoCase_Ascii(t.Type, "Windows"))
    {
      CMyComPtr<ISequentialInStream> inStream;
      if (
          // ((IInArchiveGetStream *)this)->
          GetStream(fileIndex, &inStream) == S_OK && inStream)
      {
        const char *fs = NMbr::GetFileSystem(inStream, item.GetSize(_sectorSizeLog));
        if (fs)
          item.Ext = fs;
      }
    }
  }

  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _sectorSizeLog = 0;
  _totalSize = 0;
  memset(Guid, 0, sizeof(Guid));
  _items.Clear();
  _stream.Release();
  return S_OK;
}

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidFileSystem,
  kpidCharacts,
  kpidOffset,
  kpidId
};

static const Byte kArcProps[] =
{
  kpidSectorSize,
  kpidId
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile:
    {
      if (_items.Size() == 1)
        prop = (UInt32)0;
      break;
    }
    case kpidPhySize: prop = _totalSize; break;
    case kpidSectorSize: prop = (UInt32)((UInt32)1 << _sectorSizeLog); break;
    case kpidId:
    {
      char s[48];
      RawLeGuidToString_Upper(Guid, s);
      prop = s;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
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

  const CPartition &item = _items[index];

  switch (propID)
  {
    case kpidPath:
    {
      // Windows BDP partitions can have identical names.
      // So we add the partition number at front
      UString s;
      s.Add_UInt32(index);
      {
        UString s2;
        for (unsigned i = 0; i < kNameLen; i++)
        {
          wchar_t c = (wchar_t)Get16(item.Name + i * 2);
          if (c == 0)
            break;
          s2 += c;
        }
        if (!s2.IsEmpty())
        {
          s.Add_Dot();
          s += s2;
        }
      }
      {
        s.Add_Dot();
        if (item.Ext)
        {
          AString fs (item.Ext);
          fs.MakeLower_Ascii();
          s += fs;
        }
        else
          s += "img";
      }
      prop = s;
      break;
    }
    
    case kpidSize:
    case kpidPackSize: prop = item.GetSize(_sectorSizeLog); break;
    case kpidOffset: prop = item.GetPos(_sectorSizeLog); break;

    case kpidFileSystem:
    {
      char s[48];
      const char *res;
      const int typeIndex = FindPartType(item.Type);
      if (typeIndex >= 0 && kPartTypes[(unsigned)typeIndex].Type)
        res = kPartTypes[(unsigned)typeIndex].Type;
      else
      {
        RawLeGuidToString_Upper(item.Type, s);
        res = s;
      }
      prop = res;
      break;
    }

    case kpidId:
    {
      char s[48];
      RawLeGuidToString_Upper(item.Id, s);
      prop = s;
      break;
    }

    case kpidCharacts: FLAGS64_TO_PROP(g_PartitionFlags, item.Flags, prop); break;
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

// we suppport signature only for 512-bytes sector.
REGISTER_ARC_I(
  "GPT", "gpt mbr", NULL, 0xCB,
  k_Signature,
  1 << 9,
  0,
  NULL)

}}
