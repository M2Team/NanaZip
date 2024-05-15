// ApmHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

// #define Get16(p) GetBe16(p)
#define Get32(p) GetBe32(p)

using namespace NWindows;

namespace NArchive {

namespace NDmg {
  const char *Find_Apple_FS_Ext(const AString &name);
  bool Is_Apple_FS_Or_Unknown(const AString &name);
}

namespace NApm {

static const Byte kSig0 = 'E';
static const Byte kSig1 = 'R';

static const CUInt32PCharPair k_Flags[] =
{
  { 0, "VALID" },
  { 1, "ALLOCATED" },
  { 2, "IN_USE" },
  { 3, "BOOTABLE" },
  { 4, "READABLE" },
  { 5, "WRITABLE" },
  { 6, "OS_PIC_CODE" },
  // { 7, "OS_SPECIFIC_2" }, // "Unused"
  // { 8, "ChainCompatible" }, // "OS_SPECIFIC_1"
  // { 9, "RealDeviceDriver" },
  // { 10, "CanChainToNext" },
  { 30, "MOUNTED_AT_STARTUP" },
  { 31, "STARTUP" }
};

#define DPME_FLAGS_VALID      (1u << 0)
#define DPME_FLAGS_ALLOCATED  (1u << 1)

static const unsigned k_Str_Size = 32;

struct CItem
{
  UInt32 StartBlock;
  UInt32 NumBlocks;
  UInt32 Flags; // pmPartStatus
  char Name[k_Str_Size];
  char Type[k_Str_Size];
  /*
  UInt32 DataStartBlock;
  UInt32 NumDataBlocks;
  UInt32 BootStartBlock;
  UInt32 BootSize;
  UInt32 BootAddr;
  UInt32 BootEntry;
  UInt32 BootChecksum;
  char Processor[16];
  */

  bool Is_Valid_and_Allocated() const
    { return (Flags & (DPME_FLAGS_VALID | DPME_FLAGS_ALLOCATED)) != 0; }

  bool Parse(const Byte *p, UInt32 &numBlocksInMap)
  {
    numBlocksInMap = Get32(p + 4);
    StartBlock = Get32(p + 8);
    NumBlocks = Get32(p + 0xc);
    Flags = Get32(p + 0x58);
    memcpy(Name, p + 0x10, k_Str_Size);
    memcpy(Type, p + 0x30, k_Str_Size);
    if (GetUi32(p) != 0x4d50) // "PM"
      return false;
    /*
    DataStartBlock = Get32(p + 0x50);
    NumDataBlocks = Get32(p + 0x54);
    BootStartBlock = Get32(p + 0x5c);
    BootSize = Get32(p + 0x60);
    BootAddr = Get32(p + 0x64);
    if (Get32(p + 0x68) != 0)
      return false;
    BootEntry = Get32(p + 0x6c);
    if (Get32(p + 0x70) != 0)
      return false;
    BootChecksum = Get32(p + 0x74);
    memcpy(Processor, p + 0x78, 16);
    */
    return true;
  }
};


Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  CRecordVector<CItem> _items;
  unsigned _blockSizeLog;
  UInt32 _numBlocks;
  UInt64 _phySize;
  bool _isArc;

