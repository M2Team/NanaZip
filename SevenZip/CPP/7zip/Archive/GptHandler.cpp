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
namespace NGpt {

#define SIGNATURE { 'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T', 0, 0, 1, 0 }
  
static const unsigned k_SignatureSize = 12;
static const Byte k_Signature[k_SignatureSize] = SIGNATURE;

static const UInt32 kSectorSize = 512;

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
  Byte Name[kNameLen * 2];

  bool IsUnused() const
  {
    for (unsigned i = 0; i < 16; i++)
      if (Type[i] != 0)
        return false;
    return true;
  }

  UInt64 GetSize() const { return (LastLba - FirstLba + 1) * kSectorSize; }
  UInt64 GetPos() const { return FirstLba * kSectorSize; }
  UInt64 GetEnd() const { return (LastLba + 1) * kSectorSize; }

  void Parse(const Byte *p)
  {
    memcpy(Type, p, 16);
    memcpy(Id, p + 16, 16);
    FirstLba = Get64(p + 32);
    LastLba = Get64(p + 40);
    Flags = Get64(p + 48);
    memcpy(Name, p + 56, kNameLen * 2);
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
  // { 0x0, 0, "Unused" },

  { 0x21686148, 0, "BIOS Boot" },

  { 0xC12A7328, 0, "EFI System" },
  { 0x024DEE41, 0, "MBR" },
      
  { 0xE3C9E316, 0, "Windows MSR" },
  { 0xEBD0A0A2, 0, "Windows BDP" },
  { 0x5808C8AA, 0, "Windows LDM Metadata" },
  { 0xAF9B60A0, 0, "Windows LDM Data" },
  { 0xDE94BBA4, 0, "Windows Recovery" },
  // { 0x37AFFC90, 0, "IBM GPFS" },
  // { 0xE75CAF8F, 0, "Windows Storage Spaces" },

  { 0x0FC63DAF, 0, "Linux Data" },
  { 0x0657FD6D, 0, "Linux Swap" },

  { 0x83BD6B9D, 0, "FreeBSD Boot" },
  { 0x516E7CB4, 0, "FreeBSD Data" },
  { 0x516E7CB5, 0, "FreeBSD Swap" },
  { 0x516E7CB6, "ufs", "FreeBSD UFS" },
  { 0x516E7CB8, 0, "FreeBSD Vinum" },
  { 0x516E7CB8, "zfs", "FreeBSD ZFS" },

  { 0x48465300, "hfsx", "HFS+" },
  { 0x7C3457EF, "apfs", "APFS" },
};

static int FindPartType(const Byte *guid)
{
  UInt32 val = Get32(guid);
  for (unsigned i = 0; i < ARRAY_SIZE(kPartTypes); i++)
    if (kPartTypes[i].Id == val)
      return i;
  return -1;
}


static void RawLeGuidToString_Upper(const Byte *g, char *s)
{
  RawLeGuidToString(g, s);
  // MyStringUpper_Ascii(s);
}


class CHandler: public CHandlerCont
{
  CRecordVector<CPartition> _items;
  UInt64 _totalSize;
  Byte Guid[16];

  CByteBuffer _buffer;