  UInt64 BlocksToBytes(UInt32 i) const { return (UInt64)i << _blockSizeLog; }

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const Z7_override
  {
    const CItem &item = _items[index];
    pos = BlocksToBytes(item.StartBlock);
    size = BlocksToBytes(item.NumBlocks);
    return NExtract::NOperationResult::kOK;
  }
};

static const UInt32 kSectorSize = 512;

// we support only 4 cluster sizes: 512, 1024, 2048, 4096 */

API_FUNC_static_IsArc IsArc_Apm(const Byte *p, size_t size)
{
  if (size < kSectorSize)
    return k_IsArc_Res_NEED_MORE;
  if (GetUi64(p + 8) != 0)
    return k_IsArc_Res_NO;
  UInt32 v = GetUi32(p); // we read as little-endian
  v ^= (kSig0 | (unsigned)kSig1 << 8);
  if ((v & ~((UInt32)0xf << 17)))
    return k_IsArc_Res_NO;
  if ((0x116u >> (v >> 17)) & 1)
    return k_IsArc_Res_YES;
  return k_IsArc_Res_NO;
}
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback * /* callback */))
{
  COM_TRY_BEGIN
  Close();

  Byte buf[kSectorSize];
  unsigned numSectors_in_Cluster;
  {
    RINOK(ReadStream_FALSE(stream, buf, kSectorSize))
    if (GetUi64(buf + 8) != 0)
      return S_FALSE;
    UInt32 v = GetUi32(buf); // we read as little-endian
    v ^= (kSig0 | (unsigned)kSig1 << 8);
    if ((v & ~((UInt32)0xf << 17)))
      return S_FALSE;
    v >>= 16;
    if (v == 0)
      return S_FALSE;
    if (v & (v - 1))
      return S_FALSE;
    const unsigned a = (0x30210u >> v) & 3;
    // a = 0; // for debug
    numSectors_in_Cluster = 1u << a;
    _blockSizeLog = 9 + a;
  }

  UInt32 numBlocks = Get32(buf + 4);
  _numBlocks = numBlocks;

  {
    for (unsigned k = numSectors_in_Cluster; --k != 0;)
    {
      RINOK(ReadStream_FALSE(stream, buf, kSectorSize))
    }
  }

  UInt32 numBlocksInMap = 0;
  
  for (unsigned i = 0;;)
  {
    RINOK(ReadStream_FALSE(stream, buf, kSectorSize))
 
    CItem item;
    
    UInt32 numBlocksInMap2 = 0;
    if (!item.Parse(buf, numBlocksInMap2))
      return S_FALSE;
    if (i == 0)
    {
      numBlocksInMap = numBlocksInMap2;
      if (numBlocksInMap > (1 << 8) || numBlocksInMap == 0)
        return S_FALSE;
    }
    else if (numBlocksInMap2 != numBlocksInMap)
      return S_FALSE;

    const UInt32 finish = item.StartBlock + item.NumBlocks;
    if (finish < item.StartBlock)
      return S_FALSE;
    if (numBlocks < finish)
        numBlocks = finish;
    
    _items.Add(item);
    for (unsigned k = numSectors_in_Cluster; --k != 0;)
    {
      RINOK(ReadStream_FALSE(stream, buf, kSectorSize))
    }
    if (++i == numBlocksInMap)
      break;
  }
  
  _phySize = BlocksToBytes(numBlocks);
  _isArc = true;
  _stream = stream;

  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;
  _phySize = 0;
  _items.Clear();
  _stream.Release();
  return S_OK;
}


static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidOffset,
  kpidCharacts
};

static const Byte kArcProps[] =
{
  kpidClusterSize,
  kpidNumBlocks
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static AString GetString(const char *s)
{
  AString res;
  res.SetFrom_CalcLen(s, k_Str_Size);
  return res;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile:
    {
      int mainIndex = -1;
      FOR_VECTOR (i, _items)
      {
        const CItem &item = _items[i];
        if (!item.Is_Valid_and_Allocated())
          continue;
        AString s (GetString(item.Type));
        if (NDmg::Is_Apple_FS_Or_Unknown(s))
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
    case kpidClusterSize: prop = (UInt32)1 << _blockSizeLog; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidNumBlocks: prop = _numBlocks; break;

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      prop = v;
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
  const CItem &item = _items[index];
  switch (propID)
  {
    case kpidPath:
    {
      AString s (GetString(item.Name));
      if (s.IsEmpty())
        s.Add_UInt32(index);
      AString type (GetString(item.Type));
      {
        const char *ext = NDmg::Find_Apple_FS_Ext(type);
        if (ext)
          type = ext;
      }
      if (!type.IsEmpty())
      {
        s.Add_Dot();
        s += type;
      }
      prop = s;
      break;
    }
    case kpidSize:
    case kpidPackSize:
      prop = BlocksToBytes(item.NumBlocks);
      break;
    case kpidOffset: prop = BlocksToBytes(item.StartBlock); break;
    case kpidCharacts: FLAGS_TO_PROP(k_Flags, item.Flags, prop); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static const Byte k_Signature[] = { kSig0, kSig1 };

REGISTER_ARC_I(
  "APM", "apm", NULL, 0xD4,
  k_Signature,
  0,
  0,
  IsArc_Apm)

}}