  HRESULT Open2(IInStream *stream);

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const
  {
    const CPartition &item = _items[index];
    pos = item.GetPos();
    size = item.GetSize();
    return NExtract::NOperationResult::kOK;
  }

public:
  INTERFACE_IInArchive_Cont(;)
};


HRESULT CHandler::Open2(IInStream *stream)
{
  _buffer.Alloc(kSectorSize * 2);
  RINOK(ReadStream_FALSE(stream, _buffer, kSectorSize * 2));
  
  const Byte *buf = _buffer;
  if (buf[0x1FE] != 0x55 || buf[0x1FF] != 0xAA)
    return S_FALSE;
  
  buf += kSectorSize;
  if (memcmp(buf, k_Signature, k_SignatureSize) != 0)
    return S_FALSE;
  {
    // if (Get32(buf + 8) != 0x10000) return S_FALSE; // revision
    UInt32 headerSize = Get32(buf + 12); // = 0x5C usually
    if (headerSize > kSectorSize)
      return S_FALSE;
    UInt32 crc = Get32(buf + 0x10);
    SetUi32(_buffer + kSectorSize + 0x10, 0);
    if (CrcCalc(_buffer + kSectorSize, headerSize) != crc)
      return S_FALSE;
  }
  // UInt32 reserved = Get32(buf + 0x14);
  UInt64 curLba = Get64(buf + 0x18);
  if (curLba != 1)
    return S_FALSE;
  UInt64 backupLba = Get64(buf + 0x20);
  // UInt64 firstUsableLba = Get64(buf + 0x28);
  // UInt64 lastUsableLba = Get64(buf + 0x30);
  memcpy(Guid, buf + 0x38, 16);
  UInt64 tableLba = Get64(buf + 0x48);
  if (tableLba < 2)
    return S_FALSE;
  UInt32 numEntries = Get32(buf + 0x50);
  UInt32 entrySize = Get32(buf + 0x54); // = 128 usually
  UInt32 entriesCrc = Get32(buf + 0x58);
  
  if (entrySize < 128
      || entrySize > (1 << 12)
      || numEntries > (1 << 16)
      || tableLba < 2
      || tableLba >= ((UInt64)1 << (64 - 10)))
    return S_FALSE;
  
  UInt32 tableSize = entrySize * numEntries;
  UInt32 tableSizeAligned = (tableSize + kSectorSize - 1) & ~(kSectorSize - 1);
  _buffer.Alloc(tableSizeAligned);
  UInt64 tableOffset = tableLba * kSectorSize;
  RINOK(stream->Seek(tableOffset, STREAM_SEEK_SET, NULL));
  RINOK(ReadStream_FALSE(stream, _buffer, tableSizeAligned));
  
  if (CrcCalc(_buffer, tableSize) != entriesCrc)
    return S_FALSE;
  
  _totalSize = tableOffset + tableSizeAligned;
  
  for (UInt32 i = 0; i < numEntries; i++)
  {
    CPartition item;
    item.Parse(_buffer + i * entrySize);
    if (item.IsUnused())
      continue;
    UInt64 endPos = item.GetEnd();
    if (_totalSize < endPos)
      _totalSize = endPos;
    _items.Add(item);
  }
  
  {
    const UInt64 end = (backupLba + 1) * kSectorSize;
    if (_totalSize < end)
      _totalSize = end;
  }

  {
    UInt64 fileEnd;
    RINOK(stream->Seek(0, STREAM_SEEK_END, &fileEnd));
    
    if (_totalSize < fileEnd)
    {
      const UInt64 rem = fileEnd - _totalSize;
      const UInt64 kRemMax = 1 << 22;
      if (rem <= kRemMax)
      {
        RINOK(stream->Seek(_totalSize, STREAM_SEEK_SET, NULL));
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

STDMETHODIMP CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */)
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(stream));
  _stream = stream;
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
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
  kpidId
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
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

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _items.Size();
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
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
          s += '.';
          s += s2;
        }
      }
      {
        s += '.';
        const char *ext = NULL;
        int typeIndex = FindPartType(item.Type);
        if (typeIndex >= 0)
          ext = kPartTypes[(unsigned)typeIndex].Ext;
        if (!ext)
          ext = "img";
        s += ext;
      }
      prop = s;
      break;
    }
    
    case kpidSize:
    case kpidPackSize: prop = item.GetSize(); break;
    case kpidOffset: prop = item.GetPos(); break;

    case kpidFileSystem:
    {
      char s[48];
      const char *res;
      int typeIndex = FindPartType(item.Type);
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

REGISTER_ARC_I(
  "GPT", "gpt mbr", NULL, 0xCB,
  k_Signature,
  kSectorSize,
  0,
  NULL)

}}
